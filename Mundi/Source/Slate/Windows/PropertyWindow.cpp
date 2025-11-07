#include "pch.h"
#include "PropertyWindow.h"
#include "Widgets/TargetActorTransformWidget.h"
#include "Widgets/WorldDetailsWidget.h"

IMPLEMENT_CLASS(UPropertyWindow)

UPropertyWindow::UPropertyWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Property Window";
	Config.DefaultSize = ImVec2(400, 620);
	Config.DefaultPosition = ImVec2(10, 10);
	Config.MinSize = ImVec2(400, 200);
	Config.DockDirection = EUIDockDirection::Top;
	Config.Priority = 15;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	auto TargetActorTransformWidget = NewObject<UTargetActorTransformWidget>();
	TargetActorTransformWidget->Initialize();
	AddWidget(TargetActorTransformWidget);

	// WorldDetailsWidget 추가 (GameMode 설정용)
	auto WorldDetailsWidget = NewObject<UWorldDetailsWidget>();
	WorldDetailsWidget->Initialize();
	AddWidget(WorldDetailsWidget);

	Config.UpdateWindowFlags();
	SetConfig(Config);
}

void UPropertyWindow::Initialize()
{
	UE_LOG("UPropertyWindow: Initialized");
}

void UPropertyWindow::SetWorld(UWorld* InWorld)
{
	// WorldDetailsWidget에 World 참조 전달
	for (UWidget* Widget : GetWidgets())
	{
		if (UWorldDetailsWidget* WorldWidget = Cast<UWorldDetailsWidget>(Widget))
		{
			WorldWidget->SetWorld(InWorld);
			break;
		}
	}
}
