# FBX 로딩 및 베이킹 시스템

---

## 트러블 슈팅

**FBX의 한 메시만 로드되는 문제**
- 단순히 FBX의 첫 번째 메시 노드만 찾아서 파싱 → 여러 메시가 있을 경우 나머지 로드 안 됨
- FbxNode를 재귀적으로 탐색하여 Scene에 존재하는 모든 FbxMesh를 찾아 적용
- Index 정보를 단순 업데이트가 아닌 누적(append) 방식으로 처리

**머티리얼 로드 실패**
- 메시 노드에서만 머티리얼을 검색하면 누락 발생
- Scene 전체의 머티리얼을 먼저 로드한 후 메시에 매핑
- 메시 노드에 직접 적용된 머티리얼이 아니라 Scene에 저장된 공유 머티리얼일 수 있음

**라이트 컬러 로드 실패**
- FBX 파일의 텍스처만 로드하고 머티리얼 색상/빛 상수를 로드하지 않음
- 텍스처 없는 FBX는 단색으로만 출력됨
- **해결**: IOSettings에서 Material/Texture 로딩 명시적 활성화 필요
```cpp
IOSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
IOSettings->SetBoolProp(IMP_FBX_TEXTURE, true);
```

---

## 스켈레톤 스키닝

**InverseBindPose (Inverse Bind Pose Matrix)**
- 바인드 포즈(T-Pose)에서 월드 좌표를 본 로컬 좌표로 변환하는 행렬
- FBX에서 추출: `Cluster->GetTransformLinkMatrix().Inverse()`
- 정점을 본 기준 좌표계로 변환

**CurrentGlobalTransform (Global Transform)**
- 현재 애니메이션 프레임에서 본 로컬 좌표를 월드 좌표로 변환하는 행렬
- 프레임마다 부모 본의 Transform을 곱하여 재귀적으로 계산
- 애니메이션으로 본을 이동/회전시킨 결과

**Skinning 공식**
```
FinalVertexPos = VertexPos * InverseBindPose * CurrentGlobalTransform
```
- 버텍스를 본 기준 좌표계로 변환 → 본을 이동/회전 → 최종 월드 위치 계산
- 여러 본의 영향을 받는 경우 가중치(Weight)를 적용하여 블렌딩

---

## 베이킹 시스템 (Derived Data Cache)

**목적**
- FBX 파싱은 FBX SDK 초기화, Scene 로드, 좌표계 변환 등 느린 작업
- 첫 로드 시 파싱 결과를 Binary로 저장 → 이후 로드 시 Binary 직접 읽기로 10배 이상 속도 향상

**캐시 구조**
```
Data/character.fbx (원본)
  ↓ 첫 로드 시 파싱
DerivedDataCache/character.fbx.sm.bin      (StaticMesh 데이터)
DerivedDataCache/character.fbx.sm.mat.bin  (StaticMesh Material)
DerivedDataCache/character.fbx.sk.bin      (SkeletalMesh 데이터)
DerivedDataCache/character.fbx.sk.mat.bin  (SkeletalMesh Material)
```

**파일명 접미사**
- `.sm.bin`: StaticMesh 전용
- `.sk.bin`: SkeletalMesh 전용
- 동일 FBX를 StaticMesh/SkeletalMesh로 각각 로드 시 충돌 방지

**캐시 검증 (Timestamp 기반)**
```cpp
if (fs::last_write_time(BinPath) > fs::last_write_time(FbxPath))
    LoadFromCache();  // 캐시가 최신
else
    ParseFBXAndSaveCache();  // FBX 변경됨, 재파싱
```

**직렬화 구현**
- **FStaticMesh**: Vertices, Indices, GroupInfos, bHasMaterial
- **FSkeletalMesh**: + Bones (계층 구조, 4개 행렬), SkinnedVertices (Bone Weights/Indices)
- **FMaterialInfo**: Diffuse/Ambient/Specular/Emissive 색상 및 텍스처 경로
- **FMatrix**: 4x4 float 배열 (64바이트) 직렬화

**로드 플로우**
```
1. 메모리 캐시 확인 (이미 로드된 에셋?)
2. Binary 캐시 존재 및 최신 여부 확인
3-A. 캐시 최신 → .bin, .mat.bin 읽기 → 메모리 캐시 등록
3-B. 캐시 없음/오래됨 → FBX 파싱 → .bin, .mat.bin 저장 → 메모리 캐시 등록
```

**주의사항**
- FBX 원본이 삭제되어도 캐시가 있으면 로드 가능 (DDC 철학)
- 캐시 손상 시 자동 재생성 (exception handling)
- Material은 별도 파일로 저장하여 Material만 변경 시 메시 재파싱 불필요
