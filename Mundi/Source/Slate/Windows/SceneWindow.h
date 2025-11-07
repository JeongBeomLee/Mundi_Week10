#pragma once
#include "UIWindow.h"

class ULevelManager;

/**
 * @brief Scene Manager 역할을 제공할 Window
 * Scene hierarchy and management UI를 제공한다
 */
class USceneWindow
	: public UUIWindow
{
public:
	DECLARE_CLASS(USceneWindow, UUIWindow)

    USceneWindow();
	void Initialize() override;
};
