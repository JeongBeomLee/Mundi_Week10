#pragma once
#include "Widget.h"
#include "GameStateBase.h"

// 전방 선언
class ARunnerGameMode;

// 게임 HUD 위젯 (PIE 모드에서만 표시)
// 스코어, 타이머, 게임 상태를 화면 상단에 오버레이로 표시
class UGameHUDWidget : public UWidget
{
public:
	DECLARE_CLASS(UGameHUDWidget, UWidget)

	UGameHUDWidget();
	virtual ~UGameHUDWidget() = default;

	// UWidget 오버라이드
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	// GameState 설정
	void SetGameState(AGameStateBase* InGameState);

	// GameMode 설정 (체력 델리게이트 바인딩용)
	void SetGameMode(ARunnerGameMode* InGameMode);

	// 뷰포트 영역 설정 (게임 오버 팝업 표시 영역 제한용)
	void SetViewportBounds(float X, float Y, float Width, float Height);

	// 델리게이트 바인딩 해제 (GameState 변경 시)
	void UnbindDelegates();

private:
	// 델리게이트 핸들러
	void OnGameStateChanged_Handler(EGameState OldState, EGameState NewState);
	void OnScoreChanged_Handler(int32 OldScore, int32 NewScore);
	void OnTimerUpdated_Handler(float ElapsedTime);
	void OnPlayerHealthDecreased_Handler(int32 CurrentHealth);

	// GameState 참조 (TWeakPtr로 안전하게 참조)
	TWeakPtr<AGameStateBase> GameState;

	// GameMode 참조 (체력 델리게이트용)
	TWeakPtr<ARunnerGameMode> GameMode;

	// 델리게이트 핸들 (바인딩 해제 시 사용)
	size_t GameStateChangedHandle;
	size_t ScoreChangedHandle;
	size_t TimerUpdatedHandle;
	size_t PlayerHealthDecreasedHandle;

	// 캐시된 UI 데이터
	int32 CachedScore;
	float CachedElapsedTime;
	FString CachedGameStateText;
	int32 CachedPlayerHealth;

	// 뷰포트 영역 (게임 오버 오버레이 제한용)
	float ViewportX;
	float ViewportY;
	float ViewportWidth;
	float ViewportHeight;

	// 하트 아이콘 텍스처
	void* HeartIconTexture;
	void* EmptyHeartIconTexture;
	int HeartIconWidth;
	int HeartIconHeight;

	// UI 렌더링 함수
	void RenderNotStartedState(); // NotStarted 상태 렌더링 (타이틀 화면 + 입력)
	void RenderGameOverState(bool bIsVictory); // GameOver/Victory 상태 렌더링 (팝업 + 입력)
	void RenderPlayingState(); // Playing/Paused 상태 렌더링 (HUD + 입력)

	// UI 헬퍼 함수
	FString FormatTime(float Seconds) const;
	FString GetGameStateText() const;
	ImVec4 GetGameStateColor() const;
	void LoadHeartIcon();
	void RenderHeartIcons();
	void RenderTitleScreen(); // 타이틀 화면 렌더링
};
