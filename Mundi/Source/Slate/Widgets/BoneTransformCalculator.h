#pragma once
#include "Vector.h"

class USkeletalMeshComponent;

/**
 * @brief Bone Transform 계산 유틸리티
 *
 * 설계:
 * - Static 메서드만 제공 (상태 없음)
 * - World ↔ Local Transform 변환 담당
 * - Skeletal hierarchy 누적 계산
 *
 * 책임:
 * - GetBoneWorldTransform: 부모 본들의 Local Transform을 누적하여 World Transform 계산
 * - SetBoneWorldTransform: World Transform을 부모 기준 Local Transform으로 역변환
 */
class FBoneTransformCalculator
{
public:
	/**
	 * @brief Bone의 World Transform 계산 (부모 누적)
	 * @param Component Skeletal Mesh Component
	 * @param BoneIndex 대상 본 인덱스
	 * @return World space transform
	 */
	static FTransform GetBoneWorldTransform(const USkeletalMeshComponent* Component, int32 BoneIndex);

	/**
	 * @brief Bone의 World Transform 설정 (World → Local 변환)
	 * @param Component Skeletal Mesh Component
	 * @param BoneIndex 대상 본 인덱스
	 * @param WorldTransform 설정할 World space transform
	 */
	static void SetBoneWorldTransform(USkeletalMeshComponent* Component, int32 BoneIndex, const FTransform& WorldTransform);
};
