#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "JsonSerializer.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "MeshBatchElement.h"
#include "Material.h"

IMPLEMENT_CLASS(UStaticMeshComponent)

BEGIN_PROPERTIES(UStaticMeshComponent)
	MARK_AS_COMPONENT("스태틱 메시 컴포넌트", "스태틱 메시를 렌더링하는 컴포넌트입니다.")
	ADD_PROPERTY_STATICMESH(UStaticMesh*, StaticMesh, "Static Mesh", true)
	ADD_PROPERTY_ARRAY(EPropertyType::Material, MaterialSlots, "Materials", true)
END_PROPERTIES()

UStaticMeshComponent::UStaticMeshComponent()
{
	SetStaticMesh(GDataDir + "/cube-tex.obj");     // 임시 기본 static mesh 설정
}

UStaticMeshComponent::~UStaticMeshComponent()
{
	if (StaticMesh != nullptr)
	{
		StaticMesh->EraseUsingComponets(this);
	}

	// 생성된 동적 머티리얼 인스턴스 해제
	ClearDynamicMaterials();
}

// 컴포넌트가 소유한 모든 UMaterialInstanceDynamic을 삭제하고, 관련 배열을 비웁니다.
void UStaticMeshComponent::ClearDynamicMaterials()
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

void UStaticMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		return;
	}

	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

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
				UE_LOG("UStaticMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
				Material = UResourceManager::GetInstance().GetDefaultMaterial();
				if (Material)
				{
					Shader = Material->GetShader();
				}
				if (!Material || !Shader)
				{
					UE_LOG("UStaticMeshComponent: 기본 머티리얼이 없습니다.");
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
			IndexCount = StaticMesh->GetIndexCount();
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
		BatchElement.VertexBuffer = StaticMesh->GetVertexBuffer();
		BatchElement.IndexBuffer = StaticMesh->GetIndexBuffer();
		BatchElement.VertexStride = StaticMesh->GetVertexStride();
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		OutMeshBatchElements.Add(BatchElement);
	}
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
	// 1. 새 메시를 설정하기 전에, 기존에 생성된 모든 MID와 슬롯 정보를 정리합니다.
	ClearDynamicMaterials();

	// 2. 기존 메시가 있다면 연결을 해제합니다.
	if (StaticMesh != nullptr)
	{
		StaticMesh->EraseUsingComponets(this);
	}

	// 3. 새 메시를 로드합니다.
	StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(PathFileName);
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
	{
		StaticMesh->AddUsingComponents(this);

		const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();

		// 4. 새 메시 정보에 맞게 슬롯을 재설정합니다.
		MaterialSlots.resize(GroupInfos.size()); // ClearDynamicMaterials()에서 비워졌으므로, 새 크기로 재할당

		for (int i = 0; i < GroupInfos.size(); ++i)
		{
			SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
		}
		MarkWorldPartitionDirty();
	}
	else
	{
		// 메시 로드에 실패한 경우, StaticMesh 포인터를 nullptr로 보장합니다.
		// (슬롯은 이미 위에서 비워졌습니다.)
		StaticMesh = nullptr;
	}
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(uint32 InSectionIndex) const
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

void UStaticMeshComponent::SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
{
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

UMaterialInstanceDynamic* UStaticMeshComponent::CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex)
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

FAABB UStaticMeshComponent::GetWorldAABB() const
{
	const FTransform WorldTransform = GetWorldTransform();
	const FMatrix WorldMatrix = GetWorldMatrix();

	if (!StaticMesh)
	{
		const FVector Origin = WorldTransform.TransformPosition(FVector());
		return FAABB(Origin, Origin);
	}

	const FAABB LocalBound = StaticMesh->GetLocalBound();
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
void UStaticMeshComponent::OnTransformUpdated()
{
	Super_t::OnTransformUpdated();
	MarkWorldPartitionDirty();
}

void UStaticMeshComponent::MarkWorldPartitionDirty()
{
	if (UWorld* World = GetWorld())
	{
		if (UWorldPartitionManager* Partition = World->GetPartitionManager())
		{
			Partition->MarkDirty(this);
		}
	}
}

void UStaticMeshComponent::DuplicateSubObjects()
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

void UStaticMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
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

// 직렬화 완료 직후 호출됨
void UStaticMeshComponent::OnSerialized()
{
	Super::OnSerialized();

	// 1. AutoSerialize로 로드된 StaticMesh에 AddUsingComponents 호출
	if (StaticMesh)
	{
		StaticMesh->AddUsingComponents(this);
	}

	// 2. 월드 파티션 업데이트
	MarkWorldPartitionDirty();
}
