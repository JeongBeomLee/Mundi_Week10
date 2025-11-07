#include "pch.h"
#include "FBXManager.h"
#include "PathUtils.h"
#include "ObjectIterator.h"
#include "SkeletalMesh.h"

FBXManager::FBXManager()
{
}

FBXManager::~FBXManager()
{
}

bool FBXManager::LoadFbxFile(const FString& InFilePath)
{
    
}

void FBXManager::Preload()
{
    const fs::path DataDir(GDataDir);

    if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
    {
        UE_LOG("FBXManager::Preload: Data directory not found: %s", DataDir.string().c_str());
        return;
    }

    size_t LoadedCount = 0;
    std::unordered_set<FString> ProcessedFiles; // 중복 로딩 방지

    for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
    {
        if (!Entry.is_regular_file())
            continue;

        const fs::path& Path = Entry.path();
        FString Extension = Path.extension().string();
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (Extension == ".fbx")
        {
            FString PathStr = NormalizePath(Path.string());

            // 이미 처리된 파일인지 확인
            if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
            {
                ProcessedFiles.insert(PathStr);
                LoadFBXSkeletalMesh(PathStr);
                ++LoadedCount;
            }
        }
        else if (Extension == ".dds" || Extension == ".jpg" || Extension == ".png")
        {
            UResourceManager::GetInstance().Load<UTexture>(Path.string()); // 데칼 텍스쳐를 ui에서 고를 수 있게 하기 위해 임시로 만듬.
        }
    }

    // 4) 모든 StaticMeshs 가져오기
    RESOURCE.SetStaticMeshs();

    UE_LOG("FObjManager::Preload: Loaded %zu .obj files from %s", LoadedCount, DataDir.string().c_str());
}

void FBXManager::Clear()
{
    for (auto& Pair : ObjStaticMeshMap)
    {
        delete Pair.second;
    }

    ObjStaticMeshMap.Empty();
}

FSkeletalMesh* FBXManager::LoadFBXSkeletalMeshAsset(const FString& PathFileName)
{
}

USkeletalMesh* FBXManager::LoadFBXSkeletalMesh(const FString& PathFileName)
{
    // 0) 경로
    FString NormalizedPathStr = NormalizePath(PathFileName);

    // 1) 이미 로드된 FSkeletalMesh가 있는지 전체 검색 (정규화된 경로로 비교)
    for (TObjectIterator<USkeletalMesh> It; It; ++It)
    {
        USkeletalMesh* SkeletalMesh = *It;

        if (SkeletalMesh->GetFilePath() == NormalizedPathStr)
        {
            return SkeletalMesh;
        }
    }

    // 2) 없으면 새로 로드 (정규화된 경로 사용)
    USkeletalMesh* SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(NormalizedPathStr);

    UE_LOG("UStaticMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
    return SkeletalMesh;
}
