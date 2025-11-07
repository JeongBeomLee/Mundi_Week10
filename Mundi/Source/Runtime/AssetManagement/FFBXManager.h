#pragma once

class USkeletalMesh;
struct FSkeletalMesh;
struct FNormalVertex;

class FFBXManager
{
public:
    FFBXManager();
    ~FFBXManager();

    static void Preload();
    static void Clear();
    static FSkeletalMesh* LoadFBXSkeletalMeshAsset(const FString& PathFileName);
    static USkeletalMesh* LoadFBXSkeletalMesh(const FString& PathFileName);

private:
    // Helper functions for LoadFBXSkeletalMeshAsset
    static void ParseMeshGeometry(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, TArray<int>& OutVertexToControlPointMap);
    static void ParseBoneHierarchy(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData);
    static void ParseSkinWeights(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, const TArray<int>& VertexToControlPointMap);

    static TMap<FString, FSkeletalMesh*> FBXSkeletalMeshMap;
};
