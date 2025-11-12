#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "MeshBatchElement.h"
#include "WorldPartitionManager.h"
#include "Widgets/BoneTransformCalculator.h"
#include "Renderer.h"


IMPLEMENT_CLASS(USkeletalMeshComponent)
BEGIN_PROPERTIES(USkeletalMeshComponent)
MARK_AS_COMPONENT("스켈레탈 메시 컴포넌트", "스켈레탈 메시")
ADD_PROPERTY_SKELETALMESH(USkeletalMesh*, SkeletalMesh, "Skeletal Mesh", true)
ADD_PROPERTY_ARRAY(EPropertyType::Material, MaterialSlots, "Materials", true)
END_PROPERTIES(USkeletalMeshComponent)

USkeletalMeshComponent::USkeletalMeshComponent()
{
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    if (SkeletalMesh)
    {
        SkeletalMesh->EraseUsingComponets(this);
    }

    ClearDynamicMaterials();
}

void USkeletalMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    const FString MaterialSlotsKey = "MaterialSlots";

    if (bInIsLoading) // --- 로드 ---
    {
        // 1. 로드 전 기존 동적 인스턴스 모두 정리
        ClearDynamicMaterials();

        JSON SlotsArrayJson;
        if (FJsonSerializer::ReadArray(InOutHandle, MaterialSlotsKey, SlotsArrayJson, JSON::Make(JSON::Class::Array), false))
        {
            MaterialSlots.resize(SlotsArrayJson.size());

            for (int i = 0; i < SlotsArrayJson.size(); ++i)
            {
                JSON& SlotJson = SlotsArrayJson.at(i);
                if (SlotJson.IsNull())
                {
                    MaterialSlots[i] = nullptr;
                    continue;
                }

                // 2. JSON에서 클래스 이름 읽기
                FString ClassName;
                FJsonSerializer::ReadString(SlotJson, "Type", ClassName, "None", false);

                UMaterialInterface* LoadedMaterial = nullptr;

                // 3. 클래스 이름에 따라 분기
                if (ClassName == UMaterialInstanceDynamic::StaticClass()->Name)
                {
                    // UMID는 인스턴스이므로, 'new'로 생성합니다.
                    // (참고: 리플렉션 팩토리가 있다면 FReflectionFactory::CreateObject(ClassName) 사용)
                    UMaterialInstanceDynamic* NewMID = new UMaterialInstanceDynamic();

                    // 4. 생성된 빈 객체에 Serialize(true)를 호출하여 데이터를 채웁니다.
                    NewMID->Serialize(true, SlotJson); // (const_cast)

                    // 5. 소유권 추적 배열에 추가합니다.
                    DynamicMaterialInstances.Add(NewMID);
                    LoadedMaterial = NewMID;
                }
                else // if(ClassName == UMaterial::StaticClass()->Name)
                {
                    // UMaterial은 리소스이므로, AssetPath로 리소스 매니저에서 로드합니다.
                    FString AssetPath;
                    FJsonSerializer::ReadString(SlotJson, "AssetPath", AssetPath, "", false);
                    if (!AssetPath.empty())
                    {
                        LoadedMaterial = UResourceManager::GetInstance().Load<UMaterial>(AssetPath);
                    }
                    else
                    {
                        LoadedMaterial = nullptr;
                    }

                    // UMaterial::Serialize(true)는 호출할 필요가 없습니다 (혹은 호출해도 됨).
                    // 리소스 로드가 주 목적이기 때문입니다.
                }

                MaterialSlots[i] = LoadedMaterial;
            }
        }
    }
    else // --- 저장 ---
    {
        JSON SlotsArrayJson = JSON::Make(JSON::Class::Array);
        for (UMaterialInterface* Mtl : MaterialSlots)
        {
            JSON SlotJson = JSON::Make(JSON::Class::Object);

            if (Mtl == nullptr)
            {
                SlotJson["Type"] = "None"; // null 슬롯 표시
            }
            else
            {
                // 1. 클래스 이름 저장 (로드 시 팩토리 구분을 위함)
                SlotJson["Type"] = Mtl->GetClass()->Name;

                // 2. 객체 스스로 데이터를 저장하도록 위임
                Mtl->Serialize(false, SlotJson);
            }

            SlotsArrayJson.append(SlotJson);
        }
        InOutHandle[MaterialSlotsKey] = SlotsArrayJson;
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    // if문은 애니메이션이 없는 경우 유효
    // 애니메이션은 실시간으로 변하니까 애니 존재 시 if문 제거
    if (bChangedSkeletalMesh)
    {
        UpdateBoneMatrices();
        UpdateSkinningMatrices();
        PerformCPUSkinning(AnimatedVertices);
        bChangedSkeletalMesh = false;
    }
}

void USkeletalMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    Super::CollectMeshBatches(OutMeshBatchElements, View);

    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
    {
        // UE_LOG("[USkeletalMeshComponent/CollectMeshBatches] SkeletalMesh is null");
        return;
    }

    const TArray<FGroupInfo>& MeshGroupInfos = SkeletalMesh->GetMeshGroupInfo();

    auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
        {
            UMaterialInterface* Material = GetMaterial(SectionIndex);
            UShader* Shader = nullptr;

            if (Material && Material->GetShader())
            {
                Shader = Material->GetShader();
            }
            else
            {
                UE_LOG("USkeletalMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
                Material = UResourceManager::GetInstance().GetDefaultMaterial();
                if (Material)
                {
                    Shader = Material->GetShader();
                }
                if (!Material || !Shader)
                {
                    UE_LOG("USkeletalMeshComponent: 기본 머티리얼이 없습니다.");
                    return { nullptr, nullptr };
                }
            }
            return { Material, Shader };
        };

    const bool bHasSections = !MeshGroupInfos.IsEmpty();
    const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

    for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
    {
        uint32 IndexCount = 0;
        uint32 StartIndex = 0;

        if (bHasSections)
        {
            const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
            IndexCount = Group.IndexCount;
            StartIndex = Group.StartIndex;
        }
        else
        {
            IndexCount = SkeletalMesh->GetIndexCount();
            StartIndex = 0;
        }

        if (IndexCount == 0)
        {
            continue;
        }

        auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
        if (!MaterialToUse || !ShaderToUse)
        {
            continue;
        }

        FMeshBatchElement BatchElement;
        FShaderVariant* ShaderVariant = ShaderToUse->GetShaderVariant(MaterialToUse->GetShaderMacros());

        if (ShaderVariant)
        {
            BatchElement.VertexShader = ShaderVariant->VertexShader;
            BatchElement.PixelShader = ShaderVariant->PixelShader;
            BatchElement.InputLayout = ShaderVariant->InputLayout;
        }

        // UMaterialInterface를 UMaterial로 캐스팅해야 할 수 있음. 렌더러가 UMaterial을 기대한다면.
        // 지금은 Material.h 구조상 UMaterialInterface에 필요한 정보가 다 있음.
        BatchElement.Material = MaterialToUse;
        BatchElement.VertexBuffer = SkeletalMesh->GetVertexBuffer();
        BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();
        BatchElement.VertexStride = SkeletalMesh->GetVertexStride();
        BatchElement.IndexCount = IndexCount;
        BatchElement.StartIndex = StartIndex;
        BatchElement.BaseVertexIndex = 0;
        BatchElement.WorldMatrix = GetWorldMatrix();
        BatchElement.ObjectID = InternalIndex;
        BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        OutMeshBatchElements.Add(BatchElement);
    }

}

void USkeletalMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // 이 함수는 '복사본' (PIE 컴포넌트)에서 실행됩니다.
    // 현재 'DynamicMaterialInstances'와 'MaterialSlots'는
    // '원본' (에디터 컴포넌트)의 포인터를 얕은 복사한 상태입니다.

    // 원본 MID -> 복사본 MID 매핑 테이블
    TMap<UMaterialInstanceDynamic*, UMaterialInstanceDynamic*> OldToNewMIDMap;

    // 1. 복사본의 MID 소유권 리스트를 비웁니다. (메모리 해제 아님)
    //    이 리스트는 새로운 '복사본 MID'들로 다시 채워질 것입니다.
    DynamicMaterialInstances.Empty();

    // 2. MaterialSlots를 순회하며 MID를 찾습니다.
    for (int32 i = 0; i < MaterialSlots.Num(); ++i)
    {
        UMaterialInterface* CurrentSlot = MaterialSlots[i];
        UMaterialInstanceDynamic* OldMID = Cast<UMaterialInstanceDynamic>(CurrentSlot);

        if (OldMID)
        {
            UMaterialInstanceDynamic* NewMID = nullptr;

            // 이 MID를 이미 복제했는지 확인합니다 (여러 슬롯이 같은 MID를 쓸 경우)
            if (OldToNewMIDMap.Contains(OldMID))
            {
                NewMID = OldToNewMIDMap[OldMID];
            }
            else
            {
                // 3. MID를 복제합니다.
                UMaterialInterface* Parent = OldMID->GetParentMaterial();
                if (!Parent)
                {
                    // 부모가 없으면 복제할 수 없으므로 nullptr 처리
                    MaterialSlots[i] = nullptr;
                    continue;
                }

                // 3-1. 새로운 MID (PIE용)를 생성합니다.
                NewMID = UMaterialInstanceDynamic::Create(Parent);

                // 3-2. 원본(OldMID)의 파라미터를 새 MID로 복사합니다.
                NewMID->CopyParametersFrom(OldMID);

                // 3-3. 이 컴포넌트(복사본)의 소유권 리스트에 새 MID를 추가합니다.
                DynamicMaterialInstances.Add(NewMID);
                OldToNewMIDMap.Add(OldMID, NewMID);
            }

            // 4. MaterialSlots가 원본(OldMID) 대신 새 복사본(NewMID)을 가리키도록 교체합니다.
            MaterialSlots[i] = NewMID;
        }
        // else (원본 UMaterial 애셋인 경우)
        // 얕은 복사된 포인터(애셋 경로)를 그대로 사용해도 안전합니다.
    }
}

void USkeletalMeshComponent::OnSerialized()
{
    Super::OnSerialized();
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& FilePath)
{
    Super::SetSkeletalMesh(FilePath);

    ClearDynamicMaterials();

    // 사용중인 메시 해제
    if (SkeletalMesh != nullptr)
    {
        SkeletalMesh->EraseUsingComponets(this);
    }

    SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(FilePath);
    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
    {
        SkeletalMesh->AddUsingComponents(this);

        const TArray<FGroupInfo>& GroupInfos = SkeletalMesh->GetMeshGroupInfo();

        MaterialSlots.resize(GroupInfos.Num());

        for (int i = 0; i < GroupInfos.Num(); ++i)
        {
            SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
        }

        // FBoneInfo에서 EditableBones 초기화
        LoadBonesFromAsset();
        bSkinningDirty = true;
    }
    else
    {
        SkeletalMesh = nullptr;
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* Mesh)
{
    ClearDynamicMaterials();

    // 사용중인 메시 해제
    if (SkeletalMesh != nullptr)
    {
        SkeletalMesh->EraseUsingComponets(this);
    }

    // 복제본 직접 설정 (ResourceManager 경유 안 함)
    SkeletalMesh = Mesh;
    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
    {
        const TArray<FGroupInfo>& GroupInfos = SkeletalMesh->GetMeshGroupInfo();

        MaterialSlots.resize(GroupInfos.Num());

        for (int i = 0; i < GroupInfos.Num(); ++i)
        {
            SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
        }

        // FBoneInfo에서 EditableBones 초기화
        LoadBonesFromAsset();
        bSkinningDirty = true;
    }
    else
    {
        SkeletalMesh = nullptr;
    }
}

void USkeletalMeshComponent::SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
{
    Super::SetMaterial(InElementIndex, InNewMaterial);

    if (InElementIndex >= static_cast<uint32>(MaterialSlots.Num()))
    {
        UE_LOG("out of range InMaterialSlotIndex: %d", InElementIndex);
        return;
    }

    if (UMaterial* OriginMaterial = Cast<UMaterial>(InNewMaterial))
    {
        TArray<FShaderMacro> Macros = { FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" } };
        OriginMaterial->SetShaderMacros(Macros);
    }

    // 1. 현재 슬롯에 할당된 머티리얼을 가져옵니다.
    UMaterialInterface* OldMaterial = MaterialSlots[InElementIndex];

    // 2. 교체될 새 머티리얼이 현재 머티리얼과 동일하면 아무것도 하지 않습니다.
    if (OldMaterial == InNewMaterial)
    {
        return;
    }

    // 3. 기존 머티리얼이 이 컴포넌트가 소유한 MID인지 확인합니다.
    if (OldMaterial != nullptr)
    {
        UMaterialInstanceDynamic* OldMID = Cast<UMaterialInstanceDynamic>(OldMaterial);
        if (OldMID)
        {
            // 4. 소유권 리스트(DynamicMaterialInstances)에서 이 MID를 찾아 제거합니다.
            // TArray::Remove()는 첫 번째로 발견된 항목만 제거합니다.
            int32 RemovedCount = DynamicMaterialInstances.Remove(OldMID);

            if (RemovedCount > 0)
            {
                // 5. 소유권 리스트에서 제거된 것이 확인되면, 메모리에서 삭제합니다.
                delete OldMID;
            }
            else
            {
                // 경고: MaterialSlots에는 MID가 있었으나, 소유권 리스트에 없는 경우입니다.
                // 이는 DuplicateSubObjects 등이 잘못 구현되었을 때 발생할 수 있습니다.
                UE_LOG("Warning: SetMaterial is replacing a MID that was not tracked by DynamicMaterialInstances.");
                // 이 경우 delete를 호출하면 다른 객체가 소유한 메모리를 해제할 수 있으므로
                // delete를 호출하지 않는 것이 더 안전할 수 있습니다. (혹은 delete 호출 후 크래시로 버그를 잡습니다.)
                // 여기서는 삭제를 시도합니다.
                delete OldMID;
            }
        }
    }

    // 6. 새 머티리얼을 슬롯에 할당합니다.
    MaterialSlots[InElementIndex] = InNewMaterial;
}

UMaterialInterface* USkeletalMeshComponent::GetMaterial(uint32 InSectionIndex) const
{
    if (MaterialSlots.size() <= InSectionIndex)
    {
        return nullptr;
    }

    UMaterialInterface* FoundMaterial = MaterialSlots[InSectionIndex];

    if (!FoundMaterial)
    {
        UE_LOG("GetMaterial: Failed to find material Section %d", InSectionIndex);
        return nullptr;
    }

    return FoundMaterial;
}

UMaterialInstanceDynamic* USkeletalMeshComponent::CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex)
{
    UMaterialInterface* CurrentMaterial = GetMaterial(ElementIndex);
    if (!CurrentMaterial)
    {
        return nullptr;
    }

    // 이미 MID인 경우, 그대로 반환
    if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(CurrentMaterial))
    {
        return ExistingMID;
    }

    // 현재 머티리얼(UMaterial 또는 다른 MID가 아닌 UMaterialInterface)을 부모로 하는 새로운 MID를 생성
    UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(CurrentMaterial);
    if (NewMID)
    {
        DynamicMaterialInstances.Add(NewMID); // 소멸자에서 해제하기 위해 추적
        SetMaterial(ElementIndex, NewMID);    // 슬롯에 새로 만든 MID 설정
        NewMID->SetFilePath("(Instance) " + CurrentMaterial->GetFilePath());
        return NewMID;
    }

    return nullptr;
}

void USkeletalMeshComponent::EnsureSkinningReady(D3D11RHI* InDevice)
{
    // 스키닝이 일어나지 않음.    
    if (bSkinningDirty)
    {
        UpdateBoneMatrices();
        UpdateSkinningMatrices();
        PerformCPUSkinning(AnimatedVertices);
        UpdateVertexBuffer(InDevice);
        bSkinningDirty = false;
    }
}

void USkeletalMeshComponent::UpdateVertexBuffer(D3D11RHI* InDevice)
{
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        //UE_LOG("[USkeletalMeshComponent/UpdateSkinnedVertices] FSkeletalMesh is null");
        return;
    }

    if (MeshAsset->Vertices.IsEmpty())
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinnedVertices] Vertices is zero");
        return;
    }

    ID3D11Buffer* VertexBuffer = SkeletalMesh->GetVertexBuffer();
    if (!VertexBuffer)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinnedVertices] VertexBuffer is null");
        return;
    }

    ID3D11DeviceContext* DeviceContext = InDevice ? InDevice->GetDeviceContext() : nullptr;
    if (!DeviceContext)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinnedVertices] DeviceContext is null");
        return;
    }

    D3D11_MAPPED_SUBRESOURCE MSR = {};
    HRESULT hr = DeviceContext->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
    if (FAILED(hr))
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinnedVertices] Map Fail");
        return;
    }

    FVertexDynamic* VertexData = static_cast<FVertexDynamic*>(MSR.pData);
    const int32 VertexCount = AnimatedVertices.Num();
    for (int i = 0; i < VertexCount; i++)
    {
        VertexData[i].FillFrom(AnimatedVertices[i]);
    }

    DeviceContext->Unmap(VertexBuffer, 0);
}

FAABB USkeletalMeshComponent::GetWorldAABB() const
{
    const FTransform WorldTransform = GetWorldTransform();
    const FMatrix WorldMatrix = GetWorldMatrix();

    if (!SkeletalMesh)
    {
        const FVector Origin = WorldTransform.TransformPosition(FVector());
        return FAABB(Origin, Origin);
    }

    const FAABB LocalBound = SkeletalMesh->GetLocalBound();
    const FVector LocalMin = LocalBound.Min;
    const FVector LocalMax = LocalBound.Max;

    const FVector LocalCorners[8] = {
        FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
        FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
        FVector(LocalMin.X, LocalMax.Y, LocalMin.Z),
        FVector(LocalMax.X, LocalMax.Y, LocalMin.Z),
        FVector(LocalMin.X, LocalMin.Y, LocalMax.Z),
        FVector(LocalMax.X, LocalMin.Y, LocalMax.Z),
        FVector(LocalMin.X, LocalMax.Y, LocalMax.Z),
        FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
    };

    FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
    FVector4 WorldMax4 = WorldMin4;

    for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
    {
        const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
            , LocalCorners[CornerIndex].Y
            , LocalCorners[CornerIndex].Z
            , 1.0f)
            * WorldMatrix;
        WorldMin4 = WorldMin4.ComponentMin(WorldPos);
        WorldMax4 = WorldMax4.ComponentMax(WorldPos);
    }

    FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
    FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
    return FAABB(WorldMin, WorldMax);
}

void USkeletalMeshComponent::UpdateBoneMatrices()
{
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
       // UE_LOG("[USkeletalMeshComponent/UpdateBoneMatrices] FSkeletalMesh is null");
        return;
    }

    const int32 BoneCount = MeshAsset->Bones.Num();
    if (BoneCount == 0)
    {
       // UE_LOG("[USkeletalMeshComponent/UpdateBoneMatrices] BoneCount is zero");
        ComponentSpaceTransforms.Empty();
        return;
    }

    ComponentSpaceTransforms.SetNum(BoneCount);

    // EditableBones가 있으면 편집된 본 transform 사용 (Skeletal Mesh Editor)
    if (!EditableBones.empty() && EditableBones.size() == BoneCount)
    {
        // EditableBones의 Local Transform을 Component Space로 변환
        for (int i = 0; i < BoneCount; i++)
        {
            const FBone& Bone = EditableBones[i];
            FTransform LocalTransform(Bone.LocalPosition, Bone.LocalRotation, Bone.LocalScale);
            FMatrix BoneLocal = LocalTransform.ToMatrix();

            int32 ParentIndex = Bone.ParentIndex;
            if (ParentIndex == -1)
            {
                // Root bone: Local = Component Space
                ComponentSpaceTransforms[i] = BoneLocal;
            }
            else
            {
                // Child bone: Component Space = Local * Parent Component Space
                ComponentSpaceTransforms[i] = BoneLocal * ComponentSpaceTransforms[ParentIndex];
            }
        }
    }
    else
    {
        // EditableBones가 없으면 BindPose 사용 (기본 동작)
        for (int i = 0; i < BoneCount; i++)
        {
            ComponentSpaceTransforms[i] = MeshAsset->Bones[i].InverseBindPoseMatrix.InverseAffine();
        }
    }
}

void USkeletalMeshComponent::UpdateSkinningMatrices()
{
    Super::UpdateSkinningMatrices();
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        //UE_LOG("[USkeletalMeshComponent/UpdateSkinningMatrices] FSkeletalMesh is null");
        return;
    }

    // 본 매트릭스가 없으면 스키닝 안함
    const int32 BoneCount = ComponentSpaceTransforms.Num();
    if (BoneCount == 0)
    {
        //UE_LOG("[USkeletalMeshComponent/UpdateSkinningMatrices] BoneMatrices is zero");
        return;
    }

    SkinningMatrix.SetNum(BoneCount);
    SkinningInvTransMatrix.SetNum(BoneCount);
    for (int i = 0; i < BoneCount; i++)
    {
        SkinningMatrix[i] = MeshAsset->Bones[i].InverseBindPoseMatrix * ComponentSpaceTransforms[i];
        if (SkinningMatrix[i].IsOrtho())
        {
            SkinningInvTransMatrix[i] = SkinningMatrix[i];
        }
        else
        {
            SkinningInvTransMatrix[i] = SkinningMatrix[i].InverseAffine().Transpose();
        }
    }
}

void USkeletalMeshComponent::ClearDynamicMaterials()
{
    // 1. 생성된 동적 머티리얼 인스턴스 해제
    for (UMaterialInstanceDynamic* MID : DynamicMaterialInstances)
    {
        delete MID;
    }
    DynamicMaterialInstances.Empty();

    // 2. 머티리얼 슬롯 배열도 비웁니다.
    // (이 배열이 MID 포인터를 가리키고 있었을 수 있으므로
    //  delete 이후에 비워야 안전합니다.)
    MaterialSlots.Empty();
}

void USkeletalMeshComponent::LoadBonesFromAsset()
{
    EditableBones.Empty();

    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
        return;

    FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
    const TArray<FBoneInfo>& BoneInfos = MeshAsset->Bones;

    // FBoneInfo -> FBone 변환
    for (int32 i = 0; i < BoneInfos.size(); ++i)
    {
        FBone Bone = FBone::FromBoneInfo(i, BoneInfos);
        EditableBones.push_back(Bone);
    }
}

FBone* USkeletalMeshComponent::GetBone(int32 Index)
{
    if (Index >= 0 && Index < EditableBones.size())
        return &EditableBones[Index];
    return nullptr;
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{
    return FBoneTransformCalculator::GetBoneWorldTransform(this, BoneIndex);
}

void USkeletalMeshComponent::RenderDebugVolume(URenderer* Renderer) const
{
    if (EditableBones.empty() || !Renderer)
        return;

    UWorld* World = GetWorld();
    if (!World || !World->IsEmbedded())
        return;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    RenderBonePyramids(StartPoints, EndPoints, Colors);
    RenderJointSpheres(StartPoints, EndPoints, Colors);

    // 본 시각화는 항상 위에 그려져야 하므로 bOverlay=true 설정 (Gizmo와 동일)
    Renderer->AddLines(StartPoints, EndPoints, Colors, true);
}

void USkeletalMeshComponent::RenderBonePyramids(
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors) const
{
    // 월드 바운딩 박스 크기를 기준으로 본 두께를 스케일링 (월드 스케일 자동 반영)
    float BoneThickness = 0.8f;
    if (SkeletalMesh)
    {
        const FAABB WorldBound = GetWorldAABB();
        const FVector BoundSize = WorldBound.Max - WorldBound.Min;
        const float MeshScale = (BoundSize.X + BoundSize.Y + BoundSize.Z) / 3.0f;
        BoneThickness = MeshScale * 0.01f;
    }

    const FVector4 WhiteColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    const FVector4 GreenColor = FVector4(0.0f, 1.0f, 0.0f, 1.0f);
    const FVector4 YellowColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);

    // 컴포넌트의 월드 트랜스폼 (본 위치를 World Space로 변환)
    const FTransform ComponentWorld = GetWorldTransform();

    // ComponentSpaceTransforms가 없으면 그릴 수 없음
    if (ComponentSpaceTransforms.IsEmpty())
        return;

    for (int32 i = 0; i < GetBoneCount(); ++i)
    {
        const FBone& Bone = EditableBones[i];
        if (Bone.ParentIndex < 0)
            continue;

        // 실제 렌더링에 사용되는 ComponentSpaceTransforms 사용
        FVector ParentScale, ChildScale;
        FQuat ParentRot, ChildRot;
        FVector ParentTranslation, ChildTranslation;
        ComponentSpaceTransforms[Bone.ParentIndex].Decompose(ParentScale, ParentRot, ParentTranslation);
        ComponentSpaceTransforms[i].Decompose(ChildScale, ChildRot, ChildTranslation);

        // Component Space → World Space 변환
        FVector ParentPos = ComponentWorld.TransformPosition(ParentTranslation);
        FVector ChildPos = ComponentWorld.TransformPosition(ChildTranslation);
        FVector BoneDir = (ChildPos - ParentPos);
        float BoneLength = BoneDir.Size();

        if (BoneLength < 0.001f)
            continue;

        BoneDir = BoneDir / BoneLength;

        // 피라미드 기저면 (단순하게 라인만)
        FVector Right, Up;
        if (std::abs(BoneDir.Z) < 0.9f)
        {
            Up = FVector::Cross(FVector(0, 0, 1), BoneDir).GetNormalized() * BoneThickness;
            Right = FVector::Cross(BoneDir, Up).GetNormalized() * BoneThickness;
        }
        else
        {
            Right = FVector::Cross(FVector(1, 0, 0), BoneDir).GetNormalized() * BoneThickness;
            Up = FVector::Cross(BoneDir, Right).GetNormalized() * BoneThickness;
        }

        FVector Base[4] = {
            ParentPos + Right + Up,
            ParentPos - Right + Up,
            ParentPos - Right - Up,
            ParentPos + Right - Up
        };

        FVector4 LineColor = WhiteColor;
        if (SelectedBoneIndex == i)
            LineColor = GreenColor;
        else if (SelectedBoneIndex == Bone.ParentIndex)
            LineColor = YellowColor;

        // 피라미드 엣지
        for (int32 j = 0; j < 4; ++j)
        {
            OutStartPoints.push_back(Base[j]);
            OutEndPoints.push_back(ChildPos);
            OutColors.push_back(LineColor);

            OutStartPoints.push_back(Base[j]);
            OutEndPoints.push_back(Base[(j + 1) % 4]);
            OutColors.push_back(LineColor);
        }
    }
}

void USkeletalMeshComponent::RenderJointSpheres(
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors) const
{
    // 월드 바운딩 박스 크기를 기준으로 조인트 구 반지름을 스케일링 (월드 스케일 자동 반영)
    float SphereRadius = 1.5f;
    if (SkeletalMesh)
    {
        const FAABB WorldBound = GetWorldAABB();
        const FVector BoundSize = WorldBound.Max - WorldBound.Min;
        const float MeshScale = (BoundSize.X + BoundSize.Y + BoundSize.Z) / 3.0f;
        SphereRadius = MeshScale * 0.015f;
    }

    const int32 NumSegments = 8;
    const FVector4 WhiteColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    const FVector4 GreenColor = FVector4(0.0f, 1.0f, 0.0f, 1.0f);

    // 컴포넌트의 월드 트랜스폼 (본 위치를 World Space로 변환)
    const FTransform ComponentWorld = GetWorldTransform();

    // ComponentSpaceTransforms가 없으면 그릴 수 없음
    if (ComponentSpaceTransforms.IsEmpty())
        return;

    for (int32 i = 0; i < GetBoneCount(); ++i)
    {
        // 실제 렌더링에 사용되는 ComponentSpaceTransforms 사용
        FVector JointScale;
        FQuat JointRot;
        FVector JointTranslation;
        ComponentSpaceTransforms[i].Decompose(JointScale, JointRot, JointTranslation);

        // Component Space → World Space 변환
        FVector JointPos = ComponentWorld.TransformPosition(JointTranslation);
        FVector4 Color = (i == SelectedBoneIndex) ? GreenColor : WhiteColor;

        // 3개의 원 (XY, YZ, ZX 평면)
        for (int32 axis = 0; axis < 3; ++axis)
        {
            for (int32 seg = 0; seg < NumSegments; ++seg)
            {
                float angle1 = (seg * 2.0f * PI) / NumSegments;
                float angle2 = ((seg + 1) * 2.0f * PI) / NumSegments;

                FVector p1, p2;
                if (axis == 0) // XY plane
                {
                    p1 = JointPos + FVector(std::cos(angle1), std::sin(angle1), 0) * SphereRadius;
                    p2 = JointPos + FVector(std::cos(angle2), std::sin(angle2), 0) * SphereRadius;
                }
                else if (axis == 1) // YZ plane
                {
                    p1 = JointPos + FVector(0, std::cos(angle1), std::sin(angle1)) * SphereRadius;
                    p2 = JointPos + FVector(0, std::cos(angle2), std::sin(angle2)) * SphereRadius;
                }
                else // ZX plane
                {
                    p1 = JointPos + FVector(std::sin(angle1), 0, std::cos(angle1)) * SphereRadius;
                    p2 = JointPos + FVector(std::sin(angle2), 0, std::cos(angle2)) * SphereRadius;
                }

                OutStartPoints.push_back(p1);
                OutEndPoints.push_back(p2);
                OutColors.push_back(Color);
            }
        }
    }
}
