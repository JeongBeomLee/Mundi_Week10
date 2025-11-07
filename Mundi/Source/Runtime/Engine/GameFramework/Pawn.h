// ────────────────────────────────────────────────────────────────────────────
// Pawn.h
// Pawn 클래스 - Controller에 의해 빙의될 수 있는 Actor
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Actor.h"

// 전방 선언
class AController;
class UInputComponent;

/**
 * APawn
 *
 * Controller에 의해 빙의(Possess)될 수 있는 Actor입니다.
 * 플레이어나 AI에 의해 제어될 수 있는 기본 단위입니다.
 *
 * 주요 기능:
 * - Controller 참조 관리
 * - 입력 바인딩 설정 (SetupPlayerInputComponent)
 * - 기본 이동 입력 처리
 */
class APawn : public AActor
{
public:
	DECLARE_CLASS(APawn, AActor)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(APawn)

	APawn();
	virtual ~APawn() override;

	// ────────────────────────────────────────────────
	// Controller 관련
	// ────────────────────────────────────────────────

	/** 현재 이 Pawn을 제어하는 Controller를 반환합니다 */
	AController* GetController() const { return Controller; }

	/** Controller에 의해 빙의당할 때 호출됩니다 */
	virtual void PossessedBy(AController* NewController);

	/** Controller의 빙의가 해제될 때 호출됩니다 */
	virtual void UnPossessed();

	// ────────────────────────────────────────────────
	// 입력 관련
	// ────────────────────────────────────────────────

	/** InputComponent를 반환합니다 */
	UInputComponent* GetInputComponent() const { return InputComponent; }

	/**
	 * 플레이어 입력 바인딩을 설정합니다.
	 * PlayerController가 Pawn을 빙의할 때 호출됩니다.
	 *
	 * @param InInputComponent - 바인딩을 설정할 InputComponent
	 */
	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent);

	// ────────────────────────────────────────────────
	// 이동 입력 처리
	// ────────────────────────────────────────────────

	/**
	 * 이동 입력을 추가합니다.
	 * 입력 벡터는 월드 스페이스 방향이며, 자동으로 정규화됩니다.
	 *
	 * @param WorldDirection - 월드 스페이스 이동 방향
	 * @param ScaleValue - 입력 스케일 (보통 -1.0 ~ 1.0)
	 */
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f);

	/**
	 * 이번 프레임에 누적된 이동 입력을 반환합니다.
	 * 자동으로 정규화되어 있습니다.
	 */
	FVector GetPendingMovementInput() const { return PendingMovementInput; }

	/**
	 * 이동 입력을 소비하고 초기화합니다.
	 * 보통 MovementComponent에서 호출합니다.
	 */
	virtual FVector ConsumeMovementInput();

	/**
	 * Controller의 Yaw 회전을 Pawn에 적용합니다.
	 * 기본 구현은 Pawn의 Yaw만 Controller의 Yaw로 설정합니다.
	 *
	 * @param DeltaYaw - 추가할 Yaw 값 (도 단위)
	 */
	virtual void AddControllerYawInput(float DeltaYaw);

	/**
	 * Controller의 Pitch 회전을 Pawn에 적용합니다.
	 * 기본 구현은 아무것도 하지 않습니다 (카메라 등에서 오버라이드).
	 *
	 * @param DeltaPitch - 추가할 Pitch 값 (도 단위)
	 */
	virtual void AddControllerPitchInput(float DeltaPitch);

	/**
	 * Controller의 Roll 회전을 Pawn에 적용합니다.
	 * 기본 구현은 아무것도 하지 않습니다.
	 *
	 * @param DeltaRoll - 추가할 Roll 값 (도 단위)
	 */
	virtual void AddControllerRollInput(float DeltaRoll);
	virtual void BeginPlay() override;
protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	
	virtual void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** 이 Pawn을 제어하는 Controller */
	AController* Controller;

	/** 입력 바인딩을 관리하는 컴포넌트 */
	UInputComponent* InputComponent;

	/** 이번 프레임에 누적된 이동 입력 */
	FVector PendingMovementInput;

	/** 이동 입력을 자동으로 정규화할지 여부 */
	bool bNormalizeMovementInput;
};
