#include "pch.h"
#include "SkeletalMeshEditorWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "Enums.h"
#include "FOffscreenViewport.h"
#include "FViewportClient.h"
#include "World.h"
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
	UE_LOG("SkeletalMeshEditorWidget: [BEFORE NewObject] GWorld=%p", GWorld);

	// 전용 World 생성
	EditorWorld = NewObject<UWorld>();
	UE_LOG("SkeletalMeshEditorWidget: [AFTER NewObject] GWorld=%p, EditorWorld=%p", GWorld, EditorWorld);

	EditorWorld->Initialize();
	UE_LOG("SkeletalMeshEditorWidget: [AFTER Initialize] GWorld=%p", GWorld);

	// DirectionalLight 추가 (기본 조명)
	ADirectionalLightActor* DirLight = EditorWorld->SpawnActor<ADirectionalLightActor>();
	DirLight->SetActorLocation(FVector(0, 0, 1000));
	DirLight->SetActorRotation(FQuat::MakeFromEulerZYX(FVector(-45, 0, 0)));
	if (DirLight->GetLightComponent())
	{
		DirLight->GetLightComponent()->SetIntensity(1.0f);
		DirLight->GetLightComponent()->SetLightColor(FLinearColor(1, 1, 1));
	}

	// AmbientLight 추가 (은은한 환경광)
	AAmbientLightActor* AmbLight = EditorWorld->SpawnActor<AAmbientLightActor>();
	if (AmbLight->GetLightComponent())
	{
		AmbLight->GetLightComponent()->SetIntensity(0.3f);
		AmbLight->GetLightComponent()->SetLightColor(FLinearColor(1, 1, 1));
	}

	// GWorld 복원 (메인 에디터 월드로 되돌림)
	GWorld = OriginalGWorld;
	UE_LOG("SkeletalMeshEditorWidget: Restored GWorld=%p", GWorld);
	UE_LOG("SkeletalMeshEditorWidget: [VERIFY] MainViewport->World=%p after GWorld restore",
		USlateManager::GetInstance().GetMainViewport() ? USlateManager::GetInstance().GetMainViewport()->GetViewportClient()->GetWorld() : nullptr);

	UE_LOG("SkeletalMeshEditorWidget: EditorWorld initialized with lights");
}

void USkeletalMeshEditorWidget::Initialize()
{
	// 디버깅: 메인 뷰포트들의 World 주소 확인 (변경 전)
	SViewportWindow* MainVP = USlateManager::GetInstance().GetMainViewport();
	UE_LOG("SkeletalMeshEditorWidget::Initialize [BEFORE] - MainViewport->ViewportClient->World=%p",
		MainVP ? MainVP->GetViewportClient()->GetWorld() : nullptr);

	// EditorWorld 초기화 (최초 1회만 실행되는 싱글톤)
	InitializeEditorWorld();

	// 디버깅: EditorWorld와 GWorld 주소 확인
	UE_LOG("SkeletalMeshEditorWidget::Initialize - EditorWorld=%p, GWorld=%p", EditorWorld, GWorld);

	// 디버깅: 메인 뷰포트들의 World 주소 확인 (변경 후)
	UE_LOG("SkeletalMeshEditorWidget::Initialize [AFTER InitializeEditorWorld] - MainViewport->ViewportClient->World=%p",
		MainVP ? MainVP->GetViewportClient()->GetWorld() : nullptr);

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

	// FViewportClient 생성
	ViewportClient = new FViewportClient();
	ViewportClient->SetViewportType(EViewportType::Perspective);

	// ViewportClient에 EditorWorld 설정 (메인 World와 격리)
	ViewportClient->SetWorld(EditorWorld);

	// 디버깅: SetWorld 후 ViewportClient의 World 주소 확인
	UE_LOG("SkeletalMeshEditorWidget::Initialize - ViewportClient->World=%p", ViewportClient->GetWorld());

	// 디버깅: 메인 뷰포트들의 World 주소 확인 (에디터 뷰포트 생성 후)
	UE_LOG("SkeletalMeshEditorWidget::Initialize [AFTER Editor Viewport Created] - MainViewport->ViewportClient->World=%p",
		MainVP ? MainVP->GetViewportClient()->GetWorld() : nullptr);

	// Viewport ↔ ViewportClient 연결
	EmbeddedViewport->SetViewportClient(ViewportClient);

	UE_LOG("SkeletalMeshEditorWidget: Viewport initialized successfully (800x600) with EditorWorld");
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
	}

	// 새로운 미리보기 액터 생성
	if (Component && !Component->EditableBones.empty())
	{
		// EmptyActor 생성 (SkeletalMeshComponent 컨테이너)
		PreviewActor = EditorWorld->SpawnActor<AEmptyActor>();
		PreviewActor->SetActorLocation(FVector(0, 0, 0));

		// SkeletalMeshComponent 추가
		USkeletalMeshComponent* PreviewMeshComp = PreviewActor->CreateDefaultSubobject<USkeletalMeshComponent>("PreviewMesh");

		// TargetComponent의 Bone 데이터 복사
		PreviewMeshComp->EditableBones = Component->EditableBones;

		PreviewActor->SetRootComponent(PreviewMeshComp);

		// 컴포넌트 등록 (World 전달)
		PreviewMeshComp->RegisterComponent(EditorWorld);

		UE_LOG("SkeletalMeshEditorWidget: PreviewActor created in EditorWorld with %d bones", PreviewMeshComp->GetBoneCount());
	}

	UE_LOG("SetTargetComponent [AFTER] - GWorld=%p, EditorWorld=%p, MainViewport->World=%p",
		GWorld, EditorWorld,
		USlateManager::GetInstance().GetMainViewport() ? USlateManager::GetInstance().GetMainViewport()->GetViewportClient()->GetWorld() : nullptr);

	// FBX Asset에서 Bone 데이터 로드
	LoadBonesFromAsset();
}

void USkeletalMeshEditorWidget::LoadBonesFromAsset()
{
	if (!TargetComponent)
		return;

	// Component의 EditableBones를 사용 (이미 더미 데이터 로드됨)
	// Editor에서 EditableBones를 수정하면 Component 렌더링에 즉시 반영됨
	UE_LOG("SkeletalMeshEditorWidget: Using Component's EditableBones (%d bones)",
		TargetComponent->EditableBones.size());
}

void USkeletalMeshEditorWidget::Update()
{
	// 선택 동기화 (Component에서 변경되었을 때)
	if (TargetComponent && SelectedBoneIndex != TargetComponent->SelectedBoneIndex)
	{
		SelectedBoneIndex = TargetComponent->SelectedBoneIndex;
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
	static int renderWidgetCount = 0;
	if (renderWidgetCount++ % 60 == 0)
	{
		UE_LOG("SkeletalMeshEditorWidget::RenderWidget - TargetComponent=%p", TargetComponent);
	}

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

	if (!TargetComponent || TargetComponent->EditableBones.empty())
	{
		ImGui::TextDisabled("No bones loaded");
		return;
	}

	// Root bone부터 재귀 렌더링
	for (size_t i = 0; i < TargetComponent->EditableBones.size(); ++i)
	{
		if (TargetComponent->EditableBones[i].ParentIndex < 0)  // Root bone
		{
			RenderBoneTreeNode(static_cast<int32>(i));
		}
	}
}

void USkeletalMeshEditorWidget::RenderBoneTreeNode(int32 BoneIndex)
{
	if (!TargetComponent || BoneIndex < 0 || BoneIndex >= TargetComponent->EditableBones.size())
		return;

	FBone& Bone = TargetComponent->EditableBones[BoneIndex];

	// Tree node flags
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
	                           ImGuiTreeNodeFlags_SpanAvailWidth;

	if (BoneIndex == SelectedBoneIndex)
		flags |= ImGuiTreeNodeFlags_Selected;

	// 자식 bone 찾기
	bool bHasChildren = false;
	for (size_t i = 0; i < TargetComponent->EditableBones.size(); ++i)
	{
		if (TargetComponent->EditableBones[i].ParentIndex == BoneIndex)
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
		if (TargetComponent)
			TargetComponent->SelectedBoneIndex = BoneIndex;
	}

	// 자식 bone 재귀 렌더링
	if (bOpen)
	{
		if (bHasChildren)
		{
			for (size_t i = 0; i < TargetComponent->EditableBones.size(); ++i)
			{
				if (TargetComponent->EditableBones[i].ParentIndex == BoneIndex)
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

	if (!TargetComponent || SelectedBoneIndex < 0 || SelectedBoneIndex >= TargetComponent->EditableBones.size())
	{
		ImGui::TextDisabled("Select a bone from the hierarchy");
		return;
	}

	FBone& SelectedBone = TargetComponent->EditableBones[SelectedBoneIndex];

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
	ImGui::TextDisabled("Note: Editing dummy FBoneInfo → FBone data");
}

void USkeletalMeshEditorWidget::RenderViewport()
{
	static int renderViewportCount = 0;
	if (renderViewportCount++ % 60 == 0)
	{
		UE_LOG("SkeletalMeshEditorWidget::RenderViewport - EmbeddedViewport=%p, ViewportClient=%p",
			EmbeddedViewport, ViewportClient);
	}

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
		ImGui::Image((void*)SRV, ImVec2((float)NewWidth, (float)NewHeight));
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
	if (!TargetComponent)
		return false;

	for (const auto& Bone : TargetComponent->EditableBones)
	{
		if (Bone.ParentIndex == BoneIndex)
			return true;
	}
	return false;
}
