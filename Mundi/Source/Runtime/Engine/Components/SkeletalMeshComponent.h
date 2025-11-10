#pragma once
#include "SkinnedMeshComponent.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
    GENERATED_REFLECTION_BODY()

    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override;

    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    void TickComponent(float DeltaTime) override;
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(USkeletalMeshComponent)

    void OnSerialized() override;

    void SetSkeletalMesh(const FString& FilePath) override;

    USkeletalMesh* GetSkeletalMesh() const  { return SkeletalMesh; }

    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    const TArray<UMaterialInterface*> GetMaterialSlots() const { return MaterialSlots; }
    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;

    UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex);

    void EnsureSkinningReady(D3D11RHI* InDevice);

    void UpdateVertexBuffer(D3D11RHI* InDevice);
protected:    
    void UpdateBoneMatrices();

    void UpdateSkinningMatrices() override;
    
    void CleareDynamicMaterials();

protected:

    // 부모 뼈 기준의 자식 배치, 부모 기준 상대적 로컬 변환
    // fbx에서 불러오거나 애니메이션에서 가져온다    
    TArray<FMatrix> BoneSpaceTransforms = {};

    // 애니메이션이 없을 때 Bind Pose, 존재하면 새로 계산해야함
    // 특정 뼈의 로컬 -> 모델 공간
    TArray<FMatrix> ComponentSpaceTransforms = {};

    // 제거 예정 -> 제거하지 말고 원래대로 스키닝된 정점 저장하고 버퍼 업뎃할 때 사용합시다.
    TArray<FNormalVertex> AnimatedVertices = {};

    
};