#pragma once
#include "MeshComponent.h"
#include "SkeletalMeshTypes.h"
#include <memory>

class URenderer;

/**
 * @brief Skeletal Mesh 컴포넌트 (더미 프로토타입)
 * @note 현재: 더미 skeleton + line 렌더링만
 * @note 나중에: 실제 mesh 렌더링 + animation 지원
 *
 * 설계:
 * - UMeshComponent 상속 (기존 계층 구조 준수)
 * - RenderDebugVolume 오버라이드 (다형성, 비침습적)
 * - 더미 skeleton (나중에 FSkeleton*로 교체)
 */
class USkeletalMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, UMeshComponent)
	GENERATED_REFLECTION_BODY()

	USkeletalMeshComponent();
	virtual ~USkeletalMeshComponent() = default;

	// ===== UI 인터페이스 =====

	/** @brief Bone 개수 반환 */
	int32 GetBoneCount() const;

	/** @brief Index로 Bone 가져오기 (nullptr 안전) */
	FBone* GetBone(int32 Index);

	/** @brief Bone의 world space transform 계산 (hierarchy 누적) */
	FTransform GetBoneWorldTransform(int32 BoneIndex) const;

	/** @brief Bone local transform 설정 (TODO: 나중에 구현) */
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& Transform);

	// ===== Editor 상태 =====

	/** @brief 현재 선택된 bone index (-1 = none) */
	int32 SelectedBoneIndex = -1;

	// ===== Viewport 렌더링 (다형성) =====

	/** @brief Bone skeleton을 line으로 시각화 */
	virtual void RenderDebugVolume(class URenderer* Renderer) const override;

private:
	/** @brief 더미 skeleton (TODO: 실제 FSkeleton*로 교체) */
	std::unique_ptr<FDummySkeleton> DummySkeleton;
};
