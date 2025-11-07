#pragma once
#include "GizmoArrowComponent.h"
class UGizmoRotateComponent :public UGizmoArrowComponent
{
public:
    UGizmoRotateComponent();

protected:
    ~UGizmoRotateComponent() override;

public:
    DECLARE_CLASS(UGizmoRotateComponent, UGizmoArrowComponent)

    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UGizmoRotateComponent)
};

