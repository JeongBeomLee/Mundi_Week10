#include "pch.h"
#include "MeshComponent.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "Shader.h"

IMPLEMENT_CLASS(UMeshComponent)

UMeshComponent::UMeshComponent()
{
}

UMeshComponent::~UMeshComponent()
{
}

void UMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
