#include "pch.h"
#include "ExperimentalFeatureWindow.h"
#include "Widgets/InputInformationWidget.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(UExperimentalFeatureWindow)

/**
 * @brief Window Constructor
 */
UExperimentalFeatureWindow::UExperimentalFeatureWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Experimental Feature";
	Config.DefaultSize = ImVec2(350, 10);
	Config.DefaultPosition = ImVec2(380, 10);
	Config.MinSize = ImVec2(350, 10);
	Config.DockDirection = EUIDockDirection::Right;
	Config.Priority = 5;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	Config.UpdateWindowFlags();
	SetConfig(Config);

	AddWidget(NewObject<UInputInformationWidget>());
}

/**
 * @brief Initializer
 */
void UExperimentalFeatureWindow::Initialize()
{
	UE_LOG("ExperimentalFeatureWindow: Window가 성공적으로 생성되었습니다");
}
