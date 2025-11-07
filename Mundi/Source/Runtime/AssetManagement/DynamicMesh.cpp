#include "pch.h"

IMPLEMENT_CLASS(UDynamicMesh)

UDynamicMesh::~UDynamicMesh()
{
    ReleaseResources();
}

void UDynamicMesh::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    //Temp Methods
}

void UDynamicMesh::Load(uint32 InMaxVertices, uint32 InMaxIndices, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    Initialize(InMaxVertices, InMaxIndices, InDevice, InVertexType);
}

bool UDynamicMesh::Initialize(uint32 InMaxVertices, uint32 InMaxIndices, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    if (!InDevice)
        return false;

    ReleaseResources();

    Device = InDevice;
    MaxVertices = InMaxVertices;
    MaxIndices = InMaxIndices;
    VertexType = InVertexType;
    
    size_t vertexSize = GetVertexSize(InVertexType);
    
    // Create dynamic vertex buffer
    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc.ByteWidth = static_cast<UINT>(MaxVertices * vertexSize);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vertexBufferDesc.MiscFlags = 0;

    HRESULT hr = Device->CreateBuffer(&vertexBufferDesc, nullptr, &VertexBuffer);
    if (FAILED(hr))
    {
        ReleaseResources();
        return false;
    }

    // Create dynamic index buffer
    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    indexBufferDesc.ByteWidth = MaxIndices * sizeof(uint32);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    indexBufferDesc.MiscFlags = 0;

    hr = Device->CreateBuffer(&indexBufferDesc, nullptr, &IndexBuffer);
    if (FAILED(hr))
    {
        ReleaseResources();
        return false;
    }

    bIsInitialized = true;
    Clear();
    
    return true;
}

bool UDynamicMesh::UpdateData(FMeshData* InData, ID3D11DeviceContext* InContext)
{
    if (!bIsInitialized || !InData || !InContext)
        return false;

    uint32 vertexCount = static_cast<uint32>(InData->Vertices.size());
    uint32 indexCount = static_cast<uint32>(InData->Indices.size());

    // Check bounds
    if (vertexCount > MaxVertices || indexCount > MaxIndices)
        return false;

    CurrentVertexCount = vertexCount;
    CurrentIndexCount = indexCount;

    if (vertexCount == 0 || indexCount == 0)
        return true; // Nothing to update

    size_t vertexSize = GetVertexSize(VertexType);

    // Update vertex buffer
    D3D11_MAPPED_SUBRESOURCE mappedVertex;
    HRESULT hr = InContext->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertex);
    if (FAILED(hr))
        return false;

    // Copy vertex data based on vertex type
    if (VertexType == EVertexLayoutType::PositionColor)
    {
        FVertexSimple* vertices = static_cast<FVertexSimple*>(mappedVertex.pData);
        for (uint32 i = 0; i < vertexCount; ++i)
        {
            vertices[i].Position = InData->Vertices[i];
            vertices[i].Color = InData->Color[i];
        }
    }
    // Add other vertex types as needed

    InContext->Unmap(VertexBuffer, 0);

    // Update index buffer
    D3D11_MAPPED_SUBRESOURCE mappedIndex;
    hr = InContext->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIndex);
    if (FAILED(hr))
        return false;

    uint32* indices = static_cast<uint32*>(mappedIndex.pData);
    memcpy(indices, InData->Indices.data(), indexCount * sizeof(uint32));

    InContext->Unmap(IndexBuffer, 0);

    return true;
}

void UDynamicMesh::Clear()
{
    CurrentVertexCount = 0;
    CurrentIndexCount = 0;
}

void UDynamicMesh::ReleaseResources()
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
    
    bIsInitialized = false;
    Device = nullptr;
}

size_t UDynamicMesh::GetVertexSize(EVertexLayoutType InVertexType)
{
    switch (InVertexType)
    {
        case EVertexLayoutType::PositionColor:
            return sizeof(FVertexSimple);
        case EVertexLayoutType::PositionBillBoard:
            return sizeof(FBillboardVertexInfo);
        // Add other vertex types as needed
        default:
            return sizeof(FVertexSimple);
    }
}
