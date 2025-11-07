#pragma once
#include "Info.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class AHeightFogActor : public AInfo
{
public:
	DECLARE_CLASS(AHeightFogActor, AInfo)
	GENERATED_REFLECTION_BODY()

	AHeightFogActor();

	DECLARE_ACTOR_DUPLICATE(AHeightFogActor)

	void DuplicateSubObjects() override;
};