// ────────────────────────────────────────────────────────────────────────────
// OverlapInfo.h
// 충돌 시스템의 Overlap 정보를 담는 구조체
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "Vector.h"

// 전방 선언
class UShapeComponent;
class AActor;

/**
 * FOverlapInfo
 *
 * 충돌 시스템에서 두 컴포넌트가 겹쳤을 때의 정보를 담는 구조체입니다.
 * Unreal Engine의 FOverlapInfo와 유사한 구조를 따릅니다.
 */
struct FOverlapInfo
{
public:
	// ────────────────────────────────────────────────
	// 핵심 데이터
	// ────────────────────────────────────────────────

	/** 충돌한 상대방 컴포넌트 */
	UShapeComponent* OtherComponent;

	/** 충돌한 상대방 액터 */
	AActor* OtherActor;

	/** 충돌 지점 (World Space) */
	FVector ContactPoint;

	/** 침투 깊이 (양수 = 겹친 정도) */
	float PenetrationDepth;

	/** Sweep 이동 중 발생한 충돌인지 여부 */
	bool bFromSweep;

	// ────────────────────────────────────────────────
	// 생성자
	// ────────────────────────────────────────────────

	/**
	 * 기본 생성자
	 */
	FOverlapInfo()
		: OtherComponent(nullptr)
		, OtherActor(nullptr)
		, ContactPoint(FVector::Zero())
		, PenetrationDepth(0.0f)
		, bFromSweep(false)
	{
	}

	/**
	 * 파라미터 생성자
	 *
	 * @param InComponent - 충돌한 상대 컴포넌트
	 * @param InActor - 충돌한 상대 액터
	 * @param InContactPoint - 충돌 지점
	 * @param InPenetrationDepth - 침투 깊이
	 * @param bInFromSweep - Sweep 충돌 여부
	 */
	FOverlapInfo(
		UShapeComponent* InComponent,
		AActor* InActor,
		const FVector& InContactPoint = FVector::Zero(),
		float InPenetrationDepth = 0.0f,
		bool bInFromSweep = false
	)
		: OtherComponent(InComponent)
		, OtherActor(InActor)
		, ContactPoint(InContactPoint)
		, PenetrationDepth(InPenetrationDepth)
		, bFromSweep(bInFromSweep)
	{
	}

	// ────────────────────────────────────────────────
	// 비교 연산자
	// ────────────────────────────────────────────────

	/**
	 * 동일한 충돌 정보인지 확인합니다.
	 * OtherComponent와 OtherActor를 기준으로 비교합니다.
	 *
	 * @param Other - 비교할 다른 FOverlapInfo
	 * @return 동일한 충돌 정보이면 true
	 */
	bool operator==(const FOverlapInfo& Other) const
	{
		return OtherComponent == Other.OtherComponent &&
		       OtherActor == Other.OtherActor;
	}

	/**
	 * 다른 충돌 정보인지 확인합니다.
	 *
	 * @param Other - 비교할 다른 FOverlapInfo
	 * @return 다른 충돌 정보이면 true
	 */
	bool operator!=(const FOverlapInfo& Other) const
	{
		return !(*this == Other);
	}

	// ────────────────────────────────────────────────
	// 유틸리티 함수
	// ────────────────────────────────────────────────

	/**
	 * 유효한 충돌 정보인지 확인합니다.
	 *
	 * @return OtherComponent와 OtherActor가 모두 유효하면 true
	 */
	bool IsValid() const
	{
		return OtherComponent != nullptr && OtherActor != nullptr;
	}

	/**
	 * 충돌 정보를 초기화합니다.
	 */
	void Reset()
	{
		OtherComponent = nullptr;
		OtherActor = nullptr;
		ContactPoint = FVector::Zero();
		PenetrationDepth = 0.0f;
		bFromSweep = false;
	}
};
