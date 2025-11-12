#include "pch.h"
#include "BoneTransformWidget.h"
#include "ImGui/imgui.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMeshTypes.h"
#include "PropertyUtils.h"

IMPLEMENT_CLASS(UBoneTransformWidget)

UBoneTransformWidget::UBoneTransformWidget()
	: UWidget("Bone Transform Editor")
{
}

void UBoneTransformWidget::Initialize()
{
	UWidget::Initialize();
}

void UBoneTransformWidget::Update()
{
	// 상태 없음 - 업데이트 불필요
}

void UBoneTransformWidget::RenderWidget()
{
	ImGui::Text("Transform Editor");
	ImGui::Separator();
	ImGui::Spacing();

	if (!PreviewComponent || !SelectedBoneIndexPtr || *SelectedBoneIndexPtr < 0 || *SelectedBoneIndexPtr >= PreviewComponent->EditableBones.size())
	{
		ImGui::TextDisabled("Select a bone from the hierarchy");
		return;
	}

	FBone& SelectedBone = PreviewComponent->EditableBones[*SelectedBoneIndexPtr];

	ImGui::Text("Bone: %s (Index: %d)", SelectedBone.Name.c_str(), *SelectedBoneIndexPtr);
	ImGui::Separator();
	ImGui::Spacing();

	// Local Transform 편집 (PropertyRenderer 스타일)
	ImGui::Text("Local Transform (Preview):");
	ImGui::Spacing();

	// Position
	if(UPropertyUtils::RenderVector3WithColorBars("Position", &SelectedBone.LocalPosition, 0.1f))
	{
		PreviewComponent->MarkSkinningDirty();		
	}

	// Rotation (Euler 저장 패턴으로 gimbal lock UI 문제 방지)
	FVector euler = SelectedBone.GetLocalRotationEuler();
	if (UPropertyUtils::RenderVector3WithColorBars("Rotation", &euler, 1.0f))
	{
		SelectedBone.SetLocalRotationEuler(euler);
		PreviewComponent->MarkSkinningDirty();
	}

	// Scale
	if(UPropertyUtils::RenderVector3WithColorBars("Scale", &SelectedBone.LocalScale, 0.01f))
	{
		PreviewComponent->MarkSkinningDirty();
	}

	

	// 버튼을 하단에 배치하기 위해 남은 공간 계산
	float availHeight = ImGui::GetContentRegionAvail().y;
	float buttonHeight = 30.0f;
	float spacing = 10.0f;

	if (availHeight > buttonHeight + spacing)
	{
		ImGui::Dummy(ImVec2(0, availHeight - buttonHeight - spacing));
	}

	ImGui::Separator();
	ImGui::Spacing();

	// Revert 버튼 (하단 고정)
	if (ImGui::Button("Revert", ImVec2(100, 0)))
	{
		if (OnRevertCallback)
			OnRevertCallback();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Revert to original bone transforms");
	}
}
