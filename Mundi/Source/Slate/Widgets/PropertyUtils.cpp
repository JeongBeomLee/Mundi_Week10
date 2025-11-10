#include "pch.h"
#include "PropertyUtils.h"
#include "ImGui/imgui.h"

bool UPropertyUtils::RenderVector3WithColorBars(const char* label, FVector* value, float speed, float labelWidth)
{
	// 축 색상 정의 (PropertyRenderer와 동일)
	ImVec4 ColorX(0.85f, 0.2f, 0.2f, 1.0f);   // 빨간색 (X)
	ImVec4 ColorY(0.3f, 0.8f, 0.3f, 1.0f);    // 초록색 (Y)
	ImVec4 ColorZ(0.2f, 0.5f, 1.0f, 1.0f);    // 파란색 (Z)

	const float InputWidth = 70.0f;
	const float FrameHeight = ImGui::GetFrameHeight();
	const float ColorBoxWidth = FrameHeight * 0.25f;
	const float ColorBoxHeight = FrameHeight;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	bool bChanged = false;

	// 프로퍼티별 고유 ID 스코프 시작 (PropertyRenderer 패턴)
	ImGui::PushID(label);

	// 라벨
	ImGui::Text("%s", label);
	ImGui::SameLine(labelWidth);

	// X축 (빨간색)
	ImGui::PushID("X");
	ImVec2 CursorPos = ImGui::GetCursorScreenPos();
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + ColorBoxWidth, CursorPos.y + ColorBoxHeight),
		ImGui::ColorConvertFloat4ToU32(ColorX)
	);
	ImGui::Dummy(ImVec2(ColorBoxWidth, ColorBoxHeight));
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushItemWidth(InputWidth);
	if (ImGui::DragFloat("##X", &value->X, speed))
	{
		bChanged = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
	ImGui::SameLine();

	// Y축 (초록색)
	ImGui::PushID("Y");
	CursorPos = ImGui::GetCursorScreenPos();
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + ColorBoxWidth, CursorPos.y + ColorBoxHeight),
		ImGui::ColorConvertFloat4ToU32(ColorY)
	);
	ImGui::Dummy(ImVec2(ColorBoxWidth, ColorBoxHeight));
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushItemWidth(InputWidth);
	if (ImGui::DragFloat("##Y", &value->Y, speed))
	{
		bChanged = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
	ImGui::SameLine();

	// Z축 (파란색)
	ImGui::PushID("Z");
	CursorPos = ImGui::GetCursorScreenPos();
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + ColorBoxWidth, CursorPos.y + ColorBoxHeight),
		ImGui::ColorConvertFloat4ToU32(ColorZ)
	);
	ImGui::Dummy(ImVec2(ColorBoxWidth, ColorBoxHeight));
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushItemWidth(InputWidth);
	if (ImGui::DragFloat("##Z", &value->Z, speed))
	{
		bChanged = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopID();

	// 프로퍼티 ID 스코프 종료
	ImGui::PopID();

	return bChanged;
}
