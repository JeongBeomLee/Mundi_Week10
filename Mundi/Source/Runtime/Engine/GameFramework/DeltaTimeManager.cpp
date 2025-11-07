#include "pch.h"
#include "DeltaTimeManager.h"
#include <algorithm>

IMPLEMENT_CLASS(UDeltaTimeManager)

UDeltaTimeManager::UDeltaTimeManager()
    : BaseTimeDilation(1.0f)
    , NextEffectID(1)
{
}

void UDeltaTimeManager::SetBaseTimeDilation(float NewDilation)
{
    // 음수 방지 및 범위 제한
    NewDilation = FMath::Clamp(NewDilation, 0.0f, 10.0f);

    if (BaseTimeDilation != NewDilation)
    {
        float OldDilation = BaseTimeDilation;
        BaseTimeDilation = NewDilation;

        /*UE_LOG("[DeltaTimeManager] Base Time Dilation changed: %.2f -> %.2f",
            OldDilation, NewDilation);*/
    }
}

uint32 UDeltaTimeManager::ApplyHitStop(float Duration, ETimeDilationPriority Priority)
{
    // Hit Stop은 Critical 우선순위로 적용
    return ApplySlomoEffect(Duration, 0.0f, Priority);
}

uint32 UDeltaTimeManager::ApplySlomoEffect(float Duration, float TimeDilation, 
                                          ETimeDilationPriority Priority)
{
    if (Duration < 0.0f)
    {
        //UE_LOG("[DeltaTimeManager] Duration must be positive or zero!");
        return 0;
    }

    // Editor 모드에서는 TimeDilation 비활성화
    if (GWorld && !GWorld->bPie)
    {
        //UE_LOG("[DeltaTimeManager] TimeDilation is disabled in Editor mode");
        return 0;
    }

    // 효과 ID 생성
    uint32 EffectID = GenerateEffectID();

    // 효과 이름 생성 (디버깅용)
    FString EffectName;
    if (Priority == ETimeDilationPriority::High)
    {
        EffectName = "HitStop_" + std::to_string(EffectID);
    }
    else if(Priority == ETimeDilationPriority::Normal)
    {
        EffectName = "Slomo_" + std::to_string(EffectID);
    }

    // 새 효과 추가
    FTimeDilationEffect NewEffect(EffectName, TimeDilation, Duration, Priority, EffectID);
    ActiveEffects.Add(NewEffect);

    // 우선순위에 따라 정렬
    SortEffectsByPriority();

    // UE_LOG("[DeltaTimeManager] Effect applied - ID: %u, Name: %s, Dilation: %.2f, Duration: %.2fs, Priority: %d",
        //EffectID, EffectName.c_str(), TimeDilation, Duration, static_cast<int>(Priority));

    //PrintDebugInfo();

    return EffectID;
}

void UDeltaTimeManager::CancelEffect(uint32 EffectID)
{
    // ID로 효과 찾아서 제거
    for (int32 i = 0; i < ActiveEffects.Num(); ++i)
    {
        if (ActiveEffects[i].EffectID == EffectID)
        {
            // UE_LOG("[DeltaTimeManager] Effect cancelled - ID: %u, Name: %s",
            //     EffectID, ActiveEffects[i].Name.c_str());

            ActiveEffects.erase(ActiveEffects.begin() + i);
            //PrintDebugInfo();
            return;
        }
    }

    //UE_LOG("[DeltaTimeManager] Warning: Effect ID %u not found", EffectID);
}

void UDeltaTimeManager::CancelAllEffects()
{
    if (!ActiveEffects.empty())
    {
        int32 Count = GetActiveEffectCount();
        ActiveEffects.clear();

        //UE_LOG("[DeltaTimeManager] All effects cancelled (%d effects)", Count);
    }
}

float UDeltaTimeManager::GetCurrentTimeDilation() const
{
    // 효과가 없으면 기본 배율 반환
    if (ActiveEffects.empty())
    {
        return BaseTimeDilation;
    }

    // 최우선 효과(첫 번째)의 배율 반환
    return ActiveEffects[0].TimeDilation;
}

float UDeltaTimeManager::GetRemainingTime() const
{
    if (ActiveEffects.empty())
    {
        return 0.0f;
    }

    return ActiveEffects[0].RemainingTime;
}

void UDeltaTimeManager::Update(float RealDeltaTime)
{
    if (ActiveEffects.empty())
    {
        return;
    }

    bool bEffectsRemoved = false;

    // 모든 효과의 타이머 감소 (실제 시간 기준)
    // 역순으로 순회하여 안전하게 제거
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        if (i == 0)
        {
			ActiveEffects[i].RemainingTime -= RealDeltaTime; // 최우선 효과만 감소
        }

        // 시간이 다 된 효과 제거
        if (ActiveEffects[i].RemainingTime <= 0.0f)
        {
            UE_LOG("[DeltaTimeManager] Effect expired - ID: %u, Name: %s",
                ActiveEffects[i].EffectID, ActiveEffects[i].Name.c_str());

            ActiveEffects.erase(ActiveEffects.begin() + i);
            bEffectsRemoved = true;
        }
    }

    // 효과가 제거되었으면 디버그 정보 출력
    if (bEffectsRemoved)
    {
        //PrintDebugInfo();
    }
}

void UDeltaTimeManager::SortEffectsByPriority()
{
    // 우선순위 내림차순 정렬 (높은 우선순위가 앞에)
    std::sort(ActiveEffects.begin(), ActiveEffects.end());
}

void UDeltaTimeManager::PrintDebugInfo() const
{
    if (ActiveEffects.empty())
    {
        UE_LOG("[DeltaTimeManager] No active effects. Current Dilation: %.2f (Base)",
            BaseTimeDilation);
        return;
    }

    UE_LOG("[DeltaTimeManager] Active Effects: %d", GetActiveEffectCount());
    UE_LOG("  -> Current Dilation: %.2f (from '%s', Priority: %d)",
        GetCurrentTimeDilation(),
        ActiveEffects[0].Name.c_str(),
        static_cast<int>(ActiveEffects[0].Priority));

    for (int32 i = 0; i < GetActiveEffectCount(); ++i)
    {
        const FTimeDilationEffect& Effect = ActiveEffects[i];
        UE_LOG("     [%d] ID:%u, Name:%s, Dilation:%.2f, Remaining:%.2fs, Priority:%d",
            i, Effect.EffectID, Effect.Name.c_str(), Effect.TimeDilation,
            Effect.RemainingTime, static_cast<int>(Effect.Priority));
    }
}