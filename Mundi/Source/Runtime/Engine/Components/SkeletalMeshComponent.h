#pragma once
#include "SkinnedMeshComponent.h"
#include "SkeletalMeshTypes.h"

class URenderer;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
    GENERATED_REFLECTION_BODY()

    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override;

    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    void TickComponent(float DeltaTime) override;
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(USkeletalMeshComponent)

    void OnSerialized() override;

    void SetSkeletalMesh(const FString& FilePath) override;

    USkeletalMesh* GetSkeletalMesh() const  { return SkeletalMesh; }

    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    const TArray<UMaterialInterface*> GetMaterialSlots() const { return MaterialSlots; }
    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;

    UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex);

    void EnsureSkinningReady(D3D11RHI* InDevice);

    void UpdateVertexBuffer(D3D11RHI* InDevice);

    FAABB GetWorldAABB() const;

    // ===== Editor UI 인터페이스 =====
    int32 GetBoneCount() const { return static_cast<int32>(EditableBones.size()); }
    FBone* GetBone(int32 Index);
    FTransform GetBoneWorldTransform(int32 BoneIndex) const;
    int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
    void SetSelectedBoneIndex(int32 Index) { SelectedBoneIndex = Index; }

    // 편집 가능한 Bone 배열 (FBoneInfo에서 초기화)
    TArray<FBone> EditableBones;

    // 시각화
    void RenderDebugVolume(URenderer* Renderer) const;

protected:
    void UpdateBoneMatrices();
    void UpdateSkinningMatrices() override;
    void ClearDynamicMaterials();
    void LoadBonesFromAsset();

    void RenderBonePyramids(
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors) const;

    void RenderJointSpheres(
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors) const;

protected:

    // 부모 뼈 기준의 자식 배치, 부모 기준 상대적 로컬 변환
    // fbx에서 불러오거나 애니메이션에서 가져온다
    TArray<FMatrix> BoneSpaceTransforms = {};

    // 애니메이션이 없을 때 Bind Pose, 존재하면 새로 계산해야함
    // 특정 뼈의 로컬 -> 모델 공간
    TArray<FMatrix> ComponentSpaceTransforms = {};

    // 제거 예정 -> 제거하지 말고 원래대로 스키닝된 정점 저장하고 버퍼 업뎃할 때 사용합시다.
    TArray<FNormalVertex> AnimatedVertices = {};

    int32 SelectedBoneIndex = -1;
};