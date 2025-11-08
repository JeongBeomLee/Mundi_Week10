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

using namespace fbxsdk;

// FNormalVertex를 unordered_map/set의 키로 사용하기 위한 std::hash 특수화
// 정점 중복 제거(Vertex Deduplication)에 사용됨
namespace std {
    template <>
    struct hash<FNormalVertex>
    {
        size_t operator()(const FNormalVertex& v) const noexcept
        {
            // 위치 해시
            size_t h1 = hash<float>()(v.pos.X);
            size_t h2 = hash<float>()(v.pos.Y);
            size_t h3 = hash<float>()(v.pos.Z);

            // 노멀 해시
            size_t h4 = hash<float>()(v.normal.X);
            size_t h5 = hash<float>()(v.normal.Y);
            size_t h6 = hash<float>()(v.normal.Z);

            // UV 해시
            size_t h7 = hash<float>()(v.tex.X);
            size_t h8 = hash<float>()(v.tex.Y);

            // 해시 조합
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1) ^
                   ((h4 ^ (h5 << 1)) >> 1) ^ (h6 << 1) ^
                   ((h7 ^ (h8 << 1)) >> 1);
        }
    };
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
    FString NormalizedPathStr = NormalizePath(PathFileName);

    // 1. 메모리 캐시 확인: 이미 로드된 에셋이 있으면 즉시 반환
    if (FSkeletalMesh** It = FBXSkeletalMeshMap.Find(NormalizedPathStr))
    {
        return *It;
    }

    // 2. FBX SDK 초기화
    FbxManager* SdkManager = FbxManager::Create();
    if (!SdkManager)
    {
        UE_LOG("FBXManager: Failed to create FbxManager!");
        return nullptr;
    }

    FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(ios);

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
        FbxAxisSystem::eParityOdd,
        FbxAxisSystem::eLeftHanded);
    
    if (SceneAxisSystem != OurAxisSystem)
    {
        OurAxisSystem.ConvertScene(Scene);
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
    FSkeletalMesh* SkeletalMeshData = new FSkeletalMesh();
    SkeletalMeshData->PathFileName = NormalizedPathStr;


    TArray<FbxMesh*> AllMeshes;
    FindAllMeshesRecursive(Scene->GetRootNode(), AllMeshes);

    if (AllMeshes.IsEmpty())
    {
        UE_LOG("FBXManager: No mesh found in FBX file: %s", NormalizedPathStr.c_str());
        return nullptr;
    }
    UE_LOG("FBXManager: Loading %s (%zu mesh parts found)", 
               NormalizedPathStr.c_str(), AllMeshes.size());

    // 스킨 정보가 있는 "메인" 메시를 찾습니다.
    // 뼈대(Hierarchy)와 가중치(Weights)는 이 메시를 기준으로 파싱합니다.
    FbxMesh* MainMeshForSkinning = AllMeshes[0];
    for (FbxMesh* Mesh : AllMeshes)
    {
        if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
        {
            MainMeshForSkinning = Mesh;
            UE_LOG("FBXManager: Found main skin deformer on mesh: %s", 
                   Mesh->GetNode() ? Mesh->GetNode()->GetName() : "");
            break;
        }
    }

    UE_LOG("FBXManager: Loading %s", NormalizedPathStr.c_str());

    // 8. 메시 데이터 파싱
    TArray<int> VertexToControlPointMap;
    ParseBoneHierarchy(MainMeshForSkinning, SkeletalMeshData);
    for (FbxMesh* Mesh : AllMeshes)
    {
        ParseMeshGeometry(Mesh, SkeletalMeshData, VertexToControlPointMap);
    }
    ParseSkinWeights(MainMeshForSkinning, SkeletalMeshData, VertexToControlPointMap);
    for (FbxMesh* Mesh : AllMeshes)
    {
        LoadMaterials(Mesh);
    }

    UE_LOG("FBXManager: Successfully loaded skeletal mesh");
    UE_LOG("  Vertices: %zu", SkeletalMeshData->Vertices.size());
    UE_LOG("  Indices: %zu", SkeletalMeshData->Indices.size());
    UE_LOG("  Bones: %zu", SkeletalMeshData->Bones.size());

    // 9. FBX SDK 리소스 정리
    Scene->Destroy();
    SdkManager->Destroy();

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

    UE_LOG("FBXManager: Parsing mesh geometry");
    UE_LOG("  Control Points: %d", ControlPointsCount);
    UE_LOG("  Polygons: %d", PolygonCount);

    // Geometry Element (Normal, UV, Tangent, Material 정보) 가져오기
    FbxGeometryElementNormal* NormalElement = FbxMeshNode->GetElementNormal();
    FbxGeometryElementUV* UVElement = FbxMeshNode->GetElementUV();
    FbxGeometryElementTangent* TangentElement = FbxMeshNode->GetElementTangent();
    FbxGeometryElementMaterial* MaterialElement = FbxMeshNode->GetElementMaterial();

    // Vertex 중복 제거를 위한 자료구조
    TArray<uint32> Indices;
    TMap<FNormalVertex, uint32> VertexMap;

    // Material별 인덱스 그룹핑
    TMap<int, TArray<uint32>> MaterialGroups;

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
            if (MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
            {
                if (MaterialElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                    MaterialIndex = MaterialElement->GetIndexArray().GetAt(PolyIndex);
                else
                    MaterialIndex = PolyIndex;
            }
            else if (MaterialElement->GetMappingMode() == FbxGeometryElement::eAllSame)
            {
                MaterialIndex = 0;
            }
        }

        TArray<uint32> TriangleIndices;

        for (int VertInPoly = 0; VertInPoly < 3; VertInPoly++)
        {
            int ControlPointIndex = FbxMeshNode->GetPolygonVertex(PolyIndex, VertInPoly);
            FNormalVertex Vertex;

            // 위치 (Position)
            FbxVector4 Position = ControlPoints[ControlPointIndex];
            Vertex.pos = FVector(
                static_cast<float>(Position[0]),
                static_cast<float>(Position[1]),
                static_cast<float>(Position[2])
            );

            // 노멀 (Normal)
            if (NormalElement)
            {
                int NormalIndex = 0;
                int VertexId = PolyIndex * 3 + VertInPoly;

                if (NormalElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
                {
                    if (NormalElement->GetReferenceMode() == FbxGeometryElement::eDirect)
                        NormalIndex = VertexId;
                    else if (NormalElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                        NormalIndex = NormalElement->GetIndexArray().GetAt(VertexId);
                }
                else if (NormalElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
                {
                    if (NormalElement->GetReferenceMode() == FbxGeometryElement::eDirect)
                        NormalIndex = ControlPointIndex;
                    else if (NormalElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                        NormalIndex = NormalElement->GetIndexArray().GetAt(ControlPointIndex);
                }

                FbxVector4 Normal = NormalElement->GetDirectArray().GetAt(NormalIndex);
                Vertex.normal = FVector(
                    static_cast<float>(Normal[0]),
                    static_cast<float>(Normal[1]),
                    static_cast<float>(Normal[2])
                );
            }
            else
            {
                Vertex.normal = FVector(0, 0, 1);
            }

            // UV 좌표
            if (UVElement)
            {
                int UVIndex = 0;
                if (UVElement->GetReferenceMode() == FbxGeometryElement::eDirect)
                    UVIndex = ControlPointIndex;
                else if (UVElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                    UVIndex = UVElement->GetIndexArray().GetAt(ControlPointIndex);

                FbxVector2 UV = UVElement->GetDirectArray().GetAt(UVIndex);
                Vertex.tex = FVector2D(
                    static_cast<float>(UV[0]),
                    1.0f - static_cast<float>(UV[1])
                );
            }
            else
            {
                Vertex.tex = FVector2D(0, 0);
            }

            // 접선 (Tangent)
            if (TangentElement)
            {
                int TangentIndex = 0;
                if (TangentElement->GetReferenceMode() == FbxGeometryElement::eDirect)
                    TangentIndex = ControlPointIndex;
                else if (TangentElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                    TangentIndex = TangentElement->GetIndexArray().GetAt(ControlPointIndex);

                FbxVector4 Tangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
                Vertex.Tangent = FVector4(
                    static_cast<float>(Tangent[0]),
                    static_cast<float>(Tangent[1]),
                    static_cast<float>(Tangent[2]),
                    1.0f
                );
            }
            else
            {
                Vertex.Tangent = FVector4(1, 0, 0, 1);
            }

            Vertex.color = FVector4(1, 1, 1, 1);

            // 정점 중복 제거
            uint32 VertexIndex;
            if (VertexMap.contains(Vertex))
            {
                VertexIndex = VertexMap[Vertex];
            }
            else
            {
                VertexIndex = static_cast<uint32>(OutMeshData->Vertices.size());
                VertexMap[Vertex] = VertexIndex;
                OutMeshData->Vertices.push_back(Vertex);
                // Vertex 인덱스 → ControlPoint 인덱스 매핑 저장 (Skinning에 사용)
                OutVertexToControlPointMap.push_back(ControlPointIndex);
            }

            TriangleIndices.push_back(VertexIndex);
            Indices.push_back(VertexIndex);
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

    // Scene에서 Material 리스트 가져오기 (GetMaterialCount()가 0일 경우 대비)
    TArray<FbxSurfaceMaterial*> SceneMaterials;
    FbxScene* Scene = FbxMeshNode->GetScene();

    if (MaterialCount == 0 && Scene)
    {
        int SceneMaterialCount = Scene->GetMaterialCount();
        UE_LOG("FBXManager: Node has no materials, using Scene materials (count: %d)", SceneMaterialCount);

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
            
            // Material 이름 설정 - 노드에서 먼저 시도, 실패하면 Scene에서 시도
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

        // Material 그룹 재정렬된 인덱스로 교체
        OutMeshData->Indices.insert(OutMeshData->Indices.end(), SortedIndices.begin(), SortedIndices.end());
        OutMeshData->bHasMaterial = true; // 그룹이 하나라도 있으면 true
    }
    else
    {
        // Material이 없는 경우 원본 Indices 사용
        FGroupInfo GroupInfo;
        GroupInfo.StartIndex = IndexBufferOffset; // (전역 오프셋)
        GroupInfo.IndexCount = static_cast<uint32>(Indices.size()); // (이 메시 조각의 전체 인덱스 수)
        GroupInfo.InitialMaterialName = "";
        OutMeshData->GroupInfos.push_back(GroupInfo);

        // OutMeshData->Indices에 대입(=)하지 않고, 뒤에 추가(Append)합니다.
        OutMeshData->Indices.insert(OutMeshData->Indices.end(), Indices.begin(), Indices.end());
        OutMeshData->bHasMaterial = OutMeshData->bHasMaterial || false; // 기존 bHasMaterial 상태를 유지
    }

    UE_LOG("FBXManager: Parsed geometry - Vertices: %zu, Indices: %zu",
        OutMeshData->Vertices.size(), OutMeshData->Indices.size());
}

/*
 * ParseBoneHierarchy()
 *
 * FBX 메시에서 Bone 계층 구조를 파싱합니다.
 *
 * 주요 처리:
 * - Skin Deformer에서 Cluster(본) 목록 가져오기
 * - 각 Cluster가 링크하는 Bone Node 찾기
 * - 부모-자식 관계 구축 (ParentIndex)
 * - Offset Matrix 계산 (Mesh Local → Bone Local 변환 행렬)
 * - Column-major → Row-major 변환 (FBX → DirectX)
 *
 * @param FbxMeshNode FBX 메시 노드
 * @param OutMeshData 파싱 결과를 저장할 FSkeletalMesh
 */
void FFBXManager::ParseBoneHierarchy(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
{
    FbxScene* Scene = FbxMeshNode->GetScene();
    int DeformerCount = FbxMeshNode->GetDeformerCount(FbxDeformer::eSkin);

    UE_LOG("FBXManager: Parsing bone hierarchy");
    UE_LOG("  Skin Deformers: %d", DeformerCount);

    if (DeformerCount == 0)
        return;

    FbxSkin* Skin = static_cast<FbxSkin*>(FbxMeshNode->GetDeformer(0, FbxDeformer::eSkin));
    int ClusterCount = Skin->GetClusterCount();

    UE_LOG("  Clusters (Bones): %d", ClusterCount);

    // 각 Cluster(본) 순회
    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();

        if (!BoneNode)
            continue;

        FBoneInfo BoneInfo;
        BoneInfo.BoneName = BoneNode->GetName();

        // 부모 본 찾기
        FbxNode* ParentNode = BoneNode->GetParent();
        BoneInfo.ParentIndex = -1;
        
        if (ParentNode && ParentNode != Scene->GetRootNode())
        {
            for (int i = 0; i < ClusterCount; i++)
            {
                FbxCluster* TestCluster = Skin->GetCluster(i);
                if (TestCluster->GetLink() == ParentNode)
                {
                    BoneInfo.ParentIndex = i;
                    break;
                }
            }
        }

        // ========================================
        // 1. InverseBindPoseMatrix 계산
        // ========================================
        // FBX는 열우선(Column-major): v' = M * v (오른쪽부터 적용)
        // DirectX는 행우선(Row-major): v' = v * M (왼쪽부터 적용)

        FbxAMatrix MeshTransform;   // Mesh Local → World
        FbxAMatrix LinkTransform;   // Bone World Transform at Bind Pose
        Cluster->GetTransformMatrix(MeshTransform);
        Cluster->GetTransformLinkMatrix(LinkTransform);

        // InverseBindPoseMatrix: 바인드 포즈에서 월드 좌표 → 본 로컬 좌표 (Model → Bone)
        // = LinkTransform.Inverse() * MeshTransform
        FbxAMatrix FbxInverseBindPose = LinkTransform.Inverse() * MeshTransform;

        // 전치 없이 그대로 복사 (i, j)
        BoneInfo.InverseBindPoseMatrix = FMatrix::Identity();
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                BoneInfo.InverseBindPoseMatrix.M[i][j] = static_cast<float>(FbxInverseBindPose.Get(i, j));
            }
        }

        // ========================================
        // 2. GlobalTransform: 본 로컬 좌표 → 월드 좌표 (Bone → Model)
        // ========================================
        // 바인드 포즈 시점의 본 월드 변환
        FbxAMatrix FbxGlobalTransform = LinkTransform;

        // 전치 없이 그대로 복사 (i, j)
        BoneInfo.GlobalTransform = FMatrix::Identity();
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                BoneInfo.GlobalTransform.M[i][j] = static_cast<float>(FbxGlobalTransform.Get(i, j));
            }
        }

        // ========================================
        // 3. SkinningMatrix: InverseBindPoseMatrix × GlobalTransform
        // ========================================
        // = (Model → Bone) × (Bone → Model)
        // = 바인드 포즈 기준으로 정점을 애니메이션된 본에 맞춰 변형
        BoneInfo.SkinningMatrix = BoneInfo.InverseBindPoseMatrix * BoneInfo.GlobalTransform;

        OutMeshData->Bones.push_back(BoneInfo);
    }

    UE_LOG("FBXManager: Parsed %d bones", OutMeshData->Bones.size());
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
 */
void FFBXManager::ParseSkinWeights(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData, const TArray<int>& VertexToControlPointMap)
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

    // Vertex에 Skinning 정보 적용
    OutMeshData->SkinnedVertices.resize(OutMeshData->Vertices.size());

    for (size_t i = 0; i < OutMeshData->Vertices.size(); i++)
    {
        FSkinnedVertex& SkinnedVert = OutMeshData->SkinnedVertices[i];
        SkinnedVert.BaseVertex = OutMeshData->Vertices[i];

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
 * FBX Mesh에서 Material 정보를 파싱하고 UMaterial 객체를 생성하여 UResourceManager에 등록합니다.
 *
 * @param FbxMeshNode FBX 메시 노드
 */
void FFBXManager::LoadMaterials(FbxMesh* FbxMeshNode)
{
    if (!FbxMeshNode)
        return;

    FbxNode* MeshNode = FbxMeshNode->GetNode();
    if (!MeshNode)
        return;

    // Material을 찾기 위한 여러 시도
    int MaterialCount = MeshNode->GetMaterialCount();

    UE_LOG("FBXManager: LoadMaterials - MeshNode MaterialCount: %d", MaterialCount);

    // Material이 노드에 없으면 Scene에서 찾기
    TArray<FbxSurfaceMaterial*> Materials;

    if (MaterialCount > 0)
    {
        // 노드에서 직접 가져오기
        for (int i = 0; i < MaterialCount; ++i)
        {
            FbxSurfaceMaterial* FbxMaterial = MeshNode->GetMaterial(i);
            if (FbxMaterial)
            {
                Materials.push_back(FbxMaterial);
            }
        }
    }
    else
    {
        // Scene 전체에서 Material 검색
        FbxScene* Scene = FbxMeshNode->GetScene();
        if (Scene)
        {
            int SceneMaterialCount = Scene->GetMaterialCount();
            UE_LOG("FBXManager: Searching Scene for materials - Found %d materials", SceneMaterialCount);

            for (int i = 0; i < SceneMaterialCount; ++i)
            {
                FbxSurfaceMaterial* FbxMaterial = Scene->GetMaterial(i);
                if (FbxMaterial)
                {
                    Materials.push_back(FbxMaterial);
                }
            }
        }
    }

    if (Materials.empty())
    {
        UE_LOG("FBXManager: No materials found in FBX file");
        return;
    }

    UE_LOG("FBXManager: Loading %zu materials", Materials.size());

    UMaterial* DefaultMaterial = UResourceManager::GetInstance().GetDefaultMaterial();
    //@TODO 추가 쉐이더 작업 필..
    UShader* DefaultShader = DefaultMaterial ? DefaultMaterial->GetShader() : nullptr;

    for (FbxSurfaceMaterial* FbxMaterial : Materials)
    {
        if (!FbxMaterial)
            continue;

        FString MaterialName = FbxMaterial->GetName();

        // 이미 로드된 Material인지 확인
        if (UResourceManager::GetInstance().Get<UMaterial>(MaterialName))
            continue;

        // FMaterialInfo 생성
        FMaterialInfo MaterialInfo;
        MaterialInfo.MaterialName = MaterialName;

        // FBX에서 텍스처 경로 파싱
        // FbxSurfaceMaterial을 FbxSurfacePhong 또는 FbxSurfaceLambert로 캐스팅하여 텍스처 정보 추출
        if (FbxMaterial->GetClassId().Is(FbxSurfacePhong::ClassId) ||
            FbxMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId))
        {
            // Diffuse 텍스처
            FbxProperty DiffuseProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
            if (DiffuseProperty.IsValid())
            {
                int TextureCount = DiffuseProperty.GetSrcObjectCount<FbxFileTexture>();
                if (TextureCount > 0)
                {
                    FbxFileTexture* Texture = DiffuseProperty.GetSrcObject<FbxFileTexture>(0);
                    if (Texture)
                    {
                        MaterialInfo.DiffuseTextureFileName = Texture->GetFileName();
                    }
                }
            }

            // Normal 텍스처
            FbxProperty NormalProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
            if (NormalProperty.IsValid())
            {
                int TextureCount = NormalProperty.GetSrcObjectCount<FbxFileTexture>();
                if (TextureCount > 0)
                {
                    FbxFileTexture* Texture = NormalProperty.GetSrcObject<FbxFileTexture>(0);
                    if (Texture)
                    {
                        MaterialInfo.NormalTextureFileName = Texture->GetFileName();
                    }
                }
            }

            // Specular 텍스처 (Phong만 해당)
            if (FbxMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
            {
                FbxProperty SpecularProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sSpecular);
                if (SpecularProperty.IsValid())
                {
                    int TextureCount = SpecularProperty.GetSrcObjectCount<FbxFileTexture>();
                    if (TextureCount > 0)
                    {
                        FbxFileTexture* Texture = SpecularProperty.GetSrcObject<FbxFileTexture>(0);
                        if (Texture)
                        {
                            MaterialInfo.SpecularTextureFileName = Texture->GetFileName();
                        }
                    }
                }
            }
        }

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
        UE_LOG("FBXManager: Created material: %s", MaterialName.c_str());
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
    FbxAxisSystem TargetAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);
    if (SceneAxisSystem != TargetAxisSystem)
    {
        TargetAxisSystem.ConvertScene(Scene);
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
    FStaticMesh* StaticMeshData = new FStaticMesh();
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
        LoadMaterials(Mesh);
    }

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
