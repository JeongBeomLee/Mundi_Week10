#pragma once
#include "Info.h"

// 게임 상태 열거형
enum class EGameState : uint8
{
	NotStarted,  // 게임 시작 전
	Playing,     // 게임 진행 중
	Paused,      // 일시 정지
	GameOver,    // 게임 오버
	Victory      // 승리
};

// 게임 상태 변경 델리게이트 (OldState, NewState)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged, EGameState, EGameState);

// 스코어 변경 델리게이트 (OldScore, NewScore)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScoreChanged, int32, int32);

// 타이머 업데이트 델리게이트 (ElapsedTime)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimerUpdated, float);

// 게임 전역 상태를 관리하는 클래스
// AInfo를 상속받아 물리적 실체 없이 데이터만 관리
class AGameStateBase : public AInfo
{
public:
	DECLARE_CLASS(AGameStateBase, AInfo)
	GENERATED_REFLECTION_BODY()

	AGameStateBase();
	virtual ~AGameStateBase() = default;

	// AActor 오버라이드
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 게임 상태 관리
	EGameState GetGameState() const { return CurrentGameState; }
	void SetGameState(EGameState NewState);

	// 스코어 관리
	int32 GetScore() const { return Score; }
	void SetScore(int32 NewScore);
	void AddScore(int32 Points);

	// 타이머 관리
	float GetElapsedTime() const { return ElapsedTime; }
	void ResetTimer();
	void PauseTimer();
	void ResumeTimer();

	// 델리게이트
	FOnGameStateChanged OnGameStateChanged;
	FOnScoreChanged OnScoreChanged;
	FOnTimerUpdated OnTimerUpdated;

	DECLARE_DUPLICATE(AGameStateBase)
	void DuplicateSubObjects() override;

protected:
	// 현재 게임 상태
	EGameState CurrentGameState;

	// 스코어
	int32 Score;

	// 경과 시간 (초 단위)
	float ElapsedTime;

	// 타이머 일시정지 플래그
	bool bTimerPaused;

	// 일시정지 효과 ID (DeltaTimeManager용)
	uint32 PauseEffectID;
};
