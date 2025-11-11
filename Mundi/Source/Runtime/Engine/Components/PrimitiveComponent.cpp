#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(UPrimitiveComponent)

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    UMaterial* MaterialToSet = nullptr;

    // 머티리얼 이름이 비어있으면 기본 머티리얼 사용
    if (InMaterialName.empty())
    {
        MaterialToSet = UResourceManager::GetInstance().GetDefaultMaterial();
    }
    else
    {
        // 머티리얼 로드 시도
        MaterialToSet = UResourceManager::GetInstance().Load<UMaterial>(InMaterialName);

        // 로드 실패 시 기본 머티리얼 사용
        if (!MaterialToSet)
        {
            MaterialToSet = UResourceManager::GetInstance().GetDefaultMaterial();
        }
    }

    SetMaterial(InElementIndex, MaterialToSet);
}

void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::OnSerialized()
{
    Super::OnSerialized();
}
