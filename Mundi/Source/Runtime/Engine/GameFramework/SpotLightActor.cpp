#include "pch.h"
#include "SpotLightActor.h"
#include "SpotLightComponent.h"

IMPLEMENT_CLASS(ASpotLightActor)

BEGIN_PROPERTIES(ASpotLightActor)
	MARK_AS_SPAWNABLE("스포트 라이트", "스포트 라이트 액터입니다.")
END_PROPERTIES()

ASpotLightActor::ASpotLightActor()
{
	Name = "Spot Light Actor";
	LightComponent = CreateDefaultSubobject<USpotLightComponent>("SpotLightComponent");

	RootComponent = LightComponent;
}

ASpotLightActor::~ASpotLightActor()
{
}

void ASpotLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (USpotLightComponent* Light = Cast<USpotLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void ASpotLightActor::OnSerialized()
{
	Super::OnSerialized();

	LightComponent = Cast<USpotLightComponent>(RootComponent);
	
}
