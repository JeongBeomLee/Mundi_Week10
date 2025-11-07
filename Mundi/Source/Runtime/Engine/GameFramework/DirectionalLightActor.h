#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class UDirectionalLightComponent;

class ADirectionalLightActor : public AActor
{
public:
	DECLARE_CLASS(ADirectionalLightActor, AActor)
	GENERATED_REFLECTION_BODY()

	ADirectionalLightActor();
protected:
	~ADirectionalLightActor() override;

public:
	UDirectionalLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(ADirectionalLightActor)

	void OnSerialized() override;

protected:
	UDirectionalLightComponent* LightComponent;
};
