#include "pch.h"
#include "LineDynamicMesh.h"

IMPLEMENT_CLASS(ULineDynamicMesh)

ULineDynamicMesh::~ULineDynamicMesh()
{
    ReleaseResources();
}

void ULineDynamicMesh::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    Device = InDevice;
}

void ULineDynamicMesh::Load(uint32 InMaxVertices, uint32 InMaxIndices, ID3D11Device* InDevice)
{
    Initialize(InMaxVertices, InMaxIndices, InDevice);
}

bool ULineDynamicMesh::Initialize(uint32 InMaxVertices, uint32 InMaxIndices, ID3D11Device* InDevice)
{
    if (!InDevice)
        return false;

    ReleaseResources();

    Device = InDevice;
    MaxVertices = InMaxVertices;
    MaxIndices = InMaxIndices;

    const size_t vertexSize = sizeof(FVertexSimple);

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
    CurrentVertexCount = 0;
    CurrentIndexCount = 0;
    return true;
}

bool ULineDynamicMesh::UpdateData(FMeshData* InData, ID3D11DeviceContext* InContext)
{
    if (!bIsInitialized || !InData || !InContext)
        return false;

    uint32 vertexCount = static_cast<uint32>(InData->Vertices.size());
    uint32 indexCount = static_cast<uint32>(InData->Indices.size());

    if (vertexCount > MaxVertices || indexCount > MaxIndices)
        return false;

    CurrentVertexCount = vertexCount;
    CurrentIndexCount = indexCount;

    if (vertexCount == 0 || indexCount == 0)
        return true;

    D3D11_MAPPED_SUBRESOURCE mappedVertex;
    HRESULT hr = InContext->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertex);
    if (FAILED(hr))
        return false;

    FVertexSimple* dstVertices = static_cast<FVertexSimple*>(mappedVertex.pData);
    for (uint32 i = 0; i < vertexCount; ++i)
    {
        dstVertices[i].Position = InData->Vertices[i];
        dstVertices[i].Color = (i < InData->Color.size()) ? InData->Color[i] : FVector4(1, 1, 1, 1);
    }
    InContext->Unmap(VertexBuffer, 0);

    D3D11_MAPPED_SUBRESOURCE mappedIndex;
    hr = InContext->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIndex);
    if (FAILED(hr))
        return false;

    uint32* indices = static_cast<uint32*>(mappedIndex.pData);
    memcpy(indices, InData->Indices.data(), indexCount * sizeof(uint32));
    InContext->Unmap(IndexBuffer, 0);

    return true;
}

void ULineDynamicMesh::ReleaseResources()
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

