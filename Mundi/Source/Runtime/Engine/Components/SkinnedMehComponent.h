#pragma once
#include "MeshComponent.h"

/*
 *  TODO
 *  dup, serialize
 *  관절 공간 -> 모델 공간
 *  skinning matrix
 *  bone transform
 *  
 */
class USkeletalMesh;

class USkinnedMeshComponent : public UMeshComponent
{
public:
    
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)
    GENERATED_REFLECTION_BODY()

    USkinnedMeshComponent();
    ~USkinnedMeshComponent() override;

    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    void TickComponent(float DeltaTime) override;
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(USkinnedMeshComponent)

    void OnSerialized() override;    

    void SetSkeletalMesh(const FString& FilePath);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

protected:

    void UpdateBoneMatrices();
    void UpdateSkinningMatrices();
    void PerformCPUSkinning();

protected:
    USkeletalMesh* SkeletalMesh = nullptr;

    // 부모 뼈 기준의 자식 배치, 부모 기준 상대적 로컬 변환
    // fbx에서 불러오거나 애니메이션에서 가져온다    
    TArray<FMatrix> BoneSpaceTransforms;

    // 애니메이션이 없을 때 Bind Pose, 존재하면 새로 계산해야함
    // 특정 뼈의 로컬 -> 모델 공간
    TArray<FMatrix> ComponentSpaceTransforms = {};

    // Skinning Matrix
    // 현재 애니메이션이 없어서 Inverse Bind Pose * Bind Pose
    // 애니메이션 생기면 Inverse Bind Pose * BoneMatrix(Transformed Bind Pose) 
    TArray<FMatrix> SkinningMatrix = {};
    
    TArray<FMatrix> SkinningInvTransMatrix = {};

    bool bChangedSkeletalMesh = false;
};