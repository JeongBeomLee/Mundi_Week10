#include "pch.h"
#include "SkeletalMeshEditorWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "Enums.h"

IMPLEMENT_CLASS(USkeletalMeshEditorWidget)

USkeletalMeshEditorWidget::USkeletalMeshEditorWidget()
	: UWidget("Skeletal Mesh Editor")
{
}

void USkeletalMeshEditorWidget::Initialize()
{
	// 초기화 로직 (필요 시)
}

void USkeletalMeshEditorWidget::SetTargetComponent(USkeletalMeshComponent* Component)
{
	TargetComponent = Component;
	SelectedBoneIndex = Component ? Component->SelectedBoneIndex : -1;

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
}

void USkeletalMeshEditorWidget::RenderWidget()
{
	if (!TargetComponent)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "No skeletal mesh component selected!");
		return;
	}

	// 2열 레이아웃 (왼쪽: Hierarchy, 오른쪽: Transform)
	float availWidth = ImGui::GetContentRegionAvail().x;

	ImGui::BeginChild("BoneHierarchyPane", ImVec2(availWidth * 0.5f, 0), true);
	RenderBoneHierarchy();
	ImGui::EndChild();

	ImGui::SameLine();

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
