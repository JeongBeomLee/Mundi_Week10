#pragma once
#include "Object.h"
#include "UEContainer.h"
#include "Actor.h"
#include <algorithm>

class ULevel : public UObject
{
public:
    DECLARE_CLASS(ULevel, UObject)
    ULevel() = default;
    ~ULevel() override = default;

    const TArray<AActor*>& GetActors() const { return Actors; }
    void AddActor(AActor* Actor) { if (Actor) Actors.Add(Actor); }
    void SpawnDefaultActors();
    bool RemoveActor(AActor* Actor)
    {
        auto it = std::find(Actors.begin(), Actors.end(), Actor);
        if (it != Actors.end()) { Actors.erase(it); return true; }
        return false;
    }
    void Clear() { Actors.Empty(); }

    void Serialize(const bool bInIsLoading, JSON& InOutHandle);
private:
    TArray<AActor*> Actors;
};

class ULevelService
{
public:
    // Create a new empty level
    static std::unique_ptr<ULevel> CreateNewLevel();
    static std::unique_ptr<ULevel> CreateDefaultLevel();
};
