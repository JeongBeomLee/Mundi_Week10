#include "pch.h"
#include "GameStateBase.h"
#include "DeltaTimeManager.h"

// AGameStateBase를 ObjectFactory에 등록
IMPLEMENT_CLASS(AGameStateBase)

// 프로퍼티 등록
BEGIN_PROPERTIES(AGameStateBase)
	// Score 프로퍼티 등록 (에디터에서 수정 가능)
	ADD_PROPERTY_RANGE(int32, Score, "GameState", 0, 999999, true, "현재 게임 스코어입니다.")
	// ElapsedTime 프로퍼티 등록 (읽기 전용)
	ADD_PROPERTY(float, ElapsedTime, "GameState", false, "게임 경과 시간(초)입니다.")
END_PROPERTIES()

AGameStateBase::AGameStateBase()
	: CurrentGameState(EGameState::NotStarted)
	, Score(0)
	, ElapsedTime(0.0f)
	, bTimerPaused(true)
{
	// GameState는 물리/렌더링 대상이 아님 (Info 상속)
}

void AGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	// 게임 시작 시 타이머 초기화
	ElapsedTime = 0.0f;
	bTimerPaused = true;
}

void AGameStateBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 타이머가 일시정지 상태가 아니고 게임이 진행 중일 때만 시간 증가
	if (CurrentGameState == EGameState::Playing || CurrentGameState == EGameState::Paused)
	{
		ElapsedTime += DeltaTime;
		OnTimerUpdated.Broadcast(ElapsedTime);
	}
}

void AGameStateBase::SetGameState(EGameState NewState)
{
	if (CurrentGameState != NewState)
	{
		EGameState OldState = CurrentGameState;
		CurrentGameState = NewState;

		// 게임 상태에 따라 타이머 자동 제어
		if (NewState == EGameState::Playing)
		{
			ResumeTimer();
		}
		else if (NewState == EGameState::Paused /*|| NewState == EGameState::GameOver*/ || NewState == EGameState::Victory)
		{
			PauseTimer();
		}

		// 델리게이트 브로드캐스트
		OnGameStateChanged.Broadcast(OldState, NewState);

		UE_LOG("GameState changed: %d -> %d", static_cast<int>(OldState), static_cast<int>(NewState));
	}
}

void AGameStateBase::SetScore(int32 NewScore)
{
	if (this && Score != NewScore)
	{
		int32 OldScore = Score;
		Score = NewScore;

		// 델리게이트 브로드캐스트
		OnScoreChanged.Broadcast(OldScore, NewScore);

		UE_LOG("Score changed: %d -> %d", OldScore, NewScore);
	}
}

void AGameStateBase::AddScore(int32 Points)
{
	SetScore(Score + Points);
}

void AGameStateBase::ResetTimer()
{
	ElapsedTime = 0.0f;
	bTimerPaused = true;
	OnTimerUpdated.Broadcast(ElapsedTime);
}

void AGameStateBase::PauseTimer()
{
	PauseEffectID = GWorld->GetDeltaTimeManager()->ApplyHitStop(FLT_MAX, ETimeDilationPriority::Critical);
	bTimerPaused = true;
}

void AGameStateBase::ResumeTimer()
{
	GWorld->GetDeltaTimeManager()->CancelEffect(PauseEffectID);
	PauseEffectID = 0; // 디버거에서 현재 Pause효과가 적용됐는지 확인용
	bTimerPaused = false;
}

void AGameStateBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	// GameState는 복제 시 상태 초기화
	CurrentGameState = EGameState::NotStarted;
	Score = 0;
	ElapsedTime = 0.0f;
	bTimerPaused = true;
}
