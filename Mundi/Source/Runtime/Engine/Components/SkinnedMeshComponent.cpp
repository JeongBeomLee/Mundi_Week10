#include "pch.h"
#include "SkinnedMehComponent.h"

IMPLEMENT_CLASS(USkinnedMeshComponent)
BEGIN_PROPERTIES(USkinnedMeshComponent)

END_PROPERTIES(USkinnedMeshComponent)

USkinnedMeshComponent::USkinnedMeshComponent()
{
    
}

USkinnedMeshComponent::~USkinnedMeshComponent()
{
    
}

void USkinnedMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UMeshComponent::Serialize(bInIsLoading, InOutHandle);
}

void USkinnedMeshComponent::TickComponent(float DeltaTime)
{
    UMeshComponent::TickComponent(DeltaTime);

    if (!SkeletalMesh)
    {
        ComponentSpaceTransforms.Empty();
        SkinningMatrix.Empty();
        return;
    }
    
    UpdateBoneMatrices();
    UpdateSkinningMatrices();
    
}

void USkinnedMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    UMeshComponent::CollectMeshBatches(OutMeshBatchElements, View);
}

void USkinnedMeshComponent::DuplicateSubObjects()
{
    UMeshComponent::DuplicateSubObjects();
}

void USkinnedMeshComponent::OnSerialized()
{
    UMeshComponent::OnSerialized();
}

void USkinnedMeshComponent::SetskeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    if(!InSkeletalMesh)
    {
        UE_LOG("[USkinnedMeshComponent/SetskeletalMesh] InSkeletalMesh is null");
        return;
    }
    
    if (InSkeletalMesh != SkeletalMesh)
    {
        SkeletalMesh = InSkeletalMesh;
        bChangedSkeletalMesh = true;
        UpdateBoneMatrices();
        UpdateSkinningMatrices();
    }    
}

void USkinnedMeshComponent::UpdateBoneMatrices()
{    
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkinnedMeshComponent/UpdateBoneMatrices] FSkeletalMesh is null");
        return;
    }

    const int32 BoneCount = MeshAsset->Bones.Num();
    if (BoneCount == 0)
    {
        UE_LOG("[USkinnedMeshComponent/UpdateBoneMatrices] BoneCount is zero");
        ComponentSpaceTransforms.Empty();
        return;
    }

    // 지금은 애니메이션이 없어서 BindPose 그대로 사용
    ComponentSpaceTransforms.SetNum(BoneCount);
    for (int i = 0; i < BoneCount; i++)
    {
        // 애니메이션 없으니까 바인드 포즈 사용        
        ComponentSpaceTransforms[i] = MeshAsset->Bones[i].GlobalTransform;
    }

    // TODO 애니메이션 구현 후 사용 BoneSpaceTransforms채워서 사용
    // for (int i = 0; i < BoneCount; i++)
    // {
    //     FMatrix BoneLocal = BoneSpaceTransforms[i];
    //     int32 ParentIndex = MeshAsset->Bones[i].ParentIndex;
    //     if (ParentIndex == -1)
    //     {
    //         ComponentSpaceTransforms[i] = BoneLocal;
    //     }
    //     else
    //     {
    //         ComponentSpaceTransforms[i] = BoneLocal * ComponentSpaceTransforms[ParentIndex];
    //     }
    // }
}

void USkinnedMeshComponent::UpdateSkinningMatrices()
{
    // 본 매트릭스가 없으면 스키닝 안함
    const int32 BoneCount = ComponentSpaceTransforms.Num();
    if (BoneCount == 0)
    {
        UE_LOG("[USkinnedMeshComponent/UpdateSkinningMatrices] BoneMatrices is zero");
        return;
    }
    
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkinnedMeshComponent/UpdateSkinningMatrices] FSkeletalMesh is null");
        return;
    }

    SkinningMatrix.SetNum(BoneCount);
    for (int i = 0; i < BoneCount; i++)
    {
        SkinningMatrix[i] = MeshAsset->Bones[i].InverseBindPoseMatrix * ComponentSpaceTransforms[i];
    }
}


void USkinnedMeshComponent::PerfromCPUSkinning()
{
}
