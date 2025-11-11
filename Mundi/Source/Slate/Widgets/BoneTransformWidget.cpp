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

	USkeletalMesh* SkeletalMesh = PreviewComponent ? PreviewComponent->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;

	if (!PreviewComponent || !SelectedBoneIndexPtr || !MeshAsset ||
		*SelectedBoneIndexPtr < 0 || *SelectedBoneIndexPtr >= MeshAsset->Bones.Num())
	{
		ImGui::TextDisabled("Select a bone from the hierarchy");
		return;
	}

	int32 BoneIndex = *SelectedBoneIndexPtr;
	const FBoneInfo& BoneInfo = MeshAsset->Bones[BoneIndex];

	ImGui::Text("Bone: %s (Index: %d)", BoneInfo.BoneName.c_str(), BoneIndex);
	ImGui::Separator();
	ImGui::Spacing();

	// Local Transform 가져오기
	FTransform LocalTransform = PreviewComponent->GetBoneLocalTransform(BoneIndex);

	// Bone이 변경되었으면 캐시 업데이트 (FBone 패턴)
	if (LastEditedBoneIndex != BoneIndex)
	{
		CachedLocalRotationEuler = LocalTransform.Rotation.ToEulerZYXDeg();
		LastEditedBoneIndex = BoneIndex;
	}

	// Local Transform 편집 (PropertyRenderer 스타일)
	ImGui::Text("Local Transform (Preview):");
	ImGui::Spacing();

	bool bChanged = false;

	// Position
	bChanged |= UPropertyUtils::RenderVector3WithColorBars("Position", &LocalTransform.Translation, 0.1f);

	// Rotation (Euler 캐시 사용, gimbal lock 방지)
	if (UPropertyUtils::RenderVector3WithColorBars("Rotation", &CachedLocalRotationEuler, 1.0f))
	{
		LocalTransform.Rotation = FQuat::MakeFromEulerZYX(CachedLocalRotationEuler).GetNormalized();
		bChanged = true;
	}

	// Scale
	bChanged |= UPropertyUtils::RenderVector3WithColorBars("Scale", &LocalTransform.Scale3D, 0.01f);

	// 값이 변경되었으면 BoneSpaceTransforms 업데이트
	if (bChanged)
	{
		PreviewComponent->SetBoneLocalTransform(BoneIndex, LocalTransform);
		PreviewComponent->MarkSkeletalMeshDirty();
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
