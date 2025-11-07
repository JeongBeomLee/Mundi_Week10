#include "pch.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "ObjectFactory.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(AStaticMeshActor)

BEGIN_PROPERTIES(AStaticMeshActor)
	MARK_AS_SPAWNABLE("스태틱 메시", "정적 메시를 렌더링하는 액터입니다.")
END_PROPERTIES()

AStaticMeshActor::AStaticMeshActor()
{
    Name = "Static Mesh Actor";
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
    
    // 루트 교체
    RootComponent = StaticMeshComponent;
}

void AStaticMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

AStaticMeshActor::~AStaticMeshActor()
{
   
}

FAABB AStaticMeshActor::GetBounds() const
{
    if (StaticMeshComponent)
    {
        return StaticMeshComponent->GetWorldAABB();
    }

    return FAABB();
}

void AStaticMeshActor::SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent)
{
    StaticMeshComponent = InStaticMeshComponent;
}

void AStaticMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
        {
            StaticMeshComponent = StaticMeshComp;
            break;
        }
    }
}

void AStaticMeshActor::OnSerialized()
{
    Super::OnSerialized();

    StaticMeshComponent = Cast<UStaticMeshComponent>(RootComponent);
    
}
