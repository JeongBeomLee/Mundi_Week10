#include "pch.h"
#include "DirectionalLightActor.h"
#include "DirectionalLightComponent.h"

IMPLEMENT_CLASS(ADirectionalLightActor)

BEGIN_PROPERTIES(ADirectionalLightActor)
	MARK_AS_SPAWNABLE("디렉셔널 라이트", "방향성 빛을 발산하는 라이트 액터입니다.")
END_PROPERTIES()

ADirectionalLightActor::ADirectionalLightActor()
{
	Name = "Directional Light Actor";
	LightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("DirectionalLightComponent");

	RootComponent = LightComponent;
}

ADirectionalLightActor::~ADirectionalLightActor()
{
}

void ADirectionalLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void ADirectionalLightActor::OnSerialized()
{
	Super::OnSerialized();

	LightComponent = Cast<UDirectionalLightComponent>(RootComponent);

}
