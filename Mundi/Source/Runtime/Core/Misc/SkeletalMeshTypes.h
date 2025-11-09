#pragma once
#include "Vector.h"

// ========================================
// ⚠️ WARNING: 더미 테스트용 구조 (추후 FBX 로딩 시 교체)
// 목적: SkeletalMesh Editor 프로토타이핑만
// ⚠️ FBX 로더 완성 시 실제 Asset 데이터 구조로 확장될 예정
// ========================================

/**
 * @brief 단일 Bone 정보 (편집 가능한 프로토타입 구조)
 *
 * ⚠️ WARNING: 이 구조체는 FBX 로더 완성 시 확장/변경될 수 있습니다!
 *
 * 현재 용도:
 * - SkeletalMesh Editor에서 본 Transform 편집
 * - Bone Hierarchy Tree 표시
 * - Debug 시각화 (라인 렌더링)
 *
 * 향후 확장 예정:
 * - Animation 데이터 참조
 * - Skinning 정보 (Vertex Weight)
 * - Inverse Bind Pose Matrix
 */
struct FBone
{
	FString Name;           // Bone 이름
	int32 Index;            // Bone 인덱스
	int32 ParentIndex;      // 부모 Bone 인덱스 (-1 = root)

	// Local transform (parent 상대)
	FVector LocalPosition = FVector(0, 0, 0);
	FQuat LocalRotation = FQuat::Identity();
	FVector LocalScale = FVector(1, 1, 1);

	// Euler angle 별도 저장 (UI 입력값 보존, SceneComponent 패턴)
	// NOTE: SetLocalRotationEuler() 호출 시에만 업데이트됨
	FVector LocalRotationEuler = FVector(0, 0, 0);

	/** @brief Euler angle 설정 (UI용, gimbal lock 방지 패턴) */
	void SetLocalRotationEuler(const FVector& EulerDegrees)
	{
		// UI 입력값 그대로 저장
		LocalRotationEuler = EulerDegrees;
		// Quaternion 변환 (실제 회전 계산용)
		LocalRotation = FQuat::MakeFromEulerZYX(EulerDegrees).GetNormalized();
	}

	/** @brief 저장된 Euler angle 반환 (UI용) */
	FVector GetLocalRotationEuler() const
	{
		return LocalRotationEuler;
	}

	/**
	 * @brief FBoneInfo에서 FBone으로 변환 (Helper)
	 * @param Index 변환할 Bone 인덱스
	 * @param AllBoneInfos 전체 FBoneInfo 배열
	 * @return 편집 가능한 FBone 인스턴스
	 * @note OffsetMatrix.Inverse() + Decompose로 Local Transform 추출
	 * @todo 나중에 FBoneInfo에 LocalTransformMatrix 추가되면 그걸 사용하도록 변경
	 */
	static FBone FromBoneInfo(int32 Index, const TArray<struct FBoneInfo>& AllBoneInfos);
};

/**
 * @brief 더미 Skeleton (하드코딩 테스트용)
 * @note TODO(FBX): 실제 FSkeleton 클래스로 교체 필요
 *
 * 마이그레이션 경로:
 * 1. FDummySkeleton → FSkeleton 교체
 * 2. FBX/GLTF 로더 구현 (Assimp 또는 FBX SDK)
 * 3. FSkeletalMeshData 추가 (vertices, bone weights, bind poses)
 * 4. Mesh 렌더링 + skinning 구현
 * 5. Animation 시스템 추가
 */
class FDummySkeleton
{
public:
	FDummySkeleton()
	{
		InitializeDummyBones();
	}

	// ===== UI가 사용할 인터페이스 (나중에 FSkeleton도 동일) =====

	/** @brief Bone 개수 반환 */
	int32 GetBoneCount() const { return static_cast<int32>(Bones.size()); }

	/** @brief Index로 Bone 가져오기 (nullptr 안전) */
	FBone* GetBone(int32 Index)
	{
		return (Index >= 0 && Index < static_cast<int32>(Bones.size()))
			? &Bones[Index]
			: nullptr;
	}

	/** @brief 전체 Bone 배열 반환 (읽기 전용) */
	const TArray<FBone>& GetBones() const { return Bones; }

private:
	void InitializeDummyBones()
	{
		// 하드코딩 더미 hierarchy (5개 bone)
		// NOTE: UI 프로토타입 목적, 실제 mesh 렌더링 없음
		Bones = {
			{"Root",      0, -1},  // Root
			{"Spine",     1,  0},  // Root의 자식
			{"Head",      2,  1},  // Spine의 자식
			{"LeftArm",   3,  1},  // Spine의 자식
			{"RightArm",  4,  1},  // Spine의 자식
		};

		// 약간의 offset (viewport 시각화 테스트용)
		Bones[1].LocalPosition = FVector(0, 0, 10);   // Spine
		Bones[2].LocalPosition = FVector(0, 0, 15);   // Head
		Bones[3].LocalPosition = FVector(-8, 0, 8);   // LeftArm
		Bones[4].LocalPosition = FVector(8, 0, 8);    // RightArm
	}

	TArray<FBone> Bones;
};

// ========================================
// 추후 FBX 로딩 시 추가될 실제 구조 (현재 미구현)
// ========================================
/*
예상 구조:

class FSkeleton {
public:
	TArray<FBone> Bones;
	TMap<FString, int32> BoneNameToIndex;
	TArray<FMatrix> InverseBindPoseMatrices;

	void UpdateBoneTransforms(const FAnimationPose& Pose);
	FMatrix GetBoneMatrix(int32 BoneIndex) const;
};

struct FSkeletalMeshData {
	TArray<FSkinnedVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FBoneWeight> BoneWeights;
	FSkeleton* Skeleton;

	void Render(const TArray<FMatrix>& BoneMatrices);
};

struct FSkinnedVertex {
	FVector Position;
	FVector Normal;
	FVector2D TexCoord;
	uint32 BoneIndices[4];
	float BoneWeights[4];
};
*/
