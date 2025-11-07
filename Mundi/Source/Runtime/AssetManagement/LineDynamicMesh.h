#pragma once
#include "ResourceBase.h"
#include "VertexData.h"

class ULineDynamicMesh : public UResourceBase
{
public:
    DECLARE_CLASS(ULineDynamicMesh, UResourceBase)

    ULineDynamicMesh() = default;
    virtual ~ULineDynamicMesh() override;

    void Load(const FString& InFilePath, ID3D11Device* InDevice);

    void Load(uint32 MaxVertices, uint32 MaxIndices, ID3D11Device* InDevice);
    bool Initialize(uint32 MaxVertices, uint32 MaxIndices, ID3D11Device* InDevice);

    bool UpdateData(FMeshData* InData, ID3D11DeviceContext* InContext);

    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetCurrentVertexCount() const { return CurrentVertexCount; }
    uint32 GetCurrentIndexCount() const { return CurrentIndexCount; }
    uint32 GetMaxVertices() const { return MaxVertices; }
    uint32 GetMaxIndices() const { return MaxIndices; }
    bool IsInitialized() const { return bIsInitialized; }

private:
    void ReleaseResources();

    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;

    uint32 MaxVertices = 0;
    uint32 MaxIndices = 0;

    uint32 CurrentVertexCount = 0;
    uint32 CurrentIndexCount = 0;

    bool bIsInitialized = false;
    ID3D11Device* Device = nullptr;
};

