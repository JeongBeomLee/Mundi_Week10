#pragma once
#include "MeshComponent.h"
class UTextRenderComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)
	GENERATED_REFLECTION_BODY()

	UTextRenderComponent();

protected:
	~UTextRenderComponent() override;

public:
	void InitCharInfoMap();
	TArray<FBillboardVertexInfo_GPU> CreateVerticesForString(const FString& text,const FVector& StartPos);

	UQuad* GetStaticMesh() const { return TextQuad; }

	// Serialize
	void OnSerialized() override;

	UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UTextRenderComponent)

private:
	FString Text;
	static TMap<char, FBillboardVertexInfo> CharInfoMap; // shared per-process, built once
	FString TextureFilePath;
	UMaterialInterface* Material;
	UQuad* TextQuad = nullptr;
};