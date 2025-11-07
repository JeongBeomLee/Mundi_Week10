// ────────────────────────────────────────────────────────────────────────────
// SphereComponent.h
// Sphere 형태의 충돌 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "ShapeComponent.h"
#include "Sphere.h"

class UShader;

/**
 * USphereComponent
 *
 * Sphere(구) 형태의 충돌 컴포넌트입니다.
 * FSphere를 사용하여 충돌을 감지합니다.
 *
 * 주요 기능:
 * - Sphere vs Sphere 충돌 감지
 * - Sphere vs Box 충돌 감지
 * - 디버그 Sphere 렌더링
 */
class USphereComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(USphereComponent, UShapeComponent)
	
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(USphereComponent)
	USphereComponent();

protected:
	~USphereComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;

public:
	// ────────────────────────────────────────────────
	// Sphere 속성
	// ────────────────────────────────────────────────

	/** Sphere의 반지름 (로컬 스페이스) */
	float SphereRadius = 50.0f;

	/**
	 * Sphere 반지름을 설정합니다.
	 *
	 * @param InRadius - 새로운 반지름
	 * @param bUpdateBounds - Bounds를 즉시 업데이트할지 여부
	 */
	void SetSphereRadius(float InRadius, bool bUpdateBounds = true);

	/**
	 * Sphere 반지름을 반환합니다 (로컬 스페이스).
	 *
	 * @return Sphere 반지름
	 */
	float GetSphereRadius() const { return SphereRadius; }

	/**
	 * 스케일이 적용된 Sphere 반지름을 반환합니다 (월드 스페이스).
	 *
	 * @return 스케일 적용된 Sphere 반지름
	 */
	float GetScaledSphereRadius() const;

	/**
	 * 월드 스페이스의 Sphere 중심을 반환합니다.
	 *
	 * @return Sphere 중심 위치 (월드 스페이스)
	 */
	FVector GetSphereCenter() const;

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
	 * Sphere를 디버그 렌더링합니다.
	 */
	void DrawDebugShape() const override;

	/**
	 * Renderer를 통해 Sphere를 디버그 렌더링합니다.
	 *
	 * @param Renderer - 렌더러 객체
	 */
	void RenderDebugVolume(class URenderer* Renderer) const override;

	/**
	 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
	 * Sphere vs Sphere, Sphere vs Box 충돌을 정밀하게 감지합니다.
	 *
	 * @param Other - 확인할 상대방 Shape 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingComponent(const UShapeComponent* Other) const override;

	// ────────────────────────────────────────────────
	// Sphere 전용 충돌 감지 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Sphere 컴포넌트와 겹쳐있는지 확인합니다.
	 * Sphere vs Sphere 충돌 (중심 거리 비교)
	 *
	 * @param OtherSphere - 확인할 상대방 Sphere 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingSphere(const USphereComponent* OtherSphere) const;

	/**
	 * Box 컴포넌트와 겹쳐있는지 확인합니다.
	 *
	 * @param OtherBox - 확인할 상대방 Box 컴포넌트
	 * @return 겹쳐있으면 true
	 */
	bool IsOverlappingBox(const class UBoxComponent* OtherBox) const;

	/**
	 * 특정 점이 Sphere 내부에 있는지 확인합니다.
	 *
	 * @param Point - 확인할 점 (월드 스페이스)
	 * @return 포함되어 있으면 true
	 */
	bool ContainsPoint(const FVector& Point) const;

private:
	/** 현재 Bounds (캐시됨) */
	FBoxSphereBounds CachedBounds;
};