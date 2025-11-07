#include "pch.h"
#include "GizmoRotateComponent.h"

IMPLEMENT_CLASS(UGizmoRotateComponent)

UGizmoRotateComponent::UGizmoRotateComponent()
{
    SetStaticMesh(GDataDir + "/Gizmo/RotationHandle.obj");
    SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");
}

UGizmoRotateComponent::~UGizmoRotateComponent()
{
}

void UGizmoRotateComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
