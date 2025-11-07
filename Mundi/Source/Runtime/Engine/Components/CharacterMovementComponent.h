// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.h
// Character의 이동을 처리하는 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "ActorComponent.h"
#include "Vector.h"
#include <sol/sol.hpp>

// 전방 선언
class ACharacter;

/**
 * EMovementMode
 *
 * Character의 이동 모드를 나타냅니다.
 */
enum class EMovementMode : uint8
{
	/** 지면에서 걷기 */
	Walking,

	/** 공중에서 낙하 */
	Falling,

	/** 비행 (중력 무시) */
	Flying,

	/** 이동 불가 */
	None
};

/**
 * UCharacterMovementComponent
 *
 * Character의 이동, 중력, 점프 등을 처리하는 컴포넌트입니다.
 * 기본 물리 시뮬레이션을 수행하며, 나중에 충돌 시스템과 연결됩니다.
 *
 * 주요 기능:
 * - 중력 적용
 * - 속도/가속도 기반 이동
 * - 점프 (타이머 기반)
 * - 이동 모드 관리 (Walking, Falling, Flying)
 */
class UCharacterMovementComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UCharacterMovementComponent, UActorComponent)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(UCharacterMovementComponent)

	UCharacterMovementComponent();
	virtual ~UCharacterMovementComponent() override;

	// ────────────────────────────────────────────────
	// 이동 설정
	// ────────────────────────────────────────────────

	/** 최대 걷기 속도 (cm/s) */
	float MaxWalkSpeed;

	/** 최대 가속도 (cm/s²) */
	float MaxAcceleration;

	/** 마찰력 (감속) */
	float GroundFriction;

	/** 공중 제어력 (0.0 ~ 1.0) */
	float AirControl;

	/** 제동력 (급정지 시) */
	float BrakingDeceleration;

	// ────────────────────────────────────────────────
	// 중력 설정
	// ────────────────────────────────────────────────

	/** 중력 가속도 (cm/s², 음수 = 아래로) */
	float GravityScale;

	/** 기본 중력 (-980 cm/s² ≈ -9.8 m/s²) */
	static constexpr float DefaultGravity = 980.0f;

	/** 중력 방향 벡터 (정규화된 방향, 기본값: 아래) */
	FVector GravityDirection;

	/**
	 * 중력 방향을 설정합니다.
	 * @param NewDirection - 새로운 중력 방향 (자동으로 정규화됨)
	 */
	void SetGravityDirection(const FVector& NewDirection);

	/**
	 * 현재 중력 방향을 반환합니다.
	 */
	FVector GetGravityDirection() const { return GravityDirection; }

	// ────────────────────────────────────────────────
	// 점프 설정
	// ────────────────────────────────────────────────

	/** 점프 초기 속도 (cm/s) */
	float JumpZVelocity;

	/** 공중에 있을 수 있는 최대 시간 (초) */
	float MaxAirTime;

	/** 점프 가능 여부 */
	bool bCanJump;

	// ────────────────────────────────────────────────
	// 이동 함수
	// ────────────────────────────────────────────────

	/**
	 * 입력 벡터를 추가합니다.
	 *
	 * @param WorldDirection - 월드 스페이스 이동 방향
	 * @param ScaleValue - 입력 스케일
	 */
	void AddInputVector(FVector WorldDirection, float ScaleValue = 1.0f);

	/**
	 * 누적된 입력 벡터를 초기화합니다.
	 */
	void ConsumeInputVector() { PendingInputVector = FVector::Zero(); }

	/**
	 * 속도를 직접 설정합니다.
	 *
	 * @param NewVelocity - 새로운 속도 벡터
	 */
	void SetVelocity(const FVector& NewVelocity) { Velocity = NewVelocity; }

	/**
	 * 현재 속도를 반환합니다.
	 */
	FVector GetVelocity() const { return Velocity; }

	/**
	 * 점프를 시도합니다.
	 *
	 * @return 점프에 성공하면 true
	 */
	bool Jump();

	/**
	 * 점프를 중단합니다 (점프 키를 뗐을 때).
	 */
	void StopJumping();

	// ────────────────────────────────────────────────
	// 이동 모드
	// ────────────────────────────────────────────────

	/**
	 * 현재 이동 모드를 반환합니다.
	 */
	EMovementMode GetMovementMode() const { return MovementMode; }

	/**
	 * 이동 모드를 설정합니다.
	 *
	 * @param NewMode - 새로운 이동 모드
	 */
	void SetMovementMode(EMovementMode NewMode);

	/**
	 * 지면에 있는지 확인합니다.
	 */
	bool IsGrounded() const { return MovementMode == EMovementMode::Walking; }

	/**
	 * 낙하 중인지 확인합니다.
	 */
	bool IsFalling() const { return MovementMode == EMovementMode::Falling; }

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	// ────────────────────────────────────────────────
	// 내부 이동 로직
	// ────────────────────────────────────────────────

	/**
	 * 이동 입력을 처리하고 속도를 계산합니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void UpdateVelocity(float DeltaTime);

	/**
	 * 중력을 적용합니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void ApplyGravity(float DeltaTime);

	/**
	 * 계산된 속도로 위치를 업데이트합니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void MoveUpdatedComponent(float DeltaTime);

	/**
	 * 지면 체크 (BoxComponent 오버랩 기반)
	 * AGravityWall의 IsFloor()가 true인 경우 지면으로 인식합니다.
	 *
	 * @return 지면에 있으면 true
	 */
	bool CheckGround();

	/**
	 * 벽 충돌 체크 (BoxComponent 오버랩 기반)
	 * AGravityWall의 IsFloor()가 false인 경우 벽으로 인식합니다.
	 *
	 * @return 벽과 충돌 중이면 true
	 */
	bool CheckWallCollision();

public:
	// ────────────────────────────────────────────────
	// Lua 콜백 (측면 충돌 이벤트)
	// ────────────────────────────────────────────────

	/**
	 * 측면 충돌 시 호출될 Lua 콜백을 설정합니다.
	 * @param Callback - Lua 함수 (매개변수: WallNormal FVector)
	 */
	void SetOnWallCollisionCallback(sol::function Callback);

	// ────────────────────────────────────────────────
	// 회전 상태 제어
	// ────────────────────────────────────────────────

	/**
	 * 회전 중 상태를 설정합니다.
	 * 회전 중일 때는 모든 움직임과 중력이 멈춥니다.
	 * @param bInIsRotating - true면 회전 중, false면 정상 이동
	 */
	void SetIsRotating(bool bInIsRotating);

	/**
	 * 현재 회전 중인지 확인합니다.
	 */
	bool IsRotating() const { return bIsRotating; }

protected:
	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** Owner Character */
	ACharacter* CharacterOwner;

	/** 현재 속도 (cm/s) */
	FVector Velocity;

	/** 이번 프레임 입력 */
	FVector PendingInputVector;

	/** 현재 이동 모드 */
	EMovementMode MovementMode;

	/** 공중에 있었던 시간 (점프/낙하) */
	float TimeInAir;

	/** 점프 중인지 여부 */
	bool bIsJumping;

	/** 회전 중인지 여부 (회전 중에는 모든 움직임과 중력 멈춤) */
	bool bIsRotating;

	/** 측면 충돌 시 호출될 Lua 콜백 함수 */
	sol::function WallCollisionLuaCallback;
};
