#pragma once
#include "MeshComponent.h"
#include "Enums.h"
#include "AABB.h"

class UStaticMesh;
class UShader;
class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FSceneCompData;

class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
	GENERATED_REFLECTION_BODY()

	UStaticMeshComponent();

protected:
	~UStaticMeshComponent() override;
	void ClearDynamicMaterials();

public:
	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	void OnSerialized() override;

	void SetStaticMesh(const FString& PathFileName);

	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	
	UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

	UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex);

	void SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName);
	const TArray<UMaterialInterface*> GetMaterialSlots() const { return MaterialSlots; }

	FAABB GetWorldAABB() const;

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UStaticMeshComponent)

protected:
	void OnTransformUpdated() override;
	void MarkWorldPartitionDirty();

protected:
	UStaticMesh* StaticMesh = nullptr;
	TArray<UMaterialInterface*> MaterialSlots;
	TArray<UMaterialInstanceDynamic*> DynamicMaterialInstances;
};
