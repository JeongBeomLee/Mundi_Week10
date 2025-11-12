#pragma once

class USkeletalMesh;
class UStaticMesh;
struct FSkeletalMesh;
struct FStaticMesh;
struct FNormalVertex;

class FFBXManager
{
public:
    FFBXManager();
    ~FFBXManager();

    static void Preload();
    static void Clear();

    // Skeletal Mesh 로딩
    static FSkeletalMesh* LoadFBXSkeletalMeshAsset(const FString& PathFileName);
    static USkeletalMesh* LoadFBXSkeletalMesh(const FString& PathFileName);

    // Static Mesh 로딩
    static FStaticMesh* LoadFBXStaticMeshAsset(const FString& PathFileName);
    static UStaticMesh* LoadFBXStaticMesh(const FString& PathFileName);

private:
    // Helper functions (Skeletal/Static Mesh 공통 사용)
    static void FindAllMeshesRecursive(FbxNode* FbxMeshNode, TArray<FbxMesh*>& OutMesh);
    static void ParseMeshGeometry(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, TArray<int>& OutVertexToControlPointMap);
    static void ParseBoneHierarchy(FbxScene* Scene, FSkeletalMesh* OutMeshData);
    static void ParseSkinWeights(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, const TArray<int>& VertexToControlPointMap, int StartVertexIndex, int VertexCount);
    static void LoadMaterials(FbxMesh* FbxMeshNode, TArray<FMaterialInfo>* OutMaterialInfos = nullptr);
    static void RegisterMaterialsFromInfos(const TArray<FMaterialInfo>& InMaterialInfos);

    // BindPose and Skeleton Hierarchy helpers
    static FbxPose* FindBindPose(FbxNode* SkeletonRoot);
    static void CollectSkeletonBoneNodes(FbxNode* Node, TArray<FbxNode*>& OutBoneNodes);
    static void FindSkeletonRootNodes(FbxNode* Node, TArray<FbxNode*>& OutSkeletonRoots);
    static bool IsSkeletonRootNode(FbxNode* Node);
    static void CollectBoneData(FbxNode* Node, FSkeletalMesh* OutMeshData, int32 ParentIndex, FbxPose* BindPose, TMap<FbxNode*, int32>& NodeToIndexMap);

    static TMap<FString, FSkeletalMesh*> FBXSkeletalMeshMap;
    static TMap<FString, FStaticMesh*> FBXStaticMeshMap;
};
