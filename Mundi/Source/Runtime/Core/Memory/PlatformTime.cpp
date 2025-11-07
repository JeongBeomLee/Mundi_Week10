#include "pch.h"
#include "PlatformTime.h"

// Define static members declared in FWindowsPlatformTime
double FWindowsPlatformTime::GSecondsPerCycle = 0.0;
bool   FWindowsPlatformTime::bInitialized     = false;
