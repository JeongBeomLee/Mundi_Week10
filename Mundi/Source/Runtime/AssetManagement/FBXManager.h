#pragma once

class USkeletalMesh;

class FBXManager
{
public:
    FBXManager();
    ~FBXManager();
    
    // FBX 파일 로드 
    bool LoadFbxFile(const FString& InFilePath); 

    static void Preload();
    static void Clear();
    static FSkeletalMesh* LoadFBXSkeletalMeshAsset(const FString& PathFileName);
    static USkeletalMesh* LoadFBXSkeletalMesh(const FString& PathFileName);
private:
    static TMap<FString, FSkeletalMesh*> ObjStaticMeshMap;
};
