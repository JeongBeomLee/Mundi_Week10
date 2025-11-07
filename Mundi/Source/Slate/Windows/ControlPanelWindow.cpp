#include "pch.h"
#include "ControlPanelWindow.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(UControlPanelWindow)

/**
 * @brief Control Panel Constructor
 * 적절한 사이즈의 윈도우 제공
 */
UControlPanelWindow::UControlPanelWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Control Panel";
	Config.DefaultSize = ImVec2(400, 620);
	Config.DefaultPosition = ImVec2(10, 10);
	Config.MinSize = ImVec2(400, 200);
	Config.DockDirection = EUIDockDirection::Left;
	Config.Priority = 15;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	Config.UpdateWindowFlags();
	SetConfig(Config);
}

/**
 * @brief 초기화 함수
 */
void UControlPanelWindow::Initialize()
{
	UE_LOG("ControlPanelWindow: Initialized");
}
