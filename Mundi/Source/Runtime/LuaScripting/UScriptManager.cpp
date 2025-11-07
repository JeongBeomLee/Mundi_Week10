#include "pch.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

#include "CollisionComponent/BoxComponent.h"
#include "CollisionComponent/CapsuleComponent.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "SpringArmComponent.h"
#include "SceneComponent.h"
#include "Source/Runtime/LuaScripting/ScriptGlobalFunction.h"
#include "Pawn.h"
#include "Character.h"
#include "RunnerCharacter.h"
#include "GameModeBase.h"
#include "GameStateBase.h"
#include "RunnerGameMode.h"
#include "CharacterMovementComponent.h"
#include "InputComponent.h"
#include "World.h"
#include "EditorEngine.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "GravityWall.h"
#include "CoinActor.h"
#include "ProjectileActor.h"
#include "ProjectileMovementComponent.h"
#include "DecalActor.h"
#include "DecalComponent.h"
#include "TextRenderComponent.h"
#include "BillboardComponent.h"
#include "ParticleComponent.h"
#include "SpriteActor.h"
#include "HeightFogActor.h"
#include "PlayerCameraManager.h"
#include "PlayerController.h"
#include "UCameraModifier.h"
#include "UCameraModifier_CameraShake.h"
#include "UCameraModifier_LetterBox.h"
#include "UCameraModifier_Vignetting.h"
#include "HeightFogComponent.h"
#include "DeltaTimeManager.h"
#include "Color.h"
#include "SoundManager.h"

IMPLEMENT_CLASS(UScriptManager)

UScriptManager::UScriptManager()
{
    Initialize();
}

UScriptManager::~UScriptManager()
{
    // Shutdown();
}

void UScriptManager::AttachScriptTo(FLuaLocalValue LuaLocalValue, const FString& ScriptName)
{
    if (GWorld->bPie)
    {
        LuaLocalValue.GameMode = GWorld->GetGameMode();
    }

    // 이미 같은 스크립트가 부착되어 있으면 return
    for (const TPair<AActor* const, TArray<FScript*>>& Script : ScriptsByOwner)
    {
        if (LuaLocalValue.MyActor == Script.first)
        {
            for (FScript* ScriptData : Script.second)
            {
                if (ScriptData && ScriptData->ScriptName == ScriptName)
                {
                    UE_LOG("[Script Manager] Actor alreay have %s", ScriptName.c_str());
                    return;
                }
            }
        }
    }

    try
    {
        FScript* Script = GetOrCreate(ScriptName);
        RegisterLocalValueToLua(Script->Env, LuaLocalValue);
        ScriptsByOwner[LuaLocalValue.MyActor].push_back(Script);
        LinkOnOverlapWithShapeComponent(
            LuaLocalValue.MyActor,
            Script->LuaTemplateFunctions.OnOverlap
        );
        LinkRestartToDeligate(Script->LuaTemplateFunctions);
    }
    catch (std::exception& e)
    {
        FString ErrorMessage = FString("[Script Manager] ") + ScriptName + " creation failed : " + e.what();
        UE_LOG(ErrorMessage.c_str());
        // 예외를 다시 던져서 호출자가 처리할 수 있도록 함
        throw;
    }
}

void UScriptManager::PrintDebugLog()
{
    // 이미 같은 스크립트가 부착되어 있으면 return
    for (const TPair<AActor* const, TArray<FScript*>>& Script : ScriptsByOwner)
    {
        for (FScript* ScriptData : Script.second)
        {
            UE_LOG(
                "[Script Manager] Debug Log - Owner %s, Script %s",
                Script.first->GetName().ToString(),
                ScriptData->ScriptName.c_str()
            );
        }
    }
}

void UScriptManager::RemoveSelfOnDelegate(
    AActor* InActor,
    FLuaTemplateFunctions& LuaTemplateFunctions
)
{
    for (UActorComponent* Component : InActor->GetOwnedComponents())
    {
        UShapeComponent* ShapeComponent = Cast<UShapeComponent>(Component);
        if (!ShapeComponent) continue;

        ShapeComponent->OnComponentBeginOverlap.RemoveDynamic(
            LuaTemplateFunctions.OnOverlapDelegateHandle
        );
    }

    UWorld* PIEWorld = GEngine.GetPIEWorld();
    if (!PIEWorld) return;

    AGameModeBase* GameMode = PIEWorld->GetGameMode();
    if (!GameMode) return;
    
    ARunnerGameMode* RunnerGameMode = Cast<ARunnerGameMode>(GEngine.GetPIEWorld()->GetGameMode());
    if (!RunnerGameMode) return;

    RunnerGameMode->GetOnGameRestarted().RemoveDynamic(
        LuaTemplateFunctions.OnRestartDelegateHandle
    );
}

void UScriptManager::DetachScriptFrom(AActor* InActor, const FString& ScriptName)
{
    // Actor에 부착된 Script를 찾는다.
    for (TPair<AActor* const, TArray<FScript*>>& Script : ScriptsByOwner)
    {
        if (InActor == Script.first)
        {
            for (auto Iter = Script.second.begin(); Iter != Script.second.end(); Iter++)
            {
                if (*Iter && (*Iter)->ScriptName == ScriptName)
                {
                    FScript* Tmp = *Iter;
                    Script.second.erase(Iter);

                    RemoveSelfOnDelegate(InActor, Tmp->LuaTemplateFunctions);

                    delete Tmp;
                    break;
                }
            }
        }
    }
}

void UScriptManager::DetachAllScriptFrom(AActor* InActor)
{
    for (TPair<AActor* const, TArray<FScript*>>& Script : ScriptsByOwner)
    {
        if (InActor == Script.first)
        {
            while (!Script.second.empty())
            {
                FScript* Tmp = Script.second.back();
                Script.second.RemoveAt(Script.second.size() - 1);

                RemoveSelfOnDelegate(InActor, Tmp->LuaTemplateFunctions);

                delete Tmp;
            }
        }
    }
}

void UScriptManager::ModifyGameModeValueInScript(AActor* InActor, class AGameModeBase* InNewGameMode)
{
    assert(InActor);
    assert(InNewGameMode);

    for (TPair<AActor* const, TArray<FScript*>>& Script : ScriptsByOwner)
    {
        if (InActor == Script.first)
        {
            for (FScript* ScriptData : Script.second)
            {
                if (ScriptData)
                {
                    // 기존 GameMode 값을 새로운 GameMode로 변경
                    sol::environment& Env = ScriptData->Env;
                    assert(Env);
                    assert(Env.valid());
                    if (ARunnerGameMode* RunnerGameMode = Cast<ARunnerGameMode>(InNewGameMode))
                    {
                        Env["GameMode"] = RunnerGameMode;
                    }
                    UE_LOG(
                        "[Script Manager] Modified GameMode in script %s for actor %s",
                        ScriptData->ScriptName.c_str(),
                        InActor->GetName().ToString().c_str()
                    );
                    return;
                }
            }
        }
    }
}

TMap<AActor*, TArray<FScript*>>& UScriptManager::GetScriptsByOwner()
{
    return ScriptsByOwner;
}

TArray<FScript*> UScriptManager::GetScriptsOfActor(AActor* InActor)
{
    auto Found = ScriptsByOwner.find(InActor);
    // Cache에 Actor가 등록이 되어있지 않으면 빈 배열을 반환한다.
    if (Found == ScriptsByOwner.end())
        return {};

    return Found->second;
}

void UScriptManager::CheckAndHotReloadLuaScript()
{
    for (auto& ScriptPair : ScriptsByOwner)
    {
        for (FScript* Script : ScriptPair.second)
        {
            // 파일이 존재하지 않을 경우
            if (!fs::exists(Script->ScriptName))
            {
                continue;
            }

            auto CurrentWriteTime = std::filesystem::last_write_time(Script->ScriptName);
            if (CurrentWriteTime > Script->LastModifiedTime)
            {
                FString HotReloadMessage =
                    FString("[Script Manager] Lua Script: ") +
                    Script->ScriptName +
                    " hot reload.";
                UE_LOG(HotReloadMessage.c_str());

                // 기존 상태 백업
                sol::environment OldEnv = Script->Env;
                sol::table OldTable = Script->Table;
                FLuaTemplateFunctions OldFuncs = Script->LuaTemplateFunctions;

                try
                {
                    fs::path Path(SCRIPT_FILE_PATH + Script->ScriptName);
                    SetLuaScriptField(
                        Path,
                        Script->Env,
                        Script->Table,
                        Script->LuaTemplateFunctions
                    );
                    RegisterLocalValueToLua(Script->Env, Script->LuaLocalValue);
                }
                catch (std::exception& e)
                {
                    FString ErrorMessage =
                        FString("[Script Manager] Lua Script: ") +
                        Script->ScriptName +
                        " hot reload failed and fall backed.";

                    Script->Env = OldEnv;
                    Script->Table = OldTable;
                    Script->LuaTemplateFunctions = OldFuncs;
                }

                Script->LastModifiedTime = CurrentWriteTime;
            }
        }
    }
}


/* public static */
UScriptManager& UScriptManager::GetInstance()
{
    static UScriptManager Instance;
    return Instance;
}

/* Private */
void UScriptManager::Initialize()
{
    /*
     * Lua Script에서 별도로 Library를 include하지 않아도 되도록
     * 전역으로 Include하는 설정
     */
    Lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::coroutine, sol::lib::os, sol::lib::package);

    // Lua require()가 Scripts 디렉토리에서 모듈을 찾을 수 있도록 package.path 설정
    std::string packagePath = Lua["package"]["path"];
    packagePath = "Scripts/?.lua;Scripts/?/init.lua;" + packagePath;
    Lua["package"]["path"] = packagePath;

    RegisterUserTypeToLua();
    RegisterGlobalValueToLua();
    RegisterGlobalFuncToLua();
}

void UScriptManager::Shutdown()
{
    lua_close(Lua);
}

/* 전역으로 lua에 타입을 등록 */
void UScriptManager::RegisterUserTypeToLua()
{
    // FName 등록
    sol::usertype<FName> NameType = Lua.new_usertype<FName>(
        "FName",
        sol::no_constructor);
    NameType["ToString"] = &FName::ToString;

    // UActorComponent 등록
    Lua.new_usertype<UActorComponent>("UActorComponent",
        sol::constructors<UActorComponent()>(),
        "SetActive", &UActorComponent::SetActive,
        "IsActive", &UActorComponent::IsActive,
        "SetTickEnabled", &UActorComponent::SetTickEnabled,
        "IsTickEnabled", &UActorComponent::IsTickEnabled,
        "SetHiddenInGame", &UActorComponent::SetHiddenInGame,
        "GetHiddenInGame", &UActorComponent::GetHiddenInGame,
        "GetOwner", &UActorComponent::GetOwner,
        "IsRegistered", &UActorComponent::IsRegistered
    );

    // USceneComponent 등록
    sol::usertype<USceneComponent> SceneComponentType = Lua.new_usertype<USceneComponent>(
        "USceneComponent",
        sol::constructors<USceneComponent()>(),
        "SetRelativeLocation", &USceneComponent::SetRelativeLocation,
        "GetRelativeLocation", &USceneComponent::GetRelativeLocation);
    SceneComponentType["GetSceneId"] = &USceneComponent::GetSceneId;

    // UShapeComponent 등록 (충돌 컴포넌트)
    Lua.new_usertype<UShapeComponent>("UShapeComponent",
        sol::base_classes, sol::bases<USceneComponent, UActorComponent>()//,
        // OnOverlap 델리게이트 바인딩
        // "BindOnOverlap", [](UShapeComponent* self, sol::function callback) {
        //     if (!self) return;
        //     self->OnComponentBeginOverlap.Add(callback);
        // }
    );

    // UBoxComponent 등록 (박스 충돌 컴포넌트)
    Lua.new_usertype<UBoxComponent>("UBoxComponent",
        sol::base_classes, sol::bases<UShapeComponent, USceneComponent, UActorComponent>(),
        "SetBoxExtent", &UBoxComponent::SetBoxExtent,
        "GetBoxExtent", &UBoxComponent::GetBoxExtent
    );

    // UCameraComponent 등록 (카메라 컴포넌트)
    Lua.new_usertype<UCameraComponent>("UCameraComponent",
        sol::base_classes, sol::bases<USceneComponent, UActorComponent>(),
        "SetFOV", &UCameraComponent::SetFOV,
        "GetFOV", &UCameraComponent::GetFOV,
        "SetAspectRatio", &UCameraComponent::SetAspectRatio,
        "GetAspectRatio", &UCameraComponent::GetAspectRatio,
        "GetForward", &UCameraComponent::GetForward,
        "GetRight", &UCameraComponent::GetRight,
        "GetUp", &UCameraComponent::GetUp
    );

    // USpringArmComponent 등록 (스프링암 컴포넌트)
    Lua.new_usertype<USpringArmComponent>("USpringArmComponent",
        sol::base_classes, sol::bases<USceneComponent, UActorComponent>(),
        "SetTargetArmLength", &USpringArmComponent::SetTargetArmLength,
        "GetTargetArmLength", &USpringArmComponent::GetTargetArmLength,
        "SetSocketOffset", &USpringArmComponent::SetSocketOffset,
        "GetSocketOffset", &USpringArmComponent::GetSocketOffset,
        "SetTargetOffset", &USpringArmComponent::SetTargetOffset,
        "GetTargetOffset", &USpringArmComponent::GetTargetOffset,
        "SetEnableCameraLag", &USpringArmComponent::SetEnableCameraLag,
        "GetEnableCameraLag", &USpringArmComponent::GetEnableCameraLag,
        "SetCameraLagSpeed", &USpringArmComponent::SetCameraLagSpeed,
        "GetCameraLagSpeed", &USpringArmComponent::GetCameraLagSpeed,
        "SetDoCollisionTest", &USpringArmComponent::SetDoCollisionTest,
        "GetDoCollisionTest", &USpringArmComponent::GetDoCollisionTest
    );

    // UCapsuleComponent 등록 (캡슐 충돌 컴포넌트)
    Lua.new_usertype<UCapsuleComponent>("UCapsuleComponent",
        sol::base_classes, sol::bases<UShapeComponent, USceneComponent, UActorComponent>(),
        "SetCapsuleSize", &UCapsuleComponent::SetCapsuleSize,
        "GetCapsuleRadius", &UCapsuleComponent::GetCapsuleRadius,
        "GetCapsuleHalfHeight", &UCapsuleComponent::GetCapsuleHalfHeight,
        "GetScaledCapsuleRadius", &UCapsuleComponent::GetScaledCapsuleRadius,
        "GetScaledCapsuleHalfHeight", &UCapsuleComponent::GetScaledCapsuleHalfHeight,
        "GetCapsuleCenter", &UCapsuleComponent::GetCapsuleCenter,
        "CapsuleRadius", &UCapsuleComponent::CapsuleRadius,
        "CapsuleHalfHeight", &UCapsuleComponent::CapsuleHalfHeight
    );

    // UTextRenderComponent 등록
    Lua.new_usertype<UTextRenderComponent>("UTextRenderComponent",
        sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent>(),
        "GetStaticMesh", &UTextRenderComponent::GetStaticMesh,
        "GetMaterial", &UTextRenderComponent::GetMaterial,
        "SetMaterial", &UTextRenderComponent::SetMaterial,
        "CreateVerticesForString", &UTextRenderComponent::CreateVerticesForString
    );

    // UBillboardComponent 등록
    Lua.new_usertype<UBillboardComponent>("UBillboardComponent",
        sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent>(),
        "GetStaticMesh", &UBillboardComponent::GetStaticMesh,
        "GetMaterial", &UBillboardComponent::GetMaterial,
        "SetMaterial", &UBillboardComponent::SetMaterial,
        "SetTextureName", &UBillboardComponent::SetTextureName,
        "GetFilePath", &UBillboardComponent::GetFilePath
    );

    // UParticleComponent 등록
    Lua.new_usertype<UParticleComponent>("UParticleComponent",
        sol::base_classes, sol::bases<UBillboardComponent, UPrimitiveComponent, USceneComponent, UActorComponent>(),
        "SetSpriteSheet", &UParticleComponent::SetSpriteSheet,
        "SetFrameRate", &UParticleComponent::SetFrameRate,
        "SetLooping", &UParticleComponent::SetLooping,
        "Play", &UParticleComponent::Play,
        "Stop", &UParticleComponent::Stop,
        "Reset", &UParticleComponent::Reset
    );

    // FVector 타입을 Lua에 등록
    Lua.new_usertype<FVector>("FVector",
        sol::call_constructor, sol::factories(
            []() { return FVector(); },
            [](float x, float y, float z) { return FVector(x, y, z); }
        ),
        "X", &FVector::X,
        "Y", &FVector::Y,
        "Z", &FVector::Z,
        // 연산자 오버로딩 (메타메서드)
        sol::meta_function::addition, [](const FVector& a, const FVector& b) { return a + b; },
        sol::meta_function::subtraction, [](const FVector& a, const FVector& b) { return a - b; },
        sol::meta_function::multiplication, [](const FVector& v, float scalar) { return v * scalar; },
        sol::meta_function::division, [](const FVector& v, float scalar) { return v / scalar; },
        sol::meta_function::unary_minus, [](const FVector& v) { return -v; },
        "Add", [](const FVector& a, const FVector& b) { return a + b; },
        "Sub", [](const FVector& a, const FVector& b) { return a - b; },
        "Mul", [](const FVector& v, float scalar) { return v * scalar; },
        "New", [](float x, float y, float z) { return FVector(x, y, z); },
        // 인스턴스 메서드
        "Size", &FVector::Size,
        "SizeSquared", &FVector::SizeSquared,
        "GetNormalized", &FVector::GetNormalized,
        "IsZero", &FVector::IsZero
    );
    // FVector 정적 함수 등록
    Lua["FVector"]["Cross"] = &FVector::Cross;
    Lua["FVector"]["Dot"] = &FVector::Dot;
    Lua["FVector"]["Lerp"] = &FVector::Lerp;

    // FVector4 타입을 Lua에 등록
    Lua.new_usertype<FVector4>("FVector4",
        sol::call_constructor, sol::factories(
            []() { return FVector4(); },
            [](float x, float y, float z, float w) { return FVector4(x, y, z, w); }
        ),
        "X", &FVector4::X,
        "Y", &FVector4::Y,
        "Z", &FVector4::Z,
        "W", &FVector4::W
    );

    // FQuat 타입을 Lua에 등록
    Lua.new_usertype<FQuat>("FQuat",
        sol::call_constructor, sol::factories(
            []() { return FQuat(); },
            [](float x, float y, float z, float w) { return FQuat(x, y, z, w); }
        ),
        // 멤버 변수 접근
        "X", &FQuat::X,
        "Y", &FQuat::Y,
        "Z", &FQuat::Z,
        "W", &FQuat::W,
        // 곱셈 연산자 (회전 결합)
        sol::meta_function::multiplication, [](const FQuat& a, const FQuat& b) { return a * b; }
    );
    // Static 함수는 별도로 등록
    Lua["FQuat"]["MakeFromEuler"] = [](float pitch, float yaw, float roll) {
        return FQuat::MakeFromEulerZYX(FVector(pitch, yaw, roll));
        };
    Lua["FQuat"]["Slerp"] = &FQuat::Slerp;
    Lua["FQuat"]["Mul"] = [](const FQuat& a, const FQuat& b) { return a * b; };

    // GetForwardVector: Quaternion의 Forward 방향 벡터
    Lua["FQuat"]["GetForwardVector"] = [](const FQuat& q) {
        return q.GetForwardVector();
        };

    // GetRightVector: Quaternion의 Right 방향 벡터
    Lua["FQuat"]["GetRightVector"] = [](const FQuat& q) {
        return q.GetRightVector();
        };

    // GetUpVector: Quaternion의 Up 방향 벡터
    Lua["FQuat"]["GetUpVector"] = [](const FQuat& q) {
        return q.GetUpVector();
        };

    // RotateVector: Quaternion으로 벡터 회전
    Lua["FQuat"]["RotateVector"] = [](const FQuat& q, const FVector& v) {
        return q.RotateVector(v);
        };

    // LookRotation: Forward와 Up 벡터로 회전 생성
    Lua["FQuat"]["LookRotation"] = [](const FVector& forward, const FVector& up) {
        return FQuat::LookRotation(forward, up);
        };

    // FromToRotation: 두 벡터 사이의 회전 Quaternion 계산
    Lua["FQuat"]["FromToRotation"] = [](const FVector& from, const FVector& to) {
        FVector fromNorm = from.GetNormalized();
        FVector toNorm = to.GetNormalized();
        FVector axis = FVector::Cross(fromNorm, toNorm);
        float dot = FVector::Dot(fromNorm, toNorm);

        // 두 벡터가 거의 평행한 경우 처리
        if (axis.SizeSquared() < 0.0001f)
        {
            if (dot > 0.0f)
                return FQuat::Identity();  // 같은 방향
            else
                // 반대 방향: 임의의 수직 축으로 180도 회전
                return FQuat(1, 0, 0, 0);
        }

        float w = std::sqrt(fromNorm.SizeSquared() * toNorm.SizeSquared()) + dot;
        return FQuat(axis.X, axis.Y, axis.Z, w).GetNormalized();
        };

    // FLinearColor 타입을 Lua에 등록
    Lua.new_usertype<FLinearColor>("FLinearColor",
        sol::call_constructor, sol::factories(
            []() { return FLinearColor(); },
            [](float r, float g, float b) { return FLinearColor(r, g, b, 1.0f); },
            [](float r, float g, float b, float a) { return FLinearColor(r, g, b, a); }
        ),
        "R", &FLinearColor::R,
        "G", &FLinearColor::G,
        "B", &FLinearColor::B,
        "A", &FLinearColor::A
    );

    // FMatrix 타입을 Lua에 등록 (카메라 행렬용)
    Lua.new_usertype<FMatrix>("FMatrix",
        sol::no_constructor
        // Matrix는 복잡하므로 직접 조작은 제한하고, 함수 결과로만 사용
    );

    // Actor 래퍼 클래스 등록
    Lua.new_usertype<AActor>("AActor",
        "GetLocation", &AActor::GetActorLocation,
        "SetLocation", &AActor::SetActorLocation,
        "GetRotation", &AActor::GetActorRotation,
        "SetRotation", sol::overload(
            static_cast<void(AActor::*)(const FVector&)>(&AActor::SetActorRotation),
            static_cast<void(AActor::*)(const FQuat&)>(&AActor::SetActorRotation)
        ),
        "GetScale", &AActor::GetActorScale,
        "SetScale", &AActor::SetActorScale,
        "GetTransform", &AActor::GetActorTransform,
        "SetTransform", &AActor::SetActorTransform,
        "AddWorldLocation", &AActor::AddActorWorldLocation,
        "AddWorldRotation", sol::overload(
            static_cast<void(AActor::*)(const FQuat&)>(&AActor::AddActorWorldRotation)
        ),
        "GetRootComponent", &AActor::GetRootComponent,
        "GetName", &AActor::GetName,
        "SetActorHiddenInGame", &AActor::SetActorHiddenInGame,
        "DestroyAllComponents", &AActor::DestroyAllComponents,
        "Destroy", &AActor::Destroy,
        "AddOwnedComponent", &AActor::AddOwnedComponent,
        // CapsuleComponent 생성 및 부착 헬퍼
        "CreateCapsuleComponent", [](AActor* self, sol::optional<FString> name) -> UCapsuleComponent* {
            if (!self) return nullptr;
            FName componentName = name.value_or("CapsuleComponent");
            UCapsuleComponent* Capsule = self->CreateDefaultSubobject<UCapsuleComponent>(componentName);
            if (Capsule && self->GetRootComponent()) {
                // RootComponent에 부착하여 Transform 동기화
                Capsule->SetupAttachment(self->GetRootComponent(), EAttachmentRule::KeepRelative);
            }
            return Capsule;
        },
        "CreateTextRenderComponent", [](AActor* self, sol::optional<FString> name) -> UTextRenderComponent* {
            if (!self) return nullptr;
            FName componentName = name.value_or("TextRenderComponent");
            UTextRenderComponent* TextRender = self->CreateDefaultSubobject<UTextRenderComponent>(componentName);
            if (TextRender) {
                // Owner 설정
                TextRender->SetOwner(self);

                // Actor에 컴포넌트 등록
                self->AddOwnedComponent(TextRender);

                // RootComponent에 부착 (옵션)
                if (self->GetRootComponent()) {
                    TextRender->SetupAttachment(self->GetRootComponent(), EAttachmentRule::KeepRelative);
                }
            }
            return TextRender;
        },
        "CreateBillboardComponent", [](AActor* self, sol::optional<FString> name) -> UBillboardComponent* {
            if (!self) return nullptr;
            FName componentName = name.value_or("BillboardComponent");
            UBillboardComponent* Billboard = self->CreateDefaultSubobject<UBillboardComponent>(componentName);
            if (Billboard) {
                // Owner 설정
                Billboard->SetOwner(self);
                // Actor에 컴포넌트 등록
                self->AddOwnedComponent(Billboard);
                // RootComponent에 부착 (옵션)
                if (self->GetRootComponent()) {
                    Billboard->SetupAttachment(self->GetRootComponent(), EAttachmentRule::KeepRelative);
                }

                Billboard->SetHiddenInGame(false);
            }

            return Billboard;
        },
        "AttachScript", [](AActor* self, const FString& scriptName) -> void {
            if (!self) return;
            FLuaLocalValue LuaLocalValue;
            LuaLocalValue.MyActor = self;
            LuaLocalValue.GameMode = self->World ? self->World->GetGameMode() : nullptr;
            UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, scriptName);
        }
    );

    // APawn 클래스 등록 (AActor 상속)
    Lua.new_usertype<APawn>("APawn",
        sol::base_classes, sol::bases<AActor>(),
        "AddMovementInput", &APawn::AddMovementInput,
        "ConsumeMovementInput", &APawn::ConsumeMovementInput,
        "GetInputComponent", &APawn::GetInputComponent
    );

    // UInputComponent 클래스 등록
    Lua.new_usertype<UInputComponent>("UInputComponent",
        sol::no_constructor,
        "BindAxis", sol::overload(
            [](UInputComponent* self, const FString& AxisName, int32 KeyCode, float Scale, sol::function callback) {
                self->BindAxis(AxisName, KeyCode, Scale, [callback](float value) {
                    callback(value);
                    });
            }
        ),
        "BindAction", sol::overload(
            // Pressed와 Released를 모두 받는 버전
            [](UInputComponent* self, const FString& ActionName, int32 KeyCode,
                sol::function pressedCallback, sol::function releasedCallback) {
                    std::function<void()> pressed;
                    std::function<void()> released;

                    if (pressedCallback.valid()) {
                        pressed = [pressedCallback]() { pressedCallback(); };
                    }

                    if (releasedCallback.valid()) {
                        released = [releasedCallback]() { releasedCallback(); };
                    }

                    self->BindAction(ActionName, KeyCode, pressed, released);
            },
            // Pressed만 받는 버전 (간단한 경우)
            [](UInputComponent* self, const FString& ActionName, int32 KeyCode, sol::function callback) {
                self->BindAction(ActionName, KeyCode, [callback]() { callback(); }, nullptr);
            }
        )
    );

    // ACharacter 클래스 등록 (APawn 상속)
    Lua.new_usertype<ACharacter>("ACharacter",
        sol::base_classes, sol::bases<APawn, AActor>(),
        "Jump", &ACharacter::Jump,
        "StopJumping", &ACharacter::StopJumping,
        "CanJump", &ACharacter::CanJump,
        "GetVelocity", &ACharacter::GetVelocity,
        "IsGrounded", &ACharacter::IsGrounded,
        "IsFalling", &ACharacter::IsFalling,
        "MoveForward", &ACharacter::MoveForward,
        "MoveRight", &ACharacter::MoveRight,
        "Turn", &ACharacter::Turn,
        "LookUp", &ACharacter::LookUp,
        "GetCharacterMovement", &ACharacter::GetCharacterMovement,
        "GetStaticMesh", &ACharacter::GetStaticMesh
    );

    // UCharacterMovementComponent 클래스 등록
    Lua.new_usertype<UCharacterMovementComponent>("UCharacterMovementComponent",
        sol::no_constructor,
        "MaxWalkSpeed", &UCharacterMovementComponent::MaxWalkSpeed,
        "JumpZVelocity", &UCharacterMovementComponent::JumpZVelocity,
        "GravityScale", &UCharacterMovementComponent::GravityScale,
        "AirControl", &UCharacterMovementComponent::AirControl,
        "SetGravityDirection", &UCharacterMovementComponent::SetGravityDirection,
        "GetGravityDirection", &UCharacterMovementComponent::GetGravityDirection,
        "SetOnWallCollisionCallback", &UCharacterMovementComponent::SetOnWallCollisionCallback,
        "SetIsRotating", &UCharacterMovementComponent::SetIsRotating,
        "IsRotating", &UCharacterMovementComponent::IsRotating
    );

    // ARunnerCharacter 클래스 등록 (ACharacter 상속)
    Lua.new_usertype<ARunnerCharacter>("ARunnerCharacter",
        sol::base_classes, sol::bases<ACharacter, APawn, AActor>(),
        "GetForwardDirection", &ARunnerCharacter::GetForwardDirection,
        "GetRightDirection", &ARunnerCharacter::GetRightDirection,
        "GetUpDirection", &ARunnerCharacter::GetUpDirection,
        "SetGravityDirection", &ARunnerCharacter::SetGravityDirection,
        "GetGravityDirection", &ARunnerCharacter::GetGravityDirection,
        "GetSpringArm", &ARunnerCharacter::GetSpringArm,
        "GetParticleComponent", &ARunnerCharacter::GetParticleComponent
    );

    // ASpriteActor 클래스 등록
    Lua.new_usertype<ASpriteActor>("ASpriteActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetParticleComponent", &ASpriteActor::GetParticleComponent,
        "SetSpriteSheet", &ASpriteActor::SetSpriteSheet,
        "SetAutoDestroy", &ASpriteActor::SetAutoDestroy
    );

    // AGameModeBase 클래스 등록
    Lua.new_usertype<AGameModeBase>("AGameModeBase",
        sol::no_constructor,
        "StartGame", &AGameModeBase::StartGame,
        "PauseGame", &AGameModeBase::PauseGame,
        "EndGame", &AGameModeBase::EndGame,
        "IsGameStarted", [](AGameModeBase* gm) { return gm->GetGameState() && gm->GetGameState()->GetGameState() == EGameState::Playing; }
    );

    // FTransform 타입 등록
    Lua.new_usertype<FTransform>("FTransform",
        sol::call_constructor, sol::factories(
            []() { return FTransform(); },
            [](const FVector& t, const FQuat& r, const FVector& s) { return FTransform(t, r, s); }
        ),
        "Translation", &FTransform::Translation,
        "Rotation", &FTransform::Rotation,
        "Scale3D", &FTransform::Scale3D
    );

    // UStaticMesh 클래스 등록
    Lua.new_usertype<UStaticMesh>("UStaticMesh",
        sol::base_classes, sol::bases<UResourceBase>(),
        "GetCacheFilePath", &UStaticMesh::GetCacheFilePath
    );

    // UStaticMeshComponent 클래스 등록
    Lua.new_usertype<UStaticMeshComponent>("UStaticMeshComponent",
        sol::base_classes, sol::bases<USceneComponent, UActorComponent>(),
        "SetStaticMesh", &UStaticMeshComponent::SetStaticMesh,
        "GetStaticMesh", &UStaticMeshComponent::GetStaticMesh
    );

    // AStaticMeshActor 클래스 등록
    Lua.new_usertype<AStaticMeshActor>("AStaticMeshActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetStaticMeshComponent", &AStaticMeshActor::GetStaticMeshComponent,
        "SetStaticMeshComponent", &AStaticMeshActor::SetStaticMeshComponent
    );

    // UProjectileMovementComponent 클래스 등록
    Lua.new_usertype<UProjectileMovementComponent>("UProjectileMovementComponent",
        sol::no_constructor,
        "FireInDirection", &UProjectileMovementComponent::FireInDirection,
        "SetInitialSpeed", &UProjectileMovementComponent::SetInitialSpeed,
        "GetInitialSpeed", &UProjectileMovementComponent::GetInitialSpeed,
        "SetGravity", &UProjectileMovementComponent::SetGravity,
        "GetGravity", &UProjectileMovementComponent::GetGravity,
        "SetGravityDirection", &UProjectileMovementComponent::SetGravityDirection,
        "GetGravityDirection", &UProjectileMovementComponent::GetGravityDirection,
        "SetMaxSpeed", &UProjectileMovementComponent::SetMaxSpeed,
        "GetMaxSpeed", &UProjectileMovementComponent::GetMaxSpeed,
        "SetRotationFollowsVelocity", &UProjectileMovementComponent::SetRotationFollowsVelocity,
        "GetRotationFollowsVelocity", &UProjectileMovementComponent::GetRotationFollowsVelocity,
        "SetProjectileLifespan", &UProjectileMovementComponent::SetProjectileLifespan,
        "GetProjectileLifespan", &UProjectileMovementComponent::GetProjectileLifespan
    );

    // AProjectileActor 클래스 등록
    Lua.new_usertype<AProjectileActor>("AProjectileActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetMeshComponent", &AProjectileActor::GetMeshComponent,
        "GetProjectileMovement", &AProjectileActor::GetProjectileMovement,
        "GetCollisionComponent", &AProjectileActor::GetCollisionComponent,
        "FireInDirection", &AProjectileActor::FireInDirection,
        "SetInitialSpeed", &AProjectileActor::SetInitialSpeed,
        "SetGravityScale", &AProjectileActor::SetGravityScale,
        "SetLifespan", &AProjectileActor::SetLifespan
    );

    // UDecalComponent 클래스 등록
    Lua.new_usertype<UDecalComponent>("UDecalComponent",
        sol::base_classes, sol::bases<USceneComponent, UActorComponent>()
    );

    // ADecalActor 클래스 등록
    Lua.new_usertype<ADecalActor>("ADecalActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetDecalComponent", &ADecalActor::GetDecalComponent
    );

    // UHeightFogComponent 클래스 등록
    Lua.new_usertype<UHeightFogComponent>("UHeightFogComponent",
        sol::base_classes, sol::bases<USceneComponent, UActorComponent>(),
        "GetFogDensity", &UHeightFogComponent::GetFogDensity,
        "SetFogDensity", &UHeightFogComponent::SetFogDensity,
        "GetFogHeightFalloff", &UHeightFogComponent::GetFogHeightFalloff,
        "SetFogHeightFalloff", &UHeightFogComponent::SetFogHeightFalloff,
        "GetStartDistance", &UHeightFogComponent::GetStartDistance,
        "SetStartDistance", &UHeightFogComponent::SetStartDistance,
        "GetFogCutoffDistance", &UHeightFogComponent::GetFogCutoffDistance,
        "SetFogCutoffDistance", &UHeightFogComponent::SetFogCutoffDistance,
        "GetFogMaxOpacity", &UHeightFogComponent::GetFogMaxOpacity,
        "SetFogMaxOpacity", &UHeightFogComponent::SetFogMaxOpacity,
        "GetFogInscatteringColor", &UHeightFogComponent::GetFogInscatteringColor,
        "SetFogInscatteringColor", &UHeightFogComponent::SetFogInscatteringColor,
        "GetFogHeight", &UHeightFogComponent::GetFogHeight
    );

    // AHeightFogActor 클래스 등록
    Lua.new_usertype<AHeightFogActor>("AHeightFogActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetHeightFogComponent", [](AHeightFogActor* Actor)-> UHeightFogComponent*
        {
            return Cast<UHeightFogComponent>(Actor->GetRootComponent());
        }
    );

    // ACameraActor 클래스 등록
    Lua.new_usertype<ACameraActor>("ACameraActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetCameraComponent", &ACameraActor::GetCameraComponent,
        "GetForward", &ACameraActor::GetForward,
        "GetRight", &ACameraActor::GetRight,
        "GetUp", &ACameraActor::GetUp,
        "GetRotation", &ACameraActor::GetActorRotation,
        "SetRotation", sol::overload(
            static_cast<void(ACameraActor::*)(const FVector&)>(&ACameraActor::SetActorRotation),
            static_cast<void(ACameraActor::*)(const FQuat&)>(&ACameraActor::SetActorRotation)
        ),
        "GetViewMatrix", &ACameraActor::GetViewMatrix,
        "GetProjectionMatrix", sol::overload(
            static_cast<FMatrix(ACameraActor::*)() const>(&ACameraActor::GetProjectionMatrix)
        )
    );

    // AGravityWall 클래스 등록 (AActor 상속)
    Lua.new_usertype<AGravityWall>("AGravityWall",
        sol::base_classes, sol::bases<AActor>(),
        "GetStaticMeshComponent", &AGravityWall::GetStaticMeshComponent,
        "GetBoxComponent", &AGravityWall::GetBoxComponent,
        "SetMeshPath", &AGravityWall::SetMeshPath,
        "GetWallNormal", &AGravityWall::GetWallNormal,
        "SetWallNormal", &AGravityWall::SetWallNormal
    );

    // FRay 구조체 등록 (마우스 방향 계산용)
    Lua.new_usertype<FRay>("FRay",
        sol::call_constructor, sol::factories(
            []() { return FRay(); }
        ),
        "Origin", &FRay::Origin,
        "Direction", &FRay::Direction
    );

    Lua.new_usertype<ACoinActor>("ACoinActor",
        sol::base_classes, sol::bases<AActor>(),
        "GetStaticMeshComponent", &ACoinActor::GetStaticMeshComponent,
        "GetBoxComponent", &ACoinActor::GetBoxComponent,
        "SetMeshPath", &ACoinActor::SetMeshPath
    );

    // UWorld 클래스 등록
    Lua.new_usertype<UWorld>("UWorld",
        sol::no_constructor,
        "SpawnActor", sol::overload(
            // StaticMeshActor 생성
            [](UWorld* World, const FTransform& Transform) -> AStaticMeshActor* {
                return World->SpawnActor<AStaticMeshActor>(Transform);
            },
            // 타입 문자열로 Actor 생성 (Lua에게 정확한 타입 반환)
            [](UWorld* World, const FTransform& Transform, const FString& ActorType, sol::this_state s) -> sol::object {
                sol::state_view lua(s);

                if (ActorType == "AGravityWall")
                {
                    AGravityWall* wall = World->SpawnActor<AGravityWall>(Transform);
                    return sol::make_object(lua, wall);
                }
                else if (ActorType == "ACoinActor")
                {
                    ACoinActor* coin = World->SpawnActor<ACoinActor>(Transform);
                    if (coin && World->bPie)
                    {
                        coin->BeginPlay();
                    }
                    return sol::make_object(lua, coin);
                }
                else if (ActorType == "AProjectileActor")
                {
                    AProjectileActor* projectile = World->SpawnActor<AProjectileActor>(Transform);
                    if (projectile && World->bPie)
                    {
                        projectile->BeginPlay();
                    }
                    return sol::make_object(lua, projectile);
                }
                else if (ActorType == "ADecalActor")
                {
                    ADecalActor* decal = World->SpawnActor<ADecalActor>(Transform);
                    return sol::make_object(lua, decal);
                }
                else if (ActorType == "AHeightFogActor")
                {
                    AHeightFogActor* fog = World->SpawnActor<AHeightFogActor>(Transform);
                    return sol::make_object(lua, fog);
                }
                else if (ActorType == "ASpriteActor")
                {
                    ASpriteActor* sprite = World->SpawnActor<ASpriteActor>(Transform);
                    if (sprite && World->bPie)
                    {
                        sprite->BeginPlay();
                    }
                    return sol::make_object(lua, sprite);
                }
                else
                {
                    return sol::nil;
                }
            }
        ),
        "SpawnGravityWall", [](UWorld* World, const FTransform& Transform) -> AGravityWall* {
            return World->SpawnActor<AGravityWall>(Transform);
        },
        "SpawnCoinActor", [](UWorld* World, const FTransform& Transform) -> ACoinActor* {
            return World->SpawnActor<ACoinActor>(Transform);
        },
        "SpawnProjectileActor", [](UWorld* World, const FTransform& Transform) -> AProjectileActor* {
            AProjectileActor* NewProjectile = World->SpawnActor<AProjectileActor>(Transform);
            if (NewProjectile && World->bPie)
            {
                NewProjectile->BeginPlay();
            }
            return NewProjectile;
        },

        "DestroyActor", &UWorld::DestroyActor,
        "GetActors", &UWorld::GetActors,
        "GetCameraActor", &UWorld::GetCameraActor,
        "GetDeltaTimeManager", & UWorld::GetDeltaTimeManager
    );

    // TimeDilationPriority enum 바인딩
    Lua.new_enum<ETimeDilationPriority>("TimeDilationPriority",
        {
            {"Low", ETimeDilationPriority::Low},
            {"Normal", ETimeDilationPriority::Normal},
            {"High", ETimeDilationPriority::High},
            {"Critical", ETimeDilationPriority::Critical}
        });

    // DeltaTimeManager 바인딩
    Lua.new_usertype<UDeltaTimeManager>("DeltaTimeManager",
        sol::no_constructor,

        // 효과 적용
        // 효과 적용 - sol::overload로 기본 인자 처리
        "ApplyHitStop", sol::overload(
            // Priority를 지정하는 오버로드
            [](UDeltaTimeManager* self, float Duration, ETimeDilationPriority Priority) {
                return self->ApplyHitStop(Duration, Priority);
            },
            // Priority를 생략하는 오버로드 (기본값 High 사용)
            [](UDeltaTimeManager* self, float Duration) {
                return self->ApplyHitStop(Duration, ETimeDilationPriority::High);
            }
        ),
        "ApplySlomoEffect", sol::overload(
            // 모든 인자 지정
            [](UDeltaTimeManager* self, float Duration, float TimeDilation, ETimeDilationPriority Priority) {
                return self->ApplySlomoEffect(Duration, TimeDilation, Priority);
            },
            // Priority 생략
            [](UDeltaTimeManager* self, float Duration, float TimeDilation) {
                return self->ApplySlomoEffect(Duration, TimeDilation, ETimeDilationPriority::Normal);
            }
        ),
        "CancelEffect", & UDeltaTimeManager::CancelEffect,
        "CancelAllEffects", & UDeltaTimeManager::CancelAllEffects,

        // 기본 배율
        "SetBaseTimeDilation", & UDeltaTimeManager::SetBaseTimeDilation,
        "GetBaseTimeDilation", & UDeltaTimeManager::GetBaseTimeDilation,
        "GetCurrentTimeDilation", & UDeltaTimeManager::GetCurrentTimeDilation,

        // 상태 쿼리
        "HasActiveEffects", & UDeltaTimeManager::HasActiveEffects,
        "GetActiveEffectCount", & UDeltaTimeManager::GetActiveEffectCount,
        "GetRemainingTime", & UDeltaTimeManager::GetRemainingTime,
        "PrintDebugInfo", & UDeltaTimeManager::PrintDebugInfo
    );

    // UEditorEngine 클래스 등록
    Lua.new_usertype<UEditorEngine>("UEditorEngine",
        sol::no_constructor,
        "GetDefaultWorld", &UEditorEngine::GetDefaultWorld,
        "GetPIEWorld", &UEditorEngine::GetPIEWorld,
        "IsPIEActive", &UEditorEngine::IsPIEActive,
        "StartPIE", &UEditorEngine::StartPIE,
        "EndPIE", &UEditorEngine::EndPIE
    );

    // AGameModeBase 클래스 등록
    Lua.new_usertype<AGameModeBase>("AGameModeBase",
        sol::base_classes, sol::bases<AInfo>(),
        "GetPlayerController", &AGameModeBase::GetPlayerController
    );

    // APlayerController 클래스 등록
    Lua.new_usertype<APlayerController>("APlayerController",
        sol::base_classes, sol::bases<AController>(),
        "GetPlayerCameraManager", &APlayerController::GetPlayerCameraManager,
        "ProcessPlayerInput", &APlayerController::ProcessPlayerInput,
        "IsInputEnabled", &APlayerController::IsInputEnabled,
        "SetInputEnabled", &APlayerController::SetInputEnabled,
        "ShowMouseCursor", &APlayerController::ShowMouseCursor,
        "HideMouseCursor", &APlayerController::HideMouseCursor,
        "IsMouseCursorVisible", &APlayerController::IsMouseCursorVisible,
        "GetMouseSensitivity", &APlayerController::GetMouseSensitivity,
        "SetMouseSensitivity", &APlayerController::SetMouseSensitivity
    );

    // ARunnerGameMode 클래스 등록 (AGameModeBase 상속)
    Lua.new_usertype<ARunnerGameMode>("ARunnerGameMode",
        sol::base_classes, sol::bases<AGameModeBase>(),
        "OnPlayerDeath", &ARunnerGameMode::OnPlayerDeath,
        "OnCoinCollected", &ARunnerGameMode::OnCoinCollected,
        "OnObstacleAvoided", &ARunnerGameMode::OnObstacleAvoided,
        "OnPlayerJump", &ARunnerGameMode::OnPlayerJump,
		"OnDecreasePlayerHealth", &ARunnerGameMode::OnDecreasePlayerHealth,
        "RestartGame", &ARunnerGameMode::RestartGame
        //// 난이도 설정
        //"BaseDifficulty", &ARunnerGameMode::BaseDifficulty,
        //"DifficultyIncreaseRate", &ARunnerGameMode::DifficultyIncreaseRate,
        //// 점수 설정
        //"JumpScore", &ARunnerGameMode::JumpScore,
        //"CoinScore", &ARunnerGameMode::CoinScore,
        //"AvoidScore", &ARunnerGameMode::AvoidScore
    );

    // APlayerCameraManager 클래스 등록
    Lua.new_usertype<APlayerCameraManager>("APlayerCameraManager",
        sol::base_classes, sol::bases<AActor>(),
        "UpdateCamera", &APlayerCameraManager::UpdateCamera,
        "AddCameraModifier", &APlayerCameraManager::AddCameraModifier,
        "RemoveCameraModifier", &APlayerCameraManager::RemoveCameraModifier,
        "SetViewTarget", &APlayerCameraManager::SetViewTarget,
        "GetViewTarget", &APlayerCameraManager::GetViewTarget,
        "GetCameraLocation", &APlayerCameraManager::GetCameraLocation,
        "GetCameraRotation", &APlayerCameraManager::GetCameraRotation,
        "GetCameraFOV", &APlayerCameraManager::GetCameraFOV,
        "StartCameraFade", &APlayerCameraManager::StartCameraFade,
        "StopCameraFade", &APlayerCameraManager::StopCameraFade,
        "AddCameraModifier", &APlayerCameraManager::AddCameraModifier,
        "RemoveCameraModifier", &APlayerCameraManager::RemoveCameraModifier,
        "RemoveDisabledCameraModifiers", &APlayerCameraManager::RemoveDisabledCameraModifiers
    );

    // UCameraModifier 클래스 등록
    Lua.new_usertype<UCameraModifier>("UCameraModifier",
        sol::base_classes, sol::bases<UObject>(),
        "IsDisabled", &UCameraModifier::IsDisabled,
        "EnableModifier", &UCameraModifier::EnableModifier,
        "DisableModifier", &UCameraModifier::DisableModifier,
        "GetAlpha", &UCameraModifier::GetAlpha,
        "SetAlpha", &UCameraModifier::SetAlpha,
        "GetAlphaInTime", &UCameraModifier::GetAlphaInTime,
        "SetAlphaInTime", &UCameraModifier::SetAlphaInTime,
        "GetAlphaOutTime", &UCameraModifier::GetAlphaOutTime,
        "SetAlphaOutTime", &UCameraModifier::SetAlphaOutTime,
        "IsFadingIn", &UCameraModifier::IsFadingIn,
        "SetIsFadingIn", &UCameraModifier::SetIsFadingIn,
        "GetPriority", &UCameraModifier::GetPriority,
        "SetPriority", &UCameraModifier::SetPriority
    );

    // UCameraModifier_CameraShake 클래스 등록
    Lua.new_usertype<UCameraModifier_CameraShake>("UCameraModifier_CameraShake",
        sol::call_constructor, sol::factories(
            []() { return new UCameraModifier_CameraShake(); }
        ),
        sol::base_classes, sol::bases<UCameraModifier, UObject>(),
        "GetRotationAmplitude", &UCameraModifier_CameraShake::GetRotationAmplitude,
        "SetRotationAmplitude", &UCameraModifier_CameraShake::SetRotationAmplitude,
        "GetNewShake", &UCameraModifier_CameraShake::GetNewShake,
        "SetNumSamples", &UCameraModifier_CameraShake::SetNumSamples
    );

    // UCameraModifier_LetterBox 클래스 등록
    Lua.new_usertype<UCameraModifier_LetterBox>("UCameraModifier_LetterBox",
        sol::call_constructor, sol::factories(
            []() { return new UCameraModifier_LetterBox(); }
        ),
        sol::base_classes, sol::bases<UCameraModifier, UObject>(),
        "SetFadeIn", &UCameraModifier_LetterBox::SetFadeIn,
        "SetFadeOut", &UCameraModifier_LetterBox::SetFadeOut,
        "GetSize", &UCameraModifier_LetterBox::GetSize,
        "SetSize", &UCameraModifier_LetterBox::SetSize,
        "GetOpacity", &UCameraModifier_LetterBox::GetOpacity,
        "SetOpacity", &UCameraModifier_LetterBox::SetOpacity
    );

    // UCameraModifier_Vignetting 클래스 등록
    Lua.new_usertype<UCameraModifier_Vignetting>("UCameraModifier_Vignetting",
        sol::call_constructor, sol::factories(
            []() { return new UCameraModifier_Vignetting(); }
        ),
        sol::base_classes, sol::bases<UCameraModifier, UObject>(),
        "GetRadius", &UCameraModifier_Vignetting::GetRadius,
        "SetRadius", &UCameraModifier_Vignetting::SetRadius,
        "GetSoftness", &UCameraModifier_Vignetting::GetSoftness,
        "SetSoftness", &UCameraModifier_Vignetting::SetSoftness,
        "GetVignettingColor", &UCameraModifier_Vignetting::GetVignettingColor,
        "SetVignettingColor", &UCameraModifier_Vignetting::SetVignettingColor
    );

    // ESoundChannelType enum 바인딩
    Lua.new_enum<ESoundChannelType>("SoundChannelType",
        {
            {"Master", ESoundChannelType::Master},
            {"Music", ESoundChannelType::Music},
            {"SFX", ESoundChannelType::SFX}
        });

    // USoundManager 클래스 등록
    Lua.new_usertype<USoundManager>("USoundManager",
        sol::no_constructor,
        // 2D 사운드 재생 - void 반환 (Channel은 내부적으로 관리)
        "PlaySound2D", sol::overload(
            // 모든 인자 지정
            [](USoundManager* self, const FString& SoundPath, bool bLoop, ESoundChannelType ChannelType) {
                self->PlaySound2D(SoundPath, bLoop, ChannelType);
            },
            // ChannelType 생략 (기본값: SFX)
            [](USoundManager* self, const FString& SoundPath, bool bLoop) {
                self->PlaySound2D(SoundPath, bLoop, ESoundChannelType::SFX);
            },
            // bLoop 생략 (기본값: false)
            [](USoundManager* self, const FString& SoundPath) {
                self->PlaySound2D(SoundPath, false, ESoundChannelType::SFX);
            }
        ),
        // 3D 사운드 재생 - void 반환
        "PlaySound3D", sol::overload(
            // 모든 인자 지정
            [](USoundManager* self, const FString& SoundPath, const FVector& Location,
                bool bLoop, float MinDistance, float MaxDistance, ESoundChannelType ChannelType) {
                    self->PlaySound3D(SoundPath, Location, bLoop, MinDistance, MaxDistance, ChannelType);
            },
            // ChannelType 생략
            [](USoundManager* self, const FString& SoundPath, const FVector& Location,
                bool bLoop, float MinDistance, float MaxDistance) {
                    self->PlaySound3D(SoundPath, Location, bLoop, MinDistance, MaxDistance, ESoundChannelType::SFX);
            },
            // MinDistance, MaxDistance 생략
            [](USoundManager* self, const FString& SoundPath, const FVector& Location, bool bLoop) {
                self->PlaySound3D(SoundPath, Location, bLoop, 100.0f, 10000.0f, ESoundChannelType::SFX);
            },
            // bLoop 생략
            [](USoundManager* self, const FString& SoundPath, const FVector& Location) {
                self->PlaySound3D(SoundPath, Location, false, 100.0f, 10000.0f, ESoundChannelType::SFX);
            }
        ),
        // BGM 제어
        "PlayBGM", &USoundManager::PlayBGM,
        "StopBGM", &USoundManager::StopBGM,
        "IsBGMPlaying", &USoundManager::IsBGMPlaying,
        // 채널 제어 - FMOD::Channel* 타입은 Lua에 노출하지 않음
        "StopAllSounds", &USoundManager::StopAllSounds,
        // 3D 리스너 설정
        "SetListenerPosition", &USoundManager::SetListenerPosition,
        // 볼륨 제어
        "SetMasterVolume", &USoundManager::SetMasterVolume,
        "GetMasterVolume", &USoundManager::GetMasterVolume,
        "SetMusicVolume", &USoundManager::SetMusicVolume,
        "GetMusicVolume", &USoundManager::GetMusicVolume,
        "SetSFXVolume", &USoundManager::SetSFXVolume,
        "GetSFXVolume", &USoundManager::GetSFXVolume,
        // 상태 확인
        "IsInitialized", &USoundManager::IsInitialized
    );

    //ActorType["GetSceneComponents"] = &AActor::GetSceneComponents;
}

void UScriptManager::RegisterGlobalValueToLua()
{
#ifdef _EDITOR
    Lua["GEngine"] = &GEngine;
#endif
    // SoundManager 싱글톤 등록
    Lua["SoundManager"] = &USoundManager::GetInstance();
}

void UScriptManager::RegisterGlobalFuncToLua()
{
    Lua["PrintToConsole"] = PrintToConsole;
    CoroutineScheduler.RegisterCoroutineTo(Lua);

    // 마우스에서 Ray 생성 함수 (마우스 방향 계산용)
    Lua["MakeRayFromMouse"] = &MakeRayFromMouse;

    Lua["GetRunnerGameMode"] = GetRunnerGameMode;
    Lua["IsGamePlaying"] = IsGamePlaying;

    // Cast 헬퍼 함수: AActor를 AStaticMeshActor로 캐스팅
    // 캐스팅 실패 시 nullptr 반환 (Lua에서는 nil)
    Lua["CastToStaticMeshActor"] = [](AActor* Actor) -> AStaticMeshActor* {
        if (!Actor) return nullptr;
        return Cast<AStaticMeshActor>(Actor);
        };

    CoroutineScheduler.RegisterCoroutineTo(Lua);
}

void UScriptManager::RegisterLocalValueToLua(sol::environment& InEnv, FLuaLocalValue LuaLocalValue)
{
    // MyActor를 실제 타입으로 캐스팅해서 등록
    // 가장 구체적인 타입부터 시도 (상속 순서 역순)
    AActor* Actor = LuaLocalValue.MyActor;

    // RunnerCharacter로 캐스팅 시도 (가장 구체적)
    if (ARunnerCharacter* RunnerCharacter = Cast<ARunnerCharacter>(Actor))
    {
        InEnv["MyActor"] = RunnerCharacter;
    }
    // Character로 캐스팅 시도
    else if (ACharacter* Character = Cast<ACharacter>(Actor))
    {
        InEnv["MyActor"] = Character;
    }
    // Pawn으로 캐스팅 시도
    else if (APawn* Pawn = Cast<APawn>(Actor))
    {
        InEnv["MyActor"] = Pawn;
    }
    // 그냥 Actor
    else
    {
        InEnv["MyActor"] = Actor;
    }

    // GameMode 등록
    if (LuaLocalValue.GameMode)
    {
        if (ARunnerGameMode* RunnerGameMode = Cast<ARunnerGameMode>(LuaLocalValue.GameMode))
        {
            InEnv["GameMode"] = RunnerGameMode;
        }
        else
        {
            InEnv["GameMode"] = LuaLocalValue.GameMode;
        }
        //UE_LOG("Params of GameMode %d", InEnv["GameMode"].JumpScore);
    }

    // World 등록 (발사체 생성 등을 위해 필요)
    if (Actor && Actor->World)
    {
        InEnv["World"] = Actor->World;
    }
}

// 스크립트를 Actor에 부착할 때 Actor의 ShapeComponent에 Lua의 OnOverlap 함수를 연결한다
void UScriptManager::LinkOnOverlapWithShapeComponent(AActor* MyActor, sol::function OnOverlap)
{
    for (UActorComponent* ActorComponent : MyActor->GetOwnedComponents())
    {
        UShapeComponent* ShapeComponent = Cast<UShapeComponent>(ActorComponent);
        if (ShapeComponent)
        {
            auto DelegateHandle = ShapeComponent->OnComponentBeginOverlap.Add(
                OnOverlap
            );
        }
    }
}

void UScriptManager::LinkRestartToDeligate(FLuaTemplateFunctions& LuaTemplateFunctions)
{
    UWorld* PIEWorld = GEngine.GetPIEWorld();
    if (!PIEWorld) return;

    AGameModeBase* GameMode = PIEWorld->GetGameMode();
    if (!GameMode) return;
    
    ARunnerGameMode* RunnerGameMode = Cast<ARunnerGameMode>(GEngine.GetPIEWorld()->GetGameMode());
    if (!RunnerGameMode) return;
    
    LuaTemplateFunctions.OnRestartDelegateHandle =
        RunnerGameMode->GetOnGameRestarted().Add(LuaTemplateFunctions.Restart);
}

// Lua로부터 Template 함수를 가져온다.
// 해당 함수가 없으면 Throw한다.
FLuaTemplateFunctions UScriptManager::GetTemplateFunctionFromScript(
    sol::environment& InEnv
)
{
    FLuaTemplateFunctions LuaTemplateFunctions;

    auto AssignFunction = [&InEnv](
        sol::function& Target,
        const char* FunctionName
        )
        {
            Target = InEnv[FunctionName];
            if (!Target.valid())
                throw std::runtime_error(FString("Missing Lua function: ") + FunctionName);
        };

    AssignFunction(LuaTemplateFunctions.BeginPlay, "BeginPlay");
    AssignFunction(LuaTemplateFunctions.EndPlay, "EndPlay");
    AssignFunction(LuaTemplateFunctions.OnOverlap, "OnOverlap");
    AssignFunction(LuaTemplateFunctions.Tick, "Tick");
    AssignFunction(LuaTemplateFunctions.Restart, "Restart");

    return LuaTemplateFunctions;
}

void UScriptManager::SetLuaScriptField(
    fs::path Path,
    sol::environment& InEnv,
    sol::table& InScriptTable,
    FLuaTemplateFunctions& InLuaTemplateFunction
)
{
    // 새 environment 생성 (globals 기반)
    InEnv = sol::environment(Lua, sol::create, Lua.globals());

    // 스크립트 로드 및 컴파일 (문법 오류 체크)
    sol::load_result scriptLoad = Lua.load_file(Path.string());
    if (!scriptLoad.valid()) {
        sol::error Err = scriptLoad;
        throw Err;  // 컴파일 타임 문법 오류 발생 시 throw
    }

    // 스크립트 실행 (런타임 오류 체크, InEnv를 '...' 가변인자로 전달)
    sol::protected_function_result result = scriptLoad(InEnv);
    if (!result.valid()) {
        sol::error Err = result;
        throw Err;  // 런타임 오류 발생 시 throw
    }

    // 혹은 스크립트가 return table이면 result 반환값 사용 가능
    sol::object returned = InEnv["_G"];
    if (returned.is<sol::table>()) {
        InScriptTable = returned.as<sol::table>();
    }

    // LuaTemplateFunction 생성
    InLuaTemplateFunction =
        GetTemplateFunctionFromScript(InEnv);
}

FScript* UScriptManager::GetOrCreate(FString InScriptName)
{
    fs::path Path(SCRIPT_FILE_PATH + InScriptName);
    FString ScriptName = Path.filename().string();

    // 파일이 존재하지 않으면 예외 발생
    if (!fs::exists(Path))
    {
        throw std::runtime_error(FString("Script file not found: ") + ScriptName +
            ". Please create the script file first.");
    }

    sol::environment Env;
    sol::table Table;
    FLuaTemplateFunctions LuaTemplateFunctions;

    SetLuaScriptField(Path, Env, Table, LuaTemplateFunctions);

    FScript* NewScript = new FScript;

    NewScript->ScriptName = ScriptName;
    NewScript->Env = Env;
    NewScript->Table = Table;
    NewScript->LuaTemplateFunctions = LuaTemplateFunctions;

    // Hot reload를 위해 초기 수정 시간 설정
    if (fs::exists(Path))
    {
        NewScript->LastModifiedTime = fs::last_write_time(Path);
    }

    return NewScript;
}

// 스크립트 파일 생성 (template.lua 복사)
bool UScriptManager::CreateScriptFile(const FString& ScriptName)
{
    try
    {
        fs::path TargetPath(SCRIPT_FILE_PATH + ScriptName);

        // 이미 파일이 존재하면 false 반환
        if (fs::exists(TargetPath))
        {
            UE_LOG("[Script Manager] Script file already exists: %s", ScriptName.c_str());
            return false;
        }

        // Scripts 디렉토리가 없으면 생성
        fs::path ScriptDir(SCRIPT_FILE_PATH);
        if (!fs::exists(ScriptDir))
        {
            fs::create_directories(ScriptDir);
        }

        // template.lua 복사
        fs::copy_file(DEFAULT_FILE_PATH, TargetPath, fs::copy_options::overwrite_existing);

        UE_LOG("[Script Manager] Created new script file: %s", ScriptName.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG("[Script Manager] Failed to create script file: %s - Error: %s",
            ScriptName.c_str(), e.what());
        return false;
    }
}

// 스크립트 파일을 외부 에디터로 열기
bool UScriptManager::OpenScriptInEditor(const FString& ScriptName)
{
    try
    {
        fs::path ScriptPath(SCRIPT_FILE_PATH + ScriptName);

        // 파일이 존재하는지 확인
        if (!fs::exists(ScriptPath))
        {
            UE_LOG("[Script Manager] Script file not found: %s", ScriptName.c_str());
            return false;
        }

        // 절대 경로로 변환
        fs::path AbsolutePath = fs::absolute(ScriptPath);
        FString PathStr = AbsolutePath.string();

        // Windows에서 기본 텍스트 에디터로 열기
#ifdef _WIN32
        HINSTANCE Result = ShellExecuteA(
            nullptr,              // hwnd
            "open",               // operation
            PathStr.c_str(),      // file
            nullptr,              // parameters
            nullptr,              // directory
            SW_SHOW               // show command
        );

        // ShellExecute는 성공 시 32보다 큰 값을 반환
        if ((INT_PTR)Result > 32)
        {
            UE_LOG("[Script Manager] Opened script in editor: %s", ScriptName.c_str());
            return true;
        }
        else
        {
            UE_LOG("[Script Manager] Failed to open script in editor: %s", ScriptName.c_str());
            return false;
        }
#else
        // Linux/Mac의 경우 시스템 명령으로 처리
        FString Command = "xdg-open \"" + PathStr + "\"";
        int Result = system(Command.c_str());
        return Result == 0;
#endif
    }
    catch (const std::exception& e)
    {
        UE_LOG("[Script Manager] Exception while opening script: %s - Error: %s",
            ScriptName.c_str(), e.what());
        return false;
    }
}
