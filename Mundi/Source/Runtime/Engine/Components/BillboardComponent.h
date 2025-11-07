#pragma once
#include "PrimitiveComponent.h"
#include "Object.h"

class UQuad;
class UTexture;
class UMaterial;
class URenderer;

class UBillboardComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)
    GENERATED_REFLECTION_BODY()

    UBillboardComponent();
    ~UBillboardComponent() override = default;

    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    // Setup
    void SetTextureName( FString TexturePath);

    UQuad* GetStaticMesh() const { return Quad; }
    FString& GetFilePath() { return TexturePath; }

    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    // Serialize
    void OnSerialized() override;

    // Duplication
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UBillboardComponent)

protected:
    FString TexturePath;
    UTexture* Texture = nullptr;  // 리플렉션 시스템용 Texture 포인터
    UMaterialInterface* Material = nullptr;
    UQuad* Quad = nullptr;
};

