// ────────────────────────────────────────────────────────────────────────────
// PlayerController.h
// 플레이어 입력을 처리하는 Controller 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Controller.h"

// 전방 선언
class UInputManager;
class UInputComponent;
class APlayerCameraManager;
class UCameraComponent;

/**
 * APlayerController
 *
 * 플레이어 입력을 처리하는 Controller입니다.
 * UInputManager와 통합되어 키보드/마우스 입력을 Pawn에 전달합니다.
 *
 * 주요 기능:
 * - InputManager와 통합
 * - InputComponent로 키 바인딩 관리
 * - 마우스 커서 제어
 * - 입력 우선순위 관리
 */
class APlayerController : public AController
{
public:
	DECLARE_CLASS(APlayerController, AController)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(APlayerController)

	APlayerController();
	virtual ~APlayerController() override;

	// ────────────────────────────────────────────────
	// Pawn 빙의 관련 (오버라이드)
	// ────────────────────────────────────────────────

	virtual void Possess(APawn* InPawn) override;
	virtual void UnPossess() override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

public:
	// ────────────────────────────────────────────────
	// 입력 처리
	// ────────────────────────────────────────────────

	/**
	 * 매 프레임 호출되어 입력을 처리합니다.
	 * Pawn의 InputComponent에 입력 이벤트를 전달합니다.
	 */
	void ProcessPlayerInput();

	/**
	 * 입력이 활성화되어 있는지 확인합니다.
	 */
	bool IsInputEnabled() const { return bInputEnabled; }

	/**
	 * 입력을 활성화/비활성화합니다.
	 *
	 * @param bEnabled - true면 입력 활성화, false면 비활성화
	 */
	void SetInputEnabled(bool bEnabled) { bInputEnabled = bEnabled; }

	// ────────────────────────────────────────────────
	// 마우스 커서 제어
	// ────────────────────────────────────────────────

	/**
	 * 마우스 커서를 표시합니다.
	 */
	void ShowMouseCursor();

	/**
	 * 마우스 커서를 숨깁니다.
	 */
	void HideMouseCursor();

	/**
	 * 마우스 커서가 표시되는지 확인합니다.
	 */
	bool IsMouseCursorVisible() const { return bShowMouseCursor; }

	/**
	 * 마우스 커서를 잠급니다 (FPS 게임용).
	 * 잠기면 커서가 중앙에 고정되고 Delta만 사용됩니다.
	 */
	void LockMouseCursor();

	/**
	 * 마우스 커서 잠금을 해제합니다.
	 */
	void UnlockMouseCursor();

	/**
	 * 마우스 커서가 잠겨있는지 확인합니다.
	 */
	bool IsMouseCursorLocked() const { return bIsMouseLocked; }

	// ────────────────────────────────────────────────
	// 마우스 감도 설정
	// ────────────────────────────────────────────────

	/**
	 * 마우스 감도를 설정합니다.
	 *
	 * @param Sensitivity - 마우스 감도 (기본값 1.0)
	 */
	void SetMouseSensitivity(float Sensitivity) { MouseSensitivity = Sensitivity; }

	/**
	 * 마우스 감도를 반환합니다.
	 */
	float GetMouseSensitivity() const { return MouseSensitivity; }

	// ────────────────────────────────────────────────
	// 카메라 관리
	// ────────────────────────────────────────────────

	/**
	 * PlayerCameraManager를 반환합니다.
	 */
	APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }

	/**
	 * 렌더링용 카메라 컴포넌트를 반환합니다.
	 * PIE 모드에서 PlayerCameraManager의 카메라를 사용합니다.
	 */
	UCameraComponent* GetCameraComponentForRendering() const;

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** InputManager 참조 */
	UInputManager* InputManager;

	/** 입력 활성화 여부 */
	bool bInputEnabled;

	/** 마우스 커서 표시 여부 */
	bool bShowMouseCursor;

	/** 마우스 커서 잠금 여부 */
	bool bIsMouseLocked;

	/** 마우스 감도 */
	float MouseSensitivity;

	/** 카메라 매니저 (PIE 모드에서만 사용) */
	APlayerCameraManager* PlayerCameraManager;
};
