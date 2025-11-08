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
	 */
	void ProcessImGuiInput(ImGuiIO& IO, bool bIsRightMouseDown);

private:
	/**
	 * 마우스 델타로 카메라 회전
	 */
	void RotateCamera(float DeltaX, float DeltaY);

	/**
	 * 방향 벡터로 카메라 이동
	 */
	void MoveCamera(const FVector& Direction, float DeltaTime);
};
