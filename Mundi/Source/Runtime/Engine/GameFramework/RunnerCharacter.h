// ────────────────────────────────────────────────────────────────────────────
// RunnerCharacter.h
// Runner 게임용 캐릭터 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Character.h"

// 전방 선언
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UParticleComponent;
/**
 * ARunnerCharacter
 *
 * Runner 게임에 특화된 캐릭터 클래스입니다.
 *
 * 주요 기능:
 * - 자동 전진 이동 (일정 속도)
 * - 좌우 자유 이동 (입력 기반)
 * - 점프 기능
 * - 동적 중력 방향 변경 지원
 * - 4방향 벽면 감지 (나중에 구현)
 */
class ARunnerCharacter : public ACharacter
{
public:
	DECLARE_CLASS(ARunnerCharacter, ACharacter)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(ARunnerCharacter)

	ARunnerCharacter();
	virtual ~ARunnerCharacter() override;

	// ────────────────────────────────────────────────
	// 컴포넌트 접근
	// ────────────────────────────────────────────────

	/** 충돌 BoxComponent를 반환합니다 */
	UBoxComponent* GetCollisionBox() const { return CollisionBox; }

	/** CameraComponent를 반환합니다 */
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	/** SpringArmComponent를 반환합니다 */
	USpringArmComponent* GetSpringArm() const { return SpringArmComponent; }

	/** ParticleComponent를 반환합니다 */
	UParticleComponent* GetParticleComponent() const { return SpriteComponent; }

	void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

	// ────────────────────────────────────────────────
	// 유틸리티 함수 (Lua에서 호출)
	// ────────────────────────────────────────────────

	/**
	 * 현재 "위쪽" 방향을 반환합니다 (중력 반대)
	 */
	FVector GetUpDirection() const;

	/**
	 * 현재 "우측" 방향을 반환합니다 (전진 방향과 위쪽에 수직)
	 */
	FVector GetRightDirection() const;

	/**
	 * 전진 방향을 반환합니다 (현재는 항상 X축)
	 */
	FVector GetForwardDirection() const;

	// ────────────────────────────────────────────────
	// 중력 방향 제어
	// ────────────────────────────────────────────────

	/**
	 * 중력 방향을 설정합니다 (4방향 벽면 전환용)
	 * @param NewGravityDir - 새로운 중력 방향 벡터
	 */
	void SetGravityDirection(const FVector& NewGravityDir);

	/**
	 * 현재 중력 방향을 반환합니다.
	 */
	FVector GetGravityDirection() const;

	// ────────────────────────────────────────────────
	// 카메라 제어
	// ────────────────────────────────────────────────

	/**
	 * 카메라 위치를 업데이트합니다.
	 */
	void UpdateCameraPosition();

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 입력
	// ────────────────────────────────────────────────



	// ────────────────────────────────────────────────
	// 복제 (Duplication)
	// ────────────────────────────────────────────────

	virtual void DuplicateSubObjects() override;

	// ────────────────────────────────────────────────
	// 컴포넌트
	// ────────────────────────────────────────────────

	/** 충돌 감지용 Box 컴포넌트 */
	UBoxComponent* CollisionBox;
	UParticleComponent* SpriteComponent;
	/** 카메라 컴포넌트 */
	UCameraComponent* CameraComponent;
	
	USpringArmComponent* SpringArmComponent;
	// ────────────────────────────────────────────────
	// 카메라 설정
	// ────────────────────────────────────────────────

	/** 카메라 오프셋 (로컬 좌표계 기준) */
	/** TODO: SpringArm */
	FVector CameraOffset;
};
