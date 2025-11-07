#include "pch.h"
#include "PropertyRenderer.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "Color.h"
#include "SceneComponent.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Material.h"
#include "BillboardComponent.h"
#include "DecalComponent.h"
#include "StaticMeshComponent.h"
#include "LightComponentBase.h"
#include "DirectionalLightComponent.h"

// 정적 멤버 변수 초기화
TArray<FString> UPropertyRenderer::CachedStaticMeshPaths;
TArray<const char*> UPropertyRenderer::CachedStaticMeshItems;
TArray<FString> UPropertyRenderer::CachedMaterialPaths;
TArray<const char*> UPropertyRenderer::CachedMaterialItems;
TArray<FString> UPropertyRenderer::CachedShaderPaths;
TArray<const char*> UPropertyRenderer::CachedShaderItems;
TArray<FString> UPropertyRenderer::CachedTexturePaths;
TArray<const char*> UPropertyRenderer::CachedTextureItems;

bool UPropertyRenderer::RenderProperty(const FProperty& Property, void* ObjectInstance)
{
	// 렌더링에 필요한 리소스가 캐시되었는지 확인하고, 없으면 로드합니다.
	CacheResources();

	bool bChanged = false;

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		bChanged = RenderBoolProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Int32:
		bChanged = RenderInt32Property(Property, ObjectInstance);
		break;

	case EPropertyType::Float:
		bChanged = RenderFloatProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FVector:
		bChanged = RenderVectorProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FLinearColor:
		bChanged = RenderColorProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FString:
		bChanged = RenderStringProperty(Property, ObjectInstance);
		break;

	case EPropertyType::FName:
		bChanged = RenderNameProperty(Property, ObjectInstance);
		break;

	case EPropertyType::ObjectPtr:
		bChanged = RenderObjectPtrProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Struct:
		bChanged = RenderStructProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Texture:
		bChanged = RenderTextureProperty(Property, ObjectInstance);
		break;

	case EPropertyType::StaticMesh:
		bChanged = RenderStaticMeshProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Material:
		bChanged = RenderMaterialProperty(Property, ObjectInstance);
		break;

	case EPropertyType::Array:
		switch (Property.InnerType)
		{
		case EPropertyType::Material:
			bChanged = RenderMaterialArrayProperty(Property, ObjectInstance);
			break;
		}
		break;

	case EPropertyType::Enum:
		bChanged = RenderEnumProperty(Property, ObjectInstance);
		break;

	default:
		ImGui::Text("%s: [Unknown Type]", Property.Name);
		break;
	}

	if (Property.Tooltip && Property.Tooltip[0] != '\0' && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Property.Tooltip);
	}

	// SceneComponent는 Transform 프로퍼티가 변경되면 Setter를 통해 동기화
	if (bChanged && ObjectInstance)
	{
		UObject* Obj = static_cast<UObject*>(ObjectInstance);
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Obj))
		{
			// 프로퍼티 이름으로 Transform 프로퍼티 판별 후 Setter 호출
			if (strcmp(Property.Name, "RelativeRotationEuler") == 0)
			{
				FVector* EulerValue = Property.GetValuePtr<FVector>(ObjectInstance);
				SceneComponent->SetRelativeRotationEuler(*EulerValue);
			}
			else if (strcmp(Property.Name, "RelativeLocation") == 0)
			{
				FVector* LocationValue = Property.GetValuePtr<FVector>(ObjectInstance);
				SceneComponent->SetRelativeLocation(*LocationValue);
			}
			else if (strcmp(Property.Name, "RelativeScale") == 0)
			{
				FVector* ScaleValue = Property.GetValuePtr<FVector>(ObjectInstance);
				SceneComponent->SetRelativeScale(*ScaleValue);
			}
		}

		// LightComponent는 Light 프로퍼티가 변경되면 UpdateLightData를 호출하여 동기화
		if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Obj))
		{
			// Light 관련 프로퍼티가 변경되면 UpdateLightData 호출
			if (strcmp(Property.Name, "LightColor") == 0 ||
				strcmp(Property.Name, "Intensity") == 0 ||
				strcmp(Property.Name, "Temperature") == 0 ||
				strcmp(Property.Name, "bIsVisible") == 0 ||
				strcmp(Property.Name, "bIsActive") == 0 ||
				strcmp(Property.Name, "AttenuationRadius") == 0 ||
				strcmp(Property.Name, "bUseInverseSquareFalloff") == 0 ||
				strcmp(Property.Name, "FalloffExponent") == 0 ||
				strcmp(Property.Name, "InnerConeAngle") == 0 ||
				strcmp(Property.Name, "OuterConeAngle") == 0 ||
				strcmp(Property.Name, "ShadowMapType") == 0 ||
				strcmp(Property.Name, "NumCascades") == 0 ||
				strcmp(Property.Name, "CSMLambda") == 0)
			{
				LightComponent->UpdateLightData();
			}
		}
	}

	return bChanged;
}

void UPropertyRenderer::RenderProperties(const TArray<FProperty>& Properties, UObject* Object)
{
	if (Properties.IsEmpty())
		return;

	// 카테고리별로 그룹화
	TMap<FString, TArray<const FProperty*>> CategorizedProps;

	for (const FProperty& Prop : Properties)
	{
		if (Prop.bIsEditAnywhere)
		{
			FString CategoryName = Prop.Category ? Prop.Category : "Default";
			if (!CategorizedProps.Contains(CategoryName))
			{
				CategorizedProps.Add(CategoryName, TArray<const FProperty*>());
			}
			CategorizedProps[CategoryName].Add(&Prop);
		}
	}

	// 카테고리별로 렌더링
	for (auto& Pair : CategorizedProps)
	{
		const FString& Category = Pair.first;
		const TArray<const FProperty*>& Props = Pair.second;

		ImGui::Separator();
		ImGui::Text("%s", Category.c_str());

		for (const FProperty* Prop : Props)
		{
			// 조건부 프로퍼티 표시 로직 (DirectionalLightComponent의 CSM 설정)
			if (ShouldRenderProperty(*Prop, Object))
			{
				RenderProperty(*Prop, Object);
			}
		}
	}
}

void UPropertyRenderer::RenderAllProperties(UObject* Object)
{
	if (!Object)
		return;

	UClass* Class = Object->GetClass();
	const TArray<FProperty>& Properties = Class->GetProperties();

	RenderProperties(Properties, Object);
}

void UPropertyRenderer::RenderAllPropertiesWithInheritance(UObject* Object)
{
	if (!Object)
		return;

	UClass* Class = Object->GetClass();
	const TArray<FProperty>& AllProperties = Class->GetAllProperties();

	RenderProperties(AllProperties, Object);
}

// ===== 조건부 프로퍼티 표시 로직 =====

bool UPropertyRenderer::ShouldRenderProperty(const FProperty& Prop, UObject* Object)
{
	if (!Object)
		return true;

	// DirectionalLightComponent의 경우, ShadowMapType에 따라 CSM 설정 프로퍼티를 숨기거나 표시
	if (UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(Object))
	{
		// NumCascades와 CSMLambda는 ShadowMapType이 CSM일 때만 표시
		if (strcmp(Prop.Name, "NumCascades") == 0 || strcmp(Prop.Name, "CSMLambda") == 0)
		{
			return DirLight->GetShadowMapType() == EShadowMapType::CSM;
		}
	}

	return true;
}

// ===== 리소스 캐싱 =====

void UPropertyRenderer::CacheResources()
{
	// 매번 캐싱을 새로 하도록 변경, NOTE: 추후에 변경된 것만 추가하도록 변경 필요
	//ClearResourcesCache();

	UResourceManager& ResMgr = UResourceManager::GetInstance();

	// 1. 스태틱 메시
	if (CachedStaticMeshPaths.IsEmpty() && CachedStaticMeshItems.IsEmpty())
	{
		CachedStaticMeshPaths = ResMgr.GetAllFilePaths<UStaticMesh>();
		for (const FString& path : CachedStaticMeshPaths)
		{
			CachedStaticMeshItems.push_back(path.c_str());
		}
		CachedStaticMeshPaths.Insert("", 0);
		CachedStaticMeshItems.Insert("None", 0);
	}

	// 2. 머티리얼
	if (CachedMaterialPaths.IsEmpty() && CachedTexturePaths.IsEmpty())
	{
		CachedMaterialPaths = ResMgr.GetAllFilePaths<UMaterial>();
		CachedMaterialItems.Add("None");
		for (const FString& path : CachedMaterialPaths)
		{
			CachedMaterialItems.push_back(path.c_str());
		}
	}

	// 3. 셰이더
	if (CachedShaderPaths.IsEmpty() && CachedShaderItems.IsEmpty())
	{
		CachedShaderPaths = ResMgr.GetAllFilePaths<UShader>();
		CachedShaderItems.Add("None");
		for (const FString& path : CachedShaderPaths)
		{
			CachedShaderItems.push_back(path.c_str());
		}
	}

	// 4. 텍스처
	if (CachedTexturePaths.IsEmpty() && CachedTextureItems.IsEmpty())
	{
		CachedTexturePaths = ResMgr.GetAllFilePaths<UTexture>();
		CachedTextureItems.Add("None");
		for (const FString& path : CachedTexturePaths)
		{
			CachedTextureItems.push_back(path.c_str());
		}
	}
}

void UPropertyRenderer::ClearResourcesCache()
{
	CachedStaticMeshPaths.Empty();
	CachedStaticMeshItems.Empty();
	CachedMaterialPaths.Empty();
	CachedMaterialItems.Empty();
	CachedShaderPaths.Empty();
	CachedShaderItems.Empty();
	CachedTexturePaths.Empty();
	CachedTextureItems.Empty();
}

// ===== 타입별 렌더링 구현 =====

bool UPropertyRenderer::RenderBoolProperty(const FProperty& Prop, void* Instance)
{
	bool* Value = Prop.GetValuePtr<bool>(Instance);
	return ImGui::Checkbox(Prop.Name, Value);
}

bool UPropertyRenderer::RenderInt32Property(const FProperty& Prop, void* Instance)
{
	int32* Value = Prop.GetValuePtr<int32>(Instance);

	// Min과 Max가 모두 0이 아니면 범위 제한을 적용
	if (Prop.MinValue != 0 || Prop.MaxValue != 0)
	{
		// InputInt를 사용하여 직접 입력 가능하게 하고, 범위를 제한
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::InputInt(Prop.Name, Value, 1, 1))
		{
			// 범위 클램핑
			*Value = FMath::Clamp(*Value, (int32)Prop.MinValue, (int32)Prop.MaxValue);
			return true;
		}
		return false;
	}
	else
	{
		// 범위 제한이 없으면 기본 InputInt 사용
		return ImGui::InputInt(Prop.Name, Value);
	}
}

bool UPropertyRenderer::RenderFloatProperty(const FProperty& Prop, void* Instance)
{
	float* Value = Prop.GetValuePtr<float>(Instance);

	// Min과 Max가 둘 다 0이면 범위 제한 없음
	if (Prop.MinValue == 0.0f && Prop.MaxValue == 0.0f)
	{
		return ImGui::DragFloat(Prop.Name, Value, 0.01f);
	}
	else
	{
		// 범위에 비례하여 speed 자동 계산 (전체 범위의 1/1000)
		// 작은 범위(예: 0.0~0.01)의 경우 매우 정밀한 조정 가능
		float Range = Prop.MaxValue - Prop.MinValue;
		float Speed = FMath::Max(Range / 1000.0f, 0.00001f);  // 최소 speed는 0.00001
		return ImGui::DragFloat(Prop.Name, Value, Speed, Prop.MinValue, Prop.MaxValue);
	}
}

bool UPropertyRenderer::RenderVectorProperty(const FProperty& Prop, void* Instance)
{
	// Transform 프로퍼티 (Location, Rotation, Scale) 감지
	bool bIsTransformProperty =
		strcmp(Prop.Name, "RelativeLocation") == 0 ||
		strcmp(Prop.Name, "RelativeRotationEuler") == 0 ||
		strcmp(Prop.Name, "RelativeScale") == 0;

	if (bIsTransformProperty)
	{
		return RenderTransformProperty(Prop, Instance);
	}

	// 일반 Vector 프로퍼티는 기본 렌더링
	FVector* Value = Prop.GetValuePtr<FVector>(Instance);
	return ImGui::DragFloat3(Prop.Name, &Value->X, 0.1f);
}

bool UPropertyRenderer::RenderColorProperty(const FProperty& Prop, void* Instance)
{
	FLinearColor* Value = Prop.GetValuePtr<FLinearColor>(Instance);
	return ImGui::ColorEdit4(Prop.Name, &Value->R);
}

bool UPropertyRenderer::RenderStringProperty(const FProperty& Prop, void* Instance)
{
	FString* Value = Prop.GetValuePtr<FString>(Instance);

	// ImGui::InputText는 char 버퍼를 사용하므로 변환 필요
	char Buffer[256];
	strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
	Buffer[sizeof(Buffer) - 1] = '\0';

	if (ImGui::InputText(Prop.Name, Buffer, sizeof(Buffer)))
	{
		*Value = Buffer;
		return true;
	}

	return false;
}

bool UPropertyRenderer::RenderNameProperty(const FProperty& Prop, void* Instance)
{
	FName* Value = Prop.GetValuePtr<FName>(Instance);

	// FName은 읽기 전용으로 표시
	FString NameStr = Value->ToString();
	ImGui::Text("%s: %s", Prop.Name, NameStr.c_str());

	return false;
}

bool UPropertyRenderer::RenderObjectPtrProperty(const FProperty& Prop, void* Instance)
{
	UObject** ObjPtr = Prop.GetValuePtr<UObject*>(Instance);

	if (*ObjPtr)
	{
		// 객체가 있으면 클래스 이름과 객체 이름 표시
		UClass* ObjClass = (*ObjPtr)->GetClass();
		FString ObjName = (*ObjPtr)->GetName();
		ImGui::Text("%s: %s (%s)", Prop.Name, ObjName.c_str(), ObjClass->Name);
	}
	else
	{
		// nullptr이면 None 표시
		ImGui::Text("%s: None", Prop.Name);
	}

	return false; // 읽기 전용
}

bool UPropertyRenderer::RenderStructProperty(const FProperty& Prop, void* Instance)
{
	// Struct는 읽기 전용으로 표시
	// FVector, FLinearColor 등 주요 타입은 이미 별도로 처리됨
	ImGui::Text("%s: [Struct]", Prop.Name);
	return false;
}

bool UPropertyRenderer::RenderTextureProperty(const FProperty& Prop, void* Instance)
{
	UTexture** TexturePtr = Prop.GetValuePtr<UTexture*>(Instance);
	UTexture* CurrentTexture = *TexturePtr;
	UTexture* NewTexture = nullptr;

	// 헬퍼 함수를 호출하여 텍스처 콤보박스를 렌더링합니다.
	bool bChanged = RenderTextureSelectionCombo(Prop.Name, CurrentTexture, NewTexture);

	if (bChanged)
	{
		// 프로퍼티 포인터 업데이트
		*TexturePtr = NewTexture;

		// 새 텍스처 경로 (None일 경우 빈 문자열)
		FString NewPath = (NewTexture) ? NewTexture->GetFilePath() : "";

		// 컴포넌트별 Setter 호출
		UObject* Obj = static_cast<UObject*>(Instance);
		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Obj))
		{
			Billboard->SetTextureName(NewPath);
		}
		else if (UDecalComponent* Decal = Cast<UDecalComponent>(Obj))
		{
			Decal->SetDecalTexture(NewPath);
		}

		return true;
	}

	return false;
}

bool UPropertyRenderer::RenderStaticMeshProperty(const FProperty& Prop, void* Instance)
{
	UStaticMesh** MeshPtr = Prop.GetValuePtr<UStaticMesh*>(Instance);

	FString CurrentPath;
	if (*MeshPtr)
	{
		CurrentPath = (*MeshPtr)->GetFilePath();
	}

	if (CachedStaticMeshPaths.empty())
	{
		ImGui::Text("%s: <No Meshes>", Prop.Name);
		return false;
	}

	int SelectedIdx = -1;
	for (int i = 0; i < static_cast<int>(CachedStaticMeshPaths.size()); ++i)
	{
		if (CachedStaticMeshPaths[i] == CurrentPath)
		{
			SelectedIdx = i;
			break;
		}
	}

	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo(Prop.Name, &SelectedIdx, CachedStaticMeshItems.data(), static_cast<int>(CachedStaticMeshItems.size())))
	{
		if (SelectedIdx >= 0 && SelectedIdx < static_cast<int>(CachedStaticMeshPaths.size()))
		{
			// 컴포넌트별 Setter 호출
			UObject* Object = static_cast<UObject*>(Instance);
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Object))
			{
				StaticMeshComponent->SetStaticMesh(CachedStaticMeshPaths[SelectedIdx]);
			}
			else
			{
				*MeshPtr = UResourceManager::GetInstance().Load<UStaticMesh>(CachedStaticMeshPaths[SelectedIdx]);
			}
			return true;
		}
	}

	// 닫힌 콤보박스 '텍스트' 부분에 마우스를 올렸을 때 전체 경로 툴팁
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		// 1. 원본 경로 표시 (CurrentTexturePath는 null일 때 "None"을 가짐)
		ImGui::TextUnformatted(CurrentPath.c_str());

		// 2. CurrentTexture가 유효하고 캐시 파일 경로가 있다면 추가로 표시
		if ((*MeshPtr))
		{
			const FString& CachedPath = (*MeshPtr)->GetCacheFilePath();
			if (!CachedPath.empty())
			{
				ImGui::Separator(); // 원본 경로와 구분하기 위해 선 추가
				ImGui::Text("Cache: %s", CachedPath.c_str());
			}
		}

		ImGui::EndTooltip();
	}

	return false;
}

bool UPropertyRenderer::RenderMaterialProperty(const FProperty& Prop, void* Instance)
{
	UMaterialInterface** MaterialPtr = Prop.GetValuePtr<UMaterialInterface*>(Instance);
	if (!MaterialPtr)
	{
		ImGui::Text("%s: [Invalid Material Ptr]", Prop.Name);
		return false;
	}

	UObject* OwningObject = static_cast<UObject*>(Instance);

	// Material 렌더 함수 호출
	return RenderSingleMaterialSlot(Prop.Name, MaterialPtr, OwningObject, 0);	// 단일 슬롯은 0번 인덱스
}

bool UPropertyRenderer::RenderMaterialArrayProperty(const FProperty& Prop, void* Instance)
{
	TArray<UMaterialInterface*>* MaterialSlots = Prop.GetValuePtr<TArray<UMaterialInterface*>>(Instance);
	if (!MaterialSlots)
	{
		ImGui::Text("%s: [Invalid Array Ptr]", Prop.Name);
		return false;
	}

	UObject* OwningObject = static_cast<UObject*>(Instance);
	bool bArrayChanged = false;

	// 배열의 각 요소를 순회하며 헬퍼 함수 호출 (매개변수 없음)
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlots->Num(); ++MaterialIndex)
	{
		FString Label = FString(Prop.Name) + " [" + std::to_string(MaterialIndex) + "]";
		UMaterialInterface** MaterialPtr = &(*MaterialSlots)[MaterialIndex];

		if (RenderSingleMaterialSlot(Label.c_str(), MaterialPtr, OwningObject, MaterialIndex))
		{
			bArrayChanged = true;
		}

		// 마지막 머티리얼은 제외하고 영역 분리
		if (MaterialIndex < MaterialSlots->Num() - 1)
		{
			ImGui::NewLine();
			ImGui::Separator();
			ImGui::NewLine();
		}
	}

	return bArrayChanged;
}

bool UPropertyRenderer::RenderSingleMaterialSlot(const char* Label, UMaterialInterface** MaterialPtr, UObject* OwningObject, uint32 MaterialIndex)
{
	bool bElementChanged = false;
	UMaterialInterface* CurrentMaterial = *MaterialPtr;

	// 캐시가 비어있으면 아무것도 렌더링하지 않음 (필수)
	if (CachedMaterialItems.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No materials available in cache.");
		return false;
	}

	// --- 1. 머티리얼 에셋 선택 콤보박스 ---
	FString CurrentMaterialPath = (CurrentMaterial) ? CurrentMaterial->GetFilePath() : "None";

	ImGui::SetNextItemWidth(240);
	if (ImGui::BeginCombo(Label, CurrentMaterialPath.c_str()))
	{
		// "None" 옵션
		bool bIsNoneSelected = (CurrentMaterial == nullptr);
		if (ImGui::Selectable(CachedMaterialItems[0], bIsNoneSelected))
		{
			bElementChanged = true;
			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(OwningObject))
			{
				PrimitiveComponent->SetMaterial(MaterialIndex, nullptr);
			}
			else
			{
				*MaterialPtr = nullptr;
			}
		}
		if (bIsNoneSelected) ImGui::SetItemDefaultFocus();

		// 캐시된 머티리얼 목록
		for (int j = 0; j < (int)CachedMaterialPaths.size(); ++j)
		{
			const FString& Path = CachedMaterialPaths[j];
			const char* DisplayName = CachedMaterialItems[j + 1];
			bool bIsSelected = (CurrentMaterialPath == Path);

			if (ImGui::Selectable(DisplayName, bIsSelected))
			{
				bElementChanged = true;
				if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(OwningObject))
				{
					PrimitiveComponent->SetMaterialByName(MaterialIndex, Path);
				}
				else
				{
					*MaterialPtr = UResourceManager::GetInstance().Load<UMaterial>(Path);
				}
			}
			if (bIsSelected) ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
		CurrentMaterial = *MaterialPtr; // 콤보박스에서 변경되었을 수 있으므로 업데이트
	}

	// --- 2. UMaterialInterface 내부 프로퍼티 렌더링 ---
	if (CurrentMaterial)
	{
		ImGui::Indent();

		// --- 2-1. 셰이더 (읽기 전용) ---
		UShader* CurrentShader = CurrentMaterial->GetShader();
		FString CurrentShaderPath = (CurrentShader) ? CurrentShader->GetFilePath() : "None";
		char ShaderPathBuffer[512];
		strncpy_s(ShaderPathBuffer, sizeof(ShaderPathBuffer), CurrentShaderPath.c_str(), _TRUNCATE);
		FString ShaderLabel = "Shader##" + FString(Label);
		ImGui::SetNextItemWidth(300);
		ImGui::BeginDisabled(true);
		ImGui::InputText(ShaderLabel.c_str(), ShaderPathBuffer, sizeof(ShaderPathBuffer), ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();

		// --- 셰이더 매크로 (읽기 전용) ---
		FString MacroKey = UShader::GenerateShaderKey(CurrentMaterial->GetShaderMacros());
		char ShaderMacroBuffer[512];
		strncpy_s(ShaderMacroBuffer, sizeof(ShaderMacroBuffer), MacroKey.c_str(), _TRUNCATE);
		FString ShaderMacroKeyLabel = "MacroKey##" + FString(Label);
		ImGui::SetNextItemWidth(300);
		ImGui::BeginDisabled(true);
		ImGui::InputText(ShaderMacroKeyLabel.c_str(), ShaderMacroBuffer, sizeof(ShaderMacroBuffer), ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();

		// --- 2-2. 텍스처 슬롯 ---

		// 이 슬롯의 머티리얼이 MID가 되도록 보장하는 람다 함수
		// ImGui 위젯 값이 변경될 때만 호출됩니다.
		auto EnsureMID = [&]() -> UMaterialInstanceDynamic*
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
				if (MID == nullptr)
				{
					if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OwningObject))
					{
						// StaticMeshComponent의 헬퍼가 MaterialSlots[MaterialIndex]와 *MaterialPtr을
						// 모두 업데이트한다고 가정합니다.
						MID = StaticMeshComp->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
						CurrentMaterial = MID; // 로컬 변수 업데이트
					}
					else
					{
						// StaticMeshComponent가 아닌 경우 (예: DecalComponent 등)
						MID = UMaterialInstanceDynamic::Create(CurrentMaterial);
						*MaterialPtr = MID; // 원본 포인터(*MaterialPtr)가 가리키는 곳의 값을 새 MID로 변경
						CurrentMaterial = MID; // 로컬 변수 업데이트

						// UPrimitiveComponent 파생 클래스라면 SetMaterial도 호출
						if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(OwningObject))
						{
							PrimitiveComponent->SetMaterial(MaterialIndex, MID);
						}
					}
				}
				return MID;
			};

		for (uint8 TexSlotIndex = 0; TexSlotIndex < (uint8)EMaterialTextureSlot::Max; ++TexSlotIndex)
		{
			EMaterialTextureSlot Slot = static_cast<EMaterialTextureSlot>(TexSlotIndex);
			UTexture* CurrentTexture = CurrentMaterial->GetTexture(Slot);

			const char* SlotName = "Unknown Slot";
			switch (Slot)
			{
			case EMaterialTextureSlot::Diffuse: SlotName = "Diffuse Texture"; break;
			case EMaterialTextureSlot::Normal: SlotName = "Normal Texture"; break;
			}
			FString TextureLabel = FString(SlotName) + "##" + Label;

			UTexture* NewTexture = nullptr;
			if (RenderTextureSelectionCombo(TextureLabel.c_str(), CurrentTexture, NewTexture))
			{
				// 텍스처 변경 시, MID를 확보하고 파라미터 설정
				if (UMaterialInstanceDynamic* MID = EnsureMID())
				{
					MID->SetTextureParameterValue(Slot, NewTexture);
					bElementChanged = true;
				}
			}
		}

		// --- 2-3. FMaterialInfo 파라미터 (스칼라 및 벡터) ---

		// ImGui에 표시하기 위해 현재 머티리얼의 최종 Info 값을 가져옵니다.
		// (MID인 경우 덮어쓰기가 적용된 복사본, UMaterial인 경우 원본)
		const FMaterialInfo& Info = CurrentMaterial->GetMaterialInfo();

		ImGui::Separator();

		// --- Colors (FVector -> ImGui::ColorEdit3) ---
		FLinearColor TempColor; // ImGui 위젯에 바인딩할 임시 변수

		// DiffuseColor
		TempColor = FLinearColor(Info.DiffuseColor); // FVector에서 FLinearColor로 변환 (생성자 가정)
		FString DiffuseLabel = "Diffuse Color##" + FString(Label);
		if (ImGui::ColorEdit3(DiffuseLabel.c_str(), &TempColor.R))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				// MID의 Setter는 FLinearColor를 받는다고 가정
				MID->SetVectorParameterValue("DiffuseColor", TempColor);
				bElementChanged = true;
			}
		}

		// AmbientColor
		TempColor = FLinearColor(Info.AmbientColor);
		FString AmbientLabel = "Ambient Color##" + FString(Label);
		if (ImGui::ColorEdit3(AmbientLabel.c_str(), &TempColor.R))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetVectorParameterValue("AmbientColor", TempColor);
				bElementChanged = true;
			}
		}

		// SpecularColor
		TempColor = FLinearColor(Info.SpecularColor);
		FString SpecularLabel = "Specular Color##" + FString(Label);
		if (ImGui::ColorEdit3(SpecularLabel.c_str(), &TempColor.R))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetVectorParameterValue("SpecularColor", TempColor);
				bElementChanged = true;
			}
		}

		// EmissiveColor
		TempColor = FLinearColor(Info.EmissiveColor);
		FString EmissiveLabel = "Emissive Color##" + FString(Label);
		if (ImGui::ColorEdit3(EmissiveLabel.c_str(), &TempColor.R))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetVectorParameterValue("EmissiveColor", TempColor);
				bElementChanged = true;
			}
		}

		// TransmissionFilter
		TempColor = FLinearColor(Info.TransmissionFilter);
		FString TransmissionLabel = "Transmission Filter##" + FString(Label);
		if (ImGui::ColorEdit3(TransmissionLabel.c_str(), &TempColor.R))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetVectorParameterValue("TransmissionFilter", TempColor);
				bElementChanged = true;
			}
		}

		// --- Floats (float -> ImGui::DragFloat) ---
		float TempFloat; // ImGui 위젯에 바인딩할 임시 변수

		// SpecularExponent (Shininess)
		TempFloat = Info.SpecularExponent;
		FString SpecExpLabel = "Specular Exponent##" + FString(Label);
		if (ImGui::DragFloat(SpecExpLabel.c_str(), &TempFloat, 1.0f, 0.0f, 1024.0f))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetScalarParameterValue("SpecularExponent", TempFloat);
				bElementChanged = true;
			}
		}

		// Transparency (d or Tr)
		TempFloat = Info.Transparency;
		FString TransparencyLabel = "Transparency##" + FString(Label);
		if (ImGui::DragFloat(TransparencyLabel.c_str(), &TempFloat, 0.01f, 0.0f, 1.0f))
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetScalarParameterValue("Transparency", TempFloat);
				bElementChanged = true;
			}
		}

		// OpticalDensity (Ni)
		TempFloat = Info.OpticalDensity;
		FString OpticalDensityLabel = "Optical Density##" + FString(Label);
		if (ImGui::DragFloat(OpticalDensityLabel.c_str(), &TempFloat, 0.01f, 0.0f, 10.0f)) // 범위는 임의로 지정
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetScalarParameterValue("OpticalDensity", TempFloat);
				bElementChanged = true;
			}
		}

		// BumpMultiplier (bm)
		TempFloat = Info.BumpMultiplier;
		FString BumpMultiplierLabel = "Bump Multiplier##" + FString(Label);
		if (ImGui::DragFloat(BumpMultiplierLabel.c_str(), &TempFloat, 0.01f, 0.0f, 5.0f)) // 범위는 임의로 지정
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				MID->SetScalarParameterValue("BumpMultiplier", TempFloat);
				bElementChanged = true;
			}
		}

		// --- Ints (IlluminationModel) ---
		int TempInt = Info.IlluminationModel;
		FString IllumModelLabel = "Illum Model##" + FString(Label);
		if (ImGui::DragInt(IllumModelLabel.c_str(), &TempInt, 1, 0, 10)) // 0-10은 OBJ 표준 범위
		{
			if (UMaterialInstanceDynamic* MID = EnsureMID())
			{
				// SetScalarParameterValue를 사용하여 int를 float로 캐스팅하여 저장
				MID->SetScalarParameterValue("IlluminationModel", (float)TempInt);
				bElementChanged = true;
			}
		}

		ImGui::Unindent();
	}

	return bElementChanged;
}

// ===== 텍스처 선택 헬퍼 함수 =====
bool UPropertyRenderer::RenderTextureSelectionCombo(const char* Label, UTexture* CurrentTexture, UTexture*& OutNewTexture)
{
	// 텍스처 캐시가 비어있으면 렌더링하지 않음
	if (CachedTextureItems.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No textures available in cache.");
		return false;
	}

	bool bChanged = false;
	FString CurrentTexturePath = (CurrentTexture) ? CurrentTexture->GetFilePath() : "None";

	// 1. 현재 선택된 텍스처의 인덱스 찾기
	int SelectedTextureIdx = 0; // "None"
	for (int j = 0; j < (int)CachedTexturePaths.size(); ++j)
	{
		if (CachedTexturePaths[j] == CurrentTexturePath)
		{
			SelectedTextureIdx = j + 1; // 0번은 "None"이므로 +1
			break;
		}
	}

	// 2. 콤보박스 왼쪽에 썸네일 표시
	const float ThumbnailSize = 24.0f;
	const ImVec2 ThumbnailImVec(ThumbnailSize, ThumbnailSize);

	if (CurrentTexture && CurrentTexture->GetShaderResourceView())
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(CurrentTexture->GetShaderResourceView()), ThumbnailImVec);

		// 썸네일 이미지에 마우스를 올렸을 때 이미지/크기 툴팁
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(CurrentTexture->GetFilePath().c_str());
			ImGui::Text("Size: %u x %u", CurrentTexture->GetWidth(), CurrentTexture->GetHeight());

			// 썸네일 크기 계산 (가로/세로 비율 유지, 최대 128x128)
			const float MAX_THUMBNAIL_SIZE = 128.0f;
			float width = (float)CurrentTexture->GetWidth();
			float height = (float)CurrentTexture->GetHeight();
			float ratio = width / height;

			ImVec2 thumbSize;
			if (ratio > 1.0f) // 가로가 더 긴 이미지
			{
				thumbSize = { MAX_THUMBNAIL_SIZE, MAX_THUMBNAIL_SIZE / ratio };
			}
			else // 세로가 더 길거나 같은 이미지
			{
				thumbSize = { MAX_THUMBNAIL_SIZE * ratio, MAX_THUMBNAIL_SIZE };
			}

			ImGui::Image(reinterpret_cast<ImTextureID>(CurrentTexture->GetShaderResourceView()), thumbSize);
			ImGui::EndTooltip();
		}
	}
	else
	{
		// 리소스가 없을 때도 레이아웃 유지를 위해 Dummy 사용
		ImGui::Dummy(ThumbnailImVec);
	}

	ImGui::SameLine();

	// 3. 커스텀 콤보박스 시작
	// 콤보박스에 표시될 텍스트 (현재 선택된 항목의 텍스트)
	const char* PreviewText = CachedTextureItems[SelectedTextureIdx];

	// SetNextItemWidth를 썸네일 크기만큼 보정
	ImGui::SetNextItemWidth(220.0f - ThumbnailSize - ImGui::GetStyle().ItemSpacing.x);

	if (ImGui::BeginCombo(Label, PreviewText))
	{
		// --- 콤보박스 드롭다운 리스트 렌더링 ---

		// 드롭다운 리스트 내부의 아이템 간 수직 간격 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 1.0f));

		// 텍스처 리스트 (미리보기 포함) - "None" 옵션(i=0)을 루프에 포함
		for (int i = 0; i < (int)CachedTextureItems.size(); ++i)
		{
			bool is_selected = (SelectedTextureIdx == i);
			const char* ItemText = CachedTextureItems[i];

			UTexture* previewTexture = nullptr;
			FString TexturePath = "None";

			if (i > 0)
			{
				TexturePath = CachedTexturePaths[i - 1];
				previewTexture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
			}

			FString SelectableID = FString("##Selectable_") + TexturePath + std::to_string(i) + Label; // Label을 추가하여 ID 고유성 보장

			// 1. Selectable을 먼저 호출
			if (ImGui::Selectable(SelectableID.c_str(), is_selected, 0, ImVec2(0, ThumbnailSize - 1.0f)))
			{
				// 선택된 텍스처 리소스를 경로로부터 로드합니다.
				UTexture* SelectedTexture = (i == 0)
					? nullptr
					: UResourceManager::GetInstance().Load<UTexture>(TexturePath);

				// OutNewTexture에 선택된 텍스처를 할당합니다.
				OutNewTexture = SelectedTexture;
				bChanged = true;
			}
			ImVec2 NextItemCursorPos = ImGui::GetCursorPos(); // 다음 아이템 위치 저장

			// 2. 툴팁 표시
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				if (previewTexture && previewTexture->GetTexture2D() && previewTexture->GetShaderResourceView())
				{
					ImGui::TextUnformatted(previewTexture->GetFilePath().c_str());
					ImGui::Text("Size: %u x %u", previewTexture->GetWidth(), previewTexture->GetHeight());
					float width = (float)previewTexture->GetWidth();
					float height = (float)previewTexture->GetHeight();
					float ratio = width / height;
					ImVec2 thumbSize = (ratio > 1.0f) ? ImVec2(128.0f, 128.0f / ratio) : ImVec2(128.0f * ratio, 128.0f);
					ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetShaderResourceView()), thumbSize);
				}
				else
				{
					ImGui::TextUnformatted("텍스처 없음");
				}
				ImGui::EndTooltip();
			}

			// 3. Selectable 위에 겹쳐 그리기 위해 커서를 되돌림
			ImVec2 ItemPos = ImGui::GetItemRectMin();

			// 4. 내용물(썸네일) 그리기
			ImVec2 ImagePos = ImVec2(ItemPos.x, ItemPos.y);
			ImGui::SetCursorScreenPos(ImagePos);


			if (previewTexture && previewTexture->GetShaderResourceView())
			{
				ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetShaderResourceView()), ThumbnailImVec);
			}
			else
			{
				ImGui::Dummy(ThumbnailImVec);
			}

			// 5. 내용물(텍스트) 그리기
			ImVec2 TextPos = ImVec2(
				ItemPos.x + ThumbnailSize + ImGui::GetStyle().ItemSpacing.x,
				ItemPos.y
			);
			ImGui::SetCursorScreenPos(TextPos);
			ImGui::TextUnformatted(ItemText);

			if (is_selected)
				ImGui::SetItemDefaultFocus();

			// 6. 커서 위치 복원
			ImGui::SetCursorPos(NextItemCursorPos);

			// --- 어설션 해결을 위해 Dummy 위젯 추가 ---
			ImGui::Dummy(ImVec2(0.0f, 0.0f));
		}

		// PushStyleVar로 변경했던 스타일을 원래대로 복원합니다.
		ImGui::PopStyleVar();

		ImGui::EndCombo();
	}

	// 닫힌 콤보박스 '텍스트' 부분에 마우스를 올렸을 때 전체 경로 툴팁
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		// 1. 원본 경로 표시 (CurrentTexturePath는 null일 때 "None"을 가짐)
		ImGui::TextUnformatted(CurrentTexturePath.c_str());

		// 2. CurrentTexture가 유효하고 캐시 파일 경로가 있다면 추가로 표시
		if (CurrentTexture)
		{
			const FString& CachedPath = CurrentTexture->GetCacheFilePath();
			if (!CachedPath.empty())
			{
				ImGui::Separator(); // 원본 경로와 구분하기 위해 선 추가
				ImGui::Text("Cache: %s", CachedPath.c_str());
			}
		}

		ImGui::EndTooltip();
	}

	return bChanged;
}

bool UPropertyRenderer::RenderTransformProperty(const FProperty& Prop, void* Instance)
{
	FVector* Value = Prop.GetValuePtr<FVector>(Instance);
	bool bAnyChanged = false;

	// 축 색상 정의
	ImVec4 ColorX(0.85f, 0.2f, 0.2f, 1.0f);   // 부드러운 빨간색
	ImVec4 ColorY(0.3f, 0.8f, 0.3f, 1.0f);    // 부드러운 초록색
	ImVec4 ColorZ(0.2f, 0.5f, 1.0f, 1.0f);    // 밝은 파란색

	float Speed = 0.1f;

	// 라벨 너비 설정 (Location, Rotation, Scale 모두 동일한 너비로 정렬)
	const float LabelWidth = 150.0f;
	const float InputWidth = 70.0f;
	const float ResetButtonWidth = 50.0f;
	const float FrameHeight = ImGui::GetFrameHeight(); // 입력 필드 높이
	const float ColorBoxWidth = FrameHeight * 0.25f;
	const float ColorBoxHeight = FrameHeight;

	// 프로퍼티별 고유 ID 스코프 시작
	ImGui::PushID(Prop.Name);

	// 라벨 출력
	ImGui::Text("%s", Prop.Name);
	ImGui::SameLine(LabelWidth);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// X축 (Red) - 색상 박스 + 입력 필드
	ImGui::PushID("X");

	// 색상 박스
	ImVec2 CursorPos = ImGui::GetCursorScreenPos();
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + ColorBoxWidth, CursorPos.y + ColorBoxHeight),
		ImGui::ColorConvertFloat4ToU32(ColorX)
	);
	ImGui::Dummy(ImVec2(ColorBoxWidth, ColorBoxHeight));
	ImGui::SameLine(0.0f, 0.0f); // 간격 없이 붙임

	// 입력 필드
	ImGui::PushItemWidth(InputWidth);
	if (ImGui::DragFloat("##X", &Value->X, Speed))
	{
		bAnyChanged = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
	ImGui::SameLine();

	// Y축 (Green) - 색상 박스 + 입력 필드
	ImGui::PushID("Y");

	// 색상 박스
	CursorPos = ImGui::GetCursorScreenPos();
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + ColorBoxWidth, CursorPos.y + ColorBoxHeight),
		ImGui::ColorConvertFloat4ToU32(ColorY)
	);
	ImGui::Dummy(ImVec2(ColorBoxWidth, ColorBoxHeight));
	ImGui::SameLine(0.0f, 0.0f); // 간격 없이 붙임

	// 입력 필드
	ImGui::PushItemWidth(InputWidth);
	if (ImGui::DragFloat("##Y", &Value->Y, Speed))
	{
		bAnyChanged = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
	ImGui::SameLine();

	// Z축 (Blue) - 색상 박스 + 입력 필드
	ImGui::PushID("Z");

	// 색상 박스
	CursorPos = ImGui::GetCursorScreenPos();
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + ColorBoxWidth, CursorPos.y + ColorBoxHeight),
		ImGui::ColorConvertFloat4ToU32(ColorZ)
	);
	ImGui::Dummy(ImVec2(ColorBoxWidth, ColorBoxHeight));
	ImGui::SameLine(0.0f, 0.0f); // 간격 없이 붙임

	// 입력 필드
	ImGui::PushItemWidth(InputWidth);
	if (ImGui::DragFloat("##Z", &Value->Z, Speed))
	{
		bAnyChanged = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
	ImGui::SameLine();

	// Reset 버튼
	ImGui::PushID("Reset");
	if (ImGui::Button("Reset", ImVec2(ResetButtonWidth, 0)))
	{
		// 프로퍼티에 따라 기본값 설정
		if (strcmp(Prop.Name, "RelativeLocation") == 0)
		{
			*Value = FVector(0.0f, 0.0f, 0.0f);
		}
		else if (strcmp(Prop.Name, "RelativeRotationEuler") == 0)
		{
			*Value = FVector(0.0f, 0.0f, 0.0f);
		}
		else if (strcmp(Prop.Name, "RelativeScale") == 0)
		{
			*Value = FVector(1.0f, 1.0f, 1.0f);
		}
		bAnyChanged = true;
	}
	ImGui::PopID();

	// 프로퍼티별 고유 ID 스코프 종료
	ImGui::PopID();

	return bAnyChanged;
}

bool UPropertyRenderer::RenderEnumProperty(const FProperty& Prop, void* Instance)
{
	if (!Prop.EnumNames || Prop.EnumCount == 0)
	{
		ImGui::Text("%s: [Invalid Enum]", Prop.Name);
		return false;
	}

	uint8* Value = Prop.GetValuePtr<uint8>(Instance);
	int CurrentValue = static_cast<int>(*Value);

	ImGui::Text("%s", Prop.Name);
	ImGui::SameLine();

	bool bChanged = false;

	// 라디오 버튼을 가로로 나란히 배치
	for (uint32 i = 0; i < Prop.EnumCount; ++i)
	{
		if (i > 0)
			ImGui::SameLine();

		ImGui::PushID(i);
		if (ImGui::RadioButton(Prop.EnumNames[i], CurrentValue == static_cast<int>(i)))
		{
			*Value = static_cast<uint8>(i);
			bChanged = true;
		}
		ImGui::PopID();
	}

	return bChanged;
}