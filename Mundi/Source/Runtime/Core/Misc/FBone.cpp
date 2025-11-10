#include "pch.h"
#include "SkeletalMeshTypes.h"
#include "Enums.h"

/**
 * @brief FBoneInfo에서 FBone으로 변환 (Helper 구현)
 * @param Index 변환할 Bone 인덱스
 * @param AllBoneInfos 전체 FBoneInfo 배열
 * @return 편집 가능한 FBone 인스턴스
 *
 * 동작 원리:
 * - BindPoseLocalTransform 행렬을 직접 Decompose
 * - FMatrix::Decompose() 사용으로 간단하고 정확한 변환
 */
FBone FBone::FromBoneInfo(int32 Index, const TArray<FBoneInfo>& AllBoneInfos)
{
	if (Index < 0 || Index >= AllBoneInfos.size())
	{
		// Invalid index - return empty bone
		return FBone();
	}

	const FBoneInfo& BoneInfo = AllBoneInfos[Index];

	FBone Bone;
	Bone.Name = BoneInfo.BoneName;
	Bone.Index = Index;
	Bone.ParentIndex = BoneInfo.ParentIndex;

	// BindPoseLocalTransform을 직접 Decompose (훨씬 간단!)
	BoneInfo.BindPoseLocalTransform.Decompose(
		Bone.LocalScale,
		Bone.LocalRotation,
		Bone.LocalPosition
	);

	// Euler angle 초기값 (Gimbal Lock UI 방지)
	Bone.LocalRotationEuler = Bone.LocalRotation.ToEulerZYXDeg();

	return Bone;
}
