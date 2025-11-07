#pragma once
#include "PrimitiveComponent.h"

class UShader;

class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

    UMeshComponent();

protected:
    ~UMeshComponent() override;

public:
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UMeshComponent)
};