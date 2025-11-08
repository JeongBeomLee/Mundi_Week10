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
 * 1. GlobalTransform = 바인드 포즈에서 본의 월드 변환 (LinkTransform)
 * 2. Child의 Local = ParentWorld^-1 * ChildWorld
 * 3. Matrix Decompose → Position, Rotation, Scale
 *
 * 최적화:
 * - Root bone: GlobalTransform을 직접 사용 (역행렬 계산 불필요)
 * - Child bone: 1번의 역행렬 계산으로 로컬 변환 추출
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

	FMatrix LocalMatrix;

	if (Bone.ParentIndex >= 0 && Bone.ParentIndex < AllBoneInfos.size())
	{
		// Child bone: Local = ParentWorld^-1 * ChildWorld
		const FMatrix& ParentGlobalTransform = AllBoneInfos[Bone.ParentIndex].GlobalTransform;
		const FMatrix& ChildGlobalTransform = BoneInfo.GlobalTransform;

		FMatrix ParentWorldInverse = ParentGlobalTransform.InverseAffine();
		LocalMatrix = ParentWorldInverse * ChildGlobalTransform;
	}
	else
	{
		// Root bone: World = Local (역행렬 계산 불필요)
		LocalMatrix = BoneInfo.GlobalTransform;
	}

	// Translation 추출
	Bone.LocalPosition = FVector(
		LocalMatrix.M[3][0],
		LocalMatrix.M[3][1],
		LocalMatrix.M[3][2]
	);

	// Rotation/Scale 추출을 위한 3x3 서브행렬
	FMatrix RotScaleMat = LocalMatrix;
	RotScaleMat.M[3][0] = RotScaleMat.M[3][1] = RotScaleMat.M[3][2] = 0.0f;
	RotScaleMat.M[0][3] = RotScaleMat.M[1][3] = RotScaleMat.M[2][3] = 0.0f;
	RotScaleMat.M[3][3] = 1.0f;

	// Scale 추출 (각 축의 길이)
	FVector ScaleX(RotScaleMat.M[0][0], RotScaleMat.M[0][1], RotScaleMat.M[0][2]);
	FVector ScaleY(RotScaleMat.M[1][0], RotScaleMat.M[1][1], RotScaleMat.M[1][2]);
	FVector ScaleZ(RotScaleMat.M[2][0], RotScaleMat.M[2][1], RotScaleMat.M[2][2]);

	Bone.LocalScale = FVector(
		ScaleX.Size(),
		ScaleY.Size(),
		ScaleZ.Size()
	);

	// Rotation 추출 (Scale 제거)
	if (Bone.LocalScale.X > 0.0001f)
	{
		RotScaleMat.M[0][0] /= Bone.LocalScale.X;
		RotScaleMat.M[0][1] /= Bone.LocalScale.X;
		RotScaleMat.M[0][2] /= Bone.LocalScale.X;
	}
	if (Bone.LocalScale.Y > 0.0001f)
	{
		RotScaleMat.M[1][0] /= Bone.LocalScale.Y;
		RotScaleMat.M[1][1] /= Bone.LocalScale.Y;
		RotScaleMat.M[1][2] /= Bone.LocalScale.Y;
	}
	if (Bone.LocalScale.Z > 0.0001f)
	{
		RotScaleMat.M[2][0] /= Bone.LocalScale.Z;
		RotScaleMat.M[2][1] /= Bone.LocalScale.Z;
		RotScaleMat.M[2][2] /= Bone.LocalScale.Z;
	}

	// Matrix → Quaternion (FQuat::FromRotationMatrix 사용)
	Bone.LocalRotation = FQuat::FromRotationMatrix(RotScaleMat);

	// Euler angle 초기값 (Gimbal Lock UI 방지)
	Bone.LocalRotationEuler = Bone.LocalRotation.ToEulerZYXDeg();

	return Bone;
}
