#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "Renderer.h"
#include "SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMeshComponent)

BEGIN_PROPERTIES(USkeletalMeshComponent)
	MARK_AS_COMPONENT("스켈레탈 메시 컴포넌트", "스켈레탈 메시를 렌더링하는 컴포넌트입니다.")
END_PROPERTIES()

// ⚠️ WARNING: 이 생성자는 더미 프로토타입 구현입니다!
// ⚠️ FBX 로더 완성 시 실제 Asset에서 본 데이터를 로드하도록 교체될 예정입니다.
USkeletalMeshComponent::USkeletalMeshComponent()
{
	// ========== 더미 FBoneInfo 데이터 생성 (테스트용) ==========
	// TODO(FBX): 실제 구현 시 Asset에서 로드하도록 변경
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

FVector USkeletalMeshComponent::GetBoneWorldPosition(int32 BoneIndex) const
{
	return GetBoneWorldTransform(BoneIndex).Translation;
}

FQuat USkeletalMeshComponent::GetBoneWorldRotation(int32 BoneIndex) const
{
	return GetBoneWorldTransform(BoneIndex).Rotation;
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

	// 메인 뷰포트(Editor/Game)에서는 본을 표시하지 않음, Embedded 뷰포트(에디터 창)에서만 표시
	UWorld* World = GetWorld();
	if (!World || !World->IsEmbedded())
		return;

	// 라인 데이터 준비 (NOTE: Renderer->BeginLineBatch()는 SceneRenderer에서 자동 호출됨)
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 1. 본 피라미드 렌더링 (Unreal Engine 색상 규칙)
	RenderBonePyramids(StartPoints, EndPoints, Colors);

	// 2. 관절 구체 렌더링 (회전 반영)
	RenderJointSpheres(StartPoints, EndPoints, Colors);

	// 렌더러에 라인 일괄 추가
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void USkeletalMeshComponent::RenderBonePyramids(
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints,
	TArray<FVector4>& OutColors) const
{
	const float BoneThickness = 0.8f;  // 피라미드 기저면 두께
	const FVector4 WhiteColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);   // 흰색 (기본)
	const FVector4 GreenColor = FVector4(0.0f, 1.0f, 0.0f, 1.0f);   // 초록색 (선택된 조인트 → 자식)
	const FVector4 YellowColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 노란색 (부모 → 선택된 조인트)

	for (int32 i = 0; i < GetBoneCount(); ++i)
	{
		const FBone& Bone = EditableBones[i];
		if (Bone.ParentIndex < 0)
			continue;  // Root bone은 피라미드 없음

		FVector ParentPos = GetBoneWorldTransform(Bone.ParentIndex).Translation;
		FVector ChildPos = GetBoneWorldTransform(i).Translation;
		FVector BoneDir = (ChildPos - ParentPos);
		float BoneLength = BoneDir.Size();

		if (BoneLength < 0.001f)
			continue;  // 길이가 0이면 스킵

		BoneDir = BoneDir / BoneLength;  // 정규화

		// Unreal Engine 색상 규칙
		FVector4 Color = WhiteColor;  // 기본: 흰색
		if (i == SelectedBoneIndex)
		{
			// 부모 → 선택된 조인트: 노란색
			Color = YellowColor;
		}
		else if (Bone.ParentIndex == SelectedBoneIndex)
		{
			// 선택된 조인트 → 자식: 초록색
			Color = GreenColor;
		}

		// 피라미드 기저면을 위한 직교 벡터 생성
		FVector Up = (FMath::Abs(BoneDir.Z) < 0.99f) ? FVector(0, 0, 1) : FVector(1, 0, 0);
		FVector Right = FVector::Cross(BoneDir, Up).GetNormalized();
		Up = FVector::Cross(Right, BoneDir).GetNormalized();

		// 피라미드 기저면 꼭짓점 (부모 위치에서 사각형)
		FVector BaseVertices[4];
		for (int32 seg = 0; seg < 4; ++seg)
		{
			float Angle = seg * 90.0f * (PI / 180.0f);
			FVector Offset = (Right * cos(Angle) + Up * sin(Angle)) * BoneThickness;
			BaseVertices[seg] = ParentPos + Offset;
		}

		// 피라미드 그리기 (기저면 → 자식)
		for (int32 seg = 0; seg < 4; ++seg)
		{
			int32 NextSeg = (seg + 1) % 4;

			// 측면 모서리
			OutStartPoints.push_back(BaseVertices[seg]);
			OutEndPoints.push_back(ChildPos);
			OutColors.push_back(Color);

			// 기저면 모서리
			OutStartPoints.push_back(BaseVertices[seg]);
			OutEndPoints.push_back(BaseVertices[NextSeg]);
			OutColors.push_back(Color);
		}

		// 중심선 (Parent → Child) - 더 얇은 색상
		FVector4 CenterLineColor = Color * 0.5f;
		CenterLineColor.W = 1.0f;
		OutStartPoints.push_back(ParentPos);
		OutEndPoints.push_back(ChildPos);
		OutColors.push_back(CenterLineColor);
	}
}

void USkeletalMeshComponent::RenderJointSpheres(
	TArray<FVector>& OutStartPoints,
	TArray<FVector>& OutEndPoints,
	TArray<FVector4>& OutColors) const
{
	const float JointRadius = 1.0f;      // 관절 구체 반지름
	const int32 CircleSegments = 8;      // 구체 세그먼트 수
	const FVector4 WhiteColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 흰색 (기본)
	const FVector4 GreenColor = FVector4(0.0f, 1.0f, 0.0f, 1.0f);  // 초록색 (선택됨)

	for (int32 i = 0; i < GetBoneCount(); ++i)
	{
		FTransform BoneWorldTransform = GetBoneWorldTransform(i);
		FVector BonePos = BoneWorldTransform.Translation;
		FQuat BoneRot = BoneWorldTransform.Rotation;

		// 선택된 bone은 초록색, 나머지는 흰색
		FVector4 Color = (i == SelectedBoneIndex) ? GreenColor : WhiteColor;

		// 3개의 직교하는 원 (XY, XZ, YZ 평면), 본의 회전 반영
		for (int32 seg = 0; seg < CircleSegments; ++seg)
		{
			float Angle1 = seg * (360.0f / CircleSegments) * (PI / 180.0f);
			float Angle2 = (seg + 1) * (360.0f / CircleSegments) * (PI / 180.0f);

			// 로컬 공간에서 원 정점 생성 후 회전 적용
			// XY 평면 원 (로컬 Z축 기준)
			FVector LocalP1_XY = FVector(cos(Angle1) * JointRadius, sin(Angle1) * JointRadius, 0);
			FVector LocalP2_XY = FVector(cos(Angle2) * JointRadius, sin(Angle2) * JointRadius, 0);
			FVector P1_XY = BonePos + BoneRot.RotateVector(LocalP1_XY);
			FVector P2_XY = BonePos + BoneRot.RotateVector(LocalP2_XY);
			OutStartPoints.push_back(P1_XY);
			OutEndPoints.push_back(P2_XY);
			OutColors.push_back(Color);

			// XZ 평면 원 (로컬 Y축 기준)
			FVector LocalP1_XZ = FVector(cos(Angle1) * JointRadius, 0, sin(Angle1) * JointRadius);
			FVector LocalP2_XZ = FVector(cos(Angle2) * JointRadius, 0, sin(Angle2) * JointRadius);
			FVector P1_XZ = BonePos + BoneRot.RotateVector(LocalP1_XZ);
			FVector P2_XZ = BonePos + BoneRot.RotateVector(LocalP2_XZ);
			OutStartPoints.push_back(P1_XZ);
			OutEndPoints.push_back(P2_XZ);
			OutColors.push_back(Color);

			// YZ 평면 원 (로컬 X축 기준)
			FVector LocalP1_YZ = FVector(0, cos(Angle1) * JointRadius, sin(Angle1) * JointRadius);
			FVector LocalP2_YZ = FVector(0, cos(Angle2) * JointRadius, sin(Angle2) * JointRadius);
			FVector P1_YZ = BonePos + BoneRot.RotateVector(LocalP1_YZ);
			FVector P2_YZ = BonePos + BoneRot.RotateVector(LocalP2_YZ);
			OutStartPoints.push_back(P1_YZ);
			OutEndPoints.push_back(P2_YZ);
			OutColors.push_back(Color);
		}
	}
}

