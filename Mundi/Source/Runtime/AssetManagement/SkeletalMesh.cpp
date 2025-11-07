#include "pch.h"
#include "SkeletalMesh.h"

#include "FFBXManager.h"

IMPLEMENT_CLASS(USkeletalMesh)

USkeletalMesh::~USkeletalMesh()
{
}

void USkeletalMesh::Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    assert(InDevice);

    SetVertexType(InVertexType);

    SkeletalMeshAsset = FFBXManager::LoadFBXSkeletalMeshAsset(InFilePath);

    // 빈 버텍스, 인덱스로 버퍼 생성 방지
    if (SkeletalMeshAsset && 0 < SkeletalMeshAsset->Vertices.size() && 0 < SkeletalMeshAsset->Indices.size())
    {
        CacheFilePath = SkeletalMeshAsset->CacheFilePath;
        // CreateVertexBuffer(SkeletalMeshAsset, InDevice, InVertexType);
        // CreateIndexBuffer(SkeletalMeshAsset, InDevice);
        // CreateLocalBound(SkeletalMeshAsset);
        VertexCount = static_cast<uint32>(SkeletalMeshAsset->Vertices.size());
        IndexCount = static_cast<uint32>(SkeletalMeshAsset->Indices.size());
    }
}

void USkeletalMesh::Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    
}

void USkeletalMesh::SetVertexType(EVertexLayoutType InVertexLayoutType)
{
    
}
