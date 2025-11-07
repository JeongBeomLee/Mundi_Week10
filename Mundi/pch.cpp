#include "pch.h"

TMap<FString, FString> EditorINI;
const FString GDataDir = "Data";
const FString GCacheDir = "DerivedDataCache";

// Global client area size used by various modules
float CLIENTWIDTH = 1024.0f;
float CLIENTHEIGHT = 1024.0f;

UEditorEngine GEngine;

UWorld* GWorld = nullptr;