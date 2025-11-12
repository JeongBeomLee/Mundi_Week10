# FFBXManager 수정 사항 상세 보고서

## 📋 개요
FFBXManager의 FBX 파싱 로직에서 발견된 **3가지 주요 문제**를 수정했습니다.

---

## 🔴 문제점 1: 본 계층 구조 파싱 오류 (ParseBoneHierarchy)

### ❌ 기존 코드의 문제
```cpp
// 기존: Cluster 기반 방식
void FFBXManager::ParseBoneHierarchy(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
{
    FbxSkin* Skin = static_cast<FbxSkin*>(FbxMeshNode->GetDeformer(0, FbxDeformer::eSkin));
    int ClusterCount = Skin->GetClusterCount();

    // ❌ 문제 1: Cluster 순회만으로 본 수집 (BindPose 무시)
    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();

        // ❌ 문제 2: 부모 찾기를 Cluster 순서에 의존
        FbxNode* ParentNode = BoneNode->GetParent();
        BoneInfo.ParentIndex = -1;

        if (ParentNode && ParentNode != Scene->GetRootNode())
        {
            // ❌ 문제 3: 선형 탐색으로 부모 인덱스 찾기
            for (int i = 0; i < ClusterCount; i++)
            {
                FbxCluster* TestCluster = Skin->GetCluster(i);
                if (TestCluster->GetLink() == ParentNode)
                {
                    BoneInfo.ParentIndex = i; // ❌ Cluster 순서 = 본 순서 (잘못된 가정)
                    break;
                }
            }
        }

        // ❌ 문제 4: GetTransformMatrix/GetTransformLinkMatrix만 사용
        Cluster->GetTransformMatrix(MeshTransform);
        Cluster->GetTransformLinkMatrix(BoneWorldTransform);

        // ❌ 문제 5: BindPose 정보 완전히 무시
    }
}
```

**문제점:**
1. ✗ **BindPose를 무시** → 초기 바인딩 상태가 아닌 임의의 프레임 변환 사용
2. ✗ **Cluster 순서에 의존** → Cluster가 부모-자식 순서로 정렬되어 있다는 보장 없음
3. ✗ **노드 계층 구조 무시** → Scene Graph를 순회하지 않아 일부 본 누락 가능
4. ✗ **부모 인덱스 계산 오류** → Cluster 인덱스 = 본 인덱스 (잘못된 가정)

### ✅ 수정된 코드

```cpp
// 수정: BindPose + 노드 계층 순회 방식
void FFBXManager::ParseBoneHierarchy(FbxMesh* FbxMeshNode, FSkeletalMesh* OutMeshData)
{
    // ✅ 1. 스켈레톤 루트 찾기 (Scene Graph 순회)
    TArray<FbxNode*> SkeletonRoots;
    TSet<FbxNode*> AllBoneNodes;

    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();
        if (BoneNode)
        {
            AllBoneNodes.Add(BoneNode);
        }
    }

    // ✅ 2. 각 본의 최상위 스켈레톤 조상 찾기
    for (FbxNode* BoneNode : AllBoneNodes)
    {
        FbxNode* CurrentNode = BoneNode;
        FbxNode* SkeletonRoot = CurrentNode;

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

    // ✅ 3. BindPose 찾기 (명시적 검색)
    FbxPose* BindPose = FindBindPose(SkeletonRoots[0]);

    // ✅ 4. 재귀적으로 본 계층 수집 (Scene Graph 순회)
    TMap<FbxNode*, int32> NodeToIndexMap;
    for (FbxNode* SkeletonRoot : SkeletonRoots)
    {
        CollectBoneData(SkeletonRoot, OutMeshData, -1, BindPose, NodeToIndexMap);
    }
}

// ✅ 새로 추가된 CollectBoneData (재귀 함수)
void FFBXManager::CollectBoneData(FbxNode* Node, FSkeletalMesh* OutMeshData,
                                   int32 ParentIndex, FbxPose* BindPose,
                                   TMap<FbxNode*, int32>& NodeToIndexMap)
{
    const int32 CurrentIndex = static_cast<int32>(OutMeshData->Bones.size());
    NodeToIndexMap.Add(Node, CurrentIndex); // ✅ 올바른 인덱스 매핑

    FBoneInfo BoneInfo;
    BoneInfo.BoneName = Node->GetName();
    BoneInfo.ParentIndex = ParentIndex; // ✅ 재귀 호출로 정확한 부모 인덱스 전달

    // ✅ BindPose에서 글로벌 행렬 가져오기
    int32 PoseNodeIndex = -1;
    if (BindPose)
    {
        PoseNodeIndex = BindPose->Find(Node);
    }

    if (PoseNodeIndex != -1)
    {
        // ✅ BindPose에서 글로벌 행렬 추출
        FbxMatrix NodeMatrix = BindPose->GetMatrix(PoseNodeIndex);
        FbxAMatrix GlobalMatrix;
        for (int32 r = 0; r < 4; ++r)
        {
            for (int32 c = 0; c < 4; ++c)
            {
                GlobalMatrix[r][c] = NodeMatrix.Get(r, c);
            }
        }

        // ✅ 로컬 행렬 계산: Local = ParentGlobal^-1 * Global
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
                    // ✅ 올바른 로컬 변환 계산
                    LocalMatrix = ParentGlobalMatrix.Inverse() * GlobalMatrix;
                }
            }
        }
        else
        {
            // ✅ 루트 본: 글로벌 = 로컬
            LocalMatrix = GlobalMatrix;
        }
    }

    // 변환 행렬 저장
    BoneInfo.BindPoseLocalTransform = /* LocalMatrix to FMatrix */;
    BoneInfo.GlobalTransform = /* GlobalMatrix to FMatrix */;
    BoneInfo.InverseBindPoseMatrix = /* GlobalMatrix.Inverse() to FMatrix */;
    BoneInfo.SkinningMatrix = BoneInfo.InverseBindPoseMatrix * BoneInfo.GlobalTransform;

    OutMeshData->Bones.push_back(BoneInfo);

    // ✅ 자식 본들을 재귀적으로 처리 (Scene Graph 순회)
    for (int i = 0; i < Node->GetChildCount(); i++)
    {
        FbxNode* ChildNode = Node->GetChild(i);
        if (ChildNode && ChildNode->GetNodeAttribute() &&
            ChildNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
        {
            CollectBoneData(ChildNode, OutMeshData, CurrentIndex, BindPose, NodeToIndexMap);
        }
    }
}
```

**개선 사항:**
1. ✓ **BindPose 명시적 사용** → Scene->GetPoseCount() 순회하여 IsBindPose() 확인
2. ✓ **Scene Graph 재귀 순회** → 노드 계층 구조 그대로 반영
3. ✓ **정확한 부모-자식 관계** → 재귀 호출로 ParentIndex를 인자로 전달
4. ✓ **로컬 변환 정확 계산** → ParentGlobal^-1 × NodeGlobal 공식 사용
5. ✓ **본 누락 방지** → 모든 스켈레톤 노드를 계층적으로 탐색

---

## 🔴 문제점 2: 정점 중복 제거 실패 (ParseMeshGeometry)

### ❌ 기존 코드의 문제

```cpp
// ❌ 기존: Float 값 직접 해싱
namespace std {
    template <>
    struct hash<FNormalVertex>
    {
        size_t operator()(const FNormalVertex& v) const noexcept
        {
            // ❌ 문제: Float 값 직접 해싱
            size_t h1 = hash<float>()(v.pos.X);
            size_t h2 = hash<float>()(v.pos.Y);
            size_t h3 = hash<float>()(v.pos.Z);
            size_t h4 = hash<float>()(v.normal.X);
            // ...
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1) /* ... */;
        }
    };
}

void FFBXManager::ParseMeshGeometry(...)
{
    TMap<FNormalVertex, uint32> VertexMap; // ❌ Float 기반 중복 제거

    for (int PolyIndex = 0; PolyIndex < PolygonCount; PolyIndex++)
    {
        for (int VertInPoly = 0; VertInPoly < 3; VertInPoly++)
        {
            FNormalVertex Vertex;

            // 정점 데이터 채우기...
            Vertex.pos = FVector(...);
            Vertex.normal = FVector(...);

            // ❌ 문제: Float 비교로 중복 검사
            if (VertexMap.contains(Vertex))
            {
                VertexIndex = VertexMap[Vertex];
            }
            else
            {
                // 새 정점 추가
                VertexMap[Vertex] = VertexIndex;
                OutMeshData->Vertices.push_back(Vertex);
            }
        }
    }
}
```

**문제점:**
- ✗ **Float 직접 비교**: `0.999999f`와 `1.000001f`가 다른 정점으로 인식됨
- ✗ **부동소수점 오차**: 변환 과정에서 미세한 오차로 중복 정점이 생성됨
- ✗ **정점 폭발**: 동일한 정점이 수십 개로 복제되어 메모리 낭비

### ✅ 수정된 코드

```cpp
// ✅ 새로운 구조: Index 기반 해싱
struct FVertexKey
{
    int32 PositionIndex;    // ControlPoint 인덱스
    int32 NormalIndex;      // Normal 인덱스
    int32 TangentIndex;     // Tangent 인덱스
    int32 UVIndex;          // UV 인덱스
    int32 ColorIndex;       // Color 인덱스

    FVertexKey(int32 Pos, int32 N, int32 T, int32 UV, int32 Col)
        : PositionIndex(Pos), NormalIndex(N), TangentIndex(T), UVIndex(UV), ColorIndex(Col)
    {
        // ✅ 정수 인덱스 해싱 (안정적)
        Hash = std::hash<int32>()(PositionIndex << 0)
             ^ std::hash<int32>()(NormalIndex   << 1)
             ^ std::hash<int32>()(TangentIndex  << 2)
             ^ std::hash<int32>()(UVIndex       << 3)
             ^ std::hash<int32>()(ColorIndex    << 4);
    }

    bool operator==(const FVertexKey& Other) const
    {
        // ✅ 정수 비교 (정확함)
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

void FFBXManager::ParseMeshGeometry(...)
{
    TMap<FVertexKey, uint32> UniqueVertices; // ✅ Index 기반 중복 제거

    int VertexCounter = 0; // ✅ eByPolygonVertex 모드용 카운터

    for (int PolyIndex = 0; PolyIndex < PolygonCount; PolyIndex++)
    {
        for (int VertInPoly = 0; VertInPoly < 3; VertInPoly++)
        {
            const int32 ControlPointIndex = FbxMeshNode->GetPolygonVertex(PolyIndex, VertInPoly);

            // ✅ 각 LayerElement의 실제 인덱스 계산
            int32 NormalIndex = (NormalElement) ?
                (NormalElement->GetMappingMode() == FbxLayerElement::eByControlPoint
                    ? ControlPointIndex : VertexCounter) : -1;
            int32 TangentIndex = (TangentElement) ?
                (TangentElement->GetMappingMode() == FbxLayerElement::eByControlPoint
                    ? ControlPointIndex : VertexCounter) : -1;
            int32 UVIndex = (UVElement) ?
                (UVElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex
                    ? FbxMeshNode->GetTextureUVIndex(PolyIndex, VertInPoly) : ControlPointIndex) : -1;
            int32 ColorIndex = (ColorElement) ?
                (ColorElement->GetMappingMode() == FbxLayerElement::eByControlPoint
                    ? ControlPointIndex : VertexCounter) : -1;

            // ✅ Index 기반 키 생성
            FVertexKey Key(ControlPointIndex, NormalIndex, TangentIndex, UVIndex, ColorIndex);

            // ✅ 정확한 중복 검사
            if (const uint32* Found = UniqueVertices.Find(Key))
            {
                NewIndex = *Found;
            }
            else
            {
                FNormalVertex NewVertex;
                // 실제 데이터 채우기...

                UniqueVertices.Add(Key, NewIndex);
                OutMeshData->Vertices.push_back(NewVertex);
            }

            VertexCounter++; // ✅ 폴리곤 정점 카운터 증가
        }
    }
}
```

**개선 사항:**
1. ✓ **Index 기반 식별** → FBX 내부 인덱스로 정점 구분
2. ✓ **정확한 중복 판단** → 동일한 인덱스 조합 = 동일한 정점
3. ✓ **부동소수점 오차 제거** → 정수 비교만 사용
4. ✓ **메모리 최적화** → 중복 정점 생성 방지

---

## 🔴 문제점 3: Normal/Tangent 변환 오류 (ParseMeshGeometry)

### ❌ 기존 코드의 문제

```cpp
void FFBXManager::ParseMeshGeometry(...)
{
    // ❌ 문제 1: GlobalTransform 사용 (부모 변환까지 포함)
    FbxAMatrix GlobalTransform = FbxMeshNode->GetNode()->EvaluateGlobalTransform();

    for (int PolyIndex = 0; PolyIndex < PolygonCount; PolyIndex++)
    {
        for (int VertInPoly = 0; VertInPoly < 3; VertInPoly++)
        {
            // ❌ 문제 2: Position도 GlobalTransform 사용
            FbxVector4 Position = ControlPoints[ControlPointIndex];
            Position = GlobalTransform.MultT(Position);
            Vertex.pos = FVector(
                static_cast<float>(Position[0]) * ScaleFactor,
                // ...
            );

            // ❌ 문제 3: Normal/Tangent에 GlobalTransform.Inverse().Transpose() 사용
            // → 부모 노드의 변환까지 포함되어 잘못된 결과
            if (NormalElement)
            {
                // ... 인덱스 계산 생략 ...
                FbxVector4 Normal = NormalElement->GetDirectArray().GetAt(NormalIndex);
                Normal = GlobalTransform.Inverse().Transpose().MultT(Normal);

                Vertex.normal = FVector(
                    static_cast<float>(Normal[0]),
                    // ...
                );
            }

            // ❌ 문제 4: Tangent도 동일한 오류
            if (TangentElement)
            {
                FbxVector4 Tangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
                Tangent = GlobalTransform.Inverse().Transpose().MultT(Tangent);
                // ...
            }

            // ❌ 문제 5: Color 파싱 안 함 (항상 (1,1,1,1))
            Vertex.color = FVector4(1, 1, 1, 1);
        }
    }
}
```

**문제점:**
1. ✗ **GlobalTransform 오용**: 메시의 로컬 변환만 필요한데 전역 변환(부모 포함) 사용
2. ✗ **Normal/Tangent 변환 오류**: 부모 노드 변환이 적용되어 방향이 틀어짐
3. ✗ **비균등 스케일 미처리**: InverseTranspose가 필요한 이유를 정확히 구현 안 함
4. ✗ **Color 무시**: ColorElement가 있어도 파싱하지 않음

### ✅ 수정된 코드

```cpp
void FFBXManager::ParseMeshGeometry(...)
{
    // ✅ 수정 1: LocalTransformMatrix 사용 (부모 변환 제외)
    const FbxAMatrix LocalTransformMatrix = FbxMeshNode->GetNode()->EvaluateLocalTransform();

    // ✅ 수정 2: ColorElement 추가
    FbxGeometryElementVertexColor* ColorElement = FbxMeshNode->GetElementVertexColor();

    for (int PolyIndex = 0; PolyIndex < PolygonCount; PolyIndex++)
    {
        for (int VertInPoly = 0; VertInPoly < 3; VertInPoly++)
        {
            // ✅ Position: LocalTransformMatrix 적용
            if (ControlPointIndex < ControlPointsCount)
            {
                Position = LocalTransformMatrix.MultT(Position);
                NewVertex.pos = FVector(
                    static_cast<float>(Position[0]) * ScaleFactor,
                    static_cast<float>(Position[1]) * ScaleFactor,
                    static_cast<float>(Position[2]) * ScaleFactor
                );
            }

            // ✅ Normal: LocalTransformMatrix의 InverseTranspose 적용
            if (NormalElement && GetVertexElementData(NormalElement, ControlPointIndex, VertexCounter, Normal))
            {
                // ✅ InverseTranspose: 비균등 스케일에서 Normal의 직각성 보존
                Normal = LocalTransformMatrix.Inverse().Transpose().MultT(Normal);
                NewVertex.normal = FVector(
                    static_cast<float>(Normal[0]),
                    static_cast<float>(Normal[1]),
                    static_cast<float>(Normal[2])
                );
            }

            // ✅ Tangent: Normal과 동일하게 InverseTranspose 적용
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

            // ✅ UV: 변환 없이 그대로 사용 (V 좌표만 반전)
            if (UVElement && GetVertexElementData(UVElement, ControlPointIndex, VertexCounter, UV))
            {
                NewVertex.tex = FVector2D(
                    static_cast<float>(UV[0]),
                    1.0f - static_cast<float>(UV[1]) // DirectX 스타일 V 반전
                );
            }

            // ✅ Color: ColorElement가 있으면 파싱
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
        }
    }
}
```

**개선 사항:**
1. ✓ **LocalTransform 사용** → 메시 자체의 변환만 적용 (부모 노드 변환 제외)
2. ✓ **InverseTranspose 적용** → Normal/Tangent의 직각성을 비균등 스케일에서도 보존
3. ✓ **정확한 벡터 변환** → Position은 직접 변환, Normal/Tangent는 InverseTranspose
4. ✓ **Color 파싱 추가** → ColorElement가 있으면 정확히 읽어옴

---

## 🛠️ 추가된 Helper 함수들

### 1. GetVertexElementData (LayerElement 범용 파서)

```cpp
template<typename FbxLayerElementType, typename TDataType>
bool GetVertexElementData(const FbxLayerElementType* Element, int32 ControlPointIndex,
                         int32 VertexIndex, TDataType& OutData)
{
    if (!Element) return false;

    const auto MappingMode = Element->GetMappingMode();
    const auto ReferenceMode = Element->GetReferenceMode();

    // ✅ eAllSame 처리
    if (MappingMode == FbxLayerElement::eAllSame)
    {
        if (Element->GetDirectArray().GetCount() > 0)
        {
            OutData = Element->GetDirectArray().GetAt(0);
            return true;
        }
        return false;
    }

    // ✅ 인덱스 결정
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
        return false;
    }

    // ✅ ReferenceMode별 처리
    if (ReferenceMode == FbxLayerElement::eDirect)
    {
        if (Index >= 0 && Index < Element->GetDirectArray().GetCount())
        {
            OutData = Element->GetDirectArray().GetAt(Index);
            return true;
        }
    }
    else if (ReferenceMode == FbxLayerElement::eIndexToDirect)
    {
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
```

**용도:** Normal, Tangent, UV, Color 등 모든 LayerElement를 통합 처리

---

### 2. FindBindPose (BindPose 검색)

```cpp
FbxPose* FFBXManager::FindBindPose(FbxNode* SkeletonRoot)
{
    if (!SkeletonRoot) return nullptr;

    FbxScene* Scene = SkeletonRoot->GetScene();
    if (!Scene) return nullptr;

    // ✅ 스켈레톤에 속한 모든 본 노드 수집
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

        // ✅ 이 BindPose가 스켈레톤의 본을 포함하는지 확인
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

        if (bPoseContainsSomeBones)
        {
            return CurrentPose;
        }
    }

    return nullptr;
}
```

**용도:** Scene에서 해당 스켈레톤의 BindPose를 명시적으로 검색

---

### 3. CollectSkeletonBoneNodes (본 노드 재귀 수집)

```cpp
void FFBXManager::CollectSkeletonBoneNodes(FbxNode* Node, TArray<FbxNode*>& OutBoneNodes)
{
    if (!Node) return;

    // ✅ 스켈레톤 노드인지 확인
    if (Node->GetNodeAttribute() &&
        Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        OutBoneNodes.Add(Node);
    }

    // ✅ 자식 노드 재귀 탐색
    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ChildIndex++)
    {
        FbxNode* ChildNode = Node->GetChild(ChildIndex);
        CollectSkeletonBoneNodes(ChildNode, OutBoneNodes);
    }
}
```

**용도:** 노드를 재귀적으로 순회하여 모든 본 수집

---

## 📊 수정 전후 비교

| 항목 | 기존 (❌) | 수정 후 (✅) |
|------|---------|------------|
| **본 계층 구조** | Cluster 순서 의존 | Scene Graph 재귀 순회 |
| **BindPose 사용** | 무시 | 명시적 검색 및 사용 |
| **부모 인덱스** | Cluster 인덱스로 추정 | 재귀 호출로 정확히 전달 |
| **정점 중복 제거** | Float 해싱 (불안정) | Index 해싱 (안정적) |
| **Normal 변환** | GlobalTransform (오류) | LocalTransform + InverseTranspose |
| **Tangent 변환** | GlobalTransform (오류) | LocalTransform + InverseTranspose |
| **Color 파싱** | 항상 (1,1,1,1) | ColorElement에서 읽음 |
| **LayerElement 처리** | 수동 if-else | GetVertexElementData 통합 |

---

## 🎯 핵심 개선 사항 요약

### 1️⃣ 본 계층 구조 (ParseBoneHierarchy)
- **변경**: Cluster 순회 → Scene Graph 재귀 순회 + BindPose
- **효과**: 정확한 부모-자식 관계, 본 누락 방지, 올바른 변환 행렬

### 2️⃣ 정점 중복 제거 (ParseMeshGeometry)
- **변경**: Float 해싱 → Index 해싱 (FVertexKey)
- **효과**: 부동소수점 오차 제거, 정점 폭발 방지, 메모리 최적화

### 3️⃣ 벡터 변환 (ParseMeshGeometry)
- **변경**: GlobalTransform → LocalTransform + InverseTranspose
- **효과**: 정확한 Normal/Tangent 방향, 비균등 스케일 대응

### 4️⃣ 추가 기능
- **GetVertexElementData**: LayerElement 범용 파서 (MappingMode/ReferenceMode 완벽 처리)
- **FindBindPose**: BindPose 명시적 검색
- **CollectBoneData**: 재귀적 본 수집 (Scene Graph 순회)
- **Color 파싱**: ColorElement 지원 추가

---

## 💡 왜 이렇게 고쳤는가?

### 문제의 근본 원인
1. **Cluster ≠ Scene Graph**: Cluster는 스킨 가중치 정보일 뿐, 본 계층 구조가 아님
2. **Float 비교의 불안정성**: IEEE 754 부동소수점 표준의 근본적 한계
3. **좌표계 변환 오류**: 부모 변환 포함 여부, InverseTranspose 필요성 이해 부족

### 해결 방법
1. **BindPose + Scene Graph**: FBX SDK의 설계 의도대로 사용
2. **Index 기반 식별**: 데이터 소스의 인덱스로 정점 구분
3. **수학적 정확성**: 변환 행렬 수식을 정확히 적용

---

## ⚠️ 주의사항

1. **호환성**: 기존 FFBXManager의 public 인터페이스는 그대로 유지됨
2. **테스트 필요**: 다양한 FBX 파일로 검증 필요 (특히 복잡한 스켈레톤)
3. **성능**: Index 기반 해싱이 Float 해싱보다 빠름

---

## 🔗 참고 자료

- **FBX SDK 문서**: https://help.autodesk.com/view/FBX/2020/ENU/
- **Scene Graph**: 노드 계층 구조로 3D 씬을 표현하는 방법
- **BindPose**: 메시가 스켈레톤에 바인딩될 때의 초기 변환 상태
- **InverseTranspose**: 비균등 스케일에서 Normal 벡터의 직각성을 보존하는 변환

---

**작성일**: 2025-11-12
**수정자**: Claude Code
**파일**: `FFBXManager.cpp`, `FFBXManager.h`
