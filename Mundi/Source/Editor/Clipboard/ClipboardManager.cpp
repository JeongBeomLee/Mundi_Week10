#include "pch.h"
#include "ClipboardManager.h"
#include "Actor.h"
#include "ActorComponent.h"
#include "SceneComponent.h"
#include "World.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(UClipboardManager)

UClipboardManager::UClipboardManager()
{
}

UClipboardManager::~UClipboardManager()
{
    ClearClipboard();
}

void UClipboardManager::CopyActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    // 클립보드 초기화
    ClearClipboard();

    // Actor 원본 참조 저장 (실제 복제는 붙여넣기 시 수행)
    CopiedActor = Actor;
}

void UClipboardManager::CopyComponent(UActorComponent* Component, AActor* OwnerActor)
{
    // 컴포넌트 복붙 실패했음.. 도전 하고 싶으면 하세요

    if (!Component || !OwnerActor)
    {
        return;
    }

    // 클립보드 초기화
    ClearClipboard();

    // Component 원본 참조 저장
    CopiedComponent = Component;
    CopiedComponentOwner = OwnerActor;
}

AActor* UClipboardManager::PasteActorToWorld(UWorld* World, const FVector& OffsetFromOriginal)
{
    if (!World || !CopiedActor)
    {
        return nullptr;
    }

    // Actor 복제
    AActor* NewActor = CopiedActor->Duplicate();
    if (!NewActor)
    {
        return nullptr;
    }

    // 원본 위치에 오프셋 적용
    FVector OriginalLocation = CopiedActor->GetActorLocation();
    FVector NewLocation = OriginalLocation + OffsetFromOriginal;
    NewActor->SetActorLocation(NewLocation);

    // 고유한 이름 생성
    FString ActorTypeName = CopiedActor->GetClass()->Name;
    FString UniqueName = World->GenerateUniqueActorName(ActorTypeName);
    NewActor->SetName(UniqueName);

    // World에 등록
    NewActor->SetWorld(World);
    World->OnActorSpawned(NewActor);

    return NewActor;
}

bool UClipboardManager::PasteComponentToActor(AActor* TargetActor)
{
    // 컴포넌트 복붙 실패했음.. 도전 하고 싶으면 하세요

    if (!TargetActor || !CopiedComponent)
    {
        return false;
    }

    // Component 복제
    UActorComponent* NewComponent = CopiedComponent->Duplicate();
    if (!NewComponent)
    {
        return false;
    }

    // 새 Owner 설정
    NewComponent->SetOwner(TargetActor);
    TargetActor->AddOwnedComponent(NewComponent);

    // SceneComponent인 경우 RootComponent에 Attach
    USceneComponent* SceneComp = Cast<USceneComponent>(NewComponent);
    if (SceneComp && TargetActor->GetRootComponent())
    {
        SceneComp->SetupAttachment(TargetActor->GetRootComponent(), EAttachmentRule::KeepRelative);

        // World에 등록
        if (TargetActor->GetWorld())
        {
            TargetActor->RegisterComponentTree(SceneComp, TargetActor->GetWorld());
        }
    }

    return true;
}

void UClipboardManager::ClearClipboard()
{
    // 참조만 저장하므로 nullptr로 초기화
    CopiedActor = nullptr;
    CopiedComponent = nullptr;
    CopiedComponentOwner = nullptr;
}
