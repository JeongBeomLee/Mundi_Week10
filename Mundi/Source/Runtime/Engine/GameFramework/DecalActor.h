#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

class UDecalComponent;

class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)
    GENERATED_REFLECTION_BODY()

    ADecalActor();
protected:
    ~ADecalActor() override;

public:
    UDecalComponent* GetDecalComponent() const { return DecalComponent; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_ACTOR_DUPLICATE(ADecalActor)

    // Serialize
    void OnSerialized() override;

protected:
    UDecalComponent* DecalComponent;
};
