#include "pch.h"
#include "AmbientLightActor.h"
#include "AmbientLightComponent.h"

IMPLEMENT_CLASS(AAmbientLightActor)

BEGIN_PROPERTIES(AAmbientLightActor)
	MARK_AS_SPAWNABLE("앰비언트 라이트", "주변광을 발산하는 라이트 액터입니다.")
END_PROPERTIES()

AAmbientLightActor::AAmbientLightActor()
{
	Name = "Ambient Light Actor";
	LightComponent = CreateDefaultSubobject<UAmbientLightComponent>("AmbientLightComponent");

	RootComponent = LightComponent;
}

AAmbientLightActor::~AAmbientLightActor()
{
}

void AAmbientLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UAmbientLightComponent* Light = Cast<UAmbientLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void AAmbientLightActor::OnSerialized()
{
	Super::OnSerialized();

	LightComponent = Cast<UAmbientLightComponent>(RootComponent);
}
