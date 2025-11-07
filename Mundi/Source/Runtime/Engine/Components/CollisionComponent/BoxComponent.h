// ────────────────────────────────────────────────────────────────────────────
// BoxComponent.h
// Box 형태의 충돌 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "ShapeComponent.h"
#include "AABB.h"

class UShader;

/**
 * UBoxComponent
 *
 * Box(직육면체) 형태의 충돌 컴포넌트입니다.
 * AABB(Axis-Aligned Bounding Box)를 사용하여 충돌을 감지합니다.
 *
 * 주요 기능:
 * - Box vs Box 충돌 감지
 * - Box vs Sphere 충돌 감지
 * - 디버그 박스 렌더링
 */
class UBoxComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

	GENERATED_REFLECTION_BODY()

	UBoxComponent();

protected:
	~UBoxComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;

public:
	// ────────────────────────────────────────────────
	// Box 속성
	// ────────────────────────────────────────────────

	/** Box의 반 크기 (로컬 스페이스, 중심에서 각 축으로의 거리) */
	FVector BoxExtent = FVector(0.50f, 0.50f, 0.50f);

	/**
	 * Box 크기를 설정합니다 (로컬 스페이스).
	 *
	 * @param InExtent - 새로운 Box 반 크기
	 * @param bUpdateBounds - Bounds를 즉시 업데이트할지 여부
	 */
	void SetBoxExtent(const FVector& InExtent, bool bUpdateBounds = true);

	/**
	 * Box 크기를 반환합니다 (로컬 스페이스).
	 *
	 * @return Box 반 크기
	 */
	FVector GetBoxExtent() const { return BoxExtent; }

	/**
	 * 스케일이 적용된 Box 크기를 반환합니다 (월드 스페이스).
	 *
	 * @return 스케일 적용된 Box 반 크기
	 */
	FVector GetScaledBoxExtent() const;

	/**
	 * 월드 스페이스의 Box 중심을 반환합니다.
	 *
	 * @return Box 중심 위치 (월드 스페이스)
	 */
	FVector GetBoxCenter() const;

	// ────────────────────────────────────────────────
	// UShapeComponent 인터페이스 구현
	// ────────────────────────────────────────────────

	/**
	 * Bounds를 업데이트합니다.
	 * Transform 변경 시 자동으로 호출됩니다.
	 */
	void UpdateBounds() override;

	/**
	 * 스케일이 적용된 Bounds를 반환합니다.
	 *
	 * @return FBoxSphereBounds 구조체
	 */
	FBoxSphereBounds GetScaledBounds() const override;

	/**
	 * Box를 디버그 렌더링합니다.
	 */
	void DrawDebugShape() const override;

	/**
	 * Renderer를 통해 Box를 디버그 렌더링합니다.
	 *
	 * @param Renderer - 렌더러 객체
	 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	/**
	 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
	 * Box vs Box, Box vs Sphere 충돌을 정밀하게 감지합니다.
	 *
	 * @param Other - 확인할 상대방 Shape 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingComponent(const UShapeComponent* Other) const override;

	// ────────────────────────────────────────────────
	// Box 전용 충돌 감지 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Box 컴포넌트와 겹쳐있는지 확인합니다.
	 *
	 * @param OtherBox - 확인할 상대방 Box 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingBox(const UBoxComponent* OtherBox) const;

	/**
	 * Sphere 컴포넌트와 겹쳐있는지 확인합니다.
	 *
	 * @param OtherSphere - 확인할 상대방 Sphere 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingSphere(const class USphereComponent* OtherSphere) const;

	/**
	 * 특정 점이 Box 내부에 있는지 확인합니다.
	 *
	 * @param Point - 확인할 점 (월드 스페이스)
	 * @return 포함되어 있으면 true
	 */
	bool ContainsPoint(const FVector& Point) const;
	DECLARE_DUPLICATE(UBoxComponent)
private:
	/** 현재 Bounds (캐시됨) */
	FBoxSphereBounds CachedBounds;
};