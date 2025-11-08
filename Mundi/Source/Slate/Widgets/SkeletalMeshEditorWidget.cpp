#include "pch.h"
#include "SkeletalMeshEditorWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "SceneComponent.h"
#include "Enums.h"
#include "FOffscreenViewport.h"
#include "FOffscreenViewportClient.h"
#include "World.h"
#include "SelectionManager.h"
#include "Gizmo/OffscreenGizmoActor.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "D3D11RHI.h"
#include "EditorEngine.h"
#include "Renderer.h"
#include "StaticMeshComponent.h"
#include "StaticMeshActor.h"
#include "EmptyActor.h"
#include "DirectionalLightActor.h"
#include "AmbientLightActor.h"
#include "DirectionalLightComponent.h"
#include "AmbientLightComponent.h"
#include "BillboardComponent.h"
#include "Color.h"
#include "USlateManager.h"
#include "PropertyUtils.h"
#include "BoneTransformCalculator.h"
#include "BoneHierarchyWidget.h"
#include "BoneTransformWidget.h"

extern UEditorEngine GEngine;

IMPLEMENT_CLASS(USkeletalMeshEditorWidget)

USkeletalMeshEditorWidget::USkeletalMeshEditorWidget()
	: UWidget("Skeletal Mesh Editor")
{
}

void USkeletalMeshEditorWidget::InitializeEditorWorld()
{
	// 이미 초기화되어 있으면 무시 (싱글톤)
	if (EditorWorld != nullptr)
	{
		UE_LOG("SkeletalMeshEditorWidget: EditorWorld already exists (%p), skipping initialization", EditorWorld);
		return;
	}

	// GWorld 백업 (NewObject나 Initialize가 GWorld를 변경할 수 있음)
	UWorld* OriginalGWorld = GWorld;

	// 전용 Embedded World 생성 (Grid/Gizmo 없음)
	EditorWorld = NewObject<UWorld>();
	// IMPORTANT: Initialize() 전에 WorldType을 Embedded로 설정하여 자동 Gizmo 생성 방지
	EditorWorld->SetWorldType(EWorldType::Embedded);
	EditorWorld->Initialize();

	// DirectionalLight 추가 (기본 조명)
	ADirectionalLightActor* DirLight = EditorWorld->SpawnActor<ADirectionalLightActor>();
	DirLight->SetActorLocation(FVector(10000, 10000, 10000));
	DirLight->SetActorRotation(FQuat::MakeFromEulerZYX(FVector(-45, -45, 0)));
	if (DirLight->GetLightComponent())
	{
		DirLight->GetLightComponent()->SetIntensity(1.0f);
		DirLight->GetLightComponent()->SetLightColor(FLinearColor(1, 1, 1));
	}

	// AmbientLight 추가 (은은한 환경광)
	AAmbientLightActor* AmbLight = EditorWorld->SpawnActor<AAmbientLightActor>();
	AmbLight->SetActorLocation(FVector(10000, 10000, 10000));
	if (AmbLight->GetLightComponent())
	{
		AmbLight->GetLightComponent()->SetIntensity(0.4f);
		AmbLight->GetLightComponent()->SetLightColor(FLinearColor(1, 1, 1));
	}

	// OffscreenGizmoActor 생성 (ImGui 입력 전용)
	BoneGizmo = EditorWorld->SpawnActor<AOffscreenGizmoActor>();
	if (BoneGizmo)
	{
		// NOTE: SpawnActor가 SetWorld()를 호출하여 SelectionManager 자동 초기화됨
		BoneGizmo->SetTickInEditor(true);  // EditorWorld에서 Tick 활성화
		BoneGizmo->SetbRender(false);  // 본 선택 전까지 숨김
		// NOTE: Camera는 Initialize()에서 ViewportClient 생성 후 설정
		BoneGizmo->SetMode(EGizmoMode::Translate);
		BoneGizmo->SetSpace(EGizmoSpace::Local);

		// BillboardComponent 아이콘 숨김 (RootComponent의 SpriteComponent)
		if (USceneComponent* RootComp = BoneGizmo->GetRootComponent())
		{
			if (UBillboardComponent* Sprite = RootComp->GetSpriteComponent())
			{
				Sprite->SetActive(false);
				UE_LOG("SkeletalMeshEditorWidget: Hidden BillboardComponent (SpriteComponent) in BoneGizmo");
			}
		}

		UE_LOG("SkeletalMeshEditorWidget: BoneGizmo created in EditorWorld (%p)", BoneGizmo);
	}
	else
	{
		UE_LOG("SkeletalMeshEditorWidget: Failed to create OffscreenGizmoActor!");
	}

	// GWorld 복원 (메인 에디터 월드로 되돌림)
	GWorld = OriginalGWorld;
}

void USkeletalMeshEditorWidget::Initialize()
{
	// EditorWorld 초기화 (최초 1회만 실행되는 싱글톤)
	InitializeEditorWorld();

	// BoneHierarchyWidget 생성 (SDetailsWindow 패턴)
	HierarchyWidget = NewObject<UBoneHierarchyWidget>();
	HierarchyWidget->Initialize();

	// BoneTransformWidget 생성 (SDetailsWindow 패턴)
	TransformWidget = NewObject<UBoneTransformWidget>();
	TransformWidget->Initialize();

	// D3D11Device 가져오기
	ID3D11Device* Device = GEngine.GetRenderer()->GetRHIDevice()->GetDevice();
	if (!Device)
	{
		UE_LOG("SkeletalMeshEditorWidget: Failed to get D3D11Device");
		return;
	}

	// FOffscreenViewport 생성 (초기 크기 800x600)
	EmbeddedViewport = new FOffscreenViewport();
	if (!EmbeddedViewport->Initialize(0, 0, 800, 600, Device))
	{
		UE_LOG("SkeletalMeshEditorWidget: Failed to initialize FOffscreenViewport");
		delete EmbeddedViewport;
		EmbeddedViewport = nullptr;
		return;
	}

	// FOffscreenViewportClient 생성 (ImGui 입력 처리 지원)
	ViewportClient = new FOffscreenViewportClient();
	ViewportClient->SetViewportType(EViewportType::Perspective);

	// ViewportClient에 EditorWorld 설정 (메인 World와 격리)
	ViewportClient->SetWorld(EditorWorld);

	// Viewport ↔ ViewportClient 연결
	EmbeddedViewport->SetViewportClient(ViewportClient);

	// EditorWorld에서 BoneGizmo 찾기 (InitializeEditorWorld()에서 생성됨, 재사용)
	if (!BoneGizmo && EditorWorld)
	{
		BoneGizmo = EditorWorld->GetActorOfClass<AOffscreenGizmoActor>();
		if (BoneGizmo)
		{
			UE_LOG("SkeletalMeshEditorWidget: Found existing BoneGizmo in EditorWorld (%p)", BoneGizmo);
		}
		else
		{
			UE_LOG("SkeletalMeshEditorWidget: WARNING - BoneGizmo not found in EditorWorld!");
		}
	}

	// BoneGizmo에 Camera 설정
	if (BoneGizmo)
	{
		BoneGizmo->SetCameraActor(ViewportClient->GetCamera());
		CurrentGizmoMode = EGizmoMode::Translate;
		CurrentGizmoSpace = EGizmoSpace::Local;
		UE_LOG("SkeletalMeshEditorWidget: BoneGizmo camera set (%p)", BoneGizmo);
	}
}

void USkeletalMeshEditorWidget::Shutdown()
{
	// 위젯 정리 (SDetailsWindow 패턴)
	if (HierarchyWidget)
	{
		DeleteObject(HierarchyWidget);
		HierarchyWidget = nullptr;
	}

	if (TransformWidget)
	{
		DeleteObject(TransformWidget);
		TransformWidget = nullptr;
	}

	// NOTE: PreviewActor는 여기서 정리하지 않음
	// - EditorWorld는 static이므로 에디터 창을 닫아도 유지됨
	// - 다음에 다른 컴포넌트를 열면 SetTargetComponent()에서 교체됨
	// - 프로그램 종료 시 EditorWorld와 함께 자동 정리됨

	// Viewport 정리
	if (EmbeddedViewport)
	{
		EmbeddedViewport->Cleanup();
		delete EmbeddedViewport;
		EmbeddedViewport = nullptr;
	}

	// ViewportClient 정리
	if (ViewportClient)
	{
		delete ViewportClient;
		ViewportClient = nullptr;
	}

	// Gizmo 정리 (EditorWorld에서 생성했으므로 World가 자동 정리)
	if (BoneGizmo)
	{
		BoneGizmo->SetbRender(false);
		// NOTE: EditorWorld->DestroyActor(BoneGizmo) 하지 않음 (static world는 프로그램 종료 시 정리)
		BoneGizmo = nullptr;
	}

	UE_LOG("SkeletalMeshEditorWidget: Shutdown complete");
}

void USkeletalMeshEditorWidget::SetTargetComponent(USkeletalMeshComponent* Component)
{
	UE_LOG("SetTargetComponent [BEFORE] - GWorld=%p, EditorWorld=%p, MainViewport->World=%p",
		GWorld, EditorWorld,
		USlateManager::GetInstance().GetMainViewport() ? USlateManager::GetInstance().GetMainViewport()->GetViewportClient()->GetWorld() : nullptr);

	// 같은 컴포넌트면 재사용 (불필요한 액터 재생성 방지)
	if (TargetComponent == Component && PreviewActor)
	{
		UE_LOG("SkeletalMeshEditorWidget: Same component, reusing PreviewActor");
		return;
	}

	TargetComponent = Component;
	SelectedBoneIndex = Component ? Component->GetSelectedBoneIndex() : -1;
	bHasUnsavedChanges = false;  // 새 컴포넌트 로드 시 리셋

	if (!EditorWorld)
	{
		UE_LOG("SkeletalMeshEditorWidget: EditorWorld not initialized!");
		return;
	}

	// 기존 PreviewActor 제거 (다른 컴포넌트로 전환 시)
	if (PreviewActor)
	{
		EditorWorld->DestroyActor(PreviewActor);
		PreviewActor = nullptr;
		PreviewMeshComponent = nullptr;
	}

	// 새로운 미리보기 액터 생성
	if (Component && !Component->EditableBones.empty())
	{
		// EmptyActor 생성 (SkeletalMeshComponent 컨테이너)
		PreviewActor = EditorWorld->SpawnActor<AEmptyActor>();
		PreviewActor->SetActorLocation(FVector(0, 0, 0));

		// SkeletalMeshComponent 추가
		PreviewMeshComponent = PreviewActor->CreateDefaultSubobject<USkeletalMeshComponent>("PreviewMesh");

		// TargetComponent의 Bone 데이터 복사 (초기 로드)
		PreviewMeshComponent->EditableBones = Component->EditableBones;
		PreviewMeshComponent->SetSelectedBoneIndex(Component->GetSelectedBoneIndex());

		PreviewActor->SetRootComponent(PreviewMeshComponent);

		// 컴포넌트 등록 (World 전달)
		PreviewMeshComponent->RegisterComponent(EditorWorld);

		// PreviewActor를 선택 상태로 등록 (RenderDebugVolume() 호출되도록)
		EditorWorld->GetSelectionManager()->ClearSelection();
		EditorWorld->GetSelectionManager()->SelectActor(PreviewActor);

		// PreviewActor의 BillboardComponent 아이콘 숨김 (RootComponent의 SpriteComponent)
		if (USceneComponent* RootComp = PreviewActor->GetRootComponent())
		{
			if (UBillboardComponent* Sprite = RootComp->GetSpriteComponent())
			{
				Sprite->SetActive(false);
				UE_LOG("SkeletalMeshEditorWidget: Hidden BillboardComponent (SpriteComponent) in PreviewActor");
			}
		}
	}

	// FBX Asset에서 Bone 데이터 로드
	LoadBonesFromAsset();
}

void USkeletalMeshEditorWidget::LoadBonesFromAsset()
{
	if (!TargetComponent)
		return;

	// Component의 EditableBones를 사용 (이미 더미 데이터 로드됨)
	// Editor에서 EditableBones를 수정하면 Component 렌더링에 즉시 반영됨
}

void USkeletalMeshEditorWidget::Update()
{
	static int UpdateCount = 0;
	if (UpdateCount++ % 60 == 0)  // 1초마다 로그
	{
		UE_LOG("SkeletalMeshEditorWidget::Update called - BoneIdx=%d, PreviewComp=%p, BoneGizmo=%p",
			SelectedBoneIndex, PreviewMeshComponent, BoneGizmo);
	}

	// 선택 동기화 (PreviewMeshComponent 기준)
	if (PreviewMeshComponent && SelectedBoneIndex != PreviewMeshComponent->GetSelectedBoneIndex())
	{
		SelectedBoneIndex = PreviewMeshComponent->GetSelectedBoneIndex();
		UE_LOG("SkeletalMeshEditorWidget: Bone selection changed to index %d", SelectedBoneIndex);
	}

	// 하위 위젯 업데이트 (SDetailsWindow 패턴)
	if (HierarchyWidget)
	{
		HierarchyWidget->Update();
	}

	if (TransformWidget)
	{
		TransformWidget->Update();
	}

	// EditorWorld Tick (BoneGizmo 등 Embedded World의 액터들을 업데이트)
	if (EditorWorld)
	{
		float DeltaTime = ImGui::GetIO().DeltaTime;
		EditorWorld->Tick(DeltaTime);
	}

	// ViewportClient 업데이트
	if (ViewportClient)
	{
		// ImGui DeltaTime 사용
		float DeltaTime = ImGui::GetIO().DeltaTime;
		ViewportClient->Tick(DeltaTime);
	}

	// Gizmo 업데이트 (선택된 본 위치로 이동)
	UpdateGizmoForSelectedBone();

	// NOTE: Gizmo 컴포넌트 가시성은 BoneGizmo->Tick()에서 UpdateComponentVisibility() 호출로 자동 업데이트됨
}

void USkeletalMeshEditorWidget::RenderWidget()
{
	if (!TargetComponent)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "No skeletal mesh component selected!");
		return;
	}

	// 3열 레이아웃 (왼쪽: Hierarchy 25%, 중앙: Viewport 50%, 오른쪽: Transform 25%)
	ImVec2 availSize = ImGui::GetContentRegionAvail();
	float col1Width = availSize.x * 0.25f;
	float col2Width = availSize.x * 0.50f;

	// 왼쪽: Bone Hierarchy (SDetailsWindow 패턴)
	ImGui::BeginChild("BoneHierarchyPane", ImVec2(col1Width, 0), true);
	if (HierarchyWidget)
	{
		// 데이터 주입
		HierarchyWidget->SetPreviewComponent(PreviewMeshComponent);
		HierarchyWidget->SetSelectedBoneIndex(&SelectedBoneIndex);
		// 렌더링
		HierarchyWidget->RenderWidget();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// 중앙: 3D Viewport
	ImGui::BeginChild("ViewportPane", ImVec2(col2Width, 0), true);
	RenderViewport();
	ImGui::EndChild();

	ImGui::SameLine();

	// 오른쪽: Transform Editor (SDetailsWindow 패턴)
	ImGui::BeginChild("TransformEditorPane", ImVec2(0, 0), true);
	if (TransformWidget)
	{
		// 데이터 주입
		TransformWidget->SetPreviewComponent(PreviewMeshComponent);
		TransformWidget->SetSelectedBoneIndex(&SelectedBoneIndex);
		TransformWidget->SetHasUnsavedChanges(&bHasUnsavedChanges);

		// 콜백 설정
		TransformWidget->SetCallbacks(
			[this]() { ApplyChanges(); },   // Apply
			[this]() { CancelChanges(); }   // Cancel
		);

		// 렌더링
		TransformWidget->RenderWidget();
	}
	ImGui::EndChild();
}

void USkeletalMeshEditorWidget::RenderViewport()
{
	if (!EmbeddedViewport || !ViewportClient)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Viewport not initialized!");
		return;
	}

	// Toolbar 렌더링 (Viewport 위에 오버레이)
	RenderViewportToolbar();
	ImGui::Spacing();

	// ImGui 영역 크기 가져오기
	ImVec2 availSize = ImGui::GetContentRegionAvail();
	uint32 NewWidth = static_cast<uint32>(availSize.x);
	uint32 NewHeight = static_cast<uint32>(availSize.y);

	// 최소 크기 보장 (너무 작으면 렌더링 문제 발생)
	if (NewWidth < 64) NewWidth = 64;
	if (NewHeight < 64) NewHeight = 64;

	// Viewport 크기 조정 (크기가 변경되었을 때만)
	if (NewWidth != EmbeddedViewport->GetSizeX() || NewHeight != EmbeddedViewport->GetSizeY())
	{
		EmbeddedViewport->Resize(0, 0, NewWidth, NewHeight);
		UE_LOG("SkeletalMeshEditorWidget: Viewport resized to %dx%d", NewWidth, NewHeight);
	}

	// 3D 씬 렌더링
	EmbeddedViewport->Render();

	// ImGui::Image로 렌더링 결과 표시
	ID3D11ShaderResourceView* SRV = EmbeddedViewport->GetShaderResourceView();
	if (SRV)
	{
		// 이미지 시작 위치 저장 (마우스 좌표 변환용)
		ImVec2 ImagePos = ImGui::GetCursorScreenPos();

		ImGui::Image((void*)SRV, ImVec2((float)NewWidth, (float)NewHeight));

		// InvisibleButton으로 마우스 이벤트 캡처 (CameraBlendEditor 패턴)
		ImGui::SetCursorScreenPos(ImagePos);
		ImGui::InvisibleButton("ViewportCanvas", ImVec2((float)NewWidth, (float)NewHeight));

		// 뷰포트 호버 여부 확인 (뒤에 다른 위젯이 있어도 감지)
		bool bIsHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		bool bIsRightMouseDown = bIsHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

		// FOffscreenViewportClient를 통해 ImGui 입력 처리
		ImGuiIO& IO = ImGui::GetIO();

		// Gizmo 드래그 상태 확인
		bool bIsGizmoDragging = BoneGizmo ? BoneGizmo->GetbIsDragging() : false;

		// 뷰포트에 호버 중이거나 Gizmo 드래그 중일 때 키보드 입력 캡처 (메인 뷰포트로 전달 방지)
		if (bIsHovered || bIsGizmoDragging)
		{
			IO.WantCaptureKeyboard = true;
		}

		// 뷰포트 중앙 좌표 계산 (스크린 좌표)
		int ViewportCenterX = static_cast<int>(ImagePos.x + NewWidth * 0.5f);
		int ViewportCenterY = static_cast<int>(ImagePos.y + NewHeight * 0.5f);

		// ViewportClient에는 우클릭만 전달 (카메라 회전은 우클릭만)
		ViewportClient->ProcessImGuiInput(IO, bIsRightMouseDown, ViewportCenterX, ViewportCenterY);

		// 우클릭 카메라 회전 중에만 마우스 커서 숨김 (무한 스크롤)
		// 좌클릭 Gizmo 드래그 시에는 커서를 보여서 사용자가 범위를 인지할 수 있도록 함
		bool bLeftMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		if (bIsRightMouseDown)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		}

		// Gizmo 모드 전환 키 (뷰포트에 호버 중이거나 Gizmo 드래그 중일 때, 우클릭 드래그 중 아닐 때)
		// NOTE: 우클릭 중에는 Q/W/E가 카메라 이동 키로 사용되므로 Gizmo 모드 전환 비활성화
		if ((bIsHovered || bIsGizmoDragging) && BoneGizmo && !bIsRightMouseDown)
		{
			// 스페이스: 모드 순환 (Translate → Rotate → Scale → Translate)
			if (ImGui::IsKeyPressed(ImGuiKey_Space))
			{
				BoneGizmo->ProcessGizmoModeSwitch();
				CurrentGizmoMode = BoneGizmo->GetMode();
				UE_LOG("SkeletalMeshEditor: Space pressed, mode switched to %d", (int)CurrentGizmoMode);
			}

			// Q: Translate 모드
			if (ImGui::IsKeyPressed(ImGuiKey_Q))
			{
				CurrentGizmoMode = EGizmoMode::Translate;
				BoneGizmo->SetMode(EGizmoMode::Translate);
				UE_LOG("SkeletalMeshEditor: Q pressed, mode set to Translate");
			}

			// W: Rotate 모드
			if (ImGui::IsKeyPressed(ImGuiKey_W))
			{
				CurrentGizmoMode = EGizmoMode::Rotate;
				BoneGizmo->SetMode(EGizmoMode::Rotate);
				UE_LOG("SkeletalMeshEditor: W pressed, mode set to Rotate");
			}

			// E: Scale 모드
			if (ImGui::IsKeyPressed(ImGuiKey_E))
			{
				CurrentGizmoMode = EGizmoMode::Scale;
				BoneGizmo->SetMode(EGizmoMode::Scale);
				UE_LOG("SkeletalMeshEditor: E pressed, mode set to Scale");
			}
		}

		// Gizmo 입력 처리 (우클릭 드래그 중이 아닐 때만)
		if (!bIsRightMouseDown && BoneGizmo && bIsHovered && PreviewMeshComponent)
		{
			// ImGui 좌표 → Viewport 상대 좌표로 변환
			ImVec2 MousePos = ImGui::GetMousePos();
			float RelativeMouseX = MousePos.x - ImagePos.x;
			float RelativeMouseY = MousePos.y - ImagePos.y;

			// ImGui 입력 상태 가져오기 (bLeftMouseDown은 이미 위에서 선언됨)
			bool bLeftMouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

			// 디버깅: 입력 상태 로그
			static int LogCounter = 0;
			if (LogCounter++ % 60 == 0) // 1초마다 (60fps 기준)
			{
				UE_LOG("Gizmo Input: Mouse(%.1f, %.1f), LeftDown=%d, Hovering=%d, Dragging=%d",
					RelativeMouseX, RelativeMouseY,
					bLeftMouseDown, BoneGizmo->GetbIsHovering(), BoneGizmo->GetbIsDragging());
			}

			// OffscreenGizmoActor의 ImGui 입력 처리
			BoneGizmo->ProcessGizmoInteractionImGui(
				ViewportClient->GetCamera(),
				EmbeddedViewport,
				RelativeMouseX,
				RelativeMouseY,
				bLeftMouseDown,
				bLeftMouseReleased
			);

			// Gizmo 드래그 중 실시간으로 본 Transform 업데이트
			bool bIsDragging = BoneGizmo->GetbIsDragging();

			if (bIsDragging && SelectedBoneIndex >= 0)
			{
				// 드래그 중 매 프레임 Gizmo Transform을 본에 적용 (실시간 피드백)
				FTransform NewWorldTransform = BoneGizmo->GetActorTransform();
				FBoneTransformCalculator::SetBoneWorldTransform(PreviewMeshComponent, SelectedBoneIndex, NewWorldTransform);
				bHasUnsavedChanges = true;
			}
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Viewport SRV is null!");
	}
}

void USkeletalMeshEditorWidget::RenderViewportToolbar()
{
	if (!BoneGizmo)
		return;

	// Gizmo 모드 버튼
	EGizmoMode CurrentMode = BoneGizmo->GetMode();

	// Translate 버튼
	bool bIsTranslateActive = (CurrentMode == EGizmoMode::Translate);
	if (bIsTranslateActive)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

	if (ImGui::Button("Translate (W)", ImVec2(100, 0)))
	{
		UE_LOG("SkeletalMeshEditor: Translate button clicked");
		CurrentGizmoMode = EGizmoMode::Translate;
		BoneGizmo->SetMode(EGizmoMode::Translate);
		UE_LOG("SkeletalMeshEditor: Mode set to Translate, Gizmo mode = %d", (int)BoneGizmo->GetMode());
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Move bone [W]");

	if (bIsTranslateActive)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentMode == EGizmoMode::Rotate);
	if (bIsRotateActive)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

	if (ImGui::Button("Rotate (E)", ImVec2(100, 0)))
	{
		UE_LOG("SkeletalMeshEditor: Rotate button clicked");
		CurrentGizmoMode = EGizmoMode::Rotate;
		BoneGizmo->SetMode(EGizmoMode::Rotate);
		UE_LOG("SkeletalMeshEditor: Mode set to Rotate, Gizmo mode = %d", (int)BoneGizmo->GetMode());
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Rotate bone [E]");

	if (bIsRotateActive)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentMode == EGizmoMode::Scale);
	if (bIsScaleActive)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

	if (ImGui::Button("Scale (R)", ImVec2(100, 0)))
	{
		UE_LOG("SkeletalMeshEditor: Scale button clicked");
		CurrentGizmoMode = EGizmoMode::Scale;
		BoneGizmo->SetMode(EGizmoMode::Scale);
		UE_LOG("SkeletalMeshEditor: Mode set to Scale, Gizmo mode = %d", (int)BoneGizmo->GetMode());
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Scale bone [R]");

	if (bIsScaleActive)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// Local/World 공간 전환 버튼
	const char* SpaceLabel = (CurrentGizmoSpace == EGizmoSpace::Local) ? "Local" : "World";
	if (ImGui::Button(SpaceLabel, ImVec2(60, 0)))
	{
		UE_LOG("SkeletalMeshEditor: Space toggle button clicked");
		CurrentGizmoSpace = (CurrentGizmoSpace == EGizmoSpace::Local) ? EGizmoSpace::World : EGizmoSpace::Local;
		BoneGizmo->SetSpace(CurrentGizmoSpace);
		UE_LOG("SkeletalMeshEditor: Space set to %s", (CurrentGizmoSpace == EGizmoSpace::Local) ? "Local" : "World");
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Local/World space");
}

void USkeletalMeshEditorWidget::HandleViewportInput()
{
	// TODO: 마우스/키보드 입력 처리
}

void USkeletalMeshEditorWidget::PerformBonePicking(float MouseX, float MouseY)
{
	// TODO: Ray-casting으로 본 선택
}

void USkeletalMeshEditorWidget::UpdateGizmoForSelectedBone()
{
	if (!BoneGizmo || !PreviewMeshComponent || !EditorWorld)
		return;

	USelectionManager* SelectionManager = EditorWorld->GetSelectionManager();
	if (!SelectionManager)
		return;

	// 본이 선택되지 않았으면 Gizmo만 숨김 (PreviewActor는 선택 상태 유지하여 디버그 본 표시)
	if (SelectedBoneIndex < 0 || SelectedBoneIndex >= PreviewMeshComponent->EditableBones.size())
	{
		BoneGizmo->SetbRender(false);
		// PreviewActor는 선택 상태로 유지 (RenderDebugVolume 계속 호출되도록)
		SelectionManager->ClearSelection();
		SelectionManager->SelectActor(PreviewActor);
		return;
	}

	// EditorWorld의 SelectionManager에 PreviewMeshComponent 선택
	// (GizmoActor의 UpdateComponentVisibility가 이를 확인하여 Gizmo를 표시함)
	SelectionManager->ClearSelection();
	SelectionManager->SelectComponent(PreviewMeshComponent);

	// 디버깅: 선택 상태 확인
	USceneComponent* Selected = SelectionManager->GetSelectedComponent();
	UE_LOG("UpdateGizmoForSelectedBone: BoneIdx=%d, SelMgr=%p, PreviewComp=%p, Selected=%p, Mode=%d",
		SelectedBoneIndex, SelectionManager, PreviewMeshComponent, Selected, (int)CurrentGizmoMode);

	// 선택된 본의 World Transform 계산
	FTransform BoneWorldTransform = FBoneTransformCalculator::GetBoneWorldTransform(PreviewMeshComponent, SelectedBoneIndex);

	// Gizmo를 본 위치로 이동
	BoneGizmo->SetActorLocation(BoneWorldTransform.Translation);

	// Gizmo Space에 따라 회전 설정
	if (CurrentGizmoSpace == EGizmoSpace::Local || CurrentGizmoMode == EGizmoMode::Scale)
	{
		// Local 모드 또는 Scale 모드: 본의 회전을 Gizmo에 적용
		BoneGizmo->SetActorRotation(BoneWorldTransform.Rotation);
	}
	else // World 모드
	{
		// World 모드: 월드 축에 정렬 (항등 회전)
		BoneGizmo->SetActorRotation(FQuat::Identity());
	}

	BoneGizmo->SetbRender(true);
}

void USkeletalMeshEditorWidget::ApplyChanges()
{
	if (!TargetComponent || !PreviewMeshComponent)
	{
		UE_LOG("SkeletalMeshEditorWidget: Cannot apply - missing component");
		return;
	}

	// PreviewMeshComponent → TargetComponent 복사
	TargetComponent->EditableBones = PreviewMeshComponent->EditableBones;
	TargetComponent->SetSelectedBoneIndex(PreviewMeshComponent->GetSelectedBoneIndex());

	bHasUnsavedChanges = false;  // 저장 완료

	UE_LOG("SkeletalMeshEditorWidget: Applied changes to TargetComponent");
}

void USkeletalMeshEditorWidget::CancelChanges()
{
	if (!TargetComponent || !PreviewMeshComponent)
	{
		UE_LOG("SkeletalMeshEditorWidget: Cannot cancel - missing component");
		return;
	}

	// TargetComponent → PreviewMeshComponent 복사 (되돌리기)
	PreviewMeshComponent->EditableBones = TargetComponent->EditableBones;
	PreviewMeshComponent->SetSelectedBoneIndex(TargetComponent->GetSelectedBoneIndex());

	bHasUnsavedChanges = false;  // 변경사항 취소됨

	// Gizmo 위치를 복구된 본 위치로 업데이트
	UpdateGizmoForSelectedBone();

	UE_LOG("SkeletalMeshEditorWidget: Cancelled changes (reverted to original)");
}
