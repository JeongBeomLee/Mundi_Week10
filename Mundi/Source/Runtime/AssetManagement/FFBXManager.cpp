/*
 * FFBXManager.cpp
 *
 * FBX 파일에서 Skeletal Mesh를 로드하고 파싱하는 매니저
 *
 * 주요 기능:
 * - FBX SDK를 사용한 스켈레탈 메시 로딩
 * - Vertex, Index, Material 정보 파싱
 * - Bone 계층 구조 파싱
 * - Skin Weight (본 가중치) 파싱
 * - Vertex Deduplication (중복 정점 제거)
 * - 로드된 메시 캐싱 (중복 로딩 방지)
 */

#include "pch.h"
#include "FFBXManager.h"
#include "PathUtils.h"
#include "ObjectIterator.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "Material.h"
#include "ResourceManager.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"

using namespace fbxsdk;

// ========================================
// FVertexKey: Index-based Vertex Deduplication
// ========================================
// FBX 데이터의 실제 인덱스를 기반으로 정점을 식별합니다.
// Float 값 직접 비교 대신 인덱스 기반으로 중복을 판단하여 부동소수점 오차 문제를 해결합니다.
struct FVertexKey
{
    int32 PositionIndex;    // ControlPoint 인덱스
    int32 NormalIndex;      // Normal 인덱스
    int32 TangentIndex;     // Tangent 인덱스
    int32 UVIndex;          // UV 인덱스
    int32 ColorIndex;       // Color 인덱스

    FVertexKey(int32 Pos, int32 N, int32 T, int32 UV, int32 Col)
        : PositionIndex(Pos)
        , NormalIndex(N)
        , TangentIndex(T)
        , UVIndex(UV)
        , ColorIndex(Col)
    {
        // 해시 값 미리 계산 (성능 최적화)
        Hash = std::hash<int32>()(PositionIndex << 0)
             ^ std::hash<int32>()(NormalIndex   << 1)
             ^ std::hash<int32>()(TangentIndex  << 2)
             ^ std::hash<int32>()(UVIndex       << 3)
             ^ std::hash<int32>()(ColorIndex    << 4);
    }

    bool operator==(const FVertexKey& Other) const
    {
        return PositionIndex == Other.PositionIndex
            && NormalIndex   == Other.NormalIndex
            && TangentIndex  == Other.TangentIndex
            && UVIndex       == Other.UVIndex
            && ColorIndex    == Other.ColorIndex;
    }

    size_t GetHash() const { return Hash; }

private:
    size_t Hash;
};

// std::hash 특수화
namespace std
{
    template<>
    struct hash<FVertexKey>
    {
        size_t operator()(const FVertexKey& Key) const
        {
            return Key.GetHash();
        }
    };
}

// ========================================
// Helper Functions for LayerElement Parsing
// ========================================
// GetVertexElementData: FBX LayerElement의 MappingMode와 ReferenceMode를 정확히 처리합니다.
// 이는 Normal, Tangent, UV, Color 등 모든 LayerElement에 대해 올바른 데이터 추출을 보장합니다.
template<typename FbxLayerElementType, typename TDataType>
bool GetVertexElementData(const FbxLayerElementType* Element, int32 ControlPointIndex, int32 VertexIndex, TDataType& OutData)
{
    if (!Element)
    {
        return false;
    }

    const auto MappingMode = Element->GetMappingMode();
    const auto ReferenceMode = Element->GetReferenceMode();

    // 1) eAllSame: 모든 정점이 같은 값
    if (MappingMode == FbxLayerElement::eAllSame)
    {
        if (Element->GetDirectArray().GetCount() > 0)
        {
            OutData = Element->GetDirectArray().GetAt(0);
            return true;
        }
        return false;
    }

    // 2) 인덱스 결정 (eByControlPoint, eByPolygonVertex만 처리)
    int32 Index = -1;
    if (MappingMode == FbxLayerElement::eByControlPoint)
    {
        Index = ControlPointIndex;
    }
    else if (MappingMode == FbxLayerElement::eByPolygonVertex)
    {
        Index = VertexIndex;
    }
    else
    {
        // eByPolygon, eByEdge 등 필요시 추가
        return false;
    }

    // 3) ReferenceMode별 분리 처리
    if (ReferenceMode == FbxLayerElement::eDirect)
    {
        // DirectArray 크기만 검사
        if (Index >= 0 && Index < Element->GetDirectArray().GetCount())
        {
            OutData = Element->GetDirectArray().GetAt(Index);
            return true;
        }
    }
    else if (ReferenceMode == FbxLayerElement::eIndexToDirect)
    {
        // IndexArray, DirectArray 순차 검사
        if (Index >= 0 && Index < Element->GetIndexArray().GetCount())
        {
            int32 DirectIndex = Element->GetIndexArray().GetAt(Index);
            if (DirectIndex >= 0 && DirectIndex < Element->GetDirectArray().GetCount())
            {
                OutData = Element->GetDirectArray().GetAt(DirectIndex);
                return true;
            }
        }
    }

    return false;
}

// ========================================
// Static Member Definition
// ========================================

// 로드된 FSkeletalMesh를 경로별로 캐싱하는 맵
// Key: 정규화된 파일 경로, Value: FSkeletalMesh 포인터
TMap<FString, FSkeletalMesh*> FFBXManager::FBXSkeletalMeshMap;
TMap<FString, FStaticMesh*> FFBXManager::FBXStaticMeshMap;

FFBXManager::FFBXManager()
{
}

FFBXManager::~FFBXManager()
{
    Clear();
}

/*
 * Preload()
 *
 * Data 디렉토리의 모든 FBX 파일을 재귀적으로 검색하여 미리 로드합니다.
 * 게임 시작 시 호출되어 로딩 시간을 단축시킵니다.
 */
void FFBXManager::Preload()
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
                LoadFBXStaticMesh(PathStr);
                ++LoadedCount;
            }
        }
        else if (Extension == ".dds" || Extension == ".jpg" || Extension == ".png")
        {
            UResourceManager::GetInstance().Load<UTexture>(Path.string()); // 데칼 텍스쳐를 ui에서 고를 수 있게 하기 위해 임시로 만듬.
        }
    }

    // 4) 모든 SkeletalMesh 가져오기
    RESOURCE.SetSkeletalMeshs();

    UE_LOG("FObjManager::Preload: Loaded %zu .obj files from %s", LoadedCount, DataDir.string().c_str());
}

/*
 * Clear()
 *
 * 로드된 모든 FSkeletalMesh 메모리를 해제하고 캐시를 비웁니다.
 * 프로그램 종료 시 호출됩니다.
 */
void FFBXManager::Clear()
{
    // 맵에 저장된 모든 FSkeletalMesh 메모리 해제
    for (auto& Pair : FBXSkeletalMeshMap)
    {
        delete Pair.second;
    }
    FBXSkeletalMeshMap.Empty();

    // 맵에 저장된 모든 FStaticMesh 메모리 해제
    for (auto& Pair : FBXStaticMeshMap)
    {
        delete Pair.second;
    }
    FBXStaticMeshMap.Empty();
}

/*
 * LoadFBXSkeletalMeshAsset()
 *
 * FBX 파일에서 FSkeletalMesh 데이터를 로드합니다.
 *
 * 처리 과정:
 * 1. 캐시 확인 (이미 로드된 파일인지 검사)
 * 2. FBX SDK 초기화 (Manager, IOSettings, Importer)
 * 3. FBX 파일 Import 및 Scene 생성
 * 4. 좌표계 변환 (Z-up, Left-handed)
 * 5. 단위 변환 (cm로 통일)
 * 6. Triangulation (모든 폴리곤을 삼각형으로 변환)
 * 7. 메시 파싱 (Geometry, Bones, Skin Weights)
 * 8. 캐시에 저장 후 반환
 *
 * @param PathFileName FBX 파일 경로
 * @return 파싱된 FSkeletalMesh 포인터 (실패 시 nullptr)
 */
FSkeletalMesh* FFBXManager::LoadFBXSkeletalMeshAsset(const FString& PathFileName)
{
    auto RegisterMaterialInfos = [](const TArray<FMaterialInfo>& Infos)
    {
        UResourceManager& ResourceManager = UResourceManager::GetInstance();
        UMaterial* DefaultMaterial = ResourceManager.GetDefaultMaterial();
        UShader* DefaultShader = DefaultMaterial ? DefaultMaterial->GetShader() : nullptr;
        const TArray<FShaderMacro>* DefaultMacros = DefaultMaterial ? &DefaultMaterial->GetShaderMacros() : nullptr;

        for (const FMaterialInfo& Info : Infos)
        {
            if (Info.MaterialName.empty() || ResourceManager.Get<UMaterial>(Info.MaterialName))
            {
                continue;
            }

            UMaterial* Material = NewObject<UMaterial>();
            Material->SetMaterialInfo(Info);
            if (DefaultMacros)
            {
                Material->SetShaderMacros(*DefaultMacros);
            }
            if (DefaultShader)
            {
                Material->SetShader(DefaultShader);
            }

            ResourceManager.Add<UMaterial>(Info.MaterialName, Material);
        }
    };
    
    FString NormalizedPathStr = NormalizePath(PathFileName);

    // 1. 메모리 캐시 확인: 이미 로드된 에셋이 있으면 즉시 반환
    if (FSkeletalMesh** It = FBXSkeletalMeshMap.Find(NormalizedPathStr))
    {
        return *It;
    }

    // 변수 선언 (USE_OBJ_CACHE 유무와 관계없이 필요)
    FSkeletalMesh* SkeletalMeshData = nullptr;
    TArray<FMaterialInfo> MaterialInfos;
    bool bLoadedFromCache = false;

#ifdef USE_OBJ_CACHE
    // 캐시 경로 생성
    FString CachePathStr = ConvertDataPathToCachePath(NormalizedPathStr);
    const FString BinPathFileName = CachePathStr + ".sk.bin";
    const FString MatBinPathFileName = CachePathStr + ".sk.mat.bin";

    // 캐시 디렉토리 없을 시 캐시 디렉토리 생성
    fs::path CacheFileDirPath(BinPathFileName);
    if (CacheFileDirPath.has_parent_path())
    {
        fs::create_directories(CacheFileDirPath.parent_path());
    }

    bool bCacheExists = fs::exists(BinPathFileName) && fs::exists(MatBinPathFileName);
    bool bCacheisNewer = false;

    // 캐시가 최신인지 확인
    if (bCacheExists)
    {
        // FBX 원본이 없으면 캐시 무조건 사용
        if (!fs::exists(NormalizedPathStr))
        {
            bCacheisNewer = true;
        }
        else
        {
            try
            {
                auto binTime = fs::last_write_time(BinPathFileName);
                auto fbxTime = fs::last_write_time(NormalizedPathStr);
                bCacheisNewer = (binTime > fbxTime);
            }
            catch (...)
            {
                bCacheisNewer = false;
            }
        }
    }

    if (bCacheisNewer)
    {
        UE_LOG("Loading FBX skeletal mesh from cache: %s", NormalizedPathStr.c_str());
        try
        {
            SkeletalMeshData = new FSkeletalMesh();

            FWindowsBinReader Reader(BinPathFileName);
            if (!Reader.IsOpen()) throw std::runtime_error("Failed to open bin");
            Reader << *SkeletalMeshData;
            Reader.Close();

            FWindowsBinReader MatReader(MatBinPathFileName);
            if (!MatReader.IsOpen()) throw std::runtime_error("Failed to open mat bin");
            Serialization::ReadArray<FMaterialInfo>(MatReader, MaterialInfos);

            RegisterMaterialInfos(MaterialInfos);
            
            MatReader.Close();

            // 캐시에서 로드한 MaterialInfos로 UMaterial 객체 생성 및 등록
            RegisterMaterialsFromInfos(MaterialInfos);

            SkeletalMeshData->CacheFilePath = BinPathFileName;
            bLoadedFromCache = true;
            UE_LOG("Successfully loaded skeletal mesh from cache");
        }
        catch (const std::exception& e)
        {
            UE_LOG("Cache load failed: %s. Regenerating...", e.what());
            delete SkeletalMeshData;
            SkeletalMeshData = nullptr;
            fs::remove(BinPathFileName);
            fs::remove(MatBinPathFileName);
        }
    }
#endif

    // 캐시 로드 실패 시 fbx 파싱 (USE_OBJ_CACHE 없으면 무조건 파싱)
    if (!bLoadedFromCache)
    {
        // 2. FBX SDK 초기화
        FbxManager* SdkManager = FbxManager::Create();
        if (!SdkManager)
        {
            UE_LOG("FBXManager: Failed to create FbxManager!");
            return nullptr;
        }

        FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
        SdkManager->SetIOSettings(ios);

        // Material 및 Texture 로딩 활성화
        ios->SetBoolProp(IMP_FBX_MATERIAL, true);
        ios->SetBoolProp(IMP_FBX_TEXTURE, true);

        // 3. FBX Importer 생성
        FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
        if (!Importer->Initialize(NormalizedPathStr.c_str(), -1, SdkManager->GetIOSettings()))
        {
            UE_LOG("FBXManager: Failed to initialize importer for %s", NormalizedPathStr.c_str());
            UE_LOG("  Error: %s", Importer->GetStatus().GetErrorString());
            Importer->Destroy();
            SdkManager->Destroy();
            return nullptr;
        }

        // Scene 생성 및 임포트
        FbxScene* Scene = FbxScene::Create(SdkManager, "TempScene");
        if (!Importer->Import(Scene))
        {
            UE_LOG("FBXManager: Failed to import FBX file: %s", NormalizedPathStr.c_str());
            Importer->Destroy();
            Scene->Destroy();
            SdkManager->Destroy();
            return nullptr;
        }
        Importer->Destroy();

        // 4. 좌표계 변환 (Z-up, X-forward, Left-handed - Unreal Engine 스타일)
        FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();
        FbxAxisSystem OurAxisSystem(
            FbxAxisSystem::eZAxis,
            FbxAxisSystem::eParityEven,
            FbxAxisSystem::eLeftHanded);

        if (SceneAxisSystem != OurAxisSystem)
        {
            OurAxisSystem.DeepConvertScene(Scene);
        }

        // 5. 단위 변환 (cm로 통일)
        FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
        if (SceneSystemUnit.GetScaleFactor() != 1.0)
        {
            FbxSystemUnit::cm.ConvertScene(Scene);
        }

        // 6. Triangulate (모든 폴리곤을 삼각형으로 변환)
        FbxGeometryConverter GeometryConverter(SdkManager);
        GeometryConverter.Triangulate(Scene, true);

        // 7. FSkeletalMesh 생성
        SkeletalMeshData = new FSkeletalMesh();
        SkeletalMeshData->PathFileName = NormalizedPathStr;

        TArray<FbxMesh*> AllMeshes;
        FindAllMeshesRecursive(Scene->GetRootNode(), AllMeshes);

        if (AllMeshes.IsEmpty())
        {
            UE_LOG("FBXManager: No mesh found in FBX file: %s", NormalizedPathStr.c_str());
            Scene->Destroy();
            SdkManager->Destroy();
            return nullptr;
        }
        UE_LOG("FBXManager: Loading %s (%zu mesh parts found)",
                   NormalizedPathStr.c_str(), AllMeshes.size());

        // 본 계층 구조는 첫 번째 스킨 디포머가 있는 메시에서 파싱
        FbxMesh* FirstSkinnedMesh = nullptr;
        for (FbxMesh* Mesh : AllMeshes)
        {
            if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
            {
                FirstSkinnedMesh = Mesh;
                UE_LOG("FBXManager: Found first skinned mesh for bone hierarchy: %s",
                       Mesh->GetNode() ? Mesh->GetNode()->GetName() : "");
                break;
            }
        }

        UE_LOG("FBXManager: Loading %s", NormalizedPathStr.c_str());

        // 8. 본 계층 구조 파싱 (한 번만)
        if (FirstSkinnedMesh)
        {
            ParseBoneHierarchy(FirstSkinnedMesh, SkeletalMeshData);
        }

        // 9. 각 메시마다 정점 데이터와 스킨 가중치를 파싱
        for (FbxMesh* Mesh : AllMeshes)
        {
            TArray<int> VertexToControlPointMap; // 각 메시마다 로컬 맵 생성

            // 이 메시의 정점 시작 인덱스 저장
            int StartVertexIndex = static_cast<int>(SkeletalMeshData->Vertices.size());

            ParseMeshGeometry(Mesh, SkeletalMeshData, VertexToControlPointMap);

            // 추가된 정점 개수 계산
            int VertexCount = static_cast<int>(SkeletalMeshData->Vertices.size()) - StartVertexIndex;

            // 이 메시에 스킨 디포머가 있으면 가중치 파싱
            if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
            {
                ParseSkinWeights(Mesh, SkeletalMeshData, VertexToControlPointMap, StartVertexIndex, VertexCount);
            }
        }
        LoadMaterials(AllMeshes[0], &MaterialInfos);

        RegisterMaterialInfos(MaterialInfos);

        UE_LOG("FBXManager: Successfully loaded skeletal mesh");
        UE_LOG("  Vertices: %zu", SkeletalMeshData->Vertices.size());
        UE_LOG("  Indices: %zu", SkeletalMeshData->Indices.size());
        UE_LOG("  Bones: %zu", SkeletalMeshData->Bones.size());

        // 본이 없으면 스켈레탈 메시가 아님
        if (SkeletalMeshData->Bones.empty())
        {
            UE_LOG("FBXManager: ERROR - No bones found in FBX. This is a static mesh, not a skeletal mesh.");
            UE_LOG("FBXManager: Use LoadFBXStaticMeshAsset() instead for file: %s", NormalizedPathStr.c_str());

            Scene->Destroy();
            SdkManager->Destroy();
            delete SkeletalMeshData;
            return nullptr;
        }

        // 9. FBX SDK 리소스 정리
        Scene->Destroy();
        SdkManager->Destroy();

#ifdef USE_OBJ_CACHE
        // 파싱 완료 후 캐시에 저장
        try {
            FWindowsBinWriter Writer(BinPathFileName);
            Writer << *SkeletalMeshData;
            Writer.Close();

            FWindowsBinWriter MatWriter(MatBinPathFileName);
            Serialization::WriteArray<FMaterialInfo>(MatWriter, MaterialInfos);
            MatWriter.Close();

            SkeletalMeshData->CacheFilePath = BinPathFileName;
            UE_LOG("FBXManager: Skeletal mesh cache saved: %s", BinPathFileName.c_str());
        }
        catch (const std::exception& e) {
            UE_LOG("FBXManager: Failed to save skeletal mesh cache: %s", e.what());
        }
#endif
    }

    // 10. 캐시에 저장하여 메모리 관리
    FBXSkeletalMeshMap.Add(NormalizedPathStr, SkeletalMeshData);

    return SkeletalMeshData;
}


USkeletalMesh* FFBXManager::LoadFBXSkeletalMesh(const FString& PathFileName)
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

// ========================================
// Helper Functions
// ========================================


/*
 * FindAllMeshesRecursive()
 *
 * FbxNode를 재귀적으로 탐색하여 씬(Scene)에 존재하는 모든 FbxMesh를 찾습니다.
 *
 * @param Node 현재 탐색 중인 FbxNode
 * @param OutMeshes 찾은 FbxMesh 포인터를 담을 배열
 */
void FFBXManager::FindAllMeshesRecursive(FbxNode* FbxMeshNode, TArray<FbxMesh*>& OutMesh)
{
    if (!FbxMeshNode)
    {
        return;
    }

    if (FbxMesh* Mesh = FbxMeshNode->GetMesh())
    {
        OutMesh.Add(Mesh);
    }

    for (int i = 0; i < FbxMeshNode->GetChildCount(); ++i)
    {
        FindAllMeshesRecursive(FbxMeshNode->GetChild(i), OutMesh);
    }
}

/*
 * FindBindPose()
 *
 * 스켈레톤 루트 노드에 대한 BindPose를 찾습니다.
 * BindPose는 메시가 본에 바인딩될 때의 정확한 초기 변환 상태를 저장합니다.
 */
FbxPose* FFBXManager::FindBindPose(FbxNode* SkeletonRoot)
{
    if (!SkeletonRoot)
    {
        return nullptr;
    }

    FbxScene* Scene = SkeletonRoot->GetScene();
    if (!Scene)
    {
        return nullptr;
    }

    // 스켈레톤에 속한 모든 본 노드를 수집
    TArray<FbxNode*> SkeletonBones;
    CollectSkeletonBoneNodes(SkeletonRoot, SkeletonBones);

    const int32 PoseCount = Scene->GetPoseCount();
    for (int32 PoseIndex = 0; PoseIndex < PoseCount; PoseIndex++)
    {
        FbxPose* CurrentPose = Scene->GetPose(PoseIndex);
        if (!CurrentPose || !CurrentPose->IsBindPose())
        {
            continue;
        }

        // 이 바인드 포즈가 스켈레톤의 일부 본을 포함하는지 확인
        bool bPoseContainsSomeBones = false;
        int32 NodeCount = CurrentPose->GetCount();

        for (int32 NodeIndex = 0; NodeIndex < NodeCount; NodeIndex++)
        {
            FbxNode* Node = CurrentPose->GetNode(NodeIndex);
            if (SkeletonBones.Contains(Node))
            {
                bPoseContainsSomeBones = true;
                break;
            }
        }

        // 이 스켈레톤에 바인드 포즈가 적어도 하나의 본을 포함하면 반환
        if (bPoseContainsSomeBones)
        {
            return CurrentPose;
        }
    }

    return nullptr; // 해당 스켈레톤에 관련된 바인드 포즈 없음
}

/*
 * CollectSkeletonBoneNodes()
 *
 * 노드와 그 자식들을 재귀적으로 탐색하여 모든 본 노드를 수집합니다.
 */
void FFBXManager::CollectSkeletonBoneNodes(FbxNode* Node, TArray<FbxNode*>& OutBoneNodes)
{
    if (!Node)
    {
        return;
    }

    // 본 노드인지 확인
    if (Node->GetNodeAttribute() &&
        Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        OutBoneNodes.Add(Node);
    }

    // 자식 노드들에 대해 재귀적으로 처리
    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ChildIndex++)
    {
        FbxNode* ChildNode = Node->GetChild(ChildIndex);
        CollectSkeletonBoneNodes(ChildNode, OutBoneNodes);
    }
}

/*
 * FindSkeletonRootNodes()
 *
 * 씬에서 모든 스켈레톤 루트 노드를 찾습니다.
 */
void FFBXManager::FindSkeletonRootNodes(FbxNode* Node, TArray<FbxNode*>& OutSkeletonRoots)
{
    if (IsSkeletonRootNode(Node))
    {
        OutSkeletonRoots.Add(Node);
        return; // 이미 루트로 식별된 노드 아래는 더 탐색하지 않음
    }

    // 자식 노드들 재귀적으로 탐색
    for (int i = 0; i < Node->GetChildCount(); i++)
    {
        FindSkeletonRootNodes(Node->GetChild(i), OutSkeletonRoots);
    }
}

/*
 * IsSkeletonRootNode()
 *
 * 노드가 스켈레톤 루트인지 확인합니다.
 * 부모가 없거나 부모가 스켈레톤이 아닌 경우에만 루트로 간주합니다.
 */
bool FFBXManager::IsSkeletonRootNode(FbxNode* Node)
{
    if (!Node)
    {
        return false;
    }

    FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
    if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        // 부모가 없거나 부모가 스켈레톤이 아닌 경우에만 루트로 간주
        FbxNode* Parent = Node->GetParent();
        if (Parent == nullptr || Parent->GetNodeAttribute() == nullptr ||
            Parent->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton)
        {
            return true;
        }
    }
    return false;
}

/*
 * CollectBoneData()
 *
 * 본 노드를 재귀적으로 순회하며 계층 구조와 BindPose 기반 변환을 계산합니다.
 * 이는 FFbxLoader의 정확한 방식을 따릅니다.
 */
void FFBXManager::CollectBoneData(FbxNode* Node, FSkeletalMesh* OutMeshData, int32 ParentIndex, FbxPose* BindPose, TMap<FbxNode*, int32>& NodeToIndexMap)
{
    if (!Node)
    {
        return;
    }

    FString BoneName = Node->GetName();
    const int32 CurrentIndex = static_cast<int32>(OutMeshData->Bones.size());

    // 노드 인덱스 맵에 추가 (빠른 검색용)
    NodeToIndexMap.Add(Node, CurrentIndex);

    FBoneInfo BoneInfo;
    BoneInfo.BoneName = BoneName;
    BoneInfo.ParentIndex = ParentIndex;

    // BindPose에서 변환 행렬 가져오기
    int32 PoseNodeIndex = -1;
    if (BindPose)
    {
        PoseNodeIndex = BindPose->Find(Node);
    }

    // 로컬 변환 행렬 계산
    FbxAMatrix LocalMatrix;
    FbxAMatrix GlobalMatrix;

    if (PoseNodeIndex != -1)
    {
        // BindPose에서 글로벌 행렬 가져오기
        FbxMatrix NodeMatrix = BindPose->GetMatrix(PoseNodeIndex);
        for (int32 r = 0; r < 4; ++r)
        {
            for (int32 c = 0; c < 4; ++c)
            {
                GlobalMatrix[r][c] = NodeMatrix.Get(r, c);
            }
        }

        // 로컬 행렬 계산
        if (ParentIndex != -1)
        {
            FbxNode* ParentNode = Node->GetParent();
            if (ParentNode)
            {
                int32 ParentPoseIndex = BindPose->Find(ParentNode);
                if (ParentPoseIndex != -1)
                {
                    FbxMatrix ParentNodeMatrix = BindPose->GetMatrix(ParentPoseIndex);
                    FbxAMatrix ParentGlobalMatrix;
                    for (int r = 0; r < 4; ++r)
                    {
                        for (int c = 0; c < 4; ++c)
                        {
                            ParentGlobalMatrix[r][c] = ParentNodeMatrix.Get(r, c);
                        }
                    }

                    // Local = ParentGlobal^-1 * Global
                    LocalMatrix = ParentGlobalMatrix.Inverse() * GlobalMatrix;
                }
                else
                {
                    LocalMatrix = Node->EvaluateLocalTransform();
                }
            }
            else
            {
                LocalMatrix = Node->EvaluateLocalTransform();
            }
        }
        else
        {
            // 루트 노드는 글로벌 = 로컬
            LocalMatrix = GlobalMatrix;
        }
    }
    else
    {
        // BindPose가 없으면 현재 노드 변환 사용
        LocalMatrix = Node->EvaluateLocalTransform();
        GlobalMatrix = Node->EvaluateGlobalTransform();
    }

    // BindPoseLocalTransform: LocalMatrix를 FMatrix로 변환
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            BoneInfo.BindPoseLocalTransform.M[i][j] = static_cast<float>(LocalMatrix.Get(i, j));
        }
    }

    // cm -> m 변환
    const float ScaleFactor = 0.01f;
    BoneInfo.BindPoseLocalTransform.M[3][0] *= ScaleFactor;
    BoneInfo.BindPoseLocalTransform.M[3][1] *= ScaleFactor;
    BoneInfo.BindPoseLocalTransform.M[3][2] *= ScaleFactor;

    // GlobalTransform: 바인드 포즈 시점의 본 월드 변환
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            BoneInfo.GlobalTransform.M[i][j] = static_cast<float>(GlobalMatrix.Get(i, j));
        }
    }
    BoneInfo.GlobalTransform.M[3][0] *= ScaleFactor;
    BoneInfo.GlobalTransform.M[3][1] *= ScaleFactor;
    BoneInfo.GlobalTransform.M[3][2] *= ScaleFactor;

    // InverseBindPoseMatrix 계산
    FbxAMatrix InverseBindMatrix = GlobalMatrix.Inverse();
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            BoneInfo.InverseBindPoseMatrix.M[i][j] = static_cast<float>(InverseBindMatrix.Get(i, j));
        }
    }
    BoneInfo.InverseBindPoseMatrix.M[3][0] *= ScaleFactor;
    BoneInfo.InverseBindPoseMatrix.M[3][1] *= ScaleFactor;
    BoneInfo.InverseBindPoseMatrix.M[3][2] *= ScaleFactor;

    // SkinningMatrix 계산
    BoneInfo.SkinningMatrix = BoneInfo.InverseBindPoseMatrix * BoneInfo.GlobalTransform;

    OutMeshData->Bones.push_back(BoneInfo);

    // 자식 노드들을 재귀적으로 처리
    for (int i = 0; i < Node->GetChildCount(); i++)
    {
        FbxNode* ChildNode = Node->GetChild(i);
        if (ChildNode &&
            ChildNode->GetNodeAttribute() &&
            ChildNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
        {
            CollectBoneData(ChildNode, OutMeshData, CurrentIndex, BindPose, NodeToIndexMap);
        }
    }
}

/*
 * ParseMeshGeometry()
 *
 * FBX 메시에서 Geometry 정보(Vertices, Indices, Materials)를 파싱합니다.
 *
 * 주요 처리:
 * - Control Point(위치 정점) 읽기
 * - Polygon(삼각형) 순회하며 Normal, UV, Tangent 추출
 * - Vertex Deduplication (중복 정점 제거)
 * - Material별로 Indices 그룹핑
 * - Vertex → ControlPoint 매핑 저장 (Skinning에 사용)
 *
 * @param FbxMeshNode FBX 메시 노드
 * @param OutMeshData 파싱 결과를 저장할 FSkeletalMesh
 * @param OutVertexToControlPointMap Vertex 인덱스 → ControlPoint 인덱스 매핑
 */
void FFBXManager::ParseMeshGeometry(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, TArray<int>& OutVertexToControlPointMap)
{
    int ControlPointsCount = FbxMeshNode->GetControlPointsCount();
    FbxVector4* ControlPoints = FbxMeshNode->GetControlPoints();
    int PolygonCount = FbxMeshNode->GetPolygonCount();

    // [수정] LocalTransformMatrix 사용 (GlobalTransform 대신)
    // 이유: 메시의 로컬 변환만 적용해야 함. 부모 노드 변환은 별도 처리
    const FbxAMatrix LocalTransformMatrix = FbxMeshNode->GetNode()->EvaluateLocalTransform();

    UE_LOG("FBXManager: Parsing mesh geometry (Fixed)");
    UE_LOG("  Control Points: %d", ControlPointsCount);
    UE_LOG("  Polygons: %d", PolygonCount);

    // Geometry Element (Normal, UV, Tangent, Color, Material 정보) 가져오기
    FbxGeometryElementNormal* NormalElement = FbxMeshNode->GetElementNormal();
    FbxGeometryElementUV* UVElement = FbxMeshNode->GetElementUV();
    FbxGeometryElementTangent* TangentElement = FbxMeshNode->GetElementTangent();
    FbxGeometryElementVertexColor* ColorElement = FbxMeshNode->GetElementVertexColor(); // [추가] Color Element
    FbxGeometryElementMaterial* MaterialElement = FbxMeshNode->GetElementMaterial();

    // [수정] Index-based Vertex Deduplication using FVertexKey
    TMap<FVertexKey, uint32> UniqueVertices;

    // Material별 인덱스 그룹핑
    TMap<int, TArray<uint32>> MaterialGroups;

    int VertexCounter = 0; // 폴리곤 정점 인덱스 (eByPolygonVertex 모드용)

    // 폴리곤 순회하며 파싱
    for (int PolyIndex = 0; PolyIndex < PolygonCount; PolyIndex++)
    {
        int PolySize = FbxMeshNode->GetPolygonSize(PolyIndex);

        if (PolySize != 3)
        {
            UE_LOG("FBXManager: Warning - Polygon %d is not a triangle! (Size: %d)", PolyIndex, PolySize);
            continue;
        }

        // 이 폴리곤의 Material 인덱스 가져오기
        int MaterialIndex = 0;
        if (MaterialElement)
        {
            auto Mode = MaterialElement->GetMappingMode();
            if (Mode == FbxGeometryElement::eByPolygon)
            {
                MaterialIndex = MaterialElement->GetIndexArray().GetAt(PolyIndex);
            }
            else if (Mode == FbxGeometryElement::eAllSame)
            {
                MaterialIndex = MaterialElement->GetIndexArray().GetAt(0);
            }
        }

        TArray<uint32> TriangleIndices;

        for (int VertInPoly = 0; VertInPoly < 3; VertInPoly++)
        {
            const int32 ControlPointIndex = FbxMeshNode->GetPolygonVertex(PolyIndex, VertInPoly);

            // [수정] FVertexKey를 사용한 인덱스 기반 중복 제거
            // 실제 데이터 인덱스를 기준으로 정점을 식별합니다
            FbxVector4 Position = ControlPoints[ControlPointIndex];
            FbxVector4 Normal;
            FbxVector4 Tangent;
            FbxVector2 UV;
            FbxColor Color;

            // 각 LayerElement의 인덱스 계산 (GetVertexElementData에서 사용)
            int32 NormalIndex = (NormalElement) ? (NormalElement->GetMappingMode() == FbxLayerElement::eByControlPoint ? ControlPointIndex : VertexCounter) : -1;
            int32 TangentIndex = (TangentElement) ? (TangentElement->GetMappingMode() == FbxLayerElement::eByControlPoint ? ControlPointIndex : VertexCounter) : -1;
            int32 UVIndex = (UVElement) ? (UVElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex ? FbxMeshNode->GetTextureUVIndex(PolyIndex, VertInPoly) : ControlPointIndex) : -1;
            int32 ColorIndex = (ColorElement) ? (ColorElement->GetMappingMode() == FbxLayerElement::eByControlPoint ? ControlPointIndex : VertexCounter) : -1;

            uint32 NewIndex;

            // 정점 병합 키 생성
            FVertexKey Key(ControlPointIndex, NormalIndex, TangentIndex, UVIndex, ColorIndex);

            // 맵에서 키 검색
            if (const uint32* Found = UniqueVertices.Find(Key))
            {
                NewIndex = *Found;
            }
            else
            {
                FNormalVertex NewVertex;

                // Position
                // cm -> m
                const float ScaleFactor = 0.01f;
                if (ControlPointIndex < ControlPointsCount)
                {
                    Position = LocalTransformMatrix.MultT(Position);
                    NewVertex.pos = FVector(
                        static_cast<float>(Position[0]) * ScaleFactor,
                        static_cast<float>(Position[1]) * ScaleFactor,
                        static_cast<float>(Position[2]) * ScaleFactor
                    );
                }

                // Normal: InverseTranspose 적용
                if (NormalElement && GetVertexElementData(NormalElement, ControlPointIndex, VertexCounter, Normal))
                {
                    Normal = LocalTransformMatrix.Inverse().Transpose().MultT(Normal);
                    NewVertex.normal = FVector(
                        static_cast<float>(Normal[0]),
                        static_cast<float>(Normal[1]),
                        static_cast<float>(Normal[2])
                    );
                }
                else
                {
                    NewVertex.normal = FVector(0, 0, 1);
                }

                // Tangent: InverseTranspose 적용
                if (TangentElement && GetVertexElementData(TangentElement, ControlPointIndex, VertexCounter, Tangent))
                {
                    Tangent = LocalTransformMatrix.Inverse().Transpose().MultT(Tangent);
                    NewVertex.Tangent = FVector4(
                        static_cast<float>(Tangent[0]),
                        static_cast<float>(Tangent[1]),
                        static_cast<float>(Tangent[2]),
                        1.0f // W (Handedness)
                    );
                }
                else
                {
                    NewVertex.Tangent = FVector4(1, 0, 0, 1);
                }

                // UV
                if (UVElement && GetVertexElementData(UVElement, ControlPointIndex, VertexCounter, UV))
                {
                    NewVertex.tex = FVector2D(
                        static_cast<float>(UV[0]),
                        1.0f - static_cast<float>(UV[1]) // V 좌표는 보통 뒤집힘 (DirectX 스타일)
                    );
                }
                else
                {
                    NewVertex.tex = FVector2D(0, 0);
                }

                // [추가] Vertex Color: ColorElement가 있으면 파싱
                if (ColorElement && GetVertexElementData(ColorElement, ControlPointIndex, VertexCounter, Color))
                {
                    NewVertex.color = FVector4(
                        static_cast<float>(Color.mRed),
                        static_cast<float>(Color.mGreen),
                        static_cast<float>(Color.mBlue),
                        static_cast<float>(Color.mAlpha)
                    );
                }
                else
                {
                    NewVertex.color = FVector4(1, 1, 1, 1);
                }

                // 새로운 정점을 Vertices 배열에 추가
                OutMeshData->Vertices.push_back(NewVertex);
                // 새 정점의 인덱스 계산
                NewIndex = static_cast<uint32>(OutMeshData->Vertices.size() - 1);
                // 맵에 새 정점 정보 추가
                UniqueVertices.Add(Key, NewIndex);
                // Vertex 인덱스 → ControlPoint 인덱스 매핑 저장 (Skinning에 사용)
                OutVertexToControlPointMap.push_back(ControlPointIndex);
            }

            TriangleIndices.push_back(NewIndex);
            VertexCounter++; // 다음 폴리곤 정점으로 이동
        }

        // Material 그룹에 삼각형 추가
        if (!MaterialGroups.contains(MaterialIndex))
        {
            MaterialGroups[MaterialIndex] = TArray<uint32>();
        }
        for (uint32 idx : TriangleIndices)
        {
            MaterialGroups[MaterialIndex].push_back(idx);
        }
    }

    // FbxNode에서 Material 이름 가져오기
    FbxNode* MeshNode = FbxMeshNode->GetNode();
    int MaterialCount = MeshNode ? MeshNode->GetMaterialCount() : 0;

    // Scene에서 Material 리스트 가져오기
    TArray<FbxSurfaceMaterial*> SceneMaterials;
    FbxScene* Scene = FbxMeshNode->GetScene();

    if (Scene)
    {
        int SceneMaterialCount = Scene->GetMaterialCount();
        if (MaterialCount == 0)
        {
            UE_LOG("FBXManager: Node has no materials, using Scene materials (count: %d)", SceneMaterialCount);
        }

        for (int i = 0; i < SceneMaterialCount; ++i)
        {
            FbxSurfaceMaterial* Mat = Scene->GetMaterial(i);
            if (Mat)
            {
                SceneMaterials.push_back(Mat);
            }
        }
    }

    UE_LOG("FBXManager: Material info - Node: %d, Scene: %zu, Groups: %zu",
           MaterialCount, SceneMaterials.size(), MaterialGroups.size());

    // MaterialGroups에서 GroupInfos 생성
    uint32 IndexBufferOffset = static_cast<uint32>(OutMeshData->Indices.size());
    if (MaterialGroups.size() > 0)
    {
        TArray<uint32> SortedIndices;

        for (auto& Pair : MaterialGroups)
        {
            int MaterialIndex = Pair.first;
            TArray<uint32>& GroupIndices = Pair.second;

            FGroupInfo GroupInfo;
            GroupInfo.StartIndex = IndexBufferOffset + static_cast<uint32>(SortedIndices.size());
            GroupInfo.IndexCount = static_cast<uint32>(GroupIndices.size());

            // Material 이름 설정
            FbxSurfaceMaterial* Material = nullptr;

            if (MeshNode && MaterialIndex >= 0 && MaterialIndex < MaterialCount)
            {
                Material = MeshNode->GetMaterial(MaterialIndex);
            }
            else if (MaterialIndex >= 0 && MaterialIndex < static_cast<int>(SceneMaterials.size()))
            {
                Material = SceneMaterials[MaterialIndex];
            }

            if (Material)
            {
                GroupInfo.InitialMaterialName = Material->GetName();
                UE_LOG("FBXManager: Group %d -> Material '%s'", MaterialIndex, GroupInfo.InitialMaterialName.c_str());
            }
            else
            {
                GroupInfo.InitialMaterialName = "";
                UE_LOG("FBXManager: Group %d -> No material assigned", MaterialIndex);
            }

            OutMeshData->GroupInfos.push_back(GroupInfo);

            for (uint32 idx : GroupIndices)
            {
                SortedIndices.push_back(idx);
            }
        }

        OutMeshData->Indices.insert(OutMeshData->Indices.end(), SortedIndices.begin(), SortedIndices.end());
        OutMeshData->bHasMaterial = true;
    }
    else
    {
        FGroupInfo GroupInfo;
        GroupInfo.StartIndex = IndexBufferOffset;
        GroupInfo.IndexCount = static_cast<uint32>(OutMeshData->Vertices.size());
        GroupInfo.InitialMaterialName = "";
        OutMeshData->GroupInfos.push_back(GroupInfo);

        // 인덱스 생성 (중복 제거로 인해 순차적이지 않을 수 있음)
        TArray<uint32> Indices;
        for (uint32 i = 0; i < static_cast<uint32>(OutMeshData->Vertices.size()); ++i)
        {
            Indices.push_back(i);
        }
        OutMeshData->Indices.insert(OutMeshData->Indices.end(), Indices.begin(), Indices.end());
        OutMeshData->bHasMaterial = OutMeshData->bHasMaterial || false;
    }

    UE_LOG("FBXManager: Parsed geometry - Vertices: %zu, Indices: %zu",
        OutMeshData->Vertices.size(), OutMeshData->Indices.size());
}

/*
 * ParseBoneHierarchy()
 *
 * FBX 메시에서 Bone 계층 구조를 파싱합니다.
 *
 * [중요] 기존 Cluster 기반 방식의 문제점:
 * - BindPose를 무시하고 GetTransformMatrix/GetTransformLinkMatrix만 사용
 * - Cluster 순서에 의존하여 부모-자식 관계가 잘못 설정될 수 있음
 * - 노드 계층 구조를 무시하여 일부 본이 누락될 수 있음
 *
 * [개선] 새로운 BindPose 기반 방식:
 * - BindPose를 명시적으로 찾아 사용
 * - 노드 계층 구조를 재귀적으로 순회
 * - ParentGlobalMatrix.Inverse() * NodeGlobalMatrix로 정확한 로컬 변환 계산
 *
 * @param FbxMeshNode FBX 메시 노드
 * @param OutMeshData 파싱 결과를 저장할 FSkeletalMesh
 */
void FFBXManager::ParseBoneHierarchy(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
{
    FbxScene* Scene = FbxMeshNode->GetScene();
    int DeformerCount = FbxMeshNode->GetDeformerCount(FbxDeformer::eSkin);

    UE_LOG("FBXManager: Parsing bone hierarchy (BindPose-based)");
    UE_LOG("  Skin Deformers: %d", DeformerCount);

    if (DeformerCount == 0)
        return;

    FbxSkin* Skin = static_cast<FbxSkin*>(FbxMeshNode->GetDeformer(0, FbxDeformer::eSkin));
    int ClusterCount = Skin->GetClusterCount();
    UE_LOG("  Clusters (Bones): %d", ClusterCount);

    // 1. 스켈레톤 루트 노드 찾기
    // Cluster의 링크 노드들 중 최상위 스켈레톤 노드를 찾습니다.
    TArray<FbxNode*> SkeletonRoots;
    TSet<FbxNode*> AllBoneNodes;

    // 모든 Cluster의 링크 노드를 수집
    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();
        if (BoneNode)
        {
            AllBoneNodes.Add(BoneNode);
        }
    }

    // 각 본 노드의 최상위 스켈레톤 조상을 찾습니다
    for (FbxNode* BoneNode : AllBoneNodes)
    {
        FbxNode* CurrentNode = BoneNode;
        FbxNode* SkeletonRoot = CurrentNode;

        // 부모를 따라 올라가며 스켈레톤 노드인 최상위 노드를 찾습니다
        while (CurrentNode)
        {
            FbxNode* Parent = CurrentNode->GetParent();
            if (Parent && Parent->GetNodeAttribute() &&
                Parent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            {
                SkeletonRoot = Parent;
                CurrentNode = Parent;
            }
            else
            {
                break;
            }
        }

        if (SkeletonRoot && !SkeletonRoots.Contains(SkeletonRoot))
        {
            SkeletonRoots.Add(SkeletonRoot);
        }
    }

    if (SkeletonRoots.IsEmpty())
    {
        UE_LOG("FBXManager: ERROR - No skeleton root found!");
        return;
    }

    UE_LOG("FBXManager: Found %d skeleton root(s)", SkeletonRoots.Num());

    // 2. BindPose 찾기
    FbxPose* BindPose = FindBindPose(SkeletonRoots[0]);
    if (!BindPose)
    {
        UE_LOG("FBXManager: WARNING - No BindPose found, using evaluated transforms");
    }
    else
    {
        UE_LOG("FBXManager: Found BindPose with %d nodes", BindPose->GetCount());
    }

    // 3. 본 계층 구조를 재귀적으로 수집
    // NodeToIndexMap: 빠른 인덱스 검색용 (ParseSkinWeights에서 사용)
    TMap<FbxNode*, int32> NodeToIndexMap;

    for (FbxNode* SkeletonRoot : SkeletonRoots)
    {
        CollectBoneData(SkeletonRoot, OutMeshData, -1, BindPose, NodeToIndexMap);
    }

    UE_LOG("FBXManager: Parsed %d bones with BindPose-based transforms", OutMeshData->Bones.size());
}

/*
 * ParseSkinWeights()
 *
 * FBX 메시에서 Skin Weight(본 가중치) 정보를 파싱합니다.
 *
 * 주요 처리:
 * - Cluster별로 영향받는 ControlPoint 목록과 Weight 수집
 * - ControlPointWeights 맵 구축 (ControlPoint → [(BoneIndex, Weight), ...])
 * - 각 Vertex에 대해 ControlPoint 매핑을 통해 Weight 적용
 * - 최대 4개 Bone만 선택 (Weight 내림차순 정렬)
 * - Weight 정규화 (합 = 1.0)
 *
 * @param FbxMeshNode FBX 메시 노드
 * @param OutMeshData 파싱 결과를 저장할 FSkeletalMesh
 * @param VertexToControlPointMap Vertex → ControlPoint 매핑 (ParseMeshGeometry에서 생성)
 * @param StartVertexIndex 이 메시의 정점 시작 인덱스
 * @param VertexCount 이 메시의 정점 개수
 */
void FFBXManager::ParseSkinWeights(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, const TArray<int>& VertexToControlPointMap, int StartVertexIndex, int VertexCount)
{
    int DeformerCount = FbxMeshNode->GetDeformerCount(FbxDeformer::eSkin);
    int ControlPointsCount = FbxMeshNode->GetControlPointsCount();

    UE_LOG("FBXManager: Parsing skin weights");

    if (DeformerCount == 0)
    {
        UE_LOG("  No skin deformers found");
        return;
    }

    FbxSkin* Skin = (FbxSkin*)FbxMeshNode->GetDeformer(0, FbxDeformer::eSkin);
    int ClusterCount = Skin->GetClusterCount();

    // ControlPoint마다 영향을 주는 Bone 정보 저장
    TMap<int, TArray<std::pair<int, float>>> ControlPointWeights;

    // Weight 정보 수집
    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);

        int* Indices = Cluster->GetControlPointIndices();
        double* Weights = Cluster->GetControlPointWeights();
        int IndexCount = Cluster->GetControlPointIndicesCount();

        for (int i = 0; i < IndexCount; i++)
        {
            int ControlPointIndex = Indices[i];
            float Weight = static_cast<float>(Weights[i]);

            ControlPointWeights[ControlPointIndex].push_back(std::make_pair(ClusterIndex, Weight));
        }
    }

    // Vertex에 Skinning 정보 적용 (이 메시의 정점들만 처리)
    OutMeshData->SkinnedVertices.resize(OutMeshData->Vertices.size());

    for (int i = 0; i < VertexCount; i++)
    {
        int GlobalVertexIndex = StartVertexIndex + i;
        FSkinnedVertex& SkinnedVert = OutMeshData->SkinnedVertices[GlobalVertexIndex];
        SkinnedVert.BaseVertex = OutMeshData->Vertices[GlobalVertexIndex];

        // ParseMeshGeometry에서 생성한 매핑을 사용하여 Vertex → ControlPoint 변환
        int ControlPointIndex = VertexToControlPointMap[i];

        if (ControlPointWeights.find(ControlPointIndex) != ControlPointWeights.end())
        {
            TArray<std::pair<int, float>>& Weights = ControlPointWeights[ControlPointIndex];

            // Weight 내림차순 정렬
            std::sort(Weights.begin(), Weights.end(),
                [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                    return a.second > b.second;
                });

            // 상위 4개 Weight만 적용
            float TotalWeight = 0.0f;
            int NumWeights = std::min(4, (int)Weights.size());

            for (int w = 0; w < NumWeights; w++)
            {
                SkinnedVert.BoneIndices[w] = static_cast<uint8>(Weights[w].first);
                SkinnedVert.BoneWeights[w] = Weights[w].second;
                TotalWeight += Weights[w].second;
            }

            // Weight 정규화 (합 = 1.0)
            if (TotalWeight > 0.0f)
            {
                for (int w = 0; w < NumWeights; w++)
                {
                    SkinnedVert.BoneWeights[w] /= TotalWeight;
                }
            }
        }
    }

    UE_LOG("FBXManager: Applied skinning to %zu vertices", OutMeshData->SkinnedVertices.size());
}

/*
 * LoadMaterials()
 *
 * FBX Mesh에서 Material 정보를 파싱하고 UMaterial 객체 생성 및 등록
 *
 * @param FbxMeshNode FBX 메시 노드
 */
void FFBXManager::LoadMaterials(FbxMesh* FbxMeshNode, TArray<FMaterialInfo>* OutMaterialInfos)
{
    if (!FbxMeshNode)
        return;

    FbxScene* Scene = FbxMeshNode->GetScene();
    if (!Scene)
    {
        UE_LOG("FBXManager: LoadMaterials failed - no scene found.");
        return;
    }

    // 씬의 모든 머티리얼을 로드
    int SceneMaterialCount = Scene->GetMaterialCount();
    if (SceneMaterialCount == 0)
    {
        UE_LOG("FBXManager: No materials found in scene.");
        return;
    }

    UE_LOG("FBXManager: Processing %d materials from scene.", SceneMaterialCount);

    UMaterial* DefaultMaterial = UResourceManager::GetInstance().GetDefaultMaterial();

    //----------------------------------------------------------------
    //@TODO Default Shader 수정 필요 -- GPU SKINNING 변환 시 --
    //----------------------------------------------------------------
    UShader* DefaultShader = DefaultMaterial ? DefaultMaterial->GetShader() : nullptr;

    for (int i = 0; i < SceneMaterialCount; ++i)
    {
        FbxSurfaceMaterial* FbxMaterial = Scene->GetMaterial(i);
        if (!FbxMaterial)
            continue;

        FString MaterialName = FbxMaterial->GetName();

        // FMaterialInfo 생성 (캐싱용, 항상 실행)
        FMaterialInfo MaterialInfo;
        MaterialInfo.MaterialName = MaterialName;

        // FBX에서 텍스처 및 색상 경로 파싱
        if (FbxMaterial->GetClassId().Is(FbxSurfacePhong::ClassId) ||
            FbxMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId))
        {
            // Ambient Color / Texture
            FbxProperty AmbientProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sAmbient);
            if (AmbientProperty.IsValid())
            {
                if (AmbientProperty.GetSrcObjectCount<FbxFileTexture>() > 0)
                {
                    FbxFileTexture* Texture = AmbientProperty.GetSrcObject<FbxFileTexture>(0);
                    if (Texture) MaterialInfo.AmbientTextureFileName = Texture->GetFileName();
                }
                else
                {
                    FbxDouble3 Color = AmbientProperty.Get<FbxDouble3>();
                    MaterialInfo.AmbientColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));
                }
            }

            // Diffuse Color / Texture
            FbxProperty DiffuseProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
            if (DiffuseProperty.IsValid())
            {
                if (DiffuseProperty.GetSrcObjectCount<FbxFileTexture>() > 0)
                {
                    FbxFileTexture* Texture = DiffuseProperty.GetSrcObject<FbxFileTexture>(0);
                    if (Texture) MaterialInfo.DiffuseTextureFileName = Texture->GetFileName();
                }
                else
                {
                    FbxDouble3 Color = DiffuseProperty.Get<FbxDouble3>();
                    MaterialInfo.DiffuseColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));
                }
            }

            // Emissive Color / Texture
            FbxProperty EmissiveProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sEmissive);
            if (EmissiveProperty.IsValid())
            {
                if (EmissiveProperty.GetSrcObjectCount<FbxFileTexture>() > 0)
                {
                    FbxFileTexture* Texture = EmissiveProperty.GetSrcObject<FbxFileTexture>(0);
                    if (Texture) MaterialInfo.EmissiveTextureFileName = Texture->GetFileName();
                }
                else
                {
                    FbxDouble3 Color = EmissiveProperty.Get<FbxDouble3>();
                    MaterialInfo.EmissiveColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));
                }
            }

            // Normal Texture
            FbxProperty NormalProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
            if (NormalProperty.IsValid())
            {
                if (NormalProperty.GetSrcObjectCount<FbxFileTexture>() > 0)
                {
                    FbxFileTexture* Texture = NormalProperty.GetSrcObject<FbxFileTexture>(0);
                    if (Texture) MaterialInfo.NormalTextureFileName = Texture->GetFileName();
                }
            }

            // Specular properties (Phong only)
            if (FbxMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
            {
                // Specular Color / Texture
                FbxProperty SpecularProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sSpecular);
                if (SpecularProperty.IsValid())
                {
                    if (SpecularProperty.GetSrcObjectCount<FbxFileTexture>() > 0)
                    {
                        FbxFileTexture* Texture = SpecularProperty.GetSrcObject<FbxFileTexture>(0);
                        if (Texture) MaterialInfo.SpecularTextureFileName = Texture->GetFileName();
                    }
                    else
                    {
                        FbxDouble3 Color = SpecularProperty.Get<FbxDouble3>();
                        MaterialInfo.SpecularColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));
                    }
                }

                // Specular Exponent (Shininess)
                FbxProperty ShininessProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
                if (ShininessProperty.IsValid())
                {
                    MaterialInfo.SpecularExponent = static_cast<float>(ShininessProperty.Get<FbxDouble>());
                }
            }
        }

        // OutMaterialInfos에 추가 (캐싱용)
        if (OutMaterialInfos)
        {
            OutMaterialInfos->push_back(MaterialInfo);
        }
    }

    // MaterialInfo가 있으면 UMaterial 객체 생성 및 등록
    if (OutMaterialInfos)
    {
        RegisterMaterialsFromInfos(*OutMaterialInfos);
    }
}

/**
 * RegisterMaterialsFromInfos()
 *
 * MaterialInfo 배열로부터 UMaterial 객체를 생성하고 ResourceManager에 등록합니다.
 * 캐시에서 MaterialInfo만 로드한 경우 UMaterial 객체가 없으므로 이 함수로 생성합니다.
 *
 * @param InMaterialInfos Material 정보 배열
 */
void FFBXManager::RegisterMaterialsFromInfos(const TArray<FMaterialInfo>& InMaterialInfos)
{
    UMaterial* DefaultMaterial = UResourceManager::GetInstance().GetDefaultMaterial();
    UShader* DefaultShader = DefaultMaterial ? DefaultMaterial->GetShader() : nullptr;

    for (const FMaterialInfo& MaterialInfo : InMaterialInfos)
    {
        FString MaterialName = MaterialInfo.MaterialName;

        // 이미 로드된 Material인지 확인 (중복 생성 방지)
        if (UResourceManager::GetInstance().Get<UMaterial>(MaterialName))
            continue;

        UE_LOG("FBXManager: Registering material from cache: '%s'", MaterialName.c_str());

        // UMaterial 생성 및 등록
        UMaterial* Material = NewObject<UMaterial>();
        Material->SetMaterialInfo(MaterialInfo);
        if (DefaultMaterial)
        {
            Material->SetShaderMacros(DefaultMaterial->GetShaderMacros());
        }
        if (DefaultShader)
        {
            Material->SetShader(DefaultShader);
        }

        UResourceManager::GetInstance().Add<UMaterial>(MaterialName, Material);
    }
}

/**
 * FBX 파일에서 Static Mesh Asset을 로드
 *
 * @param PathFileName FBX 파일 경로
 * @return 로드된 FStaticMesh 포인터 (실패 시 nullptr)
 */
FStaticMesh* FFBXManager::LoadFBXStaticMeshAsset(const FString& PathFileName)
{
    // 1. 경로 정규화
    FString NormalizedPathStr = NormalizePath(PathFileName);

    // 2. 메모리 캐시 확인: 이미 로드된 에셋이 있으면 즉시 반환
    if (FStaticMesh** It = FBXStaticMeshMap.Find(NormalizedPathStr))
    {
        UE_LOG("FBXManager: Static mesh already loaded from cache: %s", NormalizedPathStr.c_str());
        return *It;
    }

    // 변수 선언 (USE_OBJ_CACHE 유무와 관계없이 필요)
    FStaticMesh* StaticMeshData = nullptr;
    TArray<FMaterialInfo> MaterialInfos;
    bool bLoadedFromCache = false;

#ifdef USE_OBJ_CACHE
    // 캐시 경로 생성
    FString CachePathStr = ConvertDataPathToCachePath(NormalizedPathStr);
    const FString BinPathFileName = CachePathStr + ".sm.bin";
    const FString MatBinPathFileName = CachePathStr + ".sm.mat.bin";

    // 캐시 디렉토리 없을 시 캐시 디렉토리 생성
    fs::path CacheFileDirPath(BinPathFileName);
    if (CacheFileDirPath.has_parent_path())
    {
        fs::create_directories(CacheFileDirPath.parent_path());
    }

    bool bCacheExists = fs::exists(BinPathFileName) && fs::exists(MatBinPathFileName);
    bool bCacheisNewer = false;

    // 캐시가 최신인지 확인
    if (bCacheExists)
    {
        // FBX 원본이 없으면 캐시 무조건 사용
        if (!fs::exists(NormalizedPathStr))
        {
            bCacheisNewer = true;
        }
        else
        {
            try
            {
                auto binTime = fs::last_write_time(BinPathFileName);
                auto fbxTime = fs::last_write_time(NormalizedPathStr);
                bCacheisNewer = (binTime > fbxTime);
            }
            catch (...)
            {
                bCacheisNewer = false;
            }
        }
    }

    if (bCacheisNewer)
    {
        UE_LOG("Loading FBX from cache: %s", NormalizedPathStr.c_str());
        try
        {
            StaticMeshData = new FStaticMesh();

            FWindowsBinReader Reader(BinPathFileName);
            if (!Reader.IsOpen()) throw std::runtime_error("Failed to open bin");
            Reader << *StaticMeshData;
            Reader.Close();

            FWindowsBinReader MatReader(MatBinPathFileName);
            if (!MatReader.IsOpen()) throw std::runtime_error("Failed to open mat bin");
            Serialization::ReadArray<FMaterialInfo>(MatReader, MaterialInfos);
            MatReader.Close();

            // 캐시에서 로드한 MaterialInfos로 UMaterial 객체 생성 및 등록
            RegisterMaterialsFromInfos(MaterialInfos);

            StaticMeshData->CacheFilePath = BinPathFileName;
            bLoadedFromCache = true;
            UE_LOG("Successfully loaded from cache");
        }
        catch (const std::exception& e)
        {
            UE_LOG("Cache load failed: %s. Regenerating...", e.what());
            delete StaticMeshData;
            StaticMeshData = nullptr;
            fs::remove(BinPathFileName);
            fs::remove(MatBinPathFileName);
        }
    }
#endif

    // 캐시 로드 실패 시 fbx 파싱 (USE_OBJ_CACHE 없으면 무조건 파싱)
    if (!bLoadedFromCache)
    {
        UE_LOG("FBXManager: Loading static mesh from FBX: %s", NormalizedPathStr.c_str());

        // 3. FBX Manager 및 Scene 생성
        FbxManager* SdkManager = FbxManager::Create();
        if (!SdkManager)
        {
            UE_LOG("FBXManager: Failed to create FBX SDK Manager");
            return nullptr;
        }

        FbxIOSettings* IOSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
        SdkManager->SetIOSettings(IOSettings);

        // Material 및 Texture 로딩 활성화
        IOSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
        IOSettings->SetBoolProp(IMP_FBX_TEXTURE, true);

        FbxScene* Scene = FbxScene::Create(SdkManager, "MyScene");
        if (!Scene)
        {
            UE_LOG("FBXManager: Failed to create FBX Scene");
            SdkManager->Destroy();
            return nullptr;
        }

        // 4. FBX Importer 생성 및 파일 로드
        FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
        if (!Importer->Initialize(NormalizedPathStr.c_str(), -1, SdkManager->GetIOSettings()))
        {
            UE_LOG("FBXManager: Failed to initialize FBX Importer: %s", Importer->GetStatus().GetErrorString());
            Scene->Destroy();
            SdkManager->Destroy();
            return nullptr;
        }

        if (!Importer->Import(Scene))
        {
            UE_LOG("FBXManager: Failed to import FBX file: %s", Importer->GetStatus().GetErrorString());
            Importer->Destroy();
            Scene->Destroy();
            SdkManager->Destroy();
            return nullptr;
        }
        Importer->Destroy();

        // 5. Scene을 DirectX 왼손 좌표계로 변환 (Z-up → Y-up, 오른손 → 왼손)
        FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();
        FbxAxisSystem TargetAxisSystem(
            FbxAxisSystem::eZAxis,
            FbxAxisSystem::eParityEven,
            FbxAxisSystem::eLeftHanded);
        if (SceneAxisSystem != TargetAxisSystem)
        {
            TargetAxisSystem.DeepConvertScene(Scene);
        }

        // [추가] 단위 변환 (cm로 통일)
        FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
        if (SceneSystemUnit.GetScaleFactor() != 1.0)
        {
            FbxSystemUnit::cm.ConvertScene(Scene);
        }

        // 6. Scene을 Triangulate (모든 Polygon을 Triangle로 변환)
        FbxGeometryConverter GeometryConverter(SdkManager);
        GeometryConverter.Triangulate(Scene, true);

        // 7. [수정] Scene에서 모든 메시를 재귀적으로 찾습니다.
        TArray<FbxMesh*> AllMeshes;
        FindAllMeshesRecursive(Scene->GetRootNode(), AllMeshes);

        if (AllMeshes.IsEmpty())
        {
            UE_LOG("FBXManager: No mesh found in FBX file: %s", NormalizedPathStr.c_str());
            Scene->Destroy();
            SdkManager->Destroy();
            return nullptr;
        }
        UE_LOG("FBXManager: Loading %s (%zu mesh parts found)",
                   NormalizedPathStr.c_str(), AllMeshes.size());

        // 8. FStaticMesh 데이터 생성
        StaticMeshData = new FStaticMesh();
        StaticMeshData->PathFileName = NormalizedPathStr;

        // 9. [수정] 모든 메시에 대해 Geometry와 Material을 파싱하고 누적합니다.
        // Static Mesh는 Bone이나 Skinning 정보가 없으므로 Skeletal Mesh보다 과정이 간단합니다.
        TArray<int> VertexToControlPointMap; // Static Mesh에서는 사용되지 않지만, ParseMeshGeometry의 시그니처를 위해 필요합니다.

        // [수정] 위험한 캐스팅을 피하기 위해 임시 FSkeletalMesh를 사용합니다.
        // ParseMeshGeometry는 기본 데이터(Vertices, Indices, Groups)만 채우므로 이 방식이 안전합니다.
        FSkeletalMesh TempSkelData;
        for (FbxMesh* Mesh : AllMeshes)
        {
            ParseMeshGeometry(Mesh, &TempSkelData, VertexToControlPointMap);
        }
        LoadMaterials(AllMeshes[0], &MaterialInfos);

        // 임시 데이터에서 최종 StaticMesh로 복사
        StaticMeshData->Vertices = TempSkelData.Vertices;
        StaticMeshData->Indices = TempSkelData.Indices;
        StaticMeshData->GroupInfos = TempSkelData.GroupInfos;
        StaticMeshData->bHasMaterial = TempSkelData.bHasMaterial;

        UE_LOG("FBXManager: Successfully loaded static mesh");
        UE_LOG("  Vertices: %zu", StaticMeshData->Vertices.size());
        UE_LOG("  Indices: %zu", StaticMeshData->Indices.size());

        // 10. FBX SDK 리소스 정리
        Scene->Destroy();
        SdkManager->Destroy();

#ifdef USE_OBJ_CACHE
        // 파싱 완료 후 캐시에 저장
        try {
            FWindowsBinWriter Writer(BinPathFileName);
            Writer << *StaticMeshData;
            Writer.Close();

            FWindowsBinWriter MatWriter(MatBinPathFileName);
            Serialization::WriteArray<FMaterialInfo>(MatWriter, MaterialInfos);
            MatWriter.Close();

            StaticMeshData->CacheFilePath = BinPathFileName;
            UE_LOG("FBXManager: Cache saved: %s", BinPathFileName.c_str());
        }
        catch (const std::exception& e) {
            UE_LOG("FBXManager: Failed to save cache: %s", e.what());
        }
#endif
    }

    // 11. 캐시에 저장하여 메모리 관리
    FBXStaticMeshMap.Add(NormalizedPathStr, StaticMeshData);

    return StaticMeshData;
}

/**
 * FBX 파일에서 UStaticMesh 객체를 로드
 *
 * @param PathFileName FBX 파일 경로
 * @return 로드된 UStaticMesh 포인터 (실패 시 nullptr)
 */
UStaticMesh* FFBXManager::LoadFBXStaticMesh(const FString& PathFileName)
{
    // 0) 경로 정규화
    FString NormalizedPathStr = NormalizePath(PathFileName);

    // 1) 이미 로드된 UStaticMesh가 있는지 전체 검색 (정규화된 경로로 비교)
    for (TObjectIterator<UStaticMesh> It; It; ++It)
    {
        UStaticMesh* StaticMesh = *It;

        if (StaticMesh->GetFilePath() == NormalizedPathStr)
        {
            return StaticMesh;
        }
    }

    // 2) 없으면 새로 로드 (정규화된 경로 사용)
    UStaticMesh* StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(NormalizedPathStr);

    UE_LOG("UStaticMesh(filename: \'%s\') is successfully created!", NormalizedPathStr.c_str());
    return StaticMesh;
}
