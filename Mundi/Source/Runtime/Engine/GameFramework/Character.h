// ────────────────────────────────────────────────────────────────────────────
// Character.h
// 이동 가능한 Pawn 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Pawn.h"

// 전방 선언
class UCharacterMovementComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * ACharacter
 *
 * 이동, 점프 등의 기능을 가진 Pawn입니다.
 * CharacterMovementComponent를 통해 물리 기반 이동을 처리합니다.
 *
 * 주요 기능:
 * - CharacterMovementComponent 포함
 * - Jump(), Crouch() 등 기본 동작
 * - 이동 입력 처리
 */
class ACharacter : public APawn
{
public:
	DECLARE_CLASS(ACharacter, APawn)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(ACharacter)

	ACharacter();
	virtual ~ACharacter() override;

	// ────────────────────────────────────────────────
	// 컴포넌트 접근
	// ────────────────────────────────────────────────

	/** CharacterMovementComponent를 반환합니다 */
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

	/** Mesh 컴포넌트를 반환합니다 (나중에 SkeletalMesh로 교체 가능) */
	USceneComponent* GetMesh() const { return MeshComponent; }

	/** StaticMesh 컴포넌트를 반환합니다 */
	UStaticMeshComponent* GetStaticMesh() const { return StaticMeshComponent; }

	// ────────────────────────────────────────────────
	// 이동 입력 처리 (APawn 오버라이드)
	// ────────────────────────────────────────────────

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f) override;

	// ────────────────────────────────────────────────
	// Character 동작
	// ────────────────────────────────────────────────

	/**
	 * 점프를 시도합니다.
	 */
	virtual void Jump();

	/**
	 * 점프를 중단합니다.
	 */
	virtual void StopJumping();

	/**
	 * 점프 가능 여부를 확인합니다.
	 */
	virtual bool CanJump() const;

	/**
	 * 웅크리기를 시작합니다.
	 */
	virtual void Crouch();

	/**
	 * 웅크리기를 해제합니다.
	 */
	virtual void UnCrouch();

	/**
	 * 웅크리고 있는지 확인합니다.
	 */
	bool IsCrouched() const { return bIsCrouched; }

	// ────────────────────────────────────────────────
	// 상태 조회
	// ────────────────────────────────────────────────

	/**
	 * 현재 속도를 반환합니다.
	 */
	virtual FVector GetVelocity() const;

	/**
	 * 지면에 있는지 확인합니다.
	 */
	bool IsGrounded() const;

	/**
	 * 낙하 중인지 확인합니다.
	 */
	bool IsFalling() const;

	// ────────────────────────────────────────────────
	// 이동 입력 함수 (Lua에서 호출 가능)
	// ────────────────────────────────────────────────

	/** 앞/뒤 이동 */
	void MoveForward(float Value);

	/** 좌/우 이동 */
	void MoveRight(float Value);

	/** 좌/우 회전 */
	void Turn(float Value);

	/** 위/아래 회전 */
	void LookUp(float Value);

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 입력 바인딩 (오버라이드)
	// ────────────────────────────────────────────────

	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** Character 이동 컴포넌트 */
	UCharacterMovementComponent* CharacterMovement;

	/** Mesh 컴포넌트 (나중에 SkeletalMeshComponent로 교체) */
	USceneComponent* MeshComponent;

	/** StaticMesh 컴포넌트 (시각적 표현) */
	UStaticMeshComponent* StaticMeshComponent;

	/** 웅크리기 상태 */
	bool bIsCrouched;

	/** 웅크릴 때 높이 비율 */
	float CrouchedHeightRatio;
};
