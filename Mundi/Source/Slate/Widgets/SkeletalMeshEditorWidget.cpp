#include "pch.h"
#include "SkeletalMeshEditorWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.h"
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
#include "ViewportWidget.h"

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
			}
		}
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

	// ViewportWidget 생성 (SDetailsWindow 패턴)
	ViewportWidget = NewObject<UViewportWidget>();
	ViewportWidget->Initialize();

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
		if (!BoneGizmo)
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
	}
}

void USkeletalMeshEditorWidget::Shutdown()
{
	// 종료 시 자동으로 변경사항 되돌리기 (미리보기만 수행하므로 항상 revert)
	RevertChanges();

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

	if (ViewportWidget)
	{
		DeleteObject(ViewportWidget);
		ViewportWidget = nullptr;
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
}

void USkeletalMeshEditorWidget::SetTargetSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	// 같은 Skeletal Mesh면 재사용 (불필요한 액터 재생성 방지)
	if (TargetSkeletalMesh == SkeletalMesh && PreviewActor)
	{
		return;
	}

	TargetSkeletalMesh = SkeletalMesh;
	SelectedBoneIndex = -1;

	if (!EditorWorld)
	{
		UE_LOG("SkeletalMeshEditorWidget: EditorWorld not initialized!");
		return;
	}

	// 기존 PreviewActor 제거 (다른 Skeletal Mesh로 전환 시)
	if (PreviewActor)
	{
		EditorWorld->DestroyActor(PreviewActor);
		PreviewActor = nullptr;
		PreviewMeshComponent = nullptr;
	}

	// 새로운 미리보기 액터 생성
	if (SkeletalMesh && SkeletalMesh->IsValidResource())
	{
		// EmptyActor 생성 (SkeletalMeshComponent 컨테이너)
		PreviewActor = EditorWorld->SpawnActor<AEmptyActor>();
		PreviewActor->SetActorLocation(FVector(0, 0, 0));

		// SkeletalMeshComponent 생성
		PreviewMeshComponent = NewObject<USkeletalMeshComponent>();
		PreviewMeshComponent->SetOwner(PreviewActor);
		PreviewActor->AddOwnedComponent(PreviewMeshComponent);
		PreviewActor->SetRootComponent(PreviewMeshComponent);

		// Skeletal Mesh 설정 (이미 로드된 객체를 직접 전달)
		PreviewMeshComponent->SetSkeletalMesh(SkeletalMesh);

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
			}
		}
	}

	// FBX Asset에서 Bone 데이터 로드
	LoadBonesFromAsset();
}

void USkeletalMeshEditorWidget::LoadBonesFromAsset()
{
	// Component가 SkeletalMesh 로드 시 자동으로 EditableBones를 초기화함
}

void USkeletalMeshEditorWidget::Update()
{
	// 선택 동기화 (PreviewMeshComponent 기준)
	if (PreviewMeshComponent && SelectedBoneIndex != PreviewMeshComponent->GetSelectedBoneIndex())
	{
		SelectedBoneIndex = PreviewMeshComponent->GetSelectedBoneIndex();
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

	if (ViewportWidget)
	{
		ViewportWidget->Update();
	}

	// EditorWorld Tick (BoneGizmo 등 Embedded World의 액터들을 업데이트)
	// CRITICAL: PreviewComponent->TickComponent()에서 UpdateBoneMatrices() 호출됨
	if (EditorWorld)
	{
		float DeltaTime = ImGui::GetIO().DeltaTime;
		EditorWorld->Tick(DeltaTime);
	}

	// CRITICAL: Gizmo 업데이트는 EditorWorld->Tick() 이후에 호출되어야 함
	// 이유: ComponentSpaceTransforms가 UpdateBoneMatrices()에서 업데이트되어야 함
	if (ViewportWidget)
	{
		ViewportWidget->UpdateGizmoForSelectedBone();
	}

	// ViewportClient 업데이트
	if (ViewportClient)
	{
		// ImGui DeltaTime 사용
		float DeltaTime = ImGui::GetIO().DeltaTime;
		ViewportClient->Tick(DeltaTime);
	}

	// NOTE: Gizmo 컴포넌트 가시성은 BoneGizmo->Tick()에서 UpdateComponentVisibility() 호출로 자동 업데이트됨
}

void USkeletalMeshEditorWidget::RenderWidget()
{
	if (!TargetSkeletalMesh)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "No skeletal mesh selected!");
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

	// 중앙: 3D Viewport (SDetailsWindow 패턴)
	ImGui::BeginChild("ViewportPane", ImVec2(col2Width, 0), true);
	if (ViewportWidget)
	{
		// 데이터 주입
		ViewportWidget->SetViewport(EmbeddedViewport);
		ViewportWidget->SetViewportClient(ViewportClient);
		ViewportWidget->SetGizmo(BoneGizmo);
		ViewportWidget->SetPreviewComponent(PreviewMeshComponent);
		ViewportWidget->SetEditorWorld(EditorWorld);
		ViewportWidget->SetPreviewActor(PreviewActor);
		ViewportWidget->SetSelectedBoneIndex(&SelectedBoneIndex);
		ViewportWidget->SetGizmoMode(&CurrentGizmoMode);
		ViewportWidget->SetGizmoSpace(&CurrentGizmoSpace);
		// 렌더링
		ViewportWidget->RenderWidget();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// 오른쪽: Transform Editor (SDetailsWindow 패턴)
	ImGui::BeginChild("TransformEditorPane", ImVec2(0, 0), true);
	if (TransformWidget)
	{
		// 데이터 주입
		TransformWidget->SetPreviewComponent(PreviewMeshComponent);
		TransformWidget->SetSelectedBoneIndex(&SelectedBoneIndex);

		// 콜백 설정 (Revert만)
		TransformWidget->SetRevertCallback([this]() { RevertChanges(); });

		// 렌더링
		TransformWidget->RenderWidget();
	}
	ImGui::EndChild();
}

void USkeletalMeshEditorWidget::HandleViewportInput()
{
	// TODO: 마우스/키보드 입력 처리
}

void USkeletalMeshEditorWidget::PerformBonePicking(float MouseX, float MouseY)
{
	// TODO: Ray-casting으로 본 선택
}

void USkeletalMeshEditorWidget::RevertChanges()
{
	if (!TargetSkeletalMesh || !PreviewMeshComponent)
	{
		UE_LOG("SkeletalMeshEditorWidget: Cannot revert - missing skeletal mesh or component");
		return;
	}

	// BoneSpaceTransforms를 Bind Pose로 복구
	PreviewMeshComponent->RevertToBindPose();
	PreviewMeshComponent->SetSelectedBoneIndex(-1);
	SelectedBoneIndex = -1;

	// Transform Widget의 Euler 캐시 리셋
	if (TransformWidget)
	{
		TransformWidget->ResetEulerCache();
	}

	// Gizmo 위치를 복구된 본 위치로 업데이트 (ViewportWidget을 통해)
	if (ViewportWidget)
	{
		ViewportWidget->UpdateGizmoForSelectedBone();
	}
}
