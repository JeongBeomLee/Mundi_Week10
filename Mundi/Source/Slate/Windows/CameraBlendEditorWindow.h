#pragma once
#include "UIWindow.h"
#include "CameraTypes.h"

// 카메라 블렌드 커브 에디터 윈도우
class UCameraBlendEditorWindow : public UUIWindow
{
public:
	DECLARE_CLASS(UCameraBlendEditorWindow, UUIWindow)

	UCameraBlendEditorWindow();
	virtual ~UCameraBlendEditorWindow() = default;

	virtual void Initialize() override;
	virtual void RenderContent() override;
	virtual bool OnWindowClose() override;

private:
	// UI 렌더링 함수들
	void RenderPresetSelector();           // 프리셋 선택 UI
	void RenderCurveEditor();              // 커브 에디터 메인
	void RenderChannelSelector();          // 채널 선택 (Location/Rotation/FOV/SpringArm)
	void RenderBezierCurve(const FBezierControlPoints& Curve, const char* Label, ImU32 Color);
	void RenderControlPoint(int Index, ImVec2 ScreenPos, float& OutValue, bool& bModified);
	void RenderTimelinePreview();          // 타임라인 프리뷰

	// 유틸리티 함수들
	ImVec2 CurveToScreenPos(float Time, float Value) const;
	void ScreenPosToCurve(ImVec2 ScreenPos, float& OutTime, float& OutValue) const;
	void ApplyPreset(EViewTargetBlendFunction Preset);
	void ResetToDefault();

	// 저장/불러오기
	void SavePresetToFile();
	void LoadPresetFromFile();

public:
	// PlayerCameraManager 설정 (외부에서 호출)
	void SetPlayerCameraManager(class APlayerCameraManager* InCameraManager)
	{
		PlayerCameraManager = InCameraManager;
	}

private:
	// 편집 중인 블렌드 파라미터
	FViewTargetTransitionParams EditingParams;

	// UI 상태
	int CurrentPresetIndex = 2;            // 기본값: VTBlend_Cubic
	int CurrentChannelIndex = 0;           // 0: Location, 1: Rotation, 2: FOV, 3: SpringArm
	bool bShowAllChannels = false;         // 모든 채널 동시 표시

	// 커브 에디터 설정
	ImVec2 CurveEditorSize = ImVec2(600, 400);
	ImVec2 CurveEditorPadding = ImVec2(40, 40);
	ImVec2 CurveEditorOrigin;  // 커브 에디터 원점 (화면 좌표)

	// 드래그 상태
	int DraggingControlPointIndex = -1;
	int DraggingChannelIndex = -1;

	// 프리뷰 상태
	bool bPreviewing = false;
	float PreviewTime = 0.0f;
	float PreviewDuration = 2.0f;

	// PlayerCameraManager 참조
	class APlayerCameraManager* PlayerCameraManager = nullptr;

	// 채널 이름 및 색상
	static constexpr const char* ChannelNames[4] = {
		"Location",
		"Rotation",
		"FOV",
		"SpringArm"
	};

	static constexpr ImU32 ChannelColors[4] = {
		IM_COL32(255, 100, 100, 255),  // 빨강 (Location)
		IM_COL32(100, 255, 100, 255),  // 초록 (Rotation)
		IM_COL32(100, 100, 255, 255),  // 파랑 (FOV)
		IM_COL32(255, 255, 100, 255)   // 노랑 (SpringArm)
	};
};
