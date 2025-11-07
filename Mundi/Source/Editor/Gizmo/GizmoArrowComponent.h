#pragma once
#include "StaticMeshComponent.h"
class UGizmoArrowComponent : public UStaticMeshComponent
{
public:
    DECLARE_CLASS(UGizmoArrowComponent, UStaticMeshComponent)
    UGizmoArrowComponent();
    
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

protected:
    ~UGizmoArrowComponent() override;
    
public:
    const FVector& GetDirection() const { return Direction; }
    const FVector& GetColor() const { return Color; }

    void SetDirection(const FVector& InDirection) { Direction = InDirection; }
    void SetColor(const FVector& InColor) { Color = InColor; }

    // Gizmo visual state
    void SetAxisIndex(uint32 InAxisIndex) { AxisIndex = InAxisIndex; }
    void SetDefaultScale(const FVector& InSize) { DefaultScale = InSize; }
    void SetHighlighted(bool bInHighlighted, uint32 InAxisIndex) { bHighlighted = bInHighlighted; AxisIndex = InAxisIndex; }
    bool IsHighlighted() const { return bHighlighted; }
    uint32 GetAxisIndex() const { return AxisIndex; }

    // Screen-constant scale control (default: true for typical gizmos, false for world-space indicators)
    void SetUseScreenConstantScale(bool bInUseScreenConstantScale) { bUseScreenConstantScale = bInUseScreenConstantScale; }
    bool GetUseScreenConstantScale() const { return bUseScreenConstantScale; }

    // Render priority (higher values render later/on top). Default: 0 for DirectionGizmos, 100 for selection GizmoActor
    void SetRenderPriority(int32 InPriority) { RenderPriority = InPriority; }
    int32 GetRenderPriority() const { return RenderPriority; }

    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    void SetDrawScale(float ViewWidth, float ViewHeight, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix);

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UGizmoArrowComponent)
protected:
    float ComputeScreenConstantScale(float ViewWidth, float ViewHeight, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, float TargetPixels) const;

protected:
    FVector Direction;
    FVector DefaultScale{ 1.f,1.f,1.f };
    FVector Color;
    bool bHighlighted = false;
    uint32 AxisIndex = 0;

    // Screen-constant scale: true = scale with distance to maintain screen size, false = use world scale
    bool bUseScreenConstantScale = true;

    // Render priority: higher values render later (on top)
    int32 RenderPriority = 0;

    // 기즈모가 항상 사용할 고정 머티리얼입니다.
    UMaterialInterface* GizmoMaterial = nullptr;
};
