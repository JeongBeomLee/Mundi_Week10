#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "MeshBatchElement.h"

IMPLEMENT_CLASS(USkeletalMeshComponent)
BEGIN_PROPERTIES(USkeletalMeshComponent)
    MARK_AS_COMPONENT("스켈레탈 메시 컴포넌트", "스켈레탈 메시")
    ADD_PROPERTY_SKELETALMESH(USkeletalMesh*, SkeletalMesh, "Skeletal Mesh", true)
END_PROPERTIES(USkeletalMeshComponent)

USkeletalMeshComponent::USkeletalMeshComponent()
{
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
}

void USkeletalMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
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
        UE_LOG("[USkeletalMeshComponent/CollectMeshBatches] SkeletalMesh is null");
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
}

void USkeletalMeshComponent::OnSerialized()
{
    Super::OnSerialized();
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& FilePath)
{
    Super::SetSkeletalMesh(FilePath);  
}

void USkeletalMeshComponent::UpdateVertexBuffer(D3D11RHI* InDevice)
{
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinnedVertices] FSkeletalMesh is null");
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
    const int32 VertexCount = MeshAsset->Vertices.Num();    
    for (int i = 0; i < VertexCount; i++)
    {
        VertexData[i].FillFrom(MeshAsset->Vertices[i]);
    }    

    DeviceContext->Unmap(VertexBuffer, 0);
}

void USkeletalMeshComponent::UpdateBoneMatrices()
{
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateBoneMatrices] FSkeletalMesh is null");
        return;
    }

    const int32 BoneCount = MeshAsset->Bones.Num();
    if (BoneCount == 0)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateBoneMatrices] BoneCount is zero");
        ComponentSpaceTransforms.Empty();
        return;
    }

    // 지금은 애니메이션이 없어서 BindPose 그대로 사용
    ComponentSpaceTransforms.SetNum(BoneCount);
    for (int i = 0; i < BoneCount; i++)
    {
        // 애니메이션 없으니까 바인드 포즈 사용        
        ComponentSpaceTransforms[i] = MeshAsset->Bones[i].GlobalTransform;
    }

    // TODO 애니메이션 구현 후 사용 BoneSpaceTransforms채워서 사용
    // for (int i = 0; i < BoneCount; i++)
    // {
    //     FMatrix BoneLocal = BoneSpaceTransforms[i];
    //     int32 ParentIndex = MeshAsset->Bones[i].ParentIndex;
    //     if (ParentIndex == -1)
    //     {
    //         ComponentSpaceTransforms[i] = BoneLocal;
    //     }
    //     else
    //     {
    //         ComponentSpaceTransforms[i] = BoneLocal * ComponentSpaceTransforms[ParentIndex];
    //     }
    // }
}

void USkeletalMeshComponent::UpdateSkinningMatrices()
{
    Super::UpdateSkinningMatrices();
    FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
    if (!MeshAsset)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinningMatrices] FSkeletalMesh is null");
        return;
    }
    
    // 본 매트릭스가 없으면 스키닝 안함
    const int32 BoneCount = ComponentSpaceTransforms.Num();
    if (BoneCount == 0)
    {
        UE_LOG("[USkeletalMeshComponent/UpdateSkinningMatrices] BoneMatrices is zero");
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
