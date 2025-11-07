#pragma once
#include "Object.h"
#include "UEContainer.h"

class UConsoleWindow;
class UControlPanelWindow;
class UPropertyWindow;
class UExperimentalFeatureWindow;
class USceneWindow;
class UCameraPanelWindow;
class USceneManagerWidget;
class UDetailsPanelWidget;

/**
 * @brief UI 윈도우 도킹 방향
 */
enum class EUIDockDirection : uint8
{
	None, // 도킹 없음
	Left, // 왼쪽 도킹
	Right, // 오른쪽 도킹
	Top, // 상단 도킹
	Bottom, // 하단 도킹
	Center, // 중앙 도킹
};

/**
* @brief UI 윈도우들을 쉽게 생성하기 위한 팩토리 클래스
 */
class UUIWindowFactory : public UObject
{
public:
	DECLARE_CLASS(UUIWindowFactory, UObject)
	
	static void CreateDefaultUILayout();
	static UConsoleWindow* CreateConsoleWindow(EUIDockDirection InDockDirection = EUIDockDirection::Bottom);
	static UControlPanelWindow* CreateControlPanelWindow(EUIDockDirection InDockDirection = EUIDockDirection::Left);
	static UPropertyWindow* CreatePropertyWindow(EUIDockDirection InDockDirection = EUIDockDirection::Top);
	static USceneWindow* CreateSceneWindow(EUIDockDirection InDockDirection = EUIDockDirection::Center);
	static UExperimentalFeatureWindow*
		CreateExperimentalFeatureWindow(EUIDockDirection InDockDirection = EUIDockDirection::Right);
	
	// Object Management Widgets (Unreal Engine style)
	//static USceneManagerWidget* CreateSceneManagerWidget(EUIDockDirection InDockDirection = EUIDockDirection::Left);
	//static UDetailsPanelWidget* CreateDetailsPanelWidget(EUIDockDirection InDockDirection = EUIDockDirection::Right);
	//static void CreateObjectManagementLayout(); // Creates both outliner and details panel
};
