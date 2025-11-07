#include "pch.h"
#include "SelectionManager.h"
#include "Actor.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(USelectionManager)

void USelectionManager::SelectActor(AActor* Actor)
{
    if (!Actor) return;
    
    // 단일 선택 모드 (기존 선택 해제)
    ClearSelection();
    
    // 새 액터 선택
    SelectedActors.Add(Actor);
    SelectedComponent = Actor->GetRootComponent();
    bIsActorMode = true;
}

void USelectionManager::SelectComponent(UActorComponent* Component)
{

    if (!Component)
    {
        SelectedComponent = nullptr;
        bIsActorMode = false;
        return;
    }
    AActor* SelectedActor = Component->GetOwner();
    //이미 엑터가 피킹되어 있는 상황
    if (IsActorSelected(SelectedActor) )
    {
        //에딧이 안되는 컴포넌트의 경우, 부모가 있으면 부모, 없으면 루트컴포넌트 피킹
        if (!Component->IsEditable())
        {
            if (USceneComponent* SelectedSceneComponent = Cast<USceneComponent>(SelectedComponent))
            {
                if (!(SelectedComponent = SelectedSceneComponent->GetAttachParent()))
                {
                    SelectedComponent = SelectedActor->GetRootComponent();
                }
            }
        }
        else
        {
            SelectedComponent = Component;
        }
        bIsActorMode = false;
    }
    //오너 엑터가 선택 안돼있는 상태, 루트 컴포넌트 피킹
    else
    {
        bIsActorMode = true;
        ClearSelection();
        SelectedActors.Add(SelectedActor);
        SelectedComponent = SelectedActor->GetRootComponent();
    }
    
}

void USelectionManager::DeselectActor(AActor* Actor)
{
    if (!Actor) return;
    
    auto it = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (it != SelectedActors.end())
    {
        SelectedActors.erase(it);
    }
    SelectedComponent = nullptr;
}

void USelectionManager::ClearSelection()
{
    for (AActor* Actor : SelectedActors)
    {
        if (Actor) // null 체크 추가
        {
            Actor->SetIsPicked(false);
        }
    }
    SelectedActors.clear();
    SelectedComponent = nullptr;
}

bool USelectionManager::IsActorSelected(AActor* Actor) const
{
    if (!Actor) return false;
    
    return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
}

AActor* USelectionManager::GetSelectedActor() const
{
    // 첫 번째 유효한 액터 연기
    for (AActor* Actor : SelectedActors)
    {
        if (Actor) return Actor;
    }
    return nullptr;
}

void USelectionManager::CleanupInvalidActors()
{

    // null이거나 삭제된 액터들을 제거
    auto it = std::remove_if(SelectedActors.begin(), SelectedActors.end(), 
        [](AActor* Actor) { return Actor == nullptr; });
    SelectedActors.erase(it, SelectedActors.end());
}

USelectionManager::USelectionManager()
{
    SelectedActors.Reserve(1);
}

USelectionManager::~USelectionManager()
{
    // No-op: instances are destroyed by ObjectFactory::DeleteAll
}
