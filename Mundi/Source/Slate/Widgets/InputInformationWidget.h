#pragma once
#include "Widget.h"
#include "Vector.h"
#include "Enums.h"
#include "InputManager.h"

class UInputInformationWidget :
	public UWidget
{
public:
	DECLARE_CLASS(UInputInformationWidget, UWidget)

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	static void RenderKeyList(const TArray<EKeyInput>& InPressedKeys);
	void RenderMouseInfo();
	void RenderCurrentlyPressedKeys() const;

	// Special Member Function
	UInputInformationWidget();
	~UInputInformationWidget() override;

private:
	// 마우스 관련
	FVector2D LastMousePosition;
	FVector2D MouseDelta;
	
	// 마우스 위치 히스토리
	TArray<FVector2D> MousePositionHistory;
	static constexpr int32 MaxHistorySize = 50;

};
