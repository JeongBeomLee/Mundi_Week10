#include "pch.h"
#include "CameraBlendEditorWindow.h"
#include "UIManager.h"
#include "JsonSerializer.h"
#include "PlayerCameraManager.h"
#include "CameraBlendPresetLibrary.h"
#include <fstream>
#include <commdlg.h>
#include <filesystem>

IMPLEMENT_CLASS(UCameraBlendEditorWindow)

UCameraBlendEditorWindow::UCameraBlendEditorWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Camera Blend Editor";
	Config.DefaultSize = ImVec2(800, 700);
	Config.DefaultPosition = ImVec2(100, 100);
	Config.DockDirection = EUIDockDirection::Right;
	Config.Priority = 5;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;
	Config.InitialState = EUIWindowState::Hidden;  // 초기에는 숨김 상태

	Config.UpdateWindowFlags();
	SetConfig(Config);

	// 초기 상태를 Hidden으로 설정
	SetWindowState(EUIWindowState::Hidden);

	// 기본값으로 Cubic 프리셋 설정
	ApplyPreset(EViewTargetBlendFunction::VTBlend_Cubic);
	EditingParams.BlendTime = 2.0f;
	PreviewDuration = 2.0f;
}

void UCameraBlendEditorWindow::Initialize()
{
	UE_LOG("CameraBlendEditorWindow: 윈도우가 생성되었습니다");
}

bool UCameraBlendEditorWindow::OnWindowClose()
{
	// X버튼 클릭 시 윈도우를 완전히 닫지 않고 숨김 처리
	SetWindowState(EUIWindowState::Hidden);
	return false; // false를 반환하여 ImGui의 기본 닫기 동작 방지
}

void UCameraBlendEditorWindow::RenderContent()
{
	ImGui::BeginChild("CameraBlendEditor", ImVec2(0, 0), false);

	// 프리셋 선택
	RenderPresetSelector();

	ImGui::Separator();

	// 채널 선택
	RenderChannelSelector();

	ImGui::Separator();

	// 커브 에디터
	RenderCurveEditor();

	ImGui::Separator();

	// 타임라인 프리뷰
	RenderTimelinePreview();

	ImGui::EndChild();
}

void UCameraBlendEditorWindow::RenderPresetSelector()
{
	ImGui::Text("Blend Preset:");
	ImGui::SameLine();

	// 라이브러리에서 프리셋 이름 목록 가져오기
	UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
	if (Library)
	{
		TArray<FString> PresetNames = Library->GetAllPresetNames();

		// FString -> const char* 변환을 위한 벡터
		std::vector<const char*> PresetNamesCStr;
		for (const FString& Name : PresetNames)
		{
			PresetNamesCStr.push_back(Name.c_str());
		}

		// Combo 박스로 프리셋 선택
		if (!PresetNamesCStr.empty())
		{
			// CurrentPresetIndex 범위 체크
			if (CurrentPresetIndex < 0 || CurrentPresetIndex >= static_cast<int>(PresetNamesCStr.size()))
			{
				CurrentPresetIndex = 0;
			}

			if (ImGui::Combo("##PresetCombo", &CurrentPresetIndex, PresetNamesCStr.data(), static_cast<int>(PresetNamesCStr.size())))
			{
				// 선택된 프리셋 이름으로 라이브러리에서 검색
				FString SelectedPresetName = PresetNames[CurrentPresetIndex];
				const FCameraBlendPreset* Preset = Library->GetPreset(SelectedPresetName);

				if (Preset)
				{
					EditingParams = Preset->BlendParams;
					PreviewDuration = EditingParams.BlendTime;
					UE_LOG("CameraBlendEditor: 프리셋 적용 - %s", SelectedPresetName.c_str());
				}
			}
		}
		else
		{
			ImGui::Text("No presets available");
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		ResetToDefault();
	}

	ImGui::SameLine();
	if (ImGui::Button("Save Preset"))
	{
		SavePresetToFile();
	}

	ImGui::SameLine();
	if (ImGui::Button("Load Preset"))
	{
		LoadPresetFromFile();
	}

	// 블렌드 시간 설정
	ImGui::Text("Blend Time:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(150);
	if (ImGui::DragFloat("##BlendTime", &EditingParams.BlendTime, 0.1f, 0.1f, 10.0f, "%.1f sec"))
	{
		PreviewDuration = EditingParams.BlendTime;
	}

	// Register to Library 버튼 (오른쪽 정렬)
	ImGui::SameLine();
	float availWidth = ImGui::GetContentRegionAvail().x;
	float buttonWidth = 150.0f;
	if (availWidth > buttonWidth)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - buttonWidth);
	}

	static char presetNameBuffer[128] = "MyCustomPreset";

	if (ImGui::Button("Register to Library", ImVec2(buttonWidth, 0)))
	{
		ImGui::OpenPopup("Enter Preset Name");
	}

	// 프리셋 이름 입력 팝업 모달
	if (ImGui::BeginPopupModal("Enter Preset Name", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter a name for this preset:");
		ImGui::InputText("##PresetName", presetNameBuffer, sizeof(presetNameBuffer));

		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			FString PresetName(presetNameBuffer);
			FCameraBlendPreset Preset(PresetName, EditingParams);

			UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
			if (Library)
			{
				Library->RegisterPreset(Preset);
				UE_LOG("CameraBlendEditor: 프리셋을 라이브러리에 등록했습니다 - %s", PresetName.c_str());
			}

			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// BlendExp (EaseIn/Out/InOut에만 사용)
	if (CurrentPresetIndex >= 2 && CurrentPresetIndex <= 4)
	{
		ImGui::Text("Blend Exp:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150);
		ImGui::DragFloat("##BlendExp", &EditingParams.BlendExp, 0.1f, 0.5f, 5.0f, "%.1f");
	}

	// SpringArm 블렌딩 설정
	ImGui::Separator();
	ImGui::Checkbox("Blend SpringArm Length", &EditingParams.bBlendSpringArmLength);
	if (EditingParams.bBlendSpringArmLength)
	{
		ImGui::Text("Target SpringArm Length:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150);
		ImGui::DragFloat("##TargetSpringArmLength", &EditingParams.TargetSpringArmLength, 1.0f, 10.0f, 1000.0f, "%.1f");
	}
}

void UCameraBlendEditorWindow::RenderChannelSelector()
{
	ImGui::Text("Channel:");
	ImGui::SameLine();

	for (int i = 0; i < 4; ++i)
	{
		if (i > 0)
			ImGui::SameLine();

		// 채널별 색상으로 텍스트 표시
		ImGui::PushStyleColor(ImGuiCol_Text, ChannelColors[i]);
		if (ImGui::RadioButton(ChannelNames[i], &CurrentChannelIndex, i))
		{
			// 채널 변경
		}
		ImGui::PopStyleColor();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Show All", &bShowAllChannels);
}

void UCameraBlendEditorWindow::RenderCurveEditor()
{
	ImGui::Text("Bezier Curve Editor:");

	// 커브 에디터 영역 시작 - 원점 저장
	CurveEditorOrigin = ImGui::GetCursorScreenPos();
	ImVec2 CursorPos = CurveEditorOrigin;  // 기존 코드 호환성을 위해 유지
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 배경 박스
	ImVec2 BoxMin = CursorPos;
	ImVec2 BoxMax = ImVec2(CursorPos.x + CurveEditorSize.x, CursorPos.y + CurveEditorSize.y);
	DrawList->AddRectFilled(BoxMin, BoxMax, IM_COL32(30, 30, 30, 255));
	DrawList->AddRect(BoxMin, BoxMax, IM_COL32(100, 100, 100, 255), 0.0f, 0, 2.0f);

	// 그리드 선 그리기 (10등분)
	for (int i = 0; i <= 10; ++i)
	{
		float T = i / 10.0f;

		// 세로 그리드
		float X = CursorPos.x + CurveEditorPadding.x + T * (CurveEditorSize.x - 2 * CurveEditorPadding.x);
		DrawList->AddLine(
			ImVec2(X, BoxMin.y),
			ImVec2(X, BoxMax.y),
			IM_COL32(60, 60, 60, 255),
			1.0f
		);

		// 가로 그리드
		float Y = CursorPos.y + CurveEditorPadding.y + T * (CurveEditorSize.y - 2 * CurveEditorPadding.y);
		DrawList->AddLine(
			ImVec2(BoxMin.x, Y),
			ImVec2(BoxMax.x, Y),
			IM_COL32(60, 60, 60, 255),
			1.0f
		);
	}

	// 축 레이블
	DrawList->AddText(ImVec2(CursorPos.x + 5, CursorPos.y + CurveEditorSize.y - 20), IM_COL32(200, 200, 200, 255), "Time (0-1)");

	// Y축 레이블 (회전)
	ImVec2 YLabelPos = ImVec2(CursorPos.x + 5, CursorPos.y + 10);
	DrawList->AddText(YLabelPos, IM_COL32(200, 200, 200, 255), "Value (0-1)");

	// 모든 채널 표시 또는 선택된 채널만 표시
	if (bShowAllChannels)
	{
		// 모든 채널 표시
		RenderBezierCurve(EditingParams.LocationCurve, "Location", ChannelColors[0]);
		RenderBezierCurve(EditingParams.RotationCurve, "Rotation", ChannelColors[1]);
		RenderBezierCurve(EditingParams.FOVCurve, "FOV", ChannelColors[2]);
		RenderBezierCurve(EditingParams.SpringArmCurve, "SpringArm", ChannelColors[3]);
	}
	else
	{
		// 선택된 채널만 표시 및 편집
		FBezierControlPoints* CurrentCurve = nullptr;
		const char* CurrentLabel = nullptr;
		ImU32 CurrentColor = 0;

		switch (CurrentChannelIndex)
		{
		case 0:
			CurrentCurve = &EditingParams.LocationCurve;
			CurrentLabel = "Location";
			CurrentColor = ChannelColors[0];
			break;
		case 1:
			CurrentCurve = &EditingParams.RotationCurve;
			CurrentLabel = "Rotation";
			CurrentColor = ChannelColors[1];
			break;
		case 2:
			CurrentCurve = &EditingParams.FOVCurve;
			CurrentLabel = "FOV";
			CurrentColor = ChannelColors[2];
			break;
		case 3:
			CurrentCurve = &EditingParams.SpringArmCurve;
			CurrentLabel = "SpringArm";
			CurrentColor = ChannelColors[3];
			break;
		}

		if (CurrentCurve)
		{
			RenderBezierCurve(*CurrentCurve, CurrentLabel, CurrentColor);

			// 제어점 편집 (선택된 채널만)
			bool bModified = false;
			ImVec2 P0 = CurveToScreenPos(0.0f, CurrentCurve->P0);
			ImVec2 P1 = CurveToScreenPos(0.33f, CurrentCurve->P1);
			ImVec2 P2 = CurveToScreenPos(0.66f, CurrentCurve->P2);
			ImVec2 P3 = CurveToScreenPos(1.0f, CurrentCurve->P3);

			RenderControlPoint(0, P0, CurrentCurve->P0, bModified);
			RenderControlPoint(1, P1, CurrentCurve->P1, bModified);
			RenderControlPoint(2, P2, CurrentCurve->P2, bModified);
			RenderControlPoint(3, P3, CurrentCurve->P3, bModified);

			// 제어점이 수정되면 프리셋을 Custom으로 변경
			if (bModified && CurrentPresetIndex != 5)
			{
				CurrentPresetIndex = 5; // Bezier Custom
				EditingParams.BlendFunction = EViewTargetBlendFunction::VTBlend_BezierCustom;
			}
		}
	}

	// 프리뷰 시간 표시 (재생 중일 때)
	if (bPreviewing && PreviewDuration > 0.0f)
	{
		float TimeRatio = PreviewTime / PreviewDuration;
		TimeRatio = FMath::Clamp(TimeRatio, 0.0f, 1.0f);

		float X = CursorPos.x + CurveEditorPadding.x + TimeRatio * (CurveEditorSize.x - 2 * CurveEditorPadding.x);
		DrawList->AddLine(
			ImVec2(X, BoxMin.y),
			ImVec2(X, BoxMax.y),
			IM_COL32(255, 255, 255, 200),
			2.0f
		);
	}

	// 인비저블 버튼으로 영역 차지
	ImGui::SetCursorScreenPos(CursorPos);
	ImGui::InvisibleButton("CurveEditorCanvas", CurveEditorSize);
}

void UCameraBlendEditorWindow::RenderBezierCurve(const FBezierControlPoints& Curve, const char* Label, ImU32 Color)
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 제어점 4개를 화면 좌표로 변환
	ImVec2 P0 = CurveToScreenPos(0.0f, Curve.P0);
	ImVec2 P1 = CurveToScreenPos(0.33f, Curve.P1);
	ImVec2 P2 = CurveToScreenPos(0.66f, Curve.P2);
	ImVec2 P3 = CurveToScreenPos(1.0f, Curve.P3);

	// 베지어 커브 그리기 (ImGui API 사용)
	DrawList->AddBezierCubic(P0, P1, P2, P3, Color, 3.0f);

	// 제어점 연결선 (점선)
	if (!bShowAllChannels)
	{
		DrawList->AddLine(P0, P1, IM_COL32(150, 150, 150, 150), 1.0f);
		DrawList->AddLine(P2, P3, IM_COL32(150, 150, 150, 150), 1.0f);
	}
}

void UCameraBlendEditorWindow::RenderControlPoint(int Index, ImVec2 ScreenPos, float& OutValue, bool& bModified)
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImGuiIO& IO = ImGui::GetIO();

	const float Radius = 8.0f;
	const float HoverRadius = 12.0f;

	// 마우스 호버 확인
	ImVec2 MousePos = IO.MousePos;
	float Distance = sqrtf(
		(MousePos.x - ScreenPos.x) * (MousePos.x - ScreenPos.x) +
		(MousePos.y - ScreenPos.y) * (MousePos.y - ScreenPos.y)
	);

	bool bHovered = Distance < HoverRadius;
	bool bDragging = (DraggingControlPointIndex == Index && DraggingChannelIndex == CurrentChannelIndex);

	// 드래그 시작
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		DraggingControlPointIndex = Index;
		DraggingChannelIndex = CurrentChannelIndex;
		bDragging = true;
	}

	// 드래그 중
	if (bDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		// 새 위치 계산
		float Time, Value;
		ScreenPosToCurve(MousePos, Time, Value);

		// Y값(Value)만 수정 (X는 고정: 0, 0.33, 0.66, 1.0)
		OutValue = FMath::Clamp(Value, 0.0f, 1.0f);
		bModified = true;
	}

	// 드래그 종료
	if (bDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		DraggingControlPointIndex = -1;
		DraggingChannelIndex = -1;
	}

	// 제어점 그리기
	ImU32 PointColor = bHovered || bDragging ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 255);
	DrawList->AddCircleFilled(ScreenPos, Radius, PointColor);
	DrawList->AddCircle(ScreenPos, Radius, IM_COL32(0, 0, 0, 255), 0, 2.0f);

	// 제어점 레이블
	if (bHovered || bDragging)
	{
		char Label[32];
		snprintf(Label, sizeof(Label), "P%d: %.2f", Index, OutValue);
		DrawList->AddText(ImVec2(ScreenPos.x + 12, ScreenPos.y - 8), IM_COL32(255, 255, 255, 255), Label);
	}
}

void UCameraBlendEditorWindow::RenderTimelinePreview()
{
	ImGui::Text("Preview:");

	// 재생 버튼
	if (!bPreviewing)
	{
		if (ImGui::Button("Play"))
		{
			bPreviewing = true;
			PreviewTime = 0.0f;
		}
	}
	else
	{
		if (ImGui::Button("Stop"))
		{
			bPreviewing = false;
			PreviewTime = 0.0f;
		}
	}

	ImGui::SameLine();

	// 타임라인 슬라이더
	ImGui::SetNextItemWidth(400);
	ImGui::SliderFloat("##PreviewTimeline", &PreviewTime, 0.0f, PreviewDuration, "%.2f sec");

	// 재생 중이면 시간 증가
	if (bPreviewing)
	{
		PreviewTime += ImGui::GetIO().DeltaTime;
		if (PreviewTime >= PreviewDuration)
		{
			PreviewTime = PreviewDuration;
			bPreviewing = false; // 재생 종료
		}
	}

	// 현재 알파 값 표시
	float TimeRatio = PreviewDuration > 0.0f ? (PreviewTime / PreviewDuration) : 0.0f;
	TimeRatio = FMath::Clamp(TimeRatio, 0.0f, 1.0f);

	float LocationAlpha = EditingParams.LocationCurve.Evaluate(TimeRatio);
	float RotationAlpha = EditingParams.RotationCurve.Evaluate(TimeRatio);
	float FOVAlpha = EditingParams.FOVCurve.Evaluate(TimeRatio);
	float SpringArmAlpha = EditingParams.SpringArmCurve.Evaluate(TimeRatio);

	ImGui::Text("Current Alpha:");
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "  Location: %.3f", LocationAlpha);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "  Rotation: %.3f", RotationAlpha);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 1.0f, 1.0f), "  FOV: %.3f", FOVAlpha);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "  SpringArm: %.3f", SpringArmAlpha);
}

ImVec2 UCameraBlendEditorWindow::CurveToScreenPos(float Time, float Value) const
{
	// 저장된 원점 사용 (GetCursorScreenPos 사용 안 함)
	float X = CurveEditorOrigin.x + CurveEditorPadding.x + Time * (CurveEditorSize.x - 2 * CurveEditorPadding.x);

	// Y축은 반전 (화면 좌표는 위가 0, 커브는 위가 1)
	float Y = CurveEditorOrigin.y + CurveEditorPadding.y + (1.0f - Value) * (CurveEditorSize.y - 2 * CurveEditorPadding.y);

	return ImVec2(X, Y);
}

void UCameraBlendEditorWindow::ScreenPosToCurve(ImVec2 ScreenPos, float& OutTime, float& OutValue) const
{
	// 저장된 원점 사용 (GetCursorScreenPos 사용 안 함)
	float RelativeX = ScreenPos.x - CurveEditorOrigin.x - CurveEditorPadding.x;
	float RelativeY = ScreenPos.y - CurveEditorOrigin.y - CurveEditorPadding.y;

	OutTime = RelativeX / (CurveEditorSize.x - 2 * CurveEditorPadding.x);
	OutValue = 1.0f - (RelativeY / (CurveEditorSize.y - 2 * CurveEditorPadding.y));

	OutTime = FMath::Clamp(OutTime, 0.0f, 1.0f);
	OutValue = FMath::Clamp(OutValue, 0.0f, 1.0f);
}

void UCameraBlendEditorWindow::ApplyPreset(EViewTargetBlendFunction Preset)
{
	EditingParams.BlendFunction = Preset;

	switch (Preset)
	{
	case EViewTargetBlendFunction::VTBlend_Linear:
		EditingParams.LocationCurve = FBezierControlPoints::Linear();
		EditingParams.RotationCurve = FBezierControlPoints::Linear();
		EditingParams.FOVCurve = FBezierControlPoints::Linear();
		EditingParams.SpringArmCurve = FBezierControlPoints::Linear();
		break;

	case EViewTargetBlendFunction::VTBlend_Cubic:
		EditingParams.LocationCurve = FBezierControlPoints::Cubic();
		EditingParams.RotationCurve = FBezierControlPoints::Cubic();
		EditingParams.FOVCurve = FBezierControlPoints::Cubic();
		EditingParams.SpringArmCurve = FBezierControlPoints::Cubic();
		break;

	case EViewTargetBlendFunction::VTBlend_EaseIn:
		EditingParams.LocationCurve = FBezierControlPoints::EaseIn();
		EditingParams.RotationCurve = FBezierControlPoints::EaseIn();
		EditingParams.FOVCurve = FBezierControlPoints::EaseIn();
		EditingParams.SpringArmCurve = FBezierControlPoints::EaseIn();
		break;

	case EViewTargetBlendFunction::VTBlend_EaseOut:
		EditingParams.LocationCurve = FBezierControlPoints::EaseOut();
		EditingParams.RotationCurve = FBezierControlPoints::EaseOut();
		EditingParams.FOVCurve = FBezierControlPoints::EaseOut();
		EditingParams.SpringArmCurve = FBezierControlPoints::EaseOut();
		break;

	case EViewTargetBlendFunction::VTBlend_EaseInOut:
		EditingParams.LocationCurve = FBezierControlPoints::EaseInOut();
		EditingParams.RotationCurve = FBezierControlPoints::EaseInOut();
		EditingParams.FOVCurve = FBezierControlPoints::EaseInOut();
		EditingParams.SpringArmCurve = FBezierControlPoints::EaseInOut();
		break;

	case EViewTargetBlendFunction::VTBlend_BezierCustom:
		// 커스텀은 현재 값 유지
		break;

	default:
		break;
	}
}

void UCameraBlendEditorWindow::ResetToDefault()
{
	ApplyPreset(EViewTargetBlendFunction::VTBlend_Cubic);
	CurrentPresetIndex = 1; // Cubic
	EditingParams.BlendTime = 2.0f;
	EditingParams.BlendExp = 2.0f;
	PreviewDuration = 2.0f;
	PreviewTime = 0.0f;
	bPreviewing = false;
}

void UCameraBlendEditorWindow::SavePresetToFile()
{
	using std::filesystem::path;

	// 파일 다이얼로그 열기
	OPENFILENAMEW ofn;
	wchar_t szFile[260] = {};
	wchar_t szInitialDir[260] = {};

	// Data/CameraBlendPresets 폴더를 기본 경로로 설정
	std::filesystem::path presetDir = std::filesystem::current_path() / "Data" / "CameraBlendPresets";

	// 폴더가 없으면 생성
	if (!std::filesystem::exists(presetDir))
	{
		std::filesystem::create_directories(presetDir);
	}

	wcscpy_s(szInitialDir, presetDir.wstring().c_str());

	// OPENFILENAME 초기화
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
	ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = szInitialDir;
	ofn.lpstrTitle = L"Save Camera Blend Preset";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
	ofn.lpstrDefExt = L"json";

	// 다이얼로그 표시
	if (GetSaveFileNameW(&ofn) != TRUE)
	{
		UE_LOG("CameraBlendEditor: 프리셋 저장 취소됨");
		return;
	}

	// 선택된 파일 경로
	FString FilePath = std::string(path(szFile).string());

	// 파일명에서 확장자를 제외한 이름 추출 (프리셋 이름으로 사용)
	path filePath(szFile);
	FString PresetName = filePath.stem().string();

	// FCameraBlendPreset 생성
	FCameraBlendPreset Preset(PresetName, EditingParams);

	// 파일로 저장
	if (Preset.SaveToFile(FilePath))
	{
		// 라이브러리에 즉시 등록
		UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
		if (Library)
		{
			Library->RegisterPreset(Preset);
			UE_LOG("CameraBlendEditor: 프리셋을 라이브러리에 등록했습니다 - %s", PresetName.c_str());
		}
	}
}

void UCameraBlendEditorWindow::LoadPresetFromFile()
{
	using std::filesystem::path;

	// 파일 다이얼로그 열기
	OPENFILENAMEW ofn;
	wchar_t szFile[260] = {};
	wchar_t szInitialDir[260] = {};

	// Data/CameraBlendPresets 폴더를 기본 경로로 설정
	std::filesystem::path presetDir = std::filesystem::current_path() / "Data" / "CameraBlendPresets";
	if (std::filesystem::exists(presetDir))
	{
		wcscpy_s(szInitialDir, presetDir.wstring().c_str());
	}

	// OPENFILENAME 초기화
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
	ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = szInitialDir;
	ofn.lpstrTitle = L"Load Camera Blend Preset";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLESIZING;

	// 다이얼로그 표시
	if (GetOpenFileNameW(&ofn) != TRUE)
	{
		UE_LOG("CameraBlendEditor: 프리셋 로드 취소됨");
		return;
	}

	// 선택된 파일 경로
	FString FilePath = std::string(path(szFile).string());

	// FCameraBlendPreset으로 로드
	FCameraBlendPreset Preset;
	if (!FCameraBlendPreset::LoadFromFile(FilePath, Preset))
	{
		UE_LOG("CameraBlendEditor: 프리셋 로드 실패 - %s", FilePath.c_str());
		return;
	}

	// EditingParams 업데이트
	EditingParams = Preset.BlendParams;

	// UI 업데이트
	CurrentPresetIndex = static_cast<int>(EditingParams.BlendFunction);
	PreviewDuration = EditingParams.BlendTime;
	PreviewTime = 0.0f;
	bPreviewing = false;

	// 라이브러리에 등록
	UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
	if (Library)
	{
		Library->RegisterPreset(Preset);
		UE_LOG("CameraBlendEditor: 프리셋을 라이브러리에 등록했습니다 - %s", Preset.PresetName.c_str());
	}
}
