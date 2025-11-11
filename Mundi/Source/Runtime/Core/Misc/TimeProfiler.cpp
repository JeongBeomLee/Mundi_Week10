#include "pch.h"
#include "TimeProfiler.h"

FTimeProfiler::FTimeProfiler(const FString& InName, int InLine)
    : ScopeName(InName), Line(InLine)
{
    StartTime = std::chrono::high_resolution_clock::now();
}

FTimeProfiler::~FTimeProfiler()
{
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Start = std::chrono::time_point_cast<std::chrono::microseconds>(StartTime).time_since_epoch().count();
    auto End = std::chrono::time_point_cast<std::chrono::microseconds>(EndTime).time_since_epoch().count();
    double Duration = std::chrono::duration<double, std::milli>(End - Start).count();

    UE_LOG("[PROFILE] %s : %.3f ms", ScopeName.c_str(), Duration);
}
