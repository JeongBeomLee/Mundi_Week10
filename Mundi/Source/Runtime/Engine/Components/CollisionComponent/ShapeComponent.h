// ────────────────────────────────────────────────────────────────────────────
// ShapeComponent.h
// 충돌 시스템의 기본 Shape 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "PrimitiveComponent.h"
#include "BoxSphereBounds.h"
#include "OverlapInfo.h"
#include "Delegate.h"

// 전방 선언
class UShader;

/**
 * FComponentOverlapSignature
 *
 * Overlap 이벤트를 처리하는 델리게이트 시그니처입니다.
 *
 * @param OverlappedComponent - Overlap이 발생한 자신의 컴포넌트
 * @param OtherActor - 충돌한 상대방 액터
 * @param OtherComp - 충돌한 상대방 컴포넌트
 * @param ContactPoint - 충돌 지점 (World Space)
 * @param PenetrationDepth - 침투 깊이
 */
DECLARE_MULTICAST_DELEGATE_FiveParams(
	FComponentOverlapSignature,
	UShapeComponent*,      // OverlappedComponent
	AActor*,               // OtherActor
	UShapeComponent*,      // OtherComp
	const FVector&,        // ContactPoint
	float                  // PenetrationDepth
);

/**
 * UShapeComponent
 *
 * 충돌 감지를 위한 기본 Shape 컴포넌트 클래스입니다.
 * Box, Sphere, Capsule 등의 구체적인 Shape 컴포넌트들의 부모 클래스입니다.
 *
 * 주요 기능:
 * - Overlap 이벤트 처리
 * - 디버그 렌더링
 * - Bounds 계산 및 관리
 */
class UShapeComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)
	GENERATED_REFLECTION_BODY()


	UShapeComponent();

protected:
	~UShapeComponent() override;

public:
	// ────────────────────────────────────────────────
	// 복사 관련
	// ────────────────────────────────────────────────
	void DuplicateSubObjects() override;
	
public:
	// ────────────────────────────────────────────────
	// 렌더링 속성
	// ────────────────────────────────────────────────

	/** Shape의 디버그 렌더링 색상 (RGB, 0.0 ~ 1.0) */
	FVector ShapeColor = FVector(0.0f, 1.0f, 0.0f);

	/** 선택된 경우에만 디버그 렌더링 여부 */
	bool bDrawOnlyIfSelected = false;

	// ────────────────────────────────────────────────
	// 충돌 속성
	// ────────────────────────────────────────────────

	/** Overlap 이벤트 생성 여부 */
	bool bGenerateOverlapEvents = true;

	/** 물리적 충돌 차단 여부 (현재 미구현, 추후 Physics 시스템 연동용) */
	bool bBlockComponent = false;

	// ────────────────────────────────────────────────
	// Overlap 정보
	// ────────────────────────────────────────────────

	/** 현재 겹쳐있는 컴포넌트들의 정보 */
	TArray<FOverlapInfo> OverlapInfos;

	/** 현재 충돌 중인지 여부 (디버그 시각화용) */
	bool bIsOverlapping = false;

	// ────────────────────────────────────────────────
	// Overlap 델리게이트
	// ────────────────────────────────────────────────

	/** Overlap 시작 시 호출되는 멀티캐스트 델리게이트 */
	FComponentOverlapSignature OnComponentBeginOverlap;

	/** Overlap 종료 시 호출되는 멀티캐스트 델리게이트 */
	FComponentOverlapSignature OnComponentEndOverlap;

	// ────────────────────────────────────────────────
	// 순수 가상 함수 (자식 클래스에서 구현 필요)
	// ────────────────────────────────────────────────

	/**
	 * Shape를 디버그 렌더링합니다 (구형 인터페이스, 호환성 유지용).
	 * 각 Shape 타입(Box, Sphere, Capsule)에 맞게 구현해야 합니다.
	 */
	virtual void DrawDebugShape() const {};

	/**
	 * Renderer를 통해 디버그 볼륨을 렌더링합니다.
	 * SceneRenderer에서 자동으로 호출됩니다.
	 *
	 * @param Renderer - 렌더러 객체
	 */
	virtual void RenderDebugVolume(class URenderer* Renderer) const override;

	/**
	 * Bounds를 업데이트합니다.
	 * Transform 변경 시 호출되어야 합니다.
	 */
	virtual void UpdateBounds() {};

	/**
	 * 스케일이 적용된 Bounds를 반환합니다.
	 *
	 * @return FBoxSphereBounds 구조체 (Box와 Sphere 정보 포함)
	 */
	virtual FBoxSphereBounds GetScaledBounds() const { return FBoxSphereBounds(); };

	// ────────────────────────────────────────────────
	// Overlap 관련 함수
	// ────────────────────────────────────────────────

	/**
	 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
	 *
	 * @param Other - 확인할 상대방 Shape 컴포넌트
	 * @return 겹쳐있으면 true, 아니면 false
	 */
	virtual bool IsOverlappingComponent(const UShapeComponent* Other) const;

	/**
	 * 현재 Overlap 상태를 업데이트하고 이벤트를 발생시킵니다.
	 * 매 프레임 호출되어 Overlap 상태 변화를 감지합니다.
	 *
	 * @param OtherComponents - 확인할 다른 Shape 컴포넌트들의 배열
	 */
	virtual void UpdateOverlaps(const TArray<UShapeComponent*>& OtherComponents);

	/**
	 * 특정 컴포넌트와의 Overlap 정보를 찾습니다.
	 *
	 * @param OtherComponent - 찾을 컴포넌트
	 * @return Overlap 정보 포인터 (없으면 nullptr)
	 */
	FOverlapInfo* FindOverlapInfo(const UShapeComponent* OtherComponent);

	/**
	 * 특정 컴포넌트와의 Overlap 정보를 제거합니다.
	 *
	 * @param OtherComponent - 제거할 컴포넌트
	 * @return 제거 성공 여부
	 */
	bool RemoveOverlapInfo(const UShapeComponent* OtherComponent);
	
	
	DECLARE_DUPLICATE(UShapeComponent)
protected:
	// ────────────────────────────────────────────────
	// 생명주기 함수
	// ────────────────────────────────────────────────

	/**
	 * 컴포넌트가 생성되어 게임에 활성화될 때 호출됩니다.
	 * CollisionManager에 자동 등록됩니다.
	 */
	virtual void BeginPlay();

	/**
	 * 컴포넌트가 파괴될 때 호출됩니다.
	 * CollisionManager에서 자동 해제됩니다.
	 */
	void EndPlay(EEndPlayReason Reason) override;

	//virtual void EndPlay();

	/**
	 * Transform이 변경될 때 호출됩니다.
	 * CollisionManager에 Dirty 마킹합니다.
	 */
	virtual void OnTransformChanged();

	/**
	 * SceneComponent의 OnTransformUpdated 오버라이드
	 * Transform 변경 시 OnTransformChanged를 호출합니다.
	 */
	virtual void OnTransformUpdated() override;
};