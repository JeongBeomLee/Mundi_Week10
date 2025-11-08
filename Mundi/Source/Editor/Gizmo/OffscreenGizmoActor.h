#pragma once
#include "GizmoActor.h"

/**
 * @brief ImGui 입력을 사용하는 Offscreen Viewport 전용 Gizmo
 * @note InputManager 대신 ImGui 마우스 이벤트를 직접 받아 처리
 */
class AOffscreenGizmoActor : public AGizmoActor
{
public:
	DECLARE_CLASS(AOffscreenGizmoActor, AGizmoActor)
	AOffscreenGizmoActor();

	// PostActorCreated 오버라이드 (SelectionManager 초기화)
	virtual void PostActorCreated() override;

	/**
	 * @brief ImGui 입력을 사용한 Gizmo 상호작용 처리
	 * @param Camera 카메라 액터
	 * @param Viewport 뷰포트
	 * @param MousePositionX 마우스 X 좌표 (viewport 상대)
	 * @param MousePositionY 마우스 Y 좌표 (viewport 상대)
	 * @param bLeftMouseDown ImGui 좌클릭 다운 상태
	 * @param bLeftMouseReleased ImGui 좌클릭 릴리즈 상태
	 */
	void ProcessGizmoInteractionImGui(
		ACameraActor* Camera,
		FViewport* Viewport,
		float MousePositionX,
		float MousePositionY,
		bool bLeftMouseDown,
		bool bLeftMouseReleased
	);

	// Tick 비활성화 (수동 업데이트만 사용)
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * @brief OnDrag 오버라이드 - Target 대신 Gizmo Actor 자신의 Transform 업데이트
	 * @note SkeletalMeshEditor에서 본의 Transform은 별도로 동기화됨
	 */
	virtual void OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport) override;

protected:
	// ImGui 입력 기반 드래그 처리
	void ProcessGizmoDraggingImGui(
		ACameraActor* Camera,
		FViewport* Viewport,
		float MousePositionX,
		float MousePositionY,
		bool bLeftMouseDown,
		bool bLeftMouseReleased
	);
};
