#pragma once
#include "UIWindow.h"

/**
 * @brief 현재 단계에서 필요하지 않은 기능들을 모아둔 Window
 * Key Input, Actor Termination Widget을 포함한다
 */
class UExperimentalFeatureWindow : public UUIWindow
{
public:
	DECLARE_CLASS(UExperimentalFeatureWindow, UUIWindow)

	UExperimentalFeatureWindow();
	void Initialize() override;
};
