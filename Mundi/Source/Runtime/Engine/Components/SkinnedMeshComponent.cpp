#include "pch.h"
#include "SkinnedMehComponent.h"

IMPLEMENT_CLASS(USkinnedMeshComponent)
BEGIN_PROPERTIES(USkinnedMeshComponent)
    MARK_AS_COMPONENT("스킨 메시 컴포넌트", "스킨 메시")
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

void USkinnedMeshComponent::SetSkeletalMesh(const FString& FilePath)
{
    if(FilePath.empty())
    {
        UE_LOG("[USkinnedMeshComponent/SetskeletalMesh] FilePath is empty");
        return;
    }
    
    if (SkeletalMesh->GetFilePath() != FilePath)
    {
        SkeletalMesh->SetFilePath(FilePath);
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
    SkinningInvTransMatrix.SetNum(BoneCount);
    for (int i = 0; i < BoneCount; i++)
    {
        SkinningMatrix[i] = MeshAsset->Bones[i].InverseBindPoseMatrix * ComponentSpaceTransforms[i];
        if (SkinningMatrix[i].IsOrtho())
        {
            SkinningInvTransMatrix[i] = SkinningMatrix[i];
        }
        else
        {
            SkinningInvTransMatrix[i] = SkinningMatrix[i].InverseAffine().Transpose();
        }
    }
}

void USkinnedMeshComponent::PerformCPUSkinning()
{
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkinnedMeshComponent/PerfromCPUSkinning] FSkeletalMesh is null");
        return;
    }

    if (SkinningMatrix.IsEmpty())
    {
        UE_LOG("[USkinnedMeshComponent/PerfromCPUSkinning] SkinningMatrix is zero");
        return;
    }

    const int32 VertexCount = MeshAsset->SkinnedVertices.Num();
    if (VertexCount == 0)
    {
        UE_LOG("[USkinnedMeshComponent/PerfromCPUSkinning] VertexCount is zero");
        return;
    }


    // 정점 개수만큼 순회
    TArray<FNormalVertex> AnimatedVertices;
    AnimatedVertices.SetNum(VertexCount);
    for (int i = 0; i < VertexCount; i++)
    {
        const FSkinnedVertex& SourceVertex = MeshAsset->SkinnedVertices[i];
        FNormalVertex& AnimatedVertex = AnimatedVertices[i];
        AnimatedVertex.pos = {};
        AnimatedVertex.normal = {};
        AnimatedVertex.Tangent = {};
        // 총 4개의 가중치
        for (int j = 0; j < 4; j++)
        {
            int BoneIndex = SourceVertex.BoneIndices[j];
            float BoneWeight = SourceVertex.BoneWeights[j];
            // 가중치가 0이거나 유효하지 않은 뼈 인덱스는 스킵
            if (BoneWeight <= KINDA_SMALL_NUMBER || BoneIndex >= SkinningMatrix.Num() || BoneIndex < 0)
            {
                continue;;
            }

            FVector Pos = SourceVertex.BaseVertex.pos * SkinningMatrix[BoneIndex];
            FVector4 Normal4 = FVector4(SourceVertex.BaseVertex.normal, 0.0f);
            Normal4 = TransformDirection(Normal4, SkinningInvTransMatrix[BoneIndex]);
            FVector Normal = FVector(Normal4.X, Normal4.Y, Normal4.Z);
            FVector4 Tangent = TransformDirection(SourceVertex.BaseVertex.Tangent, SkinningInvTransMatrix[BoneIndex]);
            
            AnimatedVertex.pos += BoneWeight * Pos;
            AnimatedVertex.normal += BoneWeight * Normal;
            AnimatedVertex.Tangent += BoneWeight * Tangent;
        }

        AnimatedVertex.normal.Normalize();
        AnimatedVertex.Tangent.Normalize();
        AnimatedVertex.color = SourceVertex.BaseVertex.color;
        AnimatedVertex.tex = SourceVertex.BaseVertex.tex;
    }    
}
