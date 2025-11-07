// ────────────────────────────────────────────────────────────────────────────
// RunnerGameMode.h
// 런2 게임 전용 GameMode
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "GameModeBase.h"
#include "Delegate.h"

// 전방 선언
class ARunnerGameState;
class ACharacter;

// 플레이어 체력 감소 델리게이트 선언 (클래스 외부에서)
using FOnPlayerHealthDecreased = TMulticastDelegate<int32>;

constexpr int PLAYER_HEALTH = 3;

/**
 * ARunnerGameMode
 *
 * 런2 게임의 규칙과 로직을 정의합니다.
 * RunnerCharacter를 기본 Pawn으로 사용합니다.
 */
class ARunnerGameMode : public AGameModeBase
{
public:
	DECLARE_CLASS(ARunnerGameMode, AGameModeBase)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(ARunnerGameMode)

	ARunnerGameMode();
	virtual ~ARunnerGameMode() override;

	// ────────────────────────────────────────────────
	// 게임 이벤트
	// ────────────────────────────────────────────────

	void OnDecreasePlayerHealth(ACharacter* Player, uint32 DamageAmount);

	/** 플레이어 사망 */
	void OnPlayerDeath(ACharacter* Player);

	/** 코인 수집 */
	void OnCoinCollected(int32 CoinValue);

	/** 장애물 회피 */
	void OnObstacleAvoided();

	/** 점프 */
	void OnPlayerJump();

	// ────────────────────────────────────────────────
	// 델리게이트
	// ────────────────────────────────────────────────

	/** 플레이어 체력 감소 시 호출되는 델리게이트 (현재 남은 체력 전달) */
	FOnPlayerHealthDecreased OnPlayerHealthDecreased;

	// ────────────────────────────────────────────────
	// RunnerGameState 접근 (나중에 추가)
	// ────────────────────────────────────────────────

	// ARunnerGameState* GetRunnerGameState() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 카메라 블렌딩 완료 시 호출되는 핸들러
	void OnCameraBlendComplete();

	// 게임 상태 변경 핸들러 (NotStarted -> Playing 시 카메라 재설정)
	void OnGameStateChanged(EGameState OldState, EGameState NewState);

public:
	/** 게임 재시작 (플레이어 리스폰) */
	virtual void RestartGame() override;

	/** 난이도 설정 */
	float BaseDifficulty = 1.0f;
	float DifficultyIncreaseRate = 0.1f;

	/** 점수 설정 */
	int32 JumpScore = 10;
	int32 CoinScore = 0;
	int32 AvoidScore = 20;

	/** 사운드 지연 재생을 위한 타이머 */
	float SoundDelayTimer = 0.0f;
	bool bWaitingForSecondSound = false;

private:
	/** 카메라 흔들림 모디파이어 (게임 시작 시 제거하기 위해 저장) */
	class UCameraModifier_CustomBlend* HeadBobModifier = nullptr;
};
