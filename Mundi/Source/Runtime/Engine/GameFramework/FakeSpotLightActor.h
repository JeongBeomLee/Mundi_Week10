#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class UBillboardComponent;
class UPerspectiveDecalComponent;

class AFakeSpotLightActor : public AActor
{
public:
	DECLARE_CLASS(AFakeSpotLightActor, AActor)
	GENERATED_REFLECTION_BODY()

	AFakeSpotLightActor();
protected:
	~AFakeSpotLightActor() override;

public:
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UPerspectiveDecalComponent* GetDecalComponent() const { return DecalComponent; }

	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(AFakeSpotLightActor)

	void OnSerialized() override;
protected:
	UBillboardComponent* BillboardComponent{};
	UPerspectiveDecalComponent* DecalComponent{};
};
