#include "pch.h"
#include "BoneTransformCalculator.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMeshTypes.h"
#include "SkeletalMesh.h"

FTransform FBoneTransformCalculator::GetBoneWorldTransform(const USkeletalMeshComponent* Component, int32 BoneIndex)
{
	if (!Component || BoneIndex < 0 || BoneIndex >= Component->EditableBones.size())
		return FTransform();

	// 부모 본들의 Local Transform을 누적하여 World Transform 계산
	FTransform WorldTransform = FTransform();
	int32 CurrentIndex = BoneIndex;

	// Root부터 현재 본까지의 경로를 역순으로 저장
	TArray<int32> BonePath;
	while (CurrentIndex >= 0)
	{
		BonePath.push_back(CurrentIndex);
		CurrentIndex = Component->EditableBones[CurrentIndex].ParentIndex;
	}

	// Root부터 순차적으로 누적 (역순 순회)
	for (int32 i = static_cast<int32>(BonePath.size()) - 1; i >= 0; --i)
	{
		const FBone& Bone = Component->EditableBones[BonePath[i]];
		FTransform LocalTransform(Bone.LocalPosition, Bone.LocalRotation, Bone.LocalScale);
		WorldTransform = WorldTransform.GetWorldTransform(LocalTransform);
	}

	return WorldTransform;
}

void FBoneTransformCalculator::SetBoneWorldTransform(USkeletalMeshComponent* Component, int32 BoneIndex, const FTransform& WorldTransform)
{
	if (!Component || BoneIndex < 0 || BoneIndex >= Component->EditableBones.size())
		return;

	FBone& Bone = Component->EditableBones[BoneIndex];

	// 부모가 없으면 World Transform을 그대로 Local로 사용
	if (Bone.ParentIndex < 0)
	{
		Bone.LocalPosition = WorldTransform.Translation;
		Bone.LocalRotation = WorldTransform.Rotation;
		Bone.LocalScale = WorldTransform.Scale3D;
	}
	else
	{
		// 부모의 World Transform 계산
		FTransform ParentWorldTransform = GetBoneWorldTransform(Component, Bone.ParentIndex);

		// World → Local 변환
		FQuat ParentRotInv = ParentWorldTransform.Rotation.Conjugate();
		Bone.LocalRotation = ParentRotInv * WorldTransform.Rotation;

		FVector WorldPosRelative = WorldTransform.Translation - ParentWorldTransform.Translation;
		FVector LocalPos = ParentRotInv.RotateVector(WorldPosRelative);
		Bone.LocalPosition = FVector(
			LocalPos.X / ParentWorldTransform.Scale3D.X,
			LocalPos.Y / ParentWorldTransform.Scale3D.Y,
			LocalPos.Z / ParentWorldTransform.Scale3D.Z
		);

		Bone.LocalScale = FVector(
			WorldTransform.Scale3D.X / ParentWorldTransform.Scale3D.X,
			WorldTransform.Scale3D.Y / ParentWorldTransform.Scale3D.Y,
			WorldTransform.Scale3D.Z / ParentWorldTransform.Scale3D.Z
		);
	}

	Component->MarkSkinningDirty();

	// Euler angle도 업데이트
	Bone.LocalRotationEuler = Bone.LocalRotation.ToEulerZYXDeg();
}
