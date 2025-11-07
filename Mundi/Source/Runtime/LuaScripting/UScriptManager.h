#pragma once
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include "Source/Runtime/Core/Misc/Delegate.h"

#include "CoroutineScheduler.h"

struct FLuaTemplateFunctions
{
    sol::function BeginPlay;
    sol::function EndPlay;
    sol::function OnOverlap;
    sol::function Tick;
    sol::function Restart;

    TMulticastDelegate<>::DelegateHandle OnOverlapDelegateHandle;
    TMulticastDelegate<>::DelegateHandle OnRestartDelegateHandle;
};

struct FLuaLocalValue
{
    AActor* MyActor = nullptr;
    class AGameModeBase* GameMode = nullptr;
};

struct FScript
{
    FString ScriptName;

    sol::environment Env;
    sol::table Table;
    FLuaTemplateFunctions LuaTemplateFunctions;

    // hot reload support
    FLuaLocalValue LuaLocalValue;
    fs::file_time_type LastModifiedTime;
};

class UScriptManager : public UObject
{
    DECLARE_CLASS(UScriptManager, UObject);
public:
    /*
     Single Ton Pattern
     이라 원래 Private에 있어야 하지만 IMPLEMENT_CLASS 이슈로
     일단 public에 넣습니다.
     TODO: 추후 수정해야 함.
    */
    UScriptManager();
    ~UScriptManager() override;

    void AttachScriptTo(FLuaLocalValue LuaLocalValue, const FString& ScriptName);
    void RemoveSelfOnDelegate(
        AActor* InActor,
        FLuaTemplateFunctions& LuaTemplateFunctions
    );
    void DetachScriptFrom(AActor* InActor, const FString& ScriptName);
    void DetachAllScriptFrom(AActor* InActor);

    void ModifyGameModeValueInScript(
        AActor* InActor,
        class AGameModeBase* InNewGameMode
	);

    void PrintDebugLog();

    TMap<AActor*, TArray<FScript*>>& GetScriptsByOwner();
    TArray<FScript*> GetScriptsOfActor(AActor* InActor);

    // 매 Frame마다 호출되는 함수
    void CheckAndHotReloadLuaScript();
    void UpdateCoroutineState(double Dt)
    {
        CoroutineScheduler.Update(Dt);
	}

    // 스크립트 파일 생성 (template.lua 복사)
    bool CreateScriptFile(const FString& ScriptName);

    // 스크립트 파일을 외부 에디터로 열기
    bool OpenScriptInEditor(const FString& ScriptName);

public:
    static UScriptManager& GetInstance();
private:
    void Initialize();
    void Shutdown();

    void RegisterUserTypeToLua();
    void RegisterGlobalValueToLua();
    void RegisterGlobalFuncToLua();
    void RegisterLocalValueToLua(
        sol::environment& InEnv,
        FLuaLocalValue LuaLocalValue
        );

    // 스크립트를 Actor에 부착할 때 Actor의 ShapeComponent에 Lua의 OnOverlap 함수를 연결한다
    void LinkOnOverlapWithShapeComponent(AActor* MyActor, sol::function OnOverlap);
    void LinkRestartToDeligate(FLuaTemplateFunctions& LuaTemplateFunctions);
    
    // Lua로부터 Template 함수를 가져온다.
    // 해당 함수가 없으면 Throw한다.
    FLuaTemplateFunctions GetTemplateFunctionFromScript(
        sol::environment& InEnv
    );
    
    void SetLuaScriptField(
        fs::path Path,
        sol::environment& InEnv,
        sol::table& InScriptTable,
        FLuaTemplateFunctions& InLuaTemplateFunction
    );
    
    FScript* GetOrCreate(FString InScriptName);
private:
    const static inline FString SCRIPT_FILE_PATH{"Scripts/"};
    const static inline FString DEFAULT_FILE_PATH{"Scripts/template.lua"};
private:
    sol::state Lua;
    
    // 소유자 기반 접근
    TMap<AActor*, TArray<FScript*>> ScriptsByOwner;

    UCoroutineScheduler CoroutineScheduler;
};