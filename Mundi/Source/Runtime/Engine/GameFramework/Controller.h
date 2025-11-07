// ────────────────────────────────────────────────────────────────────────────
// Controller.h
// Controller 클래스 - Pawn을 빙의하여 제어하는 기본 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Actor.h"

// 전방 선언
class APawn;

/**
 * AController
 *
 * Pawn을 빙의(Possess)하여 제어하는 기본 Controller 클래스입니다.
 * PlayerController와 AIController의 부모 클래스입니다.
 *
 * 주요 기능:
 * - Pawn 빙의/빙의 해제
 * - Control Rotation 관리
 * - OnPossess/OnUnpossess 이벤트
 */
class AController : public AActor
{
public:
	DECLARE_CLASS(AController, AActor)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(AController)

	AController();
	virtual ~AController() override;

	// ────────────────────────────────────────────────
	// Pawn 빙의 관련
	// ────────────────────────────────────────────────

	/**
	 * Pawn을 빙의합니다.
	 *
	 * @param InPawn - 빙의할 Pawn
	 */
	virtual void Possess(APawn* InPawn);

	/**
	 * 현재 빙의한 Pawn의 빙의를 해제합니다.
	 */
	virtual void UnPossess();

	/**
	 * 현재 빙의한 Pawn을 반환합니다.
	 */
	APawn* GetPawn() const { return PossessedPawn.Get(); }

protected:
	/**
	 * Pawn을 빙의할 때 호출됩니다.
	 * 파생 클래스에서 오버라이드하여 추가 로직을 구현합니다.
	 *
	 * @param InPawn - 빙의한 Pawn
	 */
	virtual void OnPossess(APawn* InPawn);

	/**
	 * Pawn의 빙의를 해제할 때 호출됩니다.
	 * 파생 클래스에서 오버라이드하여 추가 로직을 구현합니다.
	 */
	virtual void OnUnPossess();

public:
	// ────────────────────────────────────────────────
	// Control Rotation
	// ────────────────────────────────────────────────

	/**
	 * Control Rotation을 반환합니다.
	 * Control Rotation은 Controller가 "보고 있는" 방향입니다.
	 */
	FQuat GetControlRotation() const { return ControlRotation; }

	/**
	 * Control Rotation을 설정합니다.
	 *
	 * @param NewRotation - 새로운 Control Rotation
	 */
	void SetControlRotation(const FQuat& NewRotation);

	/**
	 * Control Rotation에 Delta를 추가합니다.
	 *
	 * @param DeltaRotation - 추가할 회전 (Quaternion)
	 */
	void AddControlRotation(const FQuat& DeltaRotation);

	/**
	 * Control Rotation에 Yaw를 추가합니다.
	 *
	 * @param DeltaYaw - 추가할 Yaw (도 단위)
	 */
	void AddYawInput(float DeltaYaw);

	/**
	 * Control Rotation에 Pitch를 추가합니다.
	 *
	 * @param DeltaPitch - 추가할 Pitch (도 단위)
	 */
	void AddPitchInput(float DeltaPitch);

	/**
	 * Control Rotation에 Roll을 추가합니다.
	 *
	 * @param DeltaRoll - 추가할 Roll (도 단위)
	 */
	void AddRollInput(float DeltaRoll);

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** 현재 빙의한 Pawn */
	TWeakPtr<APawn> PossessedPawn;

	/** Controller의 회전 (Controller가 보고 있는 방향) */
	FQuat ControlRotation;
};
