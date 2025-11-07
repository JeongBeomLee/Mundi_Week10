#include "pch.h"

// UObject를 ObjectFactory에 등록
IMPLEMENT_CLASS(UObject)

FString UObject::GetName()
{
    return ObjectName.ToString();
}

FString UObject::GetComparisonName()
{
    return FString();
}

void UObject::OnSerialized()
{
}

// 리플렉션 기반 자동 직렬화 (현재 클래스의 프로퍼티만 처리)
void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	const TArray<FProperty>& Properties = this->GetClass()->GetAllProperties();

	for (const FProperty& Prop : Properties)
	{
		switch (Prop.Type)
		{
		case EPropertyType::Bool:
		{
			bool* Value = Prop.GetValuePtr<bool>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadBool(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = *Value;
			break;
		}
		case EPropertyType::Int32:
		{
			int32* Value = Prop.GetValuePtr<int32>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadInt32(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = *Value;
			break;
		}
		case EPropertyType::Float:
		{
			float* Value = Prop.GetValuePtr<float>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadFloat(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = *Value;
			break;
		}
		case EPropertyType::FVector:
		{
			FVector* Value = Prop.GetValuePtr<FVector>(this);
			if (bInIsLoading)
			{
				// RelativeRotationEuler는 USceneComponent::Serialize()에서 "Rotation" 키로 처리하므로 AutoSerialize에서는 건너뜀
				// (AutoSerialize가 "RelativeRotationEuler" 키를 찾지 못해 (0,0,0)으로 덮어쓰는 버그 방지)
				if (strcmp(Prop.Name, "RelativeRotationEuler") == 0)
				{
					// 키가 존재할 때만 읽어들임 (키가 없으면 현재 값 유지)
					if (InOutHandle.hasKey(Prop.Name))
					{
						FJsonSerializer::ReadVector(InOutHandle, Prop.Name, *Value);
					}
					// 키가 없으면 아무것도 하지 않음 (USceneComponent::Serialize가 이미 처리함)
				}
				else
				{
					FJsonSerializer::ReadVector(InOutHandle, Prop.Name, *Value);
				}
			}
			else
				InOutHandle[Prop.Name] = FJsonSerializer::VectorToJson(*Value);
			break;
		}
		case EPropertyType::FLinearColor:
		{
			FLinearColor* Value = Prop.GetValuePtr<FLinearColor>(this);
			if (bInIsLoading)
			{
				FVector4 ColorVec;
				FJsonSerializer::ReadVector4(InOutHandle, Prop.Name, ColorVec);
				*Value = FLinearColor(ColorVec);
			}
			else
			{
				InOutHandle[Prop.Name] = FJsonSerializer::Vector4ToJson(Value->ToFVector4());
			}
			break;
		}
		case EPropertyType::FString:
		{
			FString* Value = Prop.GetValuePtr<FString>(this);
			if (bInIsLoading)
				FJsonSerializer::ReadString(InOutHandle, Prop.Name, *Value);
			else
				InOutHandle[Prop.Name] = Value->c_str();
			break;
		}
		case EPropertyType::FName:
		{
			FName* Value = Prop.GetValuePtr<FName>(this);
			if (bInIsLoading)
			{
				FString NameString;
				FJsonSerializer::ReadString(InOutHandle, Prop.Name, NameString);
				*Value = FName(NameString);
			}
			else
			{
				InOutHandle[Prop.Name] = Value->ToString().c_str();
			}
			break;
		}
		case EPropertyType::Texture:
		{
			UTexture** Value = Prop.GetValuePtr<UTexture*>(this);
			if (bInIsLoading)
			{
				FString TexturePath;
				FJsonSerializer::ReadString(InOutHandle, Prop.Name, TexturePath);
				if (!TexturePath.empty())
				{
					*Value = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
				}
				else
				{
					*Value = nullptr;
				}
			}
			else
			{
				if (*Value)
				{
					InOutHandle[Prop.Name] = (*Value)->GetFilePath().c_str();
				}
				else
				{
					InOutHandle[Prop.Name] = "";
				}
			}
			break;
		}
		case EPropertyType::StaticMesh:
		{
			UStaticMesh** Value = Prop.GetValuePtr<UStaticMesh*>(this);
			if (bInIsLoading)
			{
				FString MeshPath;
				FJsonSerializer::ReadString(InOutHandle, Prop.Name, MeshPath);
				if (!MeshPath.empty())
				{
					*Value = UResourceManager::GetInstance().Load<UStaticMesh>(MeshPath);
				}
				else
				{
					*Value = nullptr;
				}
			}
			else
			{
				if (*Value)
				{
					InOutHandle[Prop.Name] = (*Value)->GetAssetPathFileName().c_str();
				}
				else
				{
					InOutHandle[Prop.Name] = "";
				}
			}
			break;
		}
		case EPropertyType::Material:
		{
			UMaterial** Value = Prop.GetValuePtr<UMaterial*>(this);
			if (bInIsLoading)
			{
				FString MaterialPath;
				FJsonSerializer::ReadString(InOutHandle, Prop.Name, MaterialPath);
				if (!MaterialPath.empty())
				{
					*Value = UResourceManager::GetInstance().Load<UMaterial>(MaterialPath);
				}
				else
				{
					*Value = nullptr;
				}
			}
			else
			{
				if (*Value)
				{
					InOutHandle[Prop.Name] = (*Value)->GetFilePath().c_str();
				}
				else
				{
					InOutHandle[Prop.Name] = "";
				}
			}
			break;
		}
		case EPropertyType::Enum:
		{
			// Enum은 uint8로 저장 (underlying type)
			uint8* Value = Prop.GetValuePtr<uint8>(this);
			if (bInIsLoading)
			{
				int32 EnumValue = 0;
				FJsonSerializer::ReadInt32(InOutHandle, Prop.Name, EnumValue);
				*Value = static_cast<uint8>(EnumValue);
			}
			else
			{
				InOutHandle[Prop.Name] = static_cast<int32>(*Value);
			}
			break;
		}
		case EPropertyType::Array:
		{
			// Array 직렬화는 InnerType에 따라 처리
			if (Prop.InnerType == EPropertyType::Unknown)
			{
				UE_LOG("[AutoSerialize] Array property '%s' has Unknown InnerType, skipping.", Prop.Name);
				break;
			}

			JSON ArrayJson;
			if (bInIsLoading)
			{
				// JSON에서 배열 읽기
				if (!FJsonSerializer::ReadArray(InOutHandle, Prop.Name, ArrayJson))
				{
					break; // 배열이 없거나 유효하지 않음
				}
			}
			else
			{
				// 저장용 빈 배열 생성
				ArrayJson = JSON::Make(JSON::Class::Array);
			}

			// InnerType에 따라 처리
			switch (Prop.InnerType)
			{
			case EPropertyType::Int32:
			{
				TArray<int32>* ArrayPtr = Prop.GetValuePtr<TArray<int32>>(this);
				SerializePrimitiveArray<int32>(ArrayPtr, bInIsLoading, ArrayJson);
				break;
			}
			case EPropertyType::Float:
			{
				TArray<float>* ArrayPtr = Prop.GetValuePtr<TArray<float>>(this);
				SerializePrimitiveArray<float>(ArrayPtr, bInIsLoading, ArrayJson);
				break;
			}
			case EPropertyType::Bool:
			{
				TArray<bool>* ArrayPtr = Prop.GetValuePtr<TArray<bool>>(this);
				SerializePrimitiveArray<bool>(ArrayPtr, bInIsLoading, ArrayJson);
				break;
			}
			case EPropertyType::FString:
			{
				TArray<FString>* ArrayPtr = Prop.GetValuePtr<TArray<FString>>(this);
				SerializePrimitiveArray<FString>(ArrayPtr, bInIsLoading, ArrayJson);
				break;
			}
			default:
			{
				UE_LOG("[AutoSerialize] Array property '%s' has unsupported InnerType: %d",
					   Prop.Name, static_cast<int>(Prop.InnerType));
				break;
			}
			}

			// 저장 시 JSON에 배열 쓰기
			if (!bInIsLoading)
			{
				InOutHandle[Prop.Name] = ArrayJson;
			}
			break;
		}
		// ObjectPtr, Struct 등은 필요시 추가
		}
	}
	OnSerialized();
}

void UObject::DuplicateSubObjects()
{
    UUID = GenerateUUID(); // UUID는 고유값이므로 새로 생성
}

UObject* UObject::Duplicate() const
{
    UObject* NewObject = ObjectFactory::DuplicateObject<UObject>(this); // 모든 멤버 얕은 복사

    NewObject->DuplicateSubObjects(); // 얕은 복사한 멤버들에 대해 메뉴얼하게 깊은 복사 수행

	NewObject->PostDuplicate();
    return NewObject;
}


