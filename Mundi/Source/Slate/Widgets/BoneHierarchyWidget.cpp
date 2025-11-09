#include "pch.h"
#include "BoneHierarchyWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMeshTypes.h"

IMPLEMENT_CLASS(UBoneHierarchyWidget)

UBoneHierarchyWidget::UBoneHierarchyWidget()
	: UWidget("Bone Hierarchy")
{
}

void UBoneHierarchyWidget::Initialize()
{
	UWidget::Initialize();
}

void UBoneHierarchyWidget::Update()
{
	// 상태 없음 - 업데이트 불필요
}

void UBoneHierarchyWidget::RenderWidget()
{
	ImGui::Text("Bone Hierarchy");
	ImGui::Separator();
	ImGui::Spacing();

	if (!PreviewComponent || PreviewComponent->EditableBones.empty())
	{
		ImGui::TextDisabled("No bones loaded");
		return;
	}

	// Root bone부터 재귀 렌더링
	for (size_t i = 0; i < PreviewComponent->EditableBones.size(); ++i)
	{
		if (PreviewComponent->EditableBones[i].ParentIndex < 0)  // Root bone
		{
			RenderBoneTreeNode(static_cast<int32>(i));
		}
	}
}

void UBoneHierarchyWidget::RenderBoneTreeNode(int32 BoneIndex)
{
	if (!PreviewComponent || BoneIndex < 0 || BoneIndex >= PreviewComponent->EditableBones.size())
		return;

	if (!SelectedBoneIndexPtr)
		return;

	FBone& Bone = PreviewComponent->EditableBones[BoneIndex];

	// Tree node flags
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
	                           ImGuiTreeNodeFlags_SpanAvailWidth;

	if (BoneIndex == *SelectedBoneIndexPtr)
		flags |= ImGuiTreeNodeFlags_Selected;

	// 자식 bone 찾기
	bool bHasChildren = HasChildren(BoneIndex);

	if (!bHasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf;

	// Tree node 렌더링
	ImGui::PushID(BoneIndex);
	bool bOpen = ImGui::TreeNodeEx(Bone.Name.c_str(), flags);

	// 클릭 시 선택
	if (ImGui::IsItemClicked())
	{
		*SelectedBoneIndexPtr = BoneIndex;
		if (PreviewComponent)
			PreviewComponent->SetSelectedBoneIndex(BoneIndex);
	}

	// 자식 bone 재귀 렌더링
	if (bOpen)
	{
		if (bHasChildren)
		{
			for (size_t i = 0; i < PreviewComponent->EditableBones.size(); ++i)
			{
				if (PreviewComponent->EditableBones[i].ParentIndex == BoneIndex)
				{
					RenderBoneTreeNode(static_cast<int32>(i));
				}
			}
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

bool UBoneHierarchyWidget::HasChildren(int32 BoneIndex) const
{
	if (!PreviewComponent)
		return false;

	for (const auto& Bone : PreviewComponent->EditableBones)
	{
		if (Bone.ParentIndex == BoneIndex)
			return true;
	}
	return false;
}
