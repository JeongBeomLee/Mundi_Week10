#include "pch.h"
#include "BoneTransformCalculator.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMeshTypes.h"
#include "SkeletalMesh.h"

FTransform FBoneTransformCalculator::GetBoneWorldTransform(const USkeletalMeshComponent* Component, int32 BoneIndex)
{
	if (!Component)
		return FTransform();

	// Component가 이미 계산한 ComponentSpaceTransforms 사용
	return Component->GetBoneWorldTransform(BoneIndex);
}

void FBoneTransformCalculator::SetBoneWorldTransform(USkeletalMeshComponent* Component, int32 BoneIndex, const FTransform& WorldTransform)
{
	if (!Component)
		return;

	// Component의 메서드로 위임
	Component->SetBoneWorldTransform(BoneIndex, WorldTransform);
}
