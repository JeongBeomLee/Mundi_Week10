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

    virtual void SetSkeletalMesh(const FString& FilePath);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

protected:    
    virtual void UpdateSkinningMatrices() {};

    // 매개변수 받게 변경, 자식 클래스 매개변수 전달받아서 채워주기
    void PerformCPUSkinning(TArray<FNormalVertex>& AnimatedVertices);

protected:
    USkeletalMesh* SkeletalMesh = nullptr;    

    // Skinning Matrix
    // 현재 애니메이션이 없어서 Inverse Bind Pose * Bind Pose
    // 애니메이션 생기면 Inverse Bind Pose * BoneMatrix(Transformed Bind Pose) 
    TArray<FMatrix> SkinningMatrix = {};
    
    TArray<FMatrix> SkinningInvTransMatrix = {};

    bool bChangedSkeletalMesh = false;
};