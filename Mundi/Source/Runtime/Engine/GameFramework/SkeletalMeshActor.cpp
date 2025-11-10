#include "pch.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshComponent.h"

IMPLEMENT_CLASS(ASkeletalMeshActor)

BEGIN_PROPERTIES(ASkeletalMeshActor)
    MARK_AS_SPAWNABLE("스켈레탈 메시", "스켈레탈 메시를 렌더링하는 액터입니다.")
END_PROPERTIES()

ASkeletalMeshActor::ASkeletalMeshActor()
{
    Name = "Skeletal Mesh Actor";
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");

    RootComponent = SkeletalMeshComponent;    
}

void ASkeletalMeshActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}

ASkeletalMeshActor::~ASkeletalMeshActor()
{
}

FAABB ASkeletalMeshActor::GetBounds() const
{
    if (SkeletalMeshComponent)
    {
        return SkeletalMeshComponent->GetWorldAABB();
    }

    return FAABB();
}

void ASkeletalMeshActor::SetSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
{
    SkeletalMeshComponent = InSkeletalMeshComponent;
}

void ASkeletalMeshActor::DuplicateSubObjects()
{
    AActor::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(Component))
        {
            SkeletalMeshComponent = SkeletalMeshComp;
            break;
        }
    }
}

void ASkeletalMeshActor::OnSerialized()
{
    AActor::OnSerialized();

    SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RootComponent);
}
