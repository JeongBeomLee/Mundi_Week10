#pragma once
#include "Actor.h"
#include "Enums.h"

class UGizmoArrowComponent;
class UGizmoScaleComponent;
class UGizmoRotateComponent;
class ACameraActor;
class USelectionManager;
class FViewport;

/**
 * @brief ImGui 입력을 사용하는 Offscreen Viewport 전용 Gizmo
 *
 * 설계:
 * - AGizmoActor와 독립적 (상속 없음) - LSP 위반 방지
 * - Transform 타겟: 자기자신 (AGizmoActor는 외부 컴포넌트)
 * - 입력: ImGui 직접 (AGizmoActor는 InputManager)
 *
 * 향후 리팩토링:
 * - [COMMON] 태그 코드를 GizmoActorBase로 추출 가능
 * - 현재는 boilerplate 중복이지만 명확한 분리 우선
 */
class AOffscreenGizmoActor : public AActor
{
public:
	DECLARE_CLASS(AOffscreenGizmoActor, AActor)
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

	// Tick (컴포넌트 visibility 업데이트만)
	virtual void Tick(float DeltaSeconds) override;

	// [COMMON] Getter/Setter (나중에 베이스로 이동 가능)
	void SetMode(EGizmoMode NewMode);
	EGizmoMode GetMode() const;
	void SetSpace(EGizmoSpace NewSpace) { CurrentSpace = NewSpace; }
	EGizmoSpace GetSpace() const { return CurrentSpace; }
	void SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* Target);
	bool GetbRender() const { return bRender; }
	void SetbRender(bool bInRender) { bRender = bInRender; }
	void SetCameraActor(ACameraActor* InCameraActor) { CameraActor = InCameraActor; }

	// [COMMON] State getters
	bool GetbIsHovering() const { return bIsHovering; }
	bool GetbIsDragging() const { return bIsDragging; }

	// [COMMON] Mode switching (ImGui version - no InputManager)
	void ProcessGizmoModeSwitch();

	// [COMMON] Component getters (나중에 베이스로 이동 가능)
	UGizmoArrowComponent* GetArrowX() const { return ArrowX; }
	UGizmoArrowComponent* GetArrowY() const { return ArrowY; }
	UGizmoArrowComponent* GetArrowZ() const { return ArrowZ; }
	UGizmoScaleComponent* GetScaleX() const { return ScaleX; }
	UGizmoScaleComponent* GetScaleY() const { return ScaleY; }
	UGizmoScaleComponent* GetScaleZ() const { return ScaleZ; }
	UGizmoRotateComponent* GetRotateX() const { return RotateX; }
	UGizmoRotateComponent* GetRotateY() const { return RotateY; }
	UGizmoRotateComponent* GetRotateZ() const { return RotateZ; }

	/**
	 * @brief Gizmo Actor 자신의 Transform 업데이트 (드래그 시)
	 * @note AGizmoActor::OnDrag와 다른 의미: 자기자신 수정 vs 외부 컴포넌트 수정
	 * @note 이름 변경으로 semantic 명확화
	 */
	void UpdateSelfTransformFromDrag(uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport);

protected:
	// [COMMON] 컴포넌트들 (나중에 베이스로 이동 가능)
	UGizmoArrowComponent* ArrowX = nullptr;
	UGizmoArrowComponent* ArrowY = nullptr;
	UGizmoArrowComponent* ArrowZ = nullptr;

	UGizmoScaleComponent* ScaleX = nullptr;
	UGizmoScaleComponent* ScaleY = nullptr;
	UGizmoScaleComponent* ScaleZ = nullptr;

	UGizmoRotateComponent* RotateX = nullptr;
	UGizmoRotateComponent* RotateY = nullptr;
	UGizmoRotateComponent* RotateZ = nullptr;

	// [COMMON] 상태 변수들
	bool bRender = false;
	bool bIsHovering = false;
	bool bIsDragging = false;
	EGizmoMode CurrentMode = EGizmoMode::Translate;
	EGizmoSpace CurrentSpace = EGizmoSpace::World;

	ACameraActor* CameraActor = nullptr;
	USelectionManager* SelectionManager = nullptr;

	uint32 GizmoAxis = 0;

	// [COMMON] 드래그 상태 변수들
	FQuat DragStartRotation;
	FVector DragStartLocation;
	FVector DragStartScale;
	FVector2D DragStartPosition;
	ACameraActor* DragCamera = nullptr;
	uint32 DraggingAxis = 0;
	FVector HoverImpactPoint;
	FVector DragImpactPoint;
	FVector2D DragScreenVector;

	// [COMMON] 공통 메서드들 (나중에 베이스로 이동 가능)
	void ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
	void UpdateComponentVisibility();
	static FVector2D GetStableAxisDirection(const FVector& WorldAxis, const ACameraActor* Camera);

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
