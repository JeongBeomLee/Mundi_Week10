#pragma once

#define ANONYMOUS_PROFILER_VAR_NAME_INTERNAL(name, line) name##line
#define ANONYMOUS_PROFILER_VAR_NAME ANONYMOUS_PROFILER_VAR_NAME_INTERNAL(profiler, __LINE__)

#define PROFILE_FUNCTION() FTimeProfiler ANONYMOUS_PROFILER_VAR_NAME(__FUNCTION__, __LINE__);
#define PROFILE_SCOPE(name) FTimeProfiler ANONYMOUS_PROFILER_VAR_NAME(name, __LINE__);

class FTimeProfiler
{
public:
    FTimeProfiler(const FString& InName, int InLine);
    ~FTimeProfiler();

private:
    FString ScopeName;
    int Line;
    std::chrono::time_point<std::chrono::high_resolution_clock> StartTime;
};