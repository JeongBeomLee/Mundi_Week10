#include "pch.h"
#include "FBXManager.h"
#include "PathUtils.h"
#include "ObjectIterator.h"
#include "SkeletalMesh.h"

using namespace fbxsdk;

// std::hash 특수화 for FNormalVertex
namespace std {
    template <>
    struct hash<FNormalVertex>
    {
        size_t operator()(const FNormalVertex& v) const noexcept
        {
            // Position hash
            size_t h1 = hash<float>()(v.pos.X);
            size_t h2 = hash<float>()(v.pos.Y);
            size_t h3 = hash<float>()(v.pos.Z);

            // Normal hash
            size_t h4 = hash<float>()(v.normal.X);
            size_t h5 = hash<float>()(v.normal.Y);
            size_t h6 = hash<float>()(v.normal.Z);

            // UV hash
            size_t h7 = hash<float>()(v.tex.X);
            size_t h8 = hash<float>()(v.tex.Y);

            // Combine hashes
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1) ^
                   ((h4 ^ (h5 << 1)) >> 1) ^ (h6 << 1) ^
                   ((h7 ^ (h8 << 1)) >> 1);
        }
    };
}

// Static member definition
TMap<FString, FSkeletalMesh*> FBXManager::FBXSkeletalMeshMap;

FBXManager::FBXManager()
{
}

FBXManager::~FBXManager()
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

    // 4) 모든 SkeletalMesh 가져오기
    RESOURCE.SetSkeletalMeshs();

    UE_LOG("FObjManager::Preload: Loaded %zu .obj files from %s", LoadedCount, DataDir.string().c_str());
}

void FBXManager::Clear()
{
    for (auto& Pair : FBXSkeletalMeshMap)
    {
        delete Pair.second;
    }

    FBXSkeletalMeshMap.Empty();
}

FSkeletalMesh* FBXManager::LoadFBXSkeletalMeshAsset(const FString& PathFileName)
{
    // FBX SDK 초기화
    FbxManager* SdkManager = FbxManager::Create();
    if (!SdkManager)
    {
        UE_LOG("FBXManager: Failed to create FbxManager!");
        return nullptr;
    }

    FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(ios);

    // FBX Importer 생성
    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
    if (!Importer->Initialize(PathFileName.c_str(), -1, SdkManager->GetIOSettings()))
    {
        UE_LOG("FBXManager: Failed to initialize importer for %s", PathFileName.c_str());
        UE_LOG("  Error: %s", Importer->GetStatus().GetErrorString());
        Importer->Destroy();
        SdkManager->Destroy();
        return nullptr;
    }

    // Scene 생성 및 임포트
    FbxScene* Scene = FbxScene::Create(SdkManager, "TempScene");
    if (!Importer->Import(Scene))
    {
        UE_LOG("FBXManager: Failed to import FBX file: %s", PathFileName.c_str());
        Importer->Destroy();
        Scene->Destroy();
        SdkManager->Destroy();
        return nullptr;
    }
    Importer->Destroy();

    // Axis와 Unit 변환 (Z-up, X-forward 좌표계로)
    FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();
    // Z-up, X-forward, Left-handed (Unreal Engine 스타일)
    FbxAxisSystem OurAxisSystem(
        FbxAxisSystem::eZAxis,
        FbxAxisSystem::eParityOdd,
        FbxAxisSystem::eLeftHanded);
    
    if (SceneAxisSystem != OurAxisSystem)
    {
        OurAxisSystem.ConvertScene(Scene);
    }

    // Unit 변환 (cm로 통일)
    FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
    if (SceneSystemUnit.GetScaleFactor() != 1.0)
    {
        FbxSystemUnit::cm.ConvertScene(Scene);
    }

    // Triangulate (모든 폴리곤을 삼각형으로 변환)
    FbxGeometryConverter GeometryConverter(SdkManager);
    GeometryConverter.Triangulate(Scene, true);

    // FSkeletalMesh 생성
    FSkeletalMesh* SkeletalMeshData = new FSkeletalMesh();
    SkeletalMeshData->PathFileName = PathFileName;

    // 첫 번째 Mesh 노드 찾기
    FbxNode* RootNode = Scene->GetRootNode();
    FbxMesh* FbxMeshNode = nullptr;

    // 대부분 fbx는 하나로 통합된 상태라 첫번째 자식만 사용해도 된다는디,,
    if (RootNode)
    {
        for (int i = 0; i < RootNode->GetChildCount(); i++)
        {
            FbxNode* ChildNode = RootNode->GetChild(i);
            if (ChildNode->GetMesh())
            {
                FbxMeshNode = ChildNode->GetMesh();
                break;
            }
        }
    }

    if (!FbxMeshNode)
    {
        UE_LOG("FBXManager: No mesh found in FBX file: %s", PathFileName.c_str());
        delete SkeletalMeshData;
        Scene->Destroy();
        SdkManager->Destroy();
        return nullptr;
    }

    UE_LOG("FBXManager: Loading %s", PathFileName.c_str());

    // Parse mesh geometry (vertices, indices, materials)
    ParseMeshGeometry(FbxMeshNode, SkeletalMeshData);

    // Parse bone hierarchy
    ParseBoneHierarchy(FbxMeshNode, SkeletalMeshData);

    // Parse and apply skin weights
    ParseSkinWeights(FbxMeshNode, SkeletalMeshData);

    UE_LOG("FBXManager: Successfully loaded skeletal mesh");
    UE_LOG("  Vertices: %zu", SkeletalMeshData->Vertices.size());
    UE_LOG("  Indices: %zu", SkeletalMeshData->Indices.size());
    UE_LOG("  Bones: %zu", SkeletalMeshData->Bones.size());

    // Cleanup
    Scene->Destroy();
    SdkManager->Destroy();

    return SkeletalMeshData;
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

// ========================================
// Helper Functions
// ========================================

void FBXManager::ParseMeshGeometry(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
{
    int ControlPointsCount = FbxMeshNode->GetControlPointsCount();
    FbxVector4* ControlPoints = FbxMeshNode->GetControlPoints();
    int PolygonCount = FbxMeshNode->GetPolygonCount();

    UE_LOG("FBXManager: Parsing mesh geometry");
    UE_LOG("  Control Points: %d", ControlPointsCount);
    UE_LOG("  Polygons: %d", PolygonCount);

    // Geometry Elements
    FbxGeometryElementNormal* NormalElement = FbxMeshNode->GetElementNormal();
    FbxGeometryElementUV* UVElement = FbxMeshNode->GetElementUV();
    FbxGeometryElementTangent* TangentElement = FbxMeshNode->GetElementTangent();
    FbxGeometryElementMaterial* MaterialElement = FbxMeshNode->GetElementMaterial();

    // Vertex deduplication
    TArray<uint32> Indices;
    TMap<FNormalVertex, uint32> VertexMap;

    // Material grouping
    TMap<int, TArray<uint32>> MaterialGroups;

    // Parse polygons
    for (int PolyIndex = 0; PolyIndex < PolygonCount; PolyIndex++)
    {
        int PolySize = FbxMeshNode->GetPolygonSize(PolyIndex);

        if (PolySize != 3)
        {
            UE_LOG("FBXManager: Warning - Polygon %d is not a triangle! (Size: %d)", PolyIndex, PolySize);
            continue;
        }

        // Get material index for this polygon
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

            // Position
            FbxVector4 Position = ControlPoints[ControlPointIndex];
            Vertex.pos = FVector(
                static_cast<float>(Position[0]),
                static_cast<float>(Position[1]),
                static_cast<float>(Position[2])
            );

            // Normal
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

            // UV
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

            // Tangent
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

            // Vertex deduplication
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
            }

            TriangleIndices.push_back(VertexIndex);
            Indices.push_back(VertexIndex);
        }

        // Add triangle to material group
        if (!MaterialGroups.contains(MaterialIndex))
        {
            MaterialGroups[MaterialIndex] = TArray<uint32>();
        }
        for (uint32 idx : TriangleIndices)
        {
            MaterialGroups[MaterialIndex].push_back(idx);
        }
    }

    OutMeshData->Indices = Indices;

    // Build GroupInfos from MaterialGroups
    if (MaterialGroups.size() > 0)
    {
        TArray<uint32> SortedIndices;

        for (auto& Pair : MaterialGroups)
        {
            TArray<uint32>& GroupIndices = Pair.second;

            FGroupInfo GroupInfo;
            GroupInfo.StartIndex = static_cast<uint32>(SortedIndices.size());
            GroupInfo.IndexCount = static_cast<uint32>(GroupIndices.size());
            OutMeshData->GroupInfos.push_back(GroupInfo);

            for (uint32 idx : GroupIndices)
            {
                SortedIndices.push_back(idx);
            }
        }

        OutMeshData->Indices = SortedIndices;
        OutMeshData->bHasMaterial = (MaterialGroups.size() > 1);
    }
    else
    {
        FGroupInfo GroupInfo;
        GroupInfo.StartIndex = 0;
        GroupInfo.IndexCount = static_cast<uint32>(Indices.size());
        OutMeshData->GroupInfos.push_back(GroupInfo);
        OutMeshData->bHasMaterial = false;
    }
}

void FBXManager::ParseBoneHierarchy(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
{
    FbxScene* Scene = FbxMeshNode->GetScene();
    int DeformerCount = FbxMeshNode->GetDeformerCount(FbxDeformer::eSkin);

    UE_LOG("FBXManager: Parsing bone hierarchy");
    UE_LOG("  Skin Deformers: %d", DeformerCount);

    if (DeformerCount == 0)
        return;

    FbxSkin* Skin = (FbxSkin*)FbxMeshNode->GetDeformer(0, FbxDeformer::eSkin);
    int ClusterCount = Skin->GetClusterCount();

    UE_LOG("  Clusters (Bones): %d", ClusterCount);

    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();

        if (!BoneNode)
            continue;

        FBoneInfo BoneInfo;
        BoneInfo.BoneName = BoneNode->GetName();

        // Find parent bone
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

        // Offset Matrix (Bind Pose Inverse)
        FbxAMatrix TransformMatrix;
        FbxAMatrix TransformLinkMatrix;
        Cluster->GetTransformMatrix(TransformMatrix);
        Cluster->GetTransformLinkMatrix(TransformLinkMatrix);

        FbxAMatrix OffsetMatrix = TransformLinkMatrix.Inverse() * TransformMatrix;

        BoneInfo.OffsetMatrix = FMatrix::Identity();
        for (int row = 0; row < 4; row++)
        {
            for (int col = 0; col < 4; col++)
            {
                BoneInfo.OffsetMatrix.M[row][col] = static_cast<float>(OffsetMatrix.Get(row, col));
            }
        }

        OutMeshData->Bones.push_back(BoneInfo);
    }

    UE_LOG("FBXManager: Parsed %d bones", OutMeshData->Bones.size());
}

void FBXManager::ParseSkinWeights(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
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

    // Control Point마다 영향을 주는 Bone 정보 저장
    TMap<int, TArray<std::pair<int, float>>> ControlPointWeights;

    // Collect weight information
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

    // Apply skinning to vertices
    OutMeshData->SkinnedVertices.resize(OutMeshData->Vertices.size());

    for (size_t i = 0; i < OutMeshData->Vertices.size(); i++)
    {
        FSkinnedVertex& SkinnedVert = OutMeshData->SkinnedVertices[i];
        SkinnedVert.BaseVertex = OutMeshData->Vertices[i];

        // Map vertex to control point
        int ControlPointIndex = i % ControlPointsCount;

        if (ControlPointWeights.find(ControlPointIndex) != ControlPointWeights.end())
        {
            TArray<std::pair<int, float>>& Weights = ControlPointWeights[ControlPointIndex];

            // Sort by weight (descending)
            std::sort(Weights.begin(), Weights.end(),
                [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                    return a.second > b.second;
                });

            // Apply top 4 weights
            float TotalWeight = 0.0f;
            int NumWeights = std::min(4, (int)Weights.size());

            for (int w = 0; w < NumWeights; w++)
            {
                SkinnedVert.BoneIndices[w] = static_cast<uint8>(Weights[w].first);
                SkinnedVert.BoneWeights[w] = Weights[w].second;
                TotalWeight += Weights[w].second;
            }

            // Normalize weights
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
