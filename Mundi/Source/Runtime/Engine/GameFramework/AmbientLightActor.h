#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class UAmbientLightComponent;

class AAmbientLightActor : public AActor
{
public:
	DECLARE_CLASS(AAmbientLightActor, AActor)
	GENERATED_REFLECTION_BODY()

	AAmbientLightActor();
protected:
	~AAmbientLightActor() override;

public:
	UAmbientLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(AAmbientLightActor)

	void OnSerialized() override;

protected:
	UAmbientLightComponent* LightComponent;
};
