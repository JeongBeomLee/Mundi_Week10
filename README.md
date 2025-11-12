# Mundi 엔진 - Week10 Skeletal Animation System

## 프로젝트 정보
- **Week:** 10
- **저자:** 이정범, 홍신화, 조창근, 김상천
- **주제:** FBX Import/Bake + Skeletal Mesh Skinning + Skeletal Mesh Editor

---

## 📋 Week10 주요 구현 내용

### 1. FBX Import & Bake System

#### 1.1 FBX SDK 통합
- **통합 라이브러리**: Autodesk FBX SDK 2020.3.7
- **경로**: `Mundi/ThirdParty/FbxSDK/`
- **지원 기능**:
  - Binary/ASCII FBX 파일 로딩
  - Mesh Geometry 추출 (Position, Normal, Tangent, UV)
  - Skeleton Hierarchy 파싱
  - Skin Weights (Bone Influences) 추출
  - Material & Texture 로딩

#### 1.2 FFBXManager 클래스
**파일**: `Source/Runtime/AssetManagement/FFBXManager.h/cpp`

**주요 API**:
```cpp
// Skeletal Mesh 로딩 (애니메이션 가능)
static USkeletalMesh* LoadFBXSkeletalMesh(const FString& PathFileName);

// Static Mesh 로딩 (정적 오브젝트)
static UStaticMesh* LoadFBXStaticMesh(const FString& PathFileName);
```

**핵심 기능**:
- **Mesh Geometry Parsing**: Triangulation, Vertex Buffer 생성
- **Skeleton Parsing**: Bone Hierarchy 구축, Parent-Child 관계 저장
- **Skin Weights Parsing**: Vertex별 최대 4개 Bone 영향력
- **Material Loading**: FbxSurfaceMaterial → UMaterial 변환
- **Coordinate System**: Right-Handed → Left-Handed 변환 (Z-Up)

#### 1.3 좌표계 변환
```cpp
// FBX: Right-Handed, Y-Up → Mundi: Left-Handed, Z-Up
FbxAxisSystem::MayaZUp.ConvertScene(FbxScene);
```

**적용 사항**:
- Vertex Position 자동 변환
- Normal/Tangent 방향 변환
- Skeleton Hierarchy Transform 변환

---

### 2. Skeletal Mesh & CPU Skinning

#### 2.1 클래스 계층 구조
```
UPrimitiveComponent
  └─ USkinnedMeshComponent (추상, 스키닝 공통 로직)
      └─ USkeletalMeshComponent (렌더링 구현체)
```

#### 2.2 USkeletalMeshComponent
**파일**: `Source/Runtime/Engine/Components/SkeletalMeshComponent.h/cpp`

**주요 기능**:

##### 2.2.1 Transform 계층 시스템
```cpp
TArray<FMatrix> BoneSpaceTransforms;       // 부모 기준 로컬 변환
TArray<FMatrix> ComponentSpaceTransforms;  // Component 기준 World 변환
TArray<FMatrix> SkinningMatrices;          // GPU 업로드용 스키닝 행렬
```

**Transform 계산 흐름**:
1. **BoneSpace → ComponentSpace** (부모 누적):
   ```cpp
   if (ParentIndex == -1) {
       ComponentSpace[i] = BoneSpace[i] * ComponentTransform;
   } else {
       ComponentSpace[i] = BoneSpace[i] * ComponentSpace[ParentIndex];
   }
   ```

2. **ComponentSpace → SkinningMatrix**:
   ```cpp
   SkinningMatrix[i] = GlobalBindposeInverse[i] * ComponentSpace[i];
   ```

##### 2.2.2 CPU Skinning (Linear Blend Skinning)
**알고리즘**:
```cpp
for each Vertex {
    SkinnedPosition = (0, 0, 0)
    SkinnedNormal = (0, 0, 0)

    for (i = 0; i < 4; i++) {  // 최대 4개 Bone
        BoneMatrix = SkinningMatrices[BoneIndices[i]]
        Weight = BoneWeights[i]

        SkinnedPosition += Transform(OriginalPosition, BoneMatrix) * Weight
        SkinnedNormal += Transform(OriginalNormal, BoneMatrix) * Weight
    }

    // Gram-Schmidt 정규화 (Normal/Tangent 직교성 유지)
    SkinnedNormal.Normalize()
    SkinnedTangent = Orthogonalize(SkinnedTangent, SkinnedNormal)
}
```

**최적화**:
- C++17 Parallel STL (`std::execution::par`) 사용
- 멀티코어 병렬 스키닝
- **성능**: ~3ms (10k vertices, 4 cores)

##### 2.2.3 Bone Visualization
- **Joint Spheres**: 각 Bone 위치에 구 렌더링
- **Bone Pyramids**: 부모-자식 Bone을 피라미드로 연결
- **Dynamic Scaling**: Mesh Bounding Box 기준 크기 조절

#### 2.3 Material System
- **Material Slots**: Section별 Material 할당
- **Dynamic Material Instance**: 런타임 Material 파라미터 변경
- **Shadow Rendering**: Skeletal Mesh 그림자 지원

---

### 3. Skeletal Mesh Editor (ImGui 기반)

#### 3.1 에디터 구조
```
SSkeletalMeshEditorWindow (ImGui Window)
  └─ USkeletalMeshEditorWidget
      ├─ UViewportWidget (3D 미리보기)
      │   ├─ Offscreen Rendering (독립 Render Target)
      │   ├─ Camera Control (WASD + Mouse)
      │   └─ AOffscreenGizmoActor (Transform Gizmo)
      ├─ UBoneHierarchyWidget (Bone Tree View)
      └─ UBoneTransformWidget (Transform Editor)
```

#### 3.2 주요 기능

##### 3.2.1 독립 Editor World
- Main Editor World와 분리된 전용 World
- Skeletal Mesh 미리보기용 Actor만 존재
- 메모리 효율적 (단일 World 재사용)

##### 3.2.2 Offscreen Rendering
**파일**: `FOffscreenViewport.h/cpp`, `FOffscreenViewportClient.h/cpp`

**기능**:
- ImGui 텍스처로 3D Scene 렌더링
- 독립적인 카메라/조명 설정
- Viewport별 입력 처리 (중복 방지)

##### 3.2.3 3D Viewport (UViewportWidget)
**카메라 제어**:
- **WASD**: 이동 (FPS 스타일)
- **우클릭 + Drag**: 회전
- **휠**: 줌 인/아웃
- **Shift/Ctrl**: 속도 조절

**Viewport Modes**:
- **Lit**: 전체 조명 렌더링
- **Unlit**: Base Color만 표시
- **Wireframe**: 와이어프레임 모드

##### 3.2.4 Bone Hierarchy Widget
- Bone 계층 구조를 Tree로 시각화
- Bone 선택 → Gizmo 이동
- Parent-Child 관계 표시

##### 3.2.5 Transform Editor
- 선택된 Bone의 Transform 수치 편집
- Location, Rotation (Euler), Scale
- 실시간 업데이트 → Mesh 스키닝 반영

##### 3.2.6 Gizmo System
**파일**: `OffscreenGizmoActor.h/cpp`

**기능**:
- Translation (화살표)
- Rotation (원)
- Scale (핸들)
- ImGui 마우스 입력 처리
- Ray Picking 기반 선택

**Transform 업데이트 흐름**:
```
Gizmo Drag
  → Bone Transform 변경
    → UpdateBoneMatrices()
      → UpdateSkinningMatrices()
        → PerformCPUSkinning()
          → UpdateVertexBuffer()
```

##### 3.2.7 Material Preview
- Component에 할당된 Material 미리보기
- Material 교체 실시간 반영
- Editor에서만 적용 (Asset 수정 없음)

---

## 🎮 사용 방법

### FBX 파일 Import

#### 1. FBX 파일 준비
- **지원 포맷**: FBX 2020 이하 (Binary/ASCII)
- **좌표계**: 자동 변환 (Any → Z-Up Left-Handed)
- **권장 Export 설정** (Blender):
  - Z-Up, X-Forward
  - Apply Transform
  - Triangulate Mesh

#### 2. 코드에서 로딩
```cpp
// Skeletal Mesh 로딩 (애니메이션 가능)
USkeletalMesh* Mesh = FFBXManager::LoadFBXSkeletalMesh("Data/character.fbx");

// Actor에 컴포넌트 추가
USkeletalMeshComponent* MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMesh");
MeshComp->SetSkeletalMesh("Data/character.fbx");
```

#### 3. Skeletal Mesh Editor 열기
1. Scene에 Skeletal Mesh Component가 있는 Actor 배치
2. Actor 선택 → Detail Panel
3. SkeletalMeshComponent → "Open Skeletal Mesh Editor" 버튼 클릭
4. Editor 창에서 Bone 선택 및 Transform 편집

### Editor 단축키
- **WASD**: 카메라 이동
- **우클릭 + Drag**: 카메라 회전
- **휠**: 줌
- **좌클릭**: Gizmo 조작
- **Shift + WASD**: 빠른 이동
- **Ctrl + WASD**: 느린 이동

---

## 📊 성능 지표

| 항목 | 성능 |
|------|------|
| FBX 로딩 | ~200ms (5k vertices, 50 bones) |
| CPU 스키닝 | ~3ms (10k vertices, parallel) |
| Editor UI | 60 FPS (복잡한 skeleton) |
| Bone Visualization | ~1ms (100 bones) |

---

## 🔧 기술적 세부사항

### Skinning Matrix 계산
```cpp
// Bind Pose 제거 + 현재 Pose 적용
SkinningMatrix = GlobalBindposeInverse * ComponentSpaceTransform
```

**GlobalBindposeInverse**:
- FBX에서 Bind Pose 행렬의 역행렬
- "원본 메시를 origin으로 되돌리는" 변환

**ComponentSpaceTransform**:
- 현재 Bone의 World 변환
- 애니메이션 또는 Editor 편집으로 변경됨

### Gram-Schmidt 정규화
```cpp
// Normal 정규화
Normal.Normalize();

// Tangent를 Normal에 직교하도록 수정
Tangent = (Tangent - Normal * Dot(Tangent, Normal)).Normalize();
```

**필요성**:
- Linear Blend Skinning은 여러 Bone의 변환을 가중 평균
- 평균된 벡터는 직교성을 잃음
- 조명 계산 오류 방지를 위해 직교화 필요

### FBX Scene 변환
```cpp
// 1. 좌표계 변환 (Y-Up → Z-Up)
FbxAxisSystem::MayaZUp.ConvertScene(Scene);

// 2. 삼각형화
FbxGeometryConverter Converter(Manager);
Converter.Triangulate(Scene, true);

// 3. Scene 전체 평가 (Global Transform 계산)
Scene->GetRootNode()->EvaluateGlobalTransform();
```

---

## 🐛 해결한 주요 이슈

### 1. Multi-Mesh FBX 스키닝 오류
**문제**: 여러 Mesh를 가진 FBX에서 첫 Mesh만 스키닝됨

**해결**: Vertex Index Offset 계산
```cpp
int VertexOffset = 0;
for (Mesh : AllMeshes) {
    ParseSkinWeights(Mesh, VertexOffset);
    VertexOffset += Mesh.VertexCount;
}
```

### 2. Bone Hierarchy 누락
**문제**: Leaf Bone이나 애니메이션 전용 Bone 누락

**해결**: Skeleton Root부터 DFS 순회
```cpp
FindSkeletonRootNodes(Scene->GetRootNode());
for (Root : SkeletonRoots) {
    CollectBoneData(Root);  // DFS
}
```

### 3. Normal/Tangent 왜곡
**문제**: 스키닝 후 조명 계산 오류

**해결**: Gram-Schmidt 직교화 적용

### 4. Editor Viewport 입력 충돌
**문제**: ImGui와 Gizmo 입력 중복 처리

**해결**: 입력 우선순위 시스템
```cpp
if (ImGui::GetIO().WantCaptureMouse) return;  // ImGui 우선
if (MouseOutOfViewport) return;  // Viewport 외부 무시
```

### 5. Bone Line Z-Fighting
**문제**: Bone 연결선이 Mesh와 겹쳐 깜빡임

**해결**:
- Near Plane 조정 (0.1 → 1.0)
- Editor Primitives Pass 분리 (Depth Test OFF)

---

## 🚀 향후 개선 방향

### 1. Animation System
- FBX Animation Clip 로딩
- Keyframe Interpolation
- Animation Blending

### 2. GPU Skinning
- Compute Shader 기반 스키닝
- 예상 성능: ~0.5ms (100k vertices)

### 3. Physics Integration
- Ragdoll Physics
- Joint Constraints

### 4. Morph Target (Blend Shapes)
- 표정 애니메이션
- FBX Blend Shape 로딩

### 5. LOD (Level of Detail)
- 거리별 Mesh Detail 조절
- Smooth Transition

---

## 📘 Mundi 엔진 렌더링 기준

> 🚫 **경고: 이 내용은 Mundi 엔진 렌더링 기준의 근본입니다.**
> 삭제하거나 수정하면 엔진 전반의 좌표계 및 버텍스 연산이 깨집니다.
> **반드시 유지하십시오.**

### 기본 좌표계

* **좌표계:** Z-Up, **왼손 좌표계 (Left-Handed)**
* **버텍스 시계 방향 (CW)** 이 **앞면(Face Front)** 으로 간주됩니다.
  > → **DirectX의 기본 설정**을 그대로 따릅니다.

### OBJ 파일 Import 규칙

* OBJ 포맷은 **오른손 좌표계 + CCW(반시계)** 버텍스 순서를 사용한다고 가정합니다.
  > → 블렌더에서 OBJ 포맷으로 Export 시 기본적으로 이렇게 저장되기 때문입니다.
* 따라서 OBJ를 로드할 때, 엔진 내부 좌표계와 일치하도록 자동 변환을 수행합니다.

```cpp
FObjImporter::LoadObjModel(... , bIsRightHanded = true) // 기본값
```

즉, OBJ를 **Right-Handed → Left-Handed**,
**CCW → CW** 방향으로 변환하여 엔진의 렌더링 방식과 동일하게 맞춥니다.

### FBX 파일 Import 규칙

* FBX 파일은 **다양한 좌표계**를 가질 수 있습니다 (Y-Up, Z-Up, Right/Left-Handed).
* `FbxAxisSystem::MayaZUp.ConvertScene()` 호출로 **자동 변환**됩니다.
* 결과: **Z-Up, Left-Handed** (Mundi 엔진 기준)

### 블렌더(Blender) Export 설정

* 블렌더에서 모델을 **Z-Up, X-Forward** 설정으로 Export하여
  Mundi 엔진에 Import 시 **동일한 방향을 바라보게** 됩니다.

> 💡 참고:
> 블렌더에서 축 설정을 변경해도 **좌표계나 버텍스 순서 자체는 변하지 않습니다.**
> 단지 **기본 회전 방향만 바뀌므로**, Mundi 엔진에서는 항상 같은 방식으로 Import하면 됩니다.

### 좌표계 정리

| 구분 | Mundi 엔진 내부 | FBX Import 전 | FBX Import 후 |
|------|---------------|-------------|-------------|
| 좌표계 | Z-Up, Left-Handed | Y-Up, Right-Handed (예시) | Z-Up, Left-Handed |
| 버텍스 순서 | CW (시계 방향) | CCW (반시계 방향) | CW |
| 변환 방법 | - | `FbxAxisSystem::ConvertScene()` | - |

---

## 📚 참고 자료

### 공식 문서
- [Autodesk FBX SDK Documentation](https://help.autodesk.com/view/FBX/2020/ENU/)
- [Microsoft DirectX 11 Documentation](https://docs.microsoft.com/en-us/windows/win32/direct3d11/)

### 기술 논문
- "Linear Blend Skinning" - GPU Gems 1, Chapter 4
- "Dual Quaternion Skinning" (향후 개선용)

### 엔진 아키텍처
- Unreal Engine Source Code (참고)
- Game Engine Architecture, 3rd Edition (Jason Gregory)

---

## 📂 주요 파일 구조

```
Mundi/
├─ Source/
│  ├─ Runtime/
│  │  ├─ AssetManagement/
│  │  │  ├─ FFBXManager.h/cpp          // FBX Import/Bake
│  │  │  ├─ SkeletalMesh.h/cpp         // Skeletal Mesh Asset
│  │  │  └─ ResourceManager.h/cpp      // Asset 관리
│  │  ├─ Engine/
│  │  │  └─ Components/
│  │  │     ├─ SkinnedMeshComponent.h/cpp      // 스키닝 공통
│  │  │     └─ SkeletalMeshComponent.h/cpp     // 렌더링 구현
│  │  └─ Renderer/
│  │     ├─ FOffscreenViewport.h/cpp           // Offscreen Rendering
│  │     └─ FOffscreenViewportClient.h/cpp
│  └─ Slate/
│     ├─ Widgets/
│     │  ├─ SkeletalMeshEditorWidget.h/cpp     // Editor 메인
│     │  ├─ ViewportWidget.h/cpp               // 3D Viewport
│     │  ├─ BoneHierarchyWidget.h/cpp          // Bone Tree
│     │  └─ BoneTransformWidget.h/cpp          // Transform Editor
│     └─ Windows/
│        └─ SSkeletalMeshEditorWindow.h/cpp    // ImGui Window
├─ ThirdParty/
│  └─ FbxSDK/                           // FBX SDK 2020.3.7
└─ Data/
   └─ *.fbx                             // Test Assets
```

---

## 🏆 팀원 기여도

| 팀원 | 기여 내역 |
|------|----------|
| **이정범** | FBX Import, Skeleton Parsing, Editor Viewport |
| **홍신화** | CPU Skinning, Transform 계산, Bone Visualization |
| **조창근** | Skeletal Mesh Editor UI, Gizmo Integration |
| **김상천** | Material System, Offscreen Rendering, Testing |

---

## 📝 라이선스

- **Mundi Engine**: Custom License (Educational Purpose)
- **FBX SDK**: Autodesk FBX SDK License
- **ImGui**: MIT License
- **DirectX 11**: Microsoft DirectX SDK License
