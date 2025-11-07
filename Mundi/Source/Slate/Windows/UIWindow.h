#pragma once
#include "Object.h"
#include "UEContainer.h"
#include "Object.h"
#include "ImGui/imgui.h"
#include "Factory/UIWindowFactory.h"



class UWidget;
/**
 * @brief UI 윈도우 표시 상태 열거형
 */
enum class EUIWindowState : uint8
{
	Hidden, // 숨김
	Visible, // 보임
	Minimized, // 최소화
	Maximized // 최대화
};

/**
 * @brief UI 윈도우 설정 구조체
 */
struct FUIWindowConfig
{
	FString WindowTitle = "Untitled Window"; // 윈도우 제목
	ImVec2 DefaultSize = ImVec2(300, 400); // 기본 크기
	ImVec2 DefaultPosition = ImVec2(100, 100); // 기본 위치
	ImVec2 MinSize = ImVec2(200, 150); // 최소 크기
	ImVec2 MaxSize = ImVec2(1920, 1080); // 최대 크기

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None; // ImGui 윈도우 플래그
	EUIWindowState InitialState = EUIWindowState::Visible; // 초기 상태
	EUIDockDirection DockDirection = EUIDockDirection::None; // 도킹 방향

	bool bResizable = true; // 크기 조절 가능 여부
	bool bMovable = true; // 이동 가능 여부
	bool bCollapsible = true; // 접기 가능 여부
	bool bAutoFocus = false; // 자동 포커스 여부
	bool bBringToFrontOnFocus = true; // 포커스시 앞으로 가져오기

	int Priority = 0; // 렌더링 우선순위 (낮을수록 먼저 렌더링)

	/**
	 * @brief 기본 생성자
	 */
	FUIWindowConfig() = default;

	/**
	 * @brief 윈도우 플래그 자동 설정
	 */
	void UpdateWindowFlags()
	{
		WindowFlags = ImGuiWindowFlags_None;

		if (!bResizable) WindowFlags |= ImGuiWindowFlags_NoResize;
		if (!bMovable) WindowFlags |= ImGuiWindowFlags_NoMove;
		if (!bCollapsible) WindowFlags |= ImGuiWindowFlags_NoCollapse;
		if (bAutoFocus) WindowFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
		if (!bBringToFrontOnFocus) WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}
};

/**
 * @brief 추상 UI 윈도우 기본 클래스
 *
 * @param Config 윈도우 설정
 * @param CurrentState 현재 윈도우 상태
 * @param WindowID 고유 윈도우 ID
 * @param bIsFocused 현재 포커스 상태
 * @param LastFocusTime 마지막 포커스 시간
 * @param bIsWindowOpen ImGui 윈도우 열림 상태
 * @param LastWindowSize 마지막 윈도우 크기
 * @param LastWindowPos 마지막 윈도우 위치
 */
class UUIWindow :
	public UObject
{
	friend class UUIManager;
public:
	DECLARE_CLASS(UUIWindow, UObject)
	UUIWindow(const FUIWindowConfig& InConfig = FUIWindowConfig());
	~UUIWindow() override;

	virtual void Initialize();
	virtual void Cleanup() {}
	virtual void OnFocusGained() {}
	virtual void OnFocusLost() {}

	virtual bool OnWindowClose() { return true; }
	virtual bool IsSingleton() { return false; }

	// Getter & Setter
	const FUIWindowConfig& GetConfig() const { return Config; }
	FUIWindowConfig& GetMutableConfig() { return Config; }
	EUIWindowState GetWindowState() const { return CurrentState; }
	const uint32& GetWindowID() const { return WindowID; }
	const FString& GetWindowTitle() const { return Config.WindowTitle; }
	int GetPriority() const { return Config.Priority; }
	float GetLastFocusTime() const { return LastFocusTime; }
	bool IsFocused() const { return bIsFocused; }
	const TArray<UWidget*>& GetWidgets() const { return Widgets; }
	void SetIsResized(bool IsResized) { bIsResized = IsResized; }

	bool IsVisible() const
	{
		return CurrentState == EUIWindowState::Visible || CurrentState == EUIWindowState::Maximized;
	}

	void SetWindowState(EUIWindowState NewState);
	void SetWindowTitle(const FString& NewTitle) { Config.WindowTitle = NewTitle; }
	void SetPriority(int NewPriority) { Config.Priority = NewPriority; }
	void SetConfig(const FUIWindowConfig& InConfig) { Config = InConfig; }
	void ToggleVisibility() { SetWindowState(IsVisible() ? EUIWindowState::Hidden : EUIWindowState::Visible); }
	void AddWidget(UWidget* Widget) 
	{ 
		if (Widget != nullptr) 
		{
			Widgets.push_back(Widget); 
		}
	}

	void OnMainWindowResized();
	void ClampWindow();

	void RenderWidget() const;
	void RenderWindow();
	void Update() const;

	// 서브클래스에서 커스텀 렌더링을 위한 가상 함수
	virtual void RenderContent() {};

protected:
	void ApplyDockingSettings() const;
	void UpdateWindowInfo();

private:
	static int IssuedWindowID;

	FUIWindowConfig Config;
	EUIWindowState CurrentState;
	uint32 WindowID;

	bool bIsFocused = false;
	float LastFocusTime = 0.0f;

	// ImGui 내부 상태
	bool bIsWindowOpen = false;
	ImVec2 LastWindowSize;
	ImVec2 LastWindowPosition;

	TArray<UWidget*> Widgets;

	ImVec2 PositionRatio = { 0.5f, 0.5f };
	ImVec2 SizeRatio = { 0.5f, 0.5f };
	bool bIsResized = false;
	bool bIsRatioInitialized = false;
};
