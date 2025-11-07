#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class AEmptyActor : public AActor
{
public:
	DECLARE_CLASS(AEmptyActor, AActor)
	GENERATED_REFLECTION_BODY()

	AEmptyActor();
	~AEmptyActor() override = default;

	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(AEmptyActor)

	void OnSerialized() override;
};
