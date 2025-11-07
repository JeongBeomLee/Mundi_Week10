#pragma once
#include "Object.h"
#include "WeakPtr.h"
#include "GameModeBase.h"

// PIE 모드에서 게임을 제어하는 UI 윈도우
// Start, Pause, Resume, Restart, End Game 버튼 제공
class UGameControlWindow : public UObject
{
public:
	DECLARE_CLASS(UGameControlWindow, UObject)

	UGameControlWindow();
	virtual ~UGameControlWindow() override = default;

	// 초기화
	void Initialize();

	// 매 프레임 업데이트
	void Update();

	// ImGui 렌더링
	void RenderWidget();

	// GameMode 설정
	void SetGameMode(AGameModeBase* InGameMode);

private:
	// GameMode 참조 (약한 참조로 안전하게 관리)
	TWeakPtr<AGameModeBase> GameMode;

	// 버튼 활성화 상태를 게임 상태에 따라 결정
	bool IsStartButtonEnabled() const;
	bool IsPauseButtonEnabled() const;
	bool IsResumeButtonEnabled() const;
	bool IsRestartButtonEnabled() const;
};
