#include "pch.h"
#include "SkeletalMeshEditorWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "Enums.h"
#include "FOffscreenViewport.h"
#include "FOffscreenViewportClient.h"
#include "World.h"
#include "SelectionManager.h"
#include "Gizmo/GizmoActor.h"
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
#include "Color.h"
#include "USlateManager.h"

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

	// GWorld 복원 (메인 에디터 월드로 되돌림)
	GWorld = OriginalGWorld;
}

void USkeletalMeshEditorWidget::Initialize()
{
	// EditorWorld 초기화 (최초 1회만 실행되는 싱글톤)
	InitializeEditorWorld();

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
}

void USkeletalMeshEditorWidget::Shutdown()
{
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

	// Gizmo 정리 (나중에 구현)
	BoneGizmo = nullptr;

	UE_LOG("SkeletalMeshEditorWidget: Shutdown complete");
}

void USkeletalMeshEditorWidget::SetTargetComponent(USkeletalMeshComponent* Component)
{
	UE_LOG("SetTargetComponent [BEFORE] - GWorld=%p, EditorWorld=%p, MainViewport->World=%p",
		GWorld, EditorWorld,
		USlateManager::GetInstance().GetMainViewport() ? USlateManager::GetInstance().GetMainViewport()->GetViewportClient()->GetWorld() : nullptr);

	TargetComponent = Component;
	SelectedBoneIndex = Component ? Component->SelectedBoneIndex : -1;

	if (!EditorWorld)
	{
		UE_LOG("SkeletalMeshEditorWidget: EditorWorld not initialized!");
		return;
	}

	// 기존 PreviewActor 제거
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
		PreviewMeshComponent->SelectedBoneIndex = Component->SelectedBoneIndex;

		PreviewActor->SetRootComponent(PreviewMeshComponent);

		// 컴포넌트 등록 (World 전달)
		PreviewMeshComponent->RegisterComponent(EditorWorld);

		// PreviewActor를 선택 상태로 등록 (RenderDebugVolume() 호출되도록)
		EditorWorld->GetSelectionManager()->ClearSelection();
		EditorWorld->GetSelectionManager()->SelectActor(PreviewActor);
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
	// 선택 동기화 (PreviewMeshComponent 기준)
	if (PreviewMeshComponent && SelectedBoneIndex != PreviewMeshComponent->SelectedBoneIndex)
	{
		SelectedBoneIndex = PreviewMeshComponent->SelectedBoneIndex;
	}

	// ViewportClient 업데이트
	if (ViewportClient)
	{
		// ImGui DeltaTime 사용
		float DeltaTime = ImGui::GetIO().DeltaTime;
		ViewportClient->Tick(DeltaTime);
	}

	// TODO: Gizmo 업데이트
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

	// 왼쪽: Bone Hierarchy
	ImGui::BeginChild("BoneHierarchyPane", ImVec2(col1Width, 0), true);
	RenderBoneHierarchy();
	ImGui::EndChild();

	ImGui::SameLine();

	// 중앙: 3D Viewport
	ImGui::BeginChild("ViewportPane", ImVec2(col2Width, 0), true);
	RenderViewport();
	ImGui::EndChild();

	ImGui::SameLine();

	// 오른쪽: Transform Editor
	ImGui::BeginChild("TransformEditorPane", ImVec2(0, 0), true);
	RenderTransformEditor();
	ImGui::EndChild();
}

void USkeletalMeshEditorWidget::RenderBoneHierarchy()
{
	ImGui::Text("Bone Hierarchy");
	ImGui::Separator();
	ImGui::Spacing();

	if (!PreviewMeshComponent || PreviewMeshComponent->EditableBones.empty())
	{
		ImGui::TextDisabled("No bones loaded");
		return;
	}

	// Root bone부터 재귀 렌더링
	for (size_t i = 0; i < PreviewMeshComponent->EditableBones.size(); ++i)
	{
		if (PreviewMeshComponent->EditableBones[i].ParentIndex < 0)  // Root bone
		{
			RenderBoneTreeNode(static_cast<int32>(i));
		}
	}
}

void USkeletalMeshEditorWidget::RenderBoneTreeNode(int32 BoneIndex)
{
	if (!PreviewMeshComponent || BoneIndex < 0 || BoneIndex >= PreviewMeshComponent->EditableBones.size())
		return;

	FBone& Bone = PreviewMeshComponent->EditableBones[BoneIndex];

	// Tree node flags
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
	                           ImGuiTreeNodeFlags_SpanAvailWidth;

	if (BoneIndex == SelectedBoneIndex)
		flags |= ImGuiTreeNodeFlags_Selected;

	// 자식 bone 찾기
	bool bHasChildren = false;
	for (size_t i = 0; i < PreviewMeshComponent->EditableBones.size(); ++i)
	{
		if (PreviewMeshComponent->EditableBones[i].ParentIndex == BoneIndex)
		{
			bHasChildren = true;
			break;
		}
	}

	if (!bHasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf;

	// Tree node 렌더링
	ImGui::PushID(BoneIndex);
	bool bOpen = ImGui::TreeNodeEx(Bone.Name.c_str(), flags);

	// 클릭 시 선택
	if (ImGui::IsItemClicked())
	{
		SelectedBoneIndex = BoneIndex;
		if (PreviewMeshComponent)
			PreviewMeshComponent->SelectedBoneIndex = BoneIndex;
	}

	// 자식 bone 재귀 렌더링
	if (bOpen)
	{
		if (bHasChildren)
		{
			for (size_t i = 0; i < PreviewMeshComponent->EditableBones.size(); ++i)
			{
				if (PreviewMeshComponent->EditableBones[i].ParentIndex == BoneIndex)
				{
					RenderBoneTreeNode(static_cast<int32>(i));
				}
			}
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void USkeletalMeshEditorWidget::RenderTransformEditor()
{
	ImGui::Text("Transform Editor");
	ImGui::Separator();
	ImGui::Spacing();

	if (!PreviewMeshComponent || SelectedBoneIndex < 0 || SelectedBoneIndex >= PreviewMeshComponent->EditableBones.size())
	{
		ImGui::TextDisabled("Select a bone from the hierarchy");
		return;
	}

	FBone& SelectedBone = PreviewMeshComponent->EditableBones[SelectedBoneIndex];

	ImGui::Text("Bone: %s (Index: %d)", SelectedBone.Name.c_str(), SelectedBoneIndex);
	ImGui::Separator();
	ImGui::Spacing();

	// Local Transform 편집 (PropertyRenderer 스타일)
	ImGui::Text("Local Transform:");
	ImGui::Spacing();

	// Position
	ImGui::DragFloat3("Position", &SelectedBone.LocalPosition.X, 0.1f);

	// Rotation (Euler 저장 패턴으로 gimbal lock UI 문제 방지)
	// NOTE: SceneComponent와 동일한 패턴 - Euler 입력값을 별도 저장하여 UI 값 뒤집힘 방지
	FVector euler = SelectedBone.GetLocalRotationEuler();
	if (ImGui::DragFloat3("Rotation", &euler.X, 1.0f))
	{
		SelectedBone.SetLocalRotationEuler(euler);
	}

	// Scale
	ImGui::DragFloat3("Scale", &SelectedBone.LocalScale.X, 0.01f);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Apply / Cancel 버튼
	if (ImGui::Button("Apply", ImVec2(100, 0)))
	{
		ApplyChanges();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(100, 0)))
	{
		CancelChanges();
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Apply: Save changes to main editor");
	ImGui::TextDisabled("Cancel: Revert to original");
}

void USkeletalMeshEditorWidget::RenderViewport()
{
	if (!EmbeddedViewport || !ViewportClient)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Viewport not initialized!");
		return;
	}

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

		// 우클릭 드래그 중: 마우스 커서 숨기고 키보드 입력 차단
		if (bIsRightMouseDown)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			IO.WantCaptureKeyboard = true;
		}

		// 뷰포트 중앙 좌표 계산 (스크린 좌표)
		int ViewportCenterX = static_cast<int>(ImagePos.x + NewWidth * 0.5f);
		int ViewportCenterY = static_cast<int>(ImagePos.y + NewHeight * 0.5f);

		// 입력 처리 (무한 스크롤 지원)
		ViewportClient->ProcessImGuiInput(IO, bIsRightMouseDown, ViewportCenterX, ViewportCenterY);
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Viewport SRV is null!");
	}
}

void USkeletalMeshEditorWidget::RenderViewportToolbar()
{
	// TODO: Gizmo 모드 전환 버튼 등
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
	// TODO: 선택된 본에 Gizmo 부착
}

bool USkeletalMeshEditorWidget::HasChildren(int32 BoneIndex) const
{
	if (!PreviewMeshComponent)
		return false;

	for (const auto& Bone : PreviewMeshComponent->EditableBones)
	{
		if (Bone.ParentIndex == BoneIndex)
			return true;
	}
	return false;
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
	TargetComponent->SelectedBoneIndex = PreviewMeshComponent->SelectedBoneIndex;

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
	PreviewMeshComponent->SelectedBoneIndex = TargetComponent->SelectedBoneIndex;

	UE_LOG("SkeletalMeshEditorWidget: Cancelled changes (reverted to original)");
}
