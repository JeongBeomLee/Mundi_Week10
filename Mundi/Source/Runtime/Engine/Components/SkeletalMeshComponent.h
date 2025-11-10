#pragma once
#include "MeshComponent.h"
#include "SkeletalMeshTypes.h"
#include <memory>

class URenderer;

/**
 * @brief Skeletal Mesh 컴포넌트 (더미 프로토타입)
 *
 * ⚠️ WARNING: 이 파일은 FBX 로더 완성 시 실제 구현으로 교체될 예정입니다!
 * ⚠️ 현재는 SkeletalMesh Editor 프로토타입 개발을 위한 더미 데이터만 제공합니다.
 *
 * 현재 구현:
 * - 생성자에서 하드코딩된 5개 본 생성 (Root, Spine, Head, LeftArm, RightArm)
 * - EditableBones를 통한 본 데이터 노출
 * - RenderDebugVolume()으로 본 라인 시각화
 *
 * 향후 구현 예정:
 * - FBX Asset에서 실제 본 데이터 로드
 * - Skeletal Mesh 렌더링 (현재는 라인만)
 * - Animation 시스템 통합
 * - Skinning (Vertex Deformation)
 *
 * 설계:
 * - UMeshComponent 상속 (기존 계층 구조 준수)
 * - RenderDebugVolume 오버라이드 (다형성, 비침습적)
 * - EditableBones 인터페이스는 실제 구현에서도 유지됨
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

	/** @brief Bone의 world space 위치 반환 (피킹용) */
	FVector GetBoneWorldPosition(int32 BoneIndex) const;

	/** @brief Bone의 world space 회전 반환 (Gizmo용) */
	FQuat GetBoneWorldRotation(int32 BoneIndex) const;

	/** @brief Bone local transform 설정 (TODO: 나중에 구현) */
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& Transform);

	/** @brief 현재 선택된 Bone 인덱스 반환 */
	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }

	/** @brief 선택된 Bone 인덱스 설정 */
	void SetSelectedBoneIndex(int32 Index) { SelectedBoneIndex = Index; }

	/** @brief 편집 가능한 Bone 배열 (Editor Widget이 직접 수정) */
	TArray<FBone> EditableBones;

private:
	// ===== Editor 상태 =====

	/** @brief 현재 선택된 bone index (-1 = none) */
	int32 SelectedBoneIndex = -1;

	// ===== Viewport 렌더링 (다형성) =====

	/** @brief Bone skeleton을 line으로 시각화 (Unreal Engine 스타일) */
	virtual void RenderDebugVolume(class URenderer* Renderer) const override;

	// ===== 디버그 렌더링 헬퍼 메서드 =====

	/**
	 * @brief 본 피라미드 렌더링 (Parent → Child, Unreal Engine 색상 규칙)
	 * 색상 규칙:
	 * - 선택된 조인트 → 자식: 초록색
	 * - 부모 → 선택된 조인트: 노란색
	 * - 나머지: 흰색
	 */
	void RenderBonePyramids(
		TArray<FVector>& OutStartPoints,
		TArray<FVector>& OutEndPoints,
		TArray<FVector4>& OutColors) const;

	/**
	 * @brief 관절 구체 렌더링 (본 회전이 반영된 와이어프레임 구체)
	 * 색상 규칙:
	 * - 선택된 조인트: 초록색
	 * - 나머지: 흰색
	 */
	void RenderJointSpheres(
		TArray<FVector>& OutStartPoints,
		TArray<FVector>& OutEndPoints,
		TArray<FVector4>& OutColors) const;
};
