#pragma once
#include "ResourceBase.h"

class USkeletalMesh : public UResourceBase
{
public:
    DECLARE_CLASS(USkeletalMesh, UResourceBase)

    USkeletalMesh() = default;
    virtual ~USkeletalMesh() override;

    void Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColorTexturNormal);
    void Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColorTexturNormal);

    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetIndexCount() const { return IndexCount; }
    void SetVertexType(EVertexLayoutType InVertexLayoutType);
    EVertexLayoutType GetVertexType() const { return VertexType; }
    void SetIndexCount(uint32 Cnt) { IndexCount = Cnt; }
    uint32 GetVertexStride() const { return VertexStride; };

	const FString& GetAssetPathFileName() const { return SkeletalMeshAsset ? SkeletalMeshAsset->PathFileName : FilePath; }
    void SetSkeletalMeshAsset(FSkeletalMesh* InSkeletalMesh) { SkeletalMeshAsset = InSkeletalMesh; }
	FSkeletalMesh* GetSkeletalMeshAsset() const { return SkeletalMeshAsset; }

    const TArray<FGroupInfo>& GetMeshGroupInfo() const { return SkeletalMeshAsset->GroupInfos; }
    bool HasMaterial() const { return SkeletalMeshAsset->bHasMaterial; }

    uint64 GetMeshGroupCount() const { return SkeletalMeshAsset->GroupInfos.size(); }
    
    FAABB GetLocalBound() const {return LocalBound; }
    
    // bool EraseUsingComponets(USkeletalMeshComponent* InSkeletalMeshComponent);
    // bool AddUsingComponents(USkeletalMeshComponent* InSkeletalMeshComponent);

    // TArray<USkeletalMeshComponent*>& GetUsingComponents()
    // {
    //     return UsingComponents;
    // }

    const FString& GetCacheFilePath() const { return CacheFilePath; }

private:
    void CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, EVertexLayoutType InVertexType);
	void CreateVertexBuffer(FSkeletalMesh* InSkeletalMesh, ID3D11Device* InDevice, EVertexLayoutType InVertexType);
    void CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice);
	void CreateIndexBuffer(FSkeletalMesh* InSkeletalMesh, ID3D11Device* InDevice);
    void CreateLocalBound(const FMeshData* InMeshData);
    void CreateLocalBound(const FSkeletalMesh* InSkeletalMesh);
    void ReleaseResources();

    FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube.obj.bin)

    // GPU 리소스
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수 
    uint32 VertexStride = 0;
    EVertexLayoutType VertexType = EVertexLayoutType::PositionColorTexturNormal;  // Stride를 계산하기 위한 버텍스 타입

	// CPU 리소스
    FSkeletalMesh* SkeletalMeshAsset = nullptr;

    // 메시 단위 BVH (ResourceManager에서 캐싱, 소유)
    // 초기화되지 않는 멤버변수 (참조도 ResourceManager에서만 이루어짐) 
    // FMeshBVH* MeshBVH = nullptr;

    // 로컬 AABB. (스태틱메시 액터 전체 경계 계산에 사용. SkeletalMeshAsset 로드할 때마다 갱신)
    FAABB LocalBound;
    
    //TArray<USkeletalMeshComponent*> UsingComponents; // 유저에 의해 Material이 안 바뀐 이 Mesh를 사용 중인 Component들(render state sorting 위함)
};
