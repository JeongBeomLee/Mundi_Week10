// ────────────────────────────────────────────────────────────────────────────
// CapsuleComponent.h
// Capsule 형태의 충돌 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "ShapeComponent.h"

class UShader;

/**
 * UCapsuleComponent
 *
 * Capsule(캡슐) 형태의 충돌 컴포넌트입니다.
 * 중심 선분(Line Segment) + 반지름으로 표현됩니다.
 * 캐릭터 충돌에 주로 사용됩니다.
 *
 * 주요 기능:
 * - Capsule vs Capsule 충돌 감지
 * - Capsule vs Sphere 충돌 감지
 * - Capsule vs Box 충돌 감지
 * - 디버그 Capsule 렌더링
 */
class UCapsuleComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
	DECLARE_DUPLICATE(UCapsuleComponent)
	GENERATED_REFLECTION_BODY()

	UCapsuleComponent();

protected:
	~UCapsuleComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;

public:
	// ────────────────────────────────────────────────
	// Capsule 속성
	// ────────────────────────────────────────────────

	/** Capsule의 반지름 (로컬 스페이스) */
	float CapsuleRadius = 50.0f;

	/** Capsule의 반 높이 (로컬 스페이스, 중심에서 끝까지, 반지름 제외) */
	float CapsuleHalfHeight = 100.0f;

	/**
	 * Capsule 크기를 설정합니다.
	 *
	 * @param InRadius - 새로운 반지름
	 * @param InHalfHeight - 새로운 반 높이
	 * @param bUpdateBounds - Bounds를 즉시 업데이트할지 여부
	 */
	void SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateBounds = true);

	/**
	 * Capsule 반지름을 반환합니다 (로컬 스페이스).
	 *
	 * @return Capsule 반지름
	 */
	float GetCapsuleRadius() const { return CapsuleRadius; }

	/**
	 * Capsule 반 높이를 반환합니다 (로컬 스페이스).
	 *
	 * @return Capsule 반 높이
	 */
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	/**
	 * 스케일이 적용된 Capsule 반지름을 반환합니다 (월드 스페이스).
	 *
	 * @return 스케일 적용된 Capsule 반지름
	 */
	float GetScaledCapsuleRadius() const;

	/**
	 * 스케일이 적용된 Capsule 반 높이를 반환합니다 (월드 스페이스).
	 *
	 * @return 스케일 적용된 Capsule 반 높이
	 */
	float GetScaledCapsuleHalfHeight() const;

	/**
	 * 월드 스페이스의 Capsule 중심을 반환합니다.
	 *
	 * @return Capsule 중심 위치 (월드 스페이스)
	 */
	FVector GetCapsuleCenter() const;

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
	 * Capsule을 디버그 렌더링합니다.
	 */
	void DrawDebugShape() const override;

	/**
	 * Renderer를 통해 Capsule을 디버그 렌더링합니다.
	 *
	 * @param Renderer - 렌더러 객체
	 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	/**
	 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
	 * Capsule vs Capsule, Capsule vs Sphere, Capsule vs Box 충돌을 정밀하게 감지합니다.
	 *
	 * @param Other - 확인할 상대방 Shape 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingComponent(const UShapeComponent* Other) const override;

	// ────────────────────────────────────────────────
	// Capsule 전용 충돌 감지 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Capsule 컴포넌트와 겹쳐있는지 확인합니다.
	 * Capsule vs Capsule 충돌 (선분 vs 선분 최단 거리 계산)
	 *
	 * @param OtherCapsule - 확인할 상대방 Capsule 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingCapsule(const UCapsuleComponent* OtherCapsule) const;

	/**
	 * Sphere 컴포넌트와 겹쳐있는지 확인합니다.
	 * Capsule vs Sphere 충돌 (점과 선분 사이의 거리 계산)
	 *
	 * @param OtherSphere - 확인할 상대방 Sphere 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingSphere(const class USphereComponent* OtherSphere) const;

	/**
	 * Box 컴포넌트와 겹쳐있는지 확인합니다.
	 * Capsule vs Box 충돌 (간략 구현 - Bounds 기반)
	 *
	 * @param OtherBox - 확인할 상대방 Box 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingBox(const class UBoxComponent* OtherBox) const;

	/**
	 * 특정 점이 Capsule 내부에 있는지 확인합니다.
	 *
	 * @param Point - 확인할 점 (월드 스페이스)
	 * @return 포함되어 있으면 true
	 */
	bool ContainsPoint(const FVector& Point) const;

private:
	/** 현재 Bounds (캐시됨) */
	FBoxSphereBounds CachedBounds;

	/**
	 * Capsule의 선분 끝점을 계산합니다 (월드 스페이스).
	 * Z축 방향을 기준으로 계산합니다.
	 *
	 * @param OutStart - 시작점 (하단)
	 * @param OutEnd - 끝점 (상단)
	 */
	void GetCapsuleSegment(FVector& OutStart, FVector& OutEnd) const;

	/**
	 * 점과 선분 사이의 최단 거리 제곱을 계산합니다.
	 * 제곱근 연산을 회피하여 성능을 향상시킵니다.
	 *
	 * @param Point - 확인할 점
	 * @param SegmentStart - 선분 시작점
	 * @param SegmentEnd - 선분 끝점
	 * @return 최단 거리의 제곱
	 */
	static float PointToSegmentDistanceSquared(
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd
	);

	/**
	 * 두 선분 사이의 최단 거리 제곱을 계산합니다.
	 *
	 * @param Seg1Start - 첫 번째 선분 시작점
	 * @param Seg1End - 첫 번째 선분 끝점
	 * @param Seg2Start - 두 번째 선분 시작점
	 * @param Seg2End - 두 번째 선분 끝점
	 * @return 최단 거리의 제곱
	 */
	static float SegmentToSegmentDistanceSquared(
		const FVector& Seg1Start,
		const FVector& Seg1End,
		const FVector& Seg2Start,
		const FVector& Seg2End
	);
};