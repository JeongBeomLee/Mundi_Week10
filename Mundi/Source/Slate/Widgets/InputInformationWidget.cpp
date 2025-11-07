#include "pch.h"
#include "InputInformationWidget.h"
#include "ImGui/imgui.h"
#include "InputManager.h"
#include <windows.h>
#include <sstream>
#include <iomanip>

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

using std::max;
using std::min;

IMPLEMENT_CLASS(UInputInformationWidget)

UInputInformationWidget::UInputInformationWidget()
	: UWidget("Input Information Widget")
	, LastMousePosition(0.0f, 0.0f)
	, MouseDelta(0.0f, 0.0f)
{
	// 메모리 누수 방지를 위해 미리 예약
	MousePositionHistory.Reserve(MaxHistorySize + 5);
}

UInputInformationWidget::~UInputInformationWidget()
{
	// 명시적으로 히스토리 정리 (메모리 누수 방지)
	MousePositionHistory.Empty();
	MousePositionHistory.Shrink();
}

void UInputInformationWidget::Initialize()
{
	LastMousePosition = FVector2D(0.0f, 0.0f);
	MouseDelta = FVector2D(0.0f, 0.0f);
	UE_LOG("InputInformationWidget: Successfully Initialized");
}

void UInputInformationWidget::Update()
{
}

void UInputInformationWidget::RenderWidget()
{
	ImGui::Text("Input Information");
	ImGui::Separator();

	// 탭 바 생성
	if (ImGui::BeginTabBar("InputInfoTabs", ImGuiTabBarFlags_None))
	{
		// 마우스 정보 탭
		if (ImGui::BeginTabItem("Mouse Info"))
		{
			RenderMouseInfo();
			ImGui::EndTabItem();
		}

		// 현재 눌린 키 탭
		if (ImGui::BeginTabItem("Current Keys"))
		{
			RenderCurrentlyPressedKeys();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void UInputInformationWidget::RenderKeyList(const TArray<EKeyInput>& InPressedKeys)
{
	ImGui::Text("Currently Pressed Keys:");
	for (const auto& Key : InPressedKeys)
	{
		// EKeyInput enum을 문자열로 변환하여 표시
		FString KeyName = "Unknown";
		switch (Key)
		{
		case EKeyInput::A: KeyName = "A"; break;
		case EKeyInput::B: KeyName = "B"; break;
		case EKeyInput::C: KeyName = "C"; break;
		// ... 다른 키들도 필요시 추가
		case EKeyInput::Space: KeyName = "Space"; break;
		case EKeyInput::Enter: KeyName = "Enter"; break;
		case EKeyInput::LeftMouse: KeyName = "Left Mouse"; break;
		case EKeyInput::RightMouse: KeyName = "Right Mouse"; break;
		default: KeyName = "Unknown"; break;
		}
		ImGui::Text("- %s", KeyName.c_str());
	}
}

void UInputInformationWidget::RenderMouseInfo()
{
	ImGui::Text("Mouse Information:");
	ImGui::Separator();
	
	UInputManager& InputManager = UInputManager::GetInstance();
	FVector2D CurrentMousePos = InputManager.GetMousePosition();
	FVector2D ScreenSize = InputManager.GetScreenSize();
	
	ImGui::Text("Mouse Position: (%.0f, %.0f)", CurrentMousePos.X, CurrentMousePos.Y);
	ImGui::Text("Mouse Delta: (%.2f, %.2f)", MouseDelta.X, MouseDelta.Y);
	ImGui::Text("Mouse Speed: %.2f", MouseDelta.Length());
	ImGui::Text("Screen Size: (%.0f, %.0f)", ScreenSize.X, ScreenSize.Y);

	// 마우스 버튼 상태 (직접 Windows API 사용)
	bool MouseLeft = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	bool MouseRight = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	bool MouseMiddle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

	ImGui::Spacing();
	ImGui::Text("Mouse Buttons:");
	ImVec4 PressedColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
	ImVec4 ReleasedColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
	
	ImGui::TextColored(MouseLeft ? PressedColor : ReleasedColor, "  Left");
	ImGui::SameLine();
	ImGui::TextColored(MouseRight ? PressedColor : ReleasedColor, "  Right");
	ImGui::SameLine();
	ImGui::TextColored(MouseMiddle ? PressedColor : ReleasedColor, "  Middle");

	ImGui::Spacing();
	
	// 마우스 위치를 작은 그래프로 표시
	ImGui::Text("Mouse Position Visualizer:");
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 CanvasPosition = ImGui::GetCursorScreenPos();
	ImVec2 CanvasSize = ImVec2(200, 100);

	// Background
	DrawList->AddRectFilled(CanvasPosition,
	                        ImVec2(CanvasPosition.x + CanvasSize.x, CanvasPosition.y + CanvasSize.y),
	                        IM_COL32(50, 50, 50, 255));

	// 테두리 그리기
	DrawList->AddRect(CanvasPosition,
	                  ImVec2(CanvasPosition.x + CanvasSize.x, CanvasPosition.y + CanvasSize.y),
	                  IM_COL32(100, 100, 100, 255));

	// 마우스 위치를 NDC로 변환 (0~1 범위)
	float NormalizedX = (ScreenSize.X > 0) ? CurrentMousePos.X / ScreenSize.X : 0.0f;
	float NormalizedY = (ScreenSize.Y > 0) ? CurrentMousePos.Y / ScreenSize.Y : 0.0f;
	
	// 범위 체크
	NormalizedX = max(0.0f, min(1.0f, NormalizedX));
	NormalizedY = max(0.0f, min(1.0f, NormalizedY));

	// 마우스 위치에 Dot 표시
	ImVec2 MousePosNormalized = ImVec2(NormalizedX * CanvasSize.x, NormalizedY * CanvasSize.y);

	DrawList->AddCircleFilled(ImVec2(CanvasPosition.x + MousePosNormalized.x, CanvasPosition.y + MousePosNormalized.y),
	                          3.0f, IM_COL32(255, 0, 0, 255));

	// 중앙에 십자 표시
	float CenterX = CanvasPosition.x + CanvasSize.x * 0.5f;
	float CenterY = CanvasPosition.y + CanvasSize.y * 0.5f;
	DrawList->AddLine(ImVec2(CenterX - 5, CenterY), ImVec2(CenterX + 5, CenterY), IM_COL32(128, 128, 128, 255));
	DrawList->AddLine(ImVec2(CenterX, CenterY - 5), ImVec2(CenterX, CenterY + 5), IM_COL32(128, 128, 128, 255));

	ImGui::Dummy(CanvasSize);
	
	// 추가 정보
	ImGui::Text("Normalized Position: (%.3f, %.3f)", NormalizedX, NormalizedY);

	// 현재 마우스 위치를 히스토리에 추가
	MousePositionHistory.Add(FVector2D(NormalizedX, NormalizedY));
	if (MousePositionHistory.Num() > MaxHistorySize)
	{
		MousePositionHistory.RemoveAt(0);
	}

	// 히스토리 그리기 (선 연결)
	for (int32 i = 1; i < MousePositionHistory.Num(); ++i)
	{
		float Alpha = static_cast<float>(i) / MousePositionHistory.Num();
		ImU32 TrailColor = IM_COL32(255, 255, 0, static_cast<int>(Alpha * 100));

		ImVec2 Pos1 = ImVec2(
			CanvasPosition.x + MousePositionHistory[i - 1].X * CanvasSize.x,
			CanvasPosition.y + MousePositionHistory[i - 1].Y * CanvasSize.y
		);
		ImVec2 Pos2 = ImVec2(
			CanvasPosition.x + MousePositionHistory[i].X * CanvasSize.x,
			CanvasPosition.y + MousePositionHistory[i].Y * CanvasSize.y
		);

		DrawList->AddLine(Pos1, Pos2, TrailColor, 1.0f);
	}
}

void UInputInformationWidget::RenderCurrentlyPressedKeys() const
{
	ImGui::Text("Currently Pressed Keys:");
	ImGui::Separator();
	
	FString CurrentlyPressed = "";
	TArray<FString> PressedKeysList;

	// 특수 키들 체크
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) PressedKeysList.Add("Space");
	if (GetAsyncKeyState(VK_RETURN) & 0x8000) PressedKeysList.Add("Enter");
	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) PressedKeysList.Add("Escape");
	if (GetAsyncKeyState(VK_TAB) & 0x8000) PressedKeysList.Add("Tab");
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000) PressedKeysList.Add("Shift");
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) PressedKeysList.Add("Ctrl");
	if (GetAsyncKeyState(VK_MENU) & 0x8000) PressedKeysList.Add("Alt");

	// 방향키
	if (GetAsyncKeyState(VK_UP) & 0x8000) PressedKeysList.Add("Up");
	if (GetAsyncKeyState(VK_DOWN) & 0x8000) PressedKeysList.Add("Down");
	if (GetAsyncKeyState(VK_LEFT) & 0x8000) PressedKeysList.Add("Left");
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000) PressedKeysList.Add("Right");

	// 문자 키들
	for (int key = 'A'; key <= 'Z'; ++key)
	{
		if (GetAsyncKeyState(key) & 0x8000)
		{
			PressedKeysList.Add(FString(1, static_cast<char>(key)));
		}
	}

	// 숫자 키들
	for (int key = '0'; key <= '9'; ++key)
	{
		if (GetAsyncKeyState(key) & 0x8000)
		{
			PressedKeysList.Add(FString(1, static_cast<char>(key)));
		}
	}

	// F 키들
	for (int fkey = VK_F1; fkey <= VK_F12; ++fkey)
	{
		if (GetAsyncKeyState(fkey) & 0x8000)
		{
			FString keyName = "F" + std::to_string(fkey - VK_F1 + 1);
			PressedKeysList.Add(keyName);
		}
	}

	// 마우스 버튼
	if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) PressedKeysList.Add("Left Mouse");
	if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) PressedKeysList.Add("Right Mouse");
	if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) PressedKeysList.Add("Middle Mouse");

	// 결과 표시
	if (PressedKeysList.IsEmpty())
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No keys currently pressed");
	}
	else
	{
		ImGui::Text("Total: %d key(s)", PressedKeysList.Num());
		ImGui::Spacing();
		
		// 그리드 스타일로 표시
		int32 ItemsPerRow = 6;
		for (int32 i = 0; i < PressedKeysList.Num(); ++i)
		{
			if (i % ItemsPerRow != 0) ImGui::SameLine();
			
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[%s]", PressedKeysList[i].c_str());
		}
	}
}