#pragma once
#include "Actor.h"

class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
public:
    DECLARE_CLASS(ASkeletalMeshActor, AActor);
    GENERATED_REFLECTION_BODY()
    
    ASkeletalMeshActor();
    void Tick(float DeltaTime) override;

protected:
    ~ASkeletalMeshActor() override;

public:
    FAABB GetBounds() const override;
    USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
    void SetSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent);

    void DuplicateSubObjects() override;
    DECLARE_ACTOR_DUPLICATE(ASkeletalMeshActor)

    void OnSerialized() override;

protected:
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};
