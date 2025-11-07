#pragma once
#include "ResourceBase.h"

// Dynamic mesh for efficient batch rendering
// Uses D3D11_USAGE_DYNAMIC buffers that can be updated each frame without recreation
class UDynamicMesh : public UResourceBase
{
public:
    DECLARE_CLASS(UDynamicMesh, UResourceBase)

    UDynamicMesh() = default;
    virtual ~UDynamicMesh() override;

    void Load(const FString& InFilePath, ID3D11Device* InDevice); //Temp
    void Load(uint32 MaxVertices, uint32 MaxIndices, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColor);
    // Initialize dynamic buffers with maximum capacity
    bool Initialize(uint32 MaxVertices, uint32 MaxIndices, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColor);
    
    // Update buffer data (call each frame)
    bool UpdateData(FMeshData* InData, ID3D11DeviceContext* InContext);
    
    // Clear current data
    void Clear();
    
    // Getters
    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetCurrentVertexCount() const { return CurrentVertexCount; }
    uint32 GetCurrentIndexCount() const { return CurrentIndexCount; }
    uint32 GetMaxVertices() const { return MaxVertices; }
    uint32 GetMaxIndices() const { return MaxIndices; }
    bool IsInitialized() const { return bIsInitialized; }

private:
    void ReleaseResources();
    size_t GetVertexSize(EVertexLayoutType InVertexType);
    
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    
    uint32 MaxVertices = 0;          // Maximum number of vertices
    uint32 MaxIndices = 0;           // Maximum number of indices
    
    uint32 CurrentVertexCount = 0;   // Current vertices in use
    uint32 CurrentIndexCount = 0;    // Current indices in use
    
    bool bIsInitialized = false;
    EVertexLayoutType VertexType = EVertexLayoutType::PositionColor;
    ID3D11Device* Device = nullptr;
};
