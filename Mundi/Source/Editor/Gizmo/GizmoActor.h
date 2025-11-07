#pragma once
#include "Actor.h"
#include "Enums.h"

class UGizmoArrowComponent;
class UGizmoScaleComponent;
class UGizmoRotateComponent;
class ACameraActor;
class USelectionManager;
class UInputManager;
class UUIManager;
class URenderer;
class CPickingSystem;
class FViewport;
class AGizmoActor : public AActor
{
public:
    DECLARE_CLASS(AGizmoActor, AActor)
    AGizmoActor();

    virtual void Tick(float DeltaSeconds) override;
protected:
    ~AGizmoActor() override;

public:

// ────────────────
// Getter Functions
// ────────────────
    UGizmoArrowComponent* GetArrowX() const { return ArrowX; }
    UGizmoArrowComponent* GetArrowY() const { return ArrowY; }
    UGizmoArrowComponent* GetArrowZ() const { return ArrowZ; }
    UGizmoScaleComponent* GetScaleX() const { return ScaleX; }
    UGizmoScaleComponent* GetScaleY() const { return ScaleY; }
    UGizmoScaleComponent* GetScaleZ() const { return ScaleZ; }
    UGizmoRotateComponent* GetRotateX() const { return RotateX; }
    UGizmoRotateComponent* GetRotateY() const { return RotateY; }
    UGizmoRotateComponent* GetRotateZ() const { return RotateZ; }
    void SetMode(EGizmoMode NewMode);
    EGizmoMode GetMode();
    void SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* Target);
    void SetSpace(EGizmoSpace NewSpace) { CurrentSpace = NewSpace; }
    EGizmoSpace GetSpace() const { return CurrentSpace; }

    bool GetbRender() const { return bRender; }
    void SetbRender(bool bInRender) { bRender = bInRender; }

    bool GetbIsHovering() const { return bIsHovering; }
    void SetbIsHovering(bool bInIsHovering) { bIsHovering = bInIsHovering; }

    bool GetbIsDragging() const { return bIsDragging; }

    void NextMode(EGizmoMode GizmoMode);
    TArray<USceneComponent*>* GetGizmoComponents();

    
    EGizmoMode GetGizmoMode() const;

    void OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport);
    void OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera);
    
    // Gizmo interaction methods
   // void SetTargetActor(AActor* InTargetActor) { TargetActor = InTargetActor; Tick(0.f);  }
    void SetCameraActor(ACameraActor* InCameraActor) { CameraActor = InCameraActor; }
    ACameraActor* GetCameraActor() const { return CameraActor; }
    

    void ProcessGizmoInteraction(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void ProcessGizmoModeSwitch();

    // 어차피 gizmo가 게임모드에서 안나오니까 할 필요 없을지도?
    // ───── 복사 관련 ────────────────────────────
    /*void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(AGizmoActor)*/

protected:

    UGizmoArrowComponent* ArrowX;
    UGizmoArrowComponent* ArrowY;
    UGizmoArrowComponent* ArrowZ;
    TArray<USceneComponent*> GizmoArrowComponents;

    UGizmoScaleComponent* ScaleX;
    UGizmoScaleComponent* ScaleY;
    UGizmoScaleComponent* ScaleZ;
    TArray<USceneComponent*> GizmoScaleComponents;

    UGizmoRotateComponent* RotateX;
    UGizmoRotateComponent* RotateY;
    UGizmoRotateComponent* RotateZ;
    TArray<USceneComponent*> GizmoRotateComponents;
    bool bRender = false;
    bool bIsHovering = false;
    bool bIsDragging = false;
    EGizmoMode CurrentMode;
    EGizmoSpace CurrentSpace = EGizmoSpace::World;
    
    // Interaction state
    /*AActor* TargetActor = nullptr;
    USceneComponent* SelectedComponent = nullptr;*/
    ACameraActor* CameraActor = nullptr;
    
    // Manager references
    USelectionManager* SelectionManager = nullptr;
    UInputManager* InputManager = nullptr;
    UUIManager* UIManager = nullptr;
    
    uint32 GizmoAxis{};
    // Gizmo interaction methods
   
    void ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void ProcessGizmoDragging(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void UpdateComponentVisibility();

private:
    // 드래그 시작 시점의 타겟 트랜스폼
    FQuat DragStartRotation;
    FVector DragStartLocation;
    FVector DragStartScale;

    // 드래그 시작 시점의 마우스 및 카메라 정보
    FVector2D DragStartPosition;
    ACameraActor* DragCamera = nullptr;

    // 드래그가 시작될 때 고정된 축 (0 = 드래그 중 아님)
    uint32 DraggingAxis = 0;

    // 호버 시 3D 충돌 지점 (PickingSystem이 채워줌)
    FVector HoverImpactPoint;
    // 드래그 시작 시 3D 충돌 지점
    FVector DragImpactPoint;
    // (회전용) 계산된 2D 스크린 드래그 벡터
    FVector2D DragScreenVector;
};
