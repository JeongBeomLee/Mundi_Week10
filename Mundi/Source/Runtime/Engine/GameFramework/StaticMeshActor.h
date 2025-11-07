#pragma once
#include "Actor.h"
#include "Enums.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class UStaticMeshComponent;
class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	GENERATED_REFLECTION_BODY()

	AStaticMeshActor();
	virtual void Tick(float DeltaTime) override;
protected:
	~AStaticMeshActor() override;

public:
	virtual FAABB GetBounds() const override;
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	void SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent);

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(AStaticMeshActor)

	// Serialize
	void OnSerialized() override;

protected:
	UStaticMeshComponent* StaticMeshComponent;
};

