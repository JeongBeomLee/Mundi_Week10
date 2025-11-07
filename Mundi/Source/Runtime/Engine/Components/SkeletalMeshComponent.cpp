#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "Renderer.h"

IMPLEMENT_CLASS(USkeletalMeshComponent)

BEGIN_PROPERTIES(USkeletalMeshComponent)
	MARK_AS_COMPONENT("스켈레탈 메시 컴포넌트", "스켈레탈 메시를 렌더링하는 컴포넌트입니다.")
END_PROPERTIES()

USkeletalMeshComponent::USkeletalMeshComponent()
{
	// 더미 skeleton 생성 (하드코딩)
	DummySkeleton = std::make_unique<FDummySkeleton>();

	UE_LOG("SkeletalMeshComponent: Dummy skeleton created with %d bones", DummySkeleton->GetBoneCount());
}

int32 USkeletalMeshComponent::GetBoneCount() const
{
	return DummySkeleton ? DummySkeleton->GetBoneCount() : 0;
}

FBone* USkeletalMeshComponent::GetBone(int32 Index)
{
	return DummySkeleton ? DummySkeleton->GetBone(Index) : nullptr;
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{
	if (!DummySkeleton)
		return FTransform();

	FBone* Bone = DummySkeleton->GetBone(BoneIndex);
	if (!Bone)
		return FTransform();

	// Local transform 생성
	FTransform LocalTransform;
	LocalTransform.Translation = Bone->LocalPosition;
	LocalTransform.Rotation = Bone->LocalRotation;
	LocalTransform.Scale3D = Bone->LocalScale;

	// Parent transform 재귀 누적
	if (Bone->ParentIndex >= 0)
	{
		FTransform ParentWorldTransform = GetBoneWorldTransform(Bone->ParentIndex);
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
	if (!DummySkeleton || !Renderer)
		return;

	// Bone skeleton을 line으로 시각화
	// NOTE: Renderer->BeginLineBatch()는 SceneRenderer에서 자동 호출됨

	// 1. Parent → Child 연결선 렌더링
	for (int32 i = 0; i < GetBoneCount(); ++i)
	{
		FBone* Bone = DummySkeleton->GetBone(i);
		if (!Bone || Bone->ParentIndex < 0)
			continue;  // Root bone은 line 없음

		// Parent → Child 연결선
		FVector ParentPos = GetBoneWorldTransform(Bone->ParentIndex).Translation;
		FVector ChildPos = GetBoneWorldTransform(i).Translation;

		// 선택된 bone은 빨간색, 나머지는 노란색
		FVector4 Color = (i == SelectedBoneIndex || Bone->ParentIndex == SelectedBoneIndex)
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
