#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class UPointLightComponent;

class APointLightActor : public AActor
{
public:
	DECLARE_CLASS(APointLightActor, AActor)
	GENERATED_REFLECTION_BODY()

	APointLightActor();
protected:
	~APointLightActor() override;

public:
	UPointLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(APointLightActor)

	void OnSerialized() override;

protected:
	UPointLightComponent* LightComponent;
};
