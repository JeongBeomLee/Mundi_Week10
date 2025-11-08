#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "Renderer.h"
#include "SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMeshComponent)

BEGIN_PROPERTIES(USkeletalMeshComponent)
	MARK_AS_COMPONENT("스켈레탈 메시 컴포넌트", "스켈레탈 메시를 렌더링하는 컴포넌트입니다.")
END_PROPERTIES()

USkeletalMeshComponent::USkeletalMeshComponent()
{
	// 더미 FBoneInfo 데이터 생성 (테스트용)
	TArray<FBoneInfo> DummyBoneInfos;

	// Root
	FBoneInfo rootInfo;
	rootInfo.BoneName = "Root";
	rootInfo.ParentIndex = -1;
	rootInfo.GlobalTransform = FMatrix::Identity();
	rootInfo.InverseBindPoseMatrix = rootInfo.GlobalTransform.InverseAffine();
	rootInfo.SkinningMatrix = rootInfo.InverseBindPoseMatrix * rootInfo.GlobalTransform;
	DummyBoneInfos.push_back(rootInfo);

	// Spine (Root의 자식, Z+10)
	FBoneInfo spineInfo;
	spineInfo.BoneName = "Spine";
	spineInfo.ParentIndex = 0;
	spineInfo.GlobalTransform = FMatrix::Identity();
	spineInfo.GlobalTransform.M[3][2] = 10.0f;
	spineInfo.InverseBindPoseMatrix = spineInfo.GlobalTransform.InverseAffine();
	spineInfo.SkinningMatrix = spineInfo.InverseBindPoseMatrix * spineInfo.GlobalTransform;
	DummyBoneInfos.push_back(spineInfo);

	// Head (Spine의 자식, World Z+25)
	FBoneInfo headInfo;
	headInfo.BoneName = "Head";
	headInfo.ParentIndex = 1;
	headInfo.GlobalTransform = FMatrix::Identity();
	headInfo.GlobalTransform.M[3][2] = 25.0f;
	headInfo.InverseBindPoseMatrix = headInfo.GlobalTransform.InverseAffine();
	headInfo.SkinningMatrix = headInfo.InverseBindPoseMatrix * headInfo.GlobalTransform;
	DummyBoneInfos.push_back(headInfo);

	// LeftArm (Spine의 자식, X-8, Z+18)
	FBoneInfo leftArmInfo;
	leftArmInfo.BoneName = "LeftArm";
	leftArmInfo.ParentIndex = 1;
	leftArmInfo.GlobalTransform = FMatrix::Identity();
	leftArmInfo.GlobalTransform.M[3][0] = -8.0f;
	leftArmInfo.GlobalTransform.M[3][2] = 18.0f;
	leftArmInfo.InverseBindPoseMatrix = leftArmInfo.GlobalTransform.InverseAffine();
	leftArmInfo.SkinningMatrix = leftArmInfo.InverseBindPoseMatrix * leftArmInfo.GlobalTransform;
	DummyBoneInfos.push_back(leftArmInfo);

	// RightArm (Spine의 자식, X+8, Z+18)
	FBoneInfo rightArmInfo;
	rightArmInfo.BoneName = "RightArm";
	rightArmInfo.ParentIndex = 1;
	rightArmInfo.GlobalTransform = FMatrix::Identity();
	rightArmInfo.GlobalTransform.M[3][0] = 8.0f;
	rightArmInfo.GlobalTransform.M[3][2] = 18.0f;
	rightArmInfo.InverseBindPoseMatrix = rightArmInfo.GlobalTransform.InverseAffine();
	rightArmInfo.SkinningMatrix = rightArmInfo.InverseBindPoseMatrix * rightArmInfo.GlobalTransform;
	DummyBoneInfos.push_back(rightArmInfo);

	// FBoneInfo → FBone 변환
	for (size_t i = 0; i < DummyBoneInfos.size(); i++)
	{
		FBone bone = FBone::FromBoneInfo(static_cast<int32>(i), DummyBoneInfos);
		EditableBones.push_back(bone);
	}

	UE_LOG("SkeletalMeshComponent: Loaded %d bones from dummy FBoneInfo data", EditableBones.size());
}

int32 USkeletalMeshComponent::GetBoneCount() const
{
	return static_cast<int32>(EditableBones.size());
}

FBone* USkeletalMeshComponent::GetBone(int32 Index)
{
	if (Index >= 0 && Index < static_cast<int32>(EditableBones.size()))
		return &EditableBones[Index];
	return nullptr;
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(EditableBones.size()))
		return FTransform();

	const FBone& Bone = EditableBones[BoneIndex];

	// Local transform 생성
	FTransform LocalTransform;
	LocalTransform.Translation = Bone.LocalPosition;
	LocalTransform.Rotation = Bone.LocalRotation;
	LocalTransform.Scale3D = Bone.LocalScale;

	// Parent transform 재귀 누적
	if (Bone.ParentIndex >= 0)
	{
		FTransform ParentWorldTransform = GetBoneWorldTransform(Bone.ParentIndex);
		return ParentWorldTransform.GetWorldTransform(LocalTransform);
	}

	// Root bone: component의 world transform 적용
	return GetWorldTransform().GetWorldTransform(LocalTransform);
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& Transform)
{
	// TODO(FBX): 나중에 구현
	// 현재는 더미 데이터라 편집 불가
	UE_LOG("SkeletalMeshComponent: SetBoneLocalTransform not implemented yet (dummy data)");
}

void USkeletalMeshComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (EditableBones.empty() || !Renderer)
		return;

	// Bone skeleton을 line으로 시각화
	// NOTE: Renderer->BeginLineBatch()는 SceneRenderer에서 자동 호출됨

	// 1. Parent → Child 연결선 렌더링
	for (int32 i = 0; i < GetBoneCount(); ++i)
	{
		const FBone& Bone = EditableBones[i];
		if (Bone.ParentIndex < 0)
			continue;  // Root bone은 line 없음

		// Parent → Child 연결선
		FVector ParentPos = GetBoneWorldTransform(Bone.ParentIndex).Translation;
		FVector ChildPos = GetBoneWorldTransform(i).Translation;

		// 선택된 bone은 빨간색, 나머지는 노란색
		FVector4 Color = (i == SelectedBoneIndex || Bone.ParentIndex == SelectedBoneIndex)
			? FVector4(1.0f, 0.0f, 0.0f, 1.0f)  // Red
			: FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow

		Renderer->AddLine(ParentPos, ChildPos, Color);
	}

	// 2. 각 bone의 local axis 렌더링 (rotation 시각화)
	const float AxisLength = 5.0f;  // Axis 길이
	for (int32 i = 0; i < GetBoneCount(); ++i)
	{
		FTransform BoneWorldTransform = GetBoneWorldTransform(i);
		FVector BonePos = BoneWorldTransform.Translation;
		FQuat BoneRot = BoneWorldTransform.Rotation;

		// X축 (빨간색)
		FVector XAxis = BoneRot.RotateVector(FVector(AxisLength, 0, 0));
		Renderer->AddLine(BonePos, BonePos + XAxis, FVector4(1.0f, 0.0f, 0.0f, 1.0f));

		// Y축 (초록색)
		FVector YAxis = BoneRot.RotateVector(FVector(0, AxisLength, 0));
		Renderer->AddLine(BonePos, BonePos + YAxis, FVector4(0.0f, 1.0f, 0.0f, 1.0f));

		// Z축 (파란색)
		FVector ZAxis = BoneRot.RotateVector(FVector(0, 0, AxisLength));
		Renderer->AddLine(BonePos, BonePos + ZAxis, FVector4(0.0f, 0.0f, 1.0f, 1.0f));
	}
}
