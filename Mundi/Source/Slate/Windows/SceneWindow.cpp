#include "pch.h"
#include "SceneWindow.h"
#include "Widgets/SceneManagerWidget.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(USceneWindow)

USceneWindow::USceneWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "씬 매니저";
	Config.DefaultSize = ImVec2(350, 500);
	Config.DefaultPosition = ImVec2(1225, 10);
	Config.MinSize = ImVec2(300, 400);
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;
	Config.DockDirection = EUIDockDirection::Center;

	Config.UpdateWindowFlags();
	SetConfig(Config);

	// Add Scene Manager Widget (main scene hierarchy)
	USceneManagerWidget* SceneManagerWidget = NewObject<USceneManagerWidget>();
	if (SceneManagerWidget)
	{
		AddWidget(SceneManagerWidget);
		UE_LOG("SceneWindow: SceneManagerWidget created successfully");
	}
	else
	{
		UE_LOG("SceneWindow: Failed to create SceneManagerWidget");
	}
	
}

void USceneWindow::Initialize()
{
	UE_LOG("SceneWindow: Successfully Initialized");
}
