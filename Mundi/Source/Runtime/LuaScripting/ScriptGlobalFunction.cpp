#include "pch.h"
#include "Source/Runtime/LuaScripting/ScriptGlobalFunction.h"
#include "Source/Runtime/Engine/GameFramework/RunnerGameMode.h"
#include "Source/Runtime/Engine/GameFramework/GameStateBase.h"

void PrintToConsole(const char* ch)
{
    UE_LOG(ch);
}

ARunnerGameMode* GetRunnerGameMode(UWorld* World) {
    if (!World) return nullptr;
    AGameModeBase* GameMode = World->GetGameMode();
    if (!GameMode) return nullptr;
    return Cast<ARunnerGameMode>(GameMode);
}

bool IsGamePlaying(UWorld* World) {
    if (!World) return false;
    AGameModeBase* GameMode = World->GetGameMode();
    if (!GameMode) return false;
    AGameStateBase* GameState = GameMode->GetGameState();
    if (!GameState) return false;
    return GameState->GetGameState() == EGameState::Playing;
}