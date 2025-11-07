#pragma once
#include "UIWindow.h"

// Forward Declarations
class UWorld;

class UPropertyWindow : public UUIWindow
{
public:
	DECLARE_CLASS(UPropertyWindow, UUIWindow)

	UPropertyWindow();
	virtual void Initialize() override;

	/** World 참조를 WorldDetailsWidget에 전달 */
	void SetWorld(UWorld* InWorld);
};