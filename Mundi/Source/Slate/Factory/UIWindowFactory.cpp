#include "pch.h"
#include "UIWindowFactory.h"
#include "UIManager.h"
#include "Windows/ConsoleWindow.h"  // 새로운 ConsoleWindow 사용
#include "Windows/ControlPanelWindow.h"
#include "Windows/PropertyWindow.h"
#include "Windows/ExperimentalFeatureWindow.h"
#include "Windows/SceneWindow.h"
#include "GlobalConsole.h"

IMPLEMENT_CLASS(UUIWindowFactory)

UConsoleWindow* UUIWindowFactory::CreateConsoleWindow(EUIDockDirection InDockDirection)
{
	auto* Window = new UConsoleWindow();
	Window->GetMutableConfig().DockDirection = InDockDirection;
	return Window;
}

UControlPanelWindow* UUIWindowFactory::CreateControlPanelWindow(EUIDockDirection InDockDirection)
{
	auto* Window = new UControlPanelWindow();
	Window->GetMutableConfig().DockDirection = InDockDirection;
	return Window;
}

UPropertyWindow* UUIWindowFactory::CreatePropertyWindow(EUIDockDirection InDockDirection)
{
	auto* Window = new UPropertyWindow();
	Window->GetMutableConfig().DockDirection = InDockDirection;
	return Window;
}

USceneWindow* UUIWindowFactory::CreateSceneWindow(EUIDockDirection InDockDirection)
{
	auto* Window = new USceneWindow();
	Window->GetMutableConfig().DockDirection = InDockDirection;
	return Window;
}

UExperimentalFeatureWindow* UUIWindowFactory::CreateExperimentalFeatureWindow(EUIDockDirection InDockDirection)
{
	auto* Window = new UExperimentalFeatureWindow();
	Window->GetMutableConfig().DockDirection = InDockDirection;
	return Window;
}

void UUIWindowFactory::CreateDefaultUILayout()
{
	auto& UIManager = UUIManager::GetInstance();

	// 기본 레이아웃 생성
	//UConsoleWindow* ConsoleWindow = CreateConsoleWindow(EUIDockDirection::Bottom);
	//UIManager.RegisterUIWindow(ConsoleWindow);
	
	// GlobalConsole에 ConsoleWidget 등록 (반드시 다른 UI 생성 전에)
	//UGlobalConsole::SetConsoleWidget(ConsoleWindow->GetConsoleWidget());
	
	// 콘솔 설정 후 테스트 메시지
	UE_LOG("GlobalConsole 설정 완료 - ConsoleWidget 연결됨");
	UE_LOG("Console 시스템이 정상적으로 동작하고 있습니다");
	
	/* SWindow를 통해 UI를 표시하기 위해 주석 처리 */
	// UIManager.RegisterUIWindow(CreateControlPanelWindow(EUIDockDirection::Left));
	// UIManager.RegisterUIWindow(CreatePropertyWindow(EUIDockDirection::Top));
	// UIManager.RegisterUIWindow(CreateSceneWindow(EUIDockDirection::Center));
	// UIManager.RegisterUIWindow(CreateExperimentalFeatureWindow(EUIDockDirection::Right));
	
	// 모든 UI 생성 완료 메시지
	UE_LOG("기본적인 UI 생성이 성공적으로 완료되었습니다");
	UE_LOG("Console, ControlPanel, Outliner, ExperimentalFeature 윈도우 생성됨");
}