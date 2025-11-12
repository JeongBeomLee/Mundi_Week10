#include "pch.h"
#include "SkeletalMesh.h"

#include "FFBXManager.h"

IMPLEMENT_CLASS(USkeletalMesh)

USkeletalMesh::~USkeletalMesh()
{
    ReleaseResources();
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
        CreateVertexBuffer(SkeletalMeshAsset, InDevice, InVertexType);
        CreateIndexBuffer(SkeletalMeshAsset, InDevice);
        CreateLocalBound(SkeletalMeshAsset);
        VertexCount = static_cast<uint32>(SkeletalMeshAsset->Vertices.size());
        IndexCount = static_cast<uint32>(SkeletalMeshAsset->Indices.size());
    }
}

void USkeletalMesh::Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    assert(InDevice);

    ReleaseResources();
    SetVertexType(InVertexType);

    CreateVertexBuffer(InData, InDevice, InVertexType);
    CreateIndexBuffer(InData, InDevice);
    CreateLocalBound(InData);
    
    VertexCount = static_cast<uint32>(InData->Vertices.size());
    IndexCount = static_cast<uint32>(InData->Indices.size());
}

void USkeletalMesh::SetVertexType(EVertexLayoutType InVertexLayoutType)
{
    VertexType = InVertexLayoutType;

    switch (VertexType)
    {
    case EVertexLayoutType::PositionColor:
        VertexStride = sizeof(FVertexSimple);
        break;
    case EVertexLayoutType::PositionColorTexturNormal:
        VertexStride = sizeof(FVertexDynamic);
        break;
    default:
        {
            assert(false && "Unsupported vertex layout for skeletal mesh");
            VertexStride = sizeof(FVertexDynamic);            
        }
        break;
    }
}

bool USkeletalMesh::EraseUsingComponets(USkeletalMeshComponent* InSkeletalMeshComponent)
{
    auto It = std::find(UsingComponents.begin(), UsingComponents.end(), InSkeletalMeshComponent);
    if (It != UsingComponents.end())
    {
        UsingComponents.erase(It);
        return true;
    }
    return false;
}

bool USkeletalMesh::AddUsingComponents(USkeletalMeshComponent* InSkeletalMeshComponent)
{
    UsingComponents.Add(InSkeletalMeshComponent);
    return true;
}

void USkeletalMesh::CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    HRESULT hr = D3D11RHI::CreateVertexBufferImpl<FVertexDynamic>(
        InDevice,
        *InMeshData,
        &VertexBuffer,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateVertexBuffer(FSkeletalMesh* InSkeletalMesh, ID3D11Device* InDevice,
    EVertexLayoutType InVertexType)
{
    HRESULT hr = D3D11RHI::CreateVertexBufferImpl<FVertexDynamic>(
        InDevice,
        InSkeletalMesh->Vertices,
        &VertexBuffer,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InMeshData, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateIndexBuffer(FSkeletalMesh* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InSkeletalMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateLocalBound(const FMeshData* InMeshData)
{
    TArray<FVector> Verts = InMeshData->Vertices;
    FVector Min = Verts[0];
    FVector Max = Verts[0];
    for (FVector Vertex : Verts)
    {
        Min = Min.ComponentMin(Vertex);
        Max = Max.ComponentMax(Vertex);
    }
    LocalBound = FAABB(Min, Max);
}

void USkeletalMesh::CreateLocalBound(const FSkeletalMesh* InSkeletalMesh)
{
    TArray<FNormalVertex> Verts = InSkeletalMesh->Vertices;
    FVector Min = Verts[0].pos;
    FVector Max = Verts[0].pos;
    for (FNormalVertex Vertex : Verts)
    {
        FVector Pos = Vertex.pos;
        Min = Min.ComponentMin(Pos);
        Max = Max.ComponentMax(Pos);
    }
    LocalBound = FAABB(Min, Max);
}

void USkeletalMesh::ReleaseResources()
{
    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }
    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }
}

USkeletalMesh* USkeletalMesh::DuplicateForEditor(USkeletalMesh* Original, ID3D11Device* Device)
{
    if (!Original || !Original->GetSkeletalMeshAsset() || !Device)
    {
        UE_LOG("USkeletalMesh::DuplicateForEditor - Invalid original or device");
        return nullptr;
    }

    // NewObject 대신 new로 직접 생성 (ResourceManager 등록 방지)
    USkeletalMesh* Duplicate = new USkeletalMesh();

    // FSkeletalMesh* 포인터는 원본 공유 (Material Instance Dynamic 패턴)
    Duplicate->SkeletalMeshAsset = Original->SkeletalMeshAsset;

    // 메타데이터 복사
    Duplicate->FilePath = Original->FilePath;
    Duplicate->CacheFilePath = Original->CacheFilePath;
    Duplicate->VertexType = Original->VertexType;
    Duplicate->VertexStride = Original->VertexStride;
    Duplicate->VertexCount = Original->VertexCount;
    Duplicate->IndexCount = Original->IndexCount;
    Duplicate->LocalBound = Original->LocalBound;

    // GPU 버퍼만 독립적으로 생성 (격리를 위한 핵심)
    Duplicate->CreateVertexBuffer(Duplicate->SkeletalMeshAsset, Device, Duplicate->VertexType);
    Duplicate->CreateIndexBuffer(Duplicate->SkeletalMeshAsset, Device);

    UE_LOG("USkeletalMesh::DuplicateForEditor - Created duplicate %p (Original: %p, Shared FSkeletalMesh: %p)",
        Duplicate, Original, Duplicate->SkeletalMeshAsset);

    return Duplicate;
}
