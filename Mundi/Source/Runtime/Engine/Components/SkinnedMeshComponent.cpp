#include "pch.h"
#include "SkinnedMeshComponent.h"


IMPLEMENT_CLASS(USkinnedMeshComponent)

// USkinnedMeshComponent는 Scene에 배치되지 않기 때문에 UI에 노출하지 않음
BEGIN_PROPERTIES(USkinnedMeshComponent)
    //MARK_AS_COMPONENT("스킨 메시 컴포넌트", "스킨 메시")
END_PROPERTIES(USkinnedMeshComponent)

USkinnedMeshComponent::USkinnedMeshComponent()
{
        
}

USkinnedMeshComponent::~USkinnedMeshComponent()
{
    
}

void USkinnedMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}

void USkinnedMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh)
    {
        SkinningMatrix.Empty();
        return;
    }    
}

void USkinnedMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    Super::CollectMeshBatches(OutMeshBatchElements, View);
}

void USkinnedMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void USkinnedMeshComponent::OnSerialized()
{
    Super::OnSerialized();
}

void USkinnedMeshComponent::SetSkeletalMesh(const FString& FilePath)
{
    if(FilePath.empty())
    {
        UE_LOG("[USkinnedMeshComponent/SetskeletalMesh] FilePath is empty");
        return;
    }
}

void USkinnedMeshComponent::SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
{
    Super::SetMaterial(InElementIndex, InNewMaterial);
}

void USkinnedMeshComponent::PerformCPUSkinning(TArray<FNormalVertex>& AnimatedVertices)
{
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkinnedMeshComponent/PerformCPUSkinning] FSkeletalMesh is null");
        return;
    }

    if (SkinningMatrix.IsEmpty())
    {
        UE_LOG("[USkinnedMeshComponent/PerformCPUSkinning] SkinningMatrix is zero");
        return;
    }

    const int32 VertexCount = MeshAsset->SkinnedVertices.Num();
    if (VertexCount == 0)
    {
        UE_LOG("[USkinnedMeshComponent/PerformCPUSkinning] VertexCount is zero");
        return;
    }

    FLogManager& Log = FLogManager::GetInstance();
    if (testflag)
    {
        INITIALIZE_LOG(L"CPUSkinningLog")
    }
    else
    {
        CLOSE_LOG_FILE();
    }

    // 정점 개수만큼 순회
    AnimatedVertices.Empty();
    AnimatedVertices.SetNum(VertexCount);
    for (int i = 0; i < VertexCount; i++)
    {
        const FSkinnedVertex& SourceVertex = MeshAsset->SkinnedVertices[i];
        FNormalVertex& AnimatedVertex = AnimatedVertices[i];
        //FNormalVertex& AnimatedVertex = MeshAsset->Vertices[i];
        AnimatedVertex.pos = {};
        AnimatedVertex.normal = {};
        AnimatedVertex.Tangent = {};

        // Tangent의 W는 Bitangent의 방향 요소이므로 원본 보존
        float TangentW = SourceVertex.BaseVertex.Tangent.W;

        if (testflag)
        {
            WRITE_LOG_FILE(LogType::Debug, L"before")
            WRITE_LOG_FILE(LogType::Debug, L"normal %f %f %f", SourceVertex.BaseVertex.normal.X, SourceVertex.BaseVertex.normal.Y, SourceVertex.BaseVertex.normal.Z)
            WRITE_LOG_FILE(LogType::Debug, L"tangent %f %f %f %f", SourceVertex.BaseVertex.Tangent.X, SourceVertex.BaseVertex.Tangent.Y,
                 SourceVertex.BaseVertex.Tangent.Z, SourceVertex.BaseVertex.Tangent.W)
        }
     
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
            FVector4 Normal4 = TransformDirection(SourceVertex.BaseVertex.normal, SkinningInvTransMatrix[BoneIndex]);
            // Normal4 = TransformDirection(Normal4, SkinningInvTransMatrix[BoneIndex]);
            FVector Normal = FVector(Normal4.X, Normal4.Y, Normal4.Z);            
            FVector4 Tangent = TransformDirection(SourceVertex.BaseVertex.Tangent, SkinningInvTransMatrix[BoneIndex]);
            
            AnimatedVertex.pos += BoneWeight * Pos;
            AnimatedVertex.normal += BoneWeight * Normal;
            AnimatedVertex.Tangent += BoneWeight * Tangent;
        }

        AnimatedVertex.normal.Normalize();
        AnimatedVertex.Tangent.Normalize();
        // 원본 탄젠트 w 복원
        AnimatedVertex.Tangent.W = TangentW;
        AnimatedVertex.color = SourceVertex.BaseVertex.color;
        AnimatedVertex.tex = SourceVertex.BaseVertex.tex;

        if (testflag)
        {
            WRITE_LOG_FILE(LogType::Debug, L"after")
            WRITE_LOG_FILE(LogType::Debug, L"normal %f %f %f", AnimatedVertex.normal.X, AnimatedVertex.normal.Y, AnimatedVertex.normal.Z)
            WRITE_LOG_FILE(LogType::Debug, L"tangent %f %f %f %f", AnimatedVertex.Tangent.X, AnimatedVertex.Tangent.Y,
                 AnimatedVertex.Tangent.Z, AnimatedVertex.Tangent.W)
        }
    }
    
    testflag = false;    
}
