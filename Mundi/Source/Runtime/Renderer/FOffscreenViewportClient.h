#pragma once
#include "FViewportClient.h"

struct ImGuiIO;

/**
 * @brief FOffscreenViewport 전용 ViewportClient
 * ImGui 기반 입력 처리를 위한 특수화 클래스
 */
class FOffscreenViewportClient : public FViewportClient
{
public:
	FOffscreenViewportClient();
	virtual ~FOffscreenViewportClient() override = default;

	/**
	 * ImGui 입력을 처리하여 카메라 조작
	 * @param IO ImGui IO 객체
	 * @param bIsRightMouseDown 우클릭 드래그 중인지 여부
	 * @param ViewportCenterX 뷰포트 중앙 X (스크린 좌표)
	 * @param ViewportCenterY 뷰포트 중앙 Y (스크린 좌표)
	 */
	void ProcessImGuiInput(ImGuiIO& IO, bool bIsRightMouseDown, int ViewportCenterX, int ViewportCenterY);

private:
	/**
	 * 마우스 델타로 카메라 회전
	 */
	void RotateCamera(float DeltaX, float DeltaY);

	/**
	 * 방향 벡터로 카메라 이동
	 */
	void MoveCamera(const FVector& Direction, float DeltaTime);

	// 무한 스크롤을 위한 상태
	bool bWasDragging = false;
	int LastMouseX = 0;
	int LastMouseY = 0;
};
