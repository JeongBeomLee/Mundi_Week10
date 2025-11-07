#pragma once

void PrintToConsole(const char* ch);
bool IsKeyPressed(int KeyCode);

class ARunnerGameMode;
class UWorld;

ARunnerGameMode* GetRunnerGameMode(UWorld* World);
bool IsGamePlaying(UWorld* World);
