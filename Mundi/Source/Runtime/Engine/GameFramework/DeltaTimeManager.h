#pragma once
#include "Object.h"
#include "UEContainer.h"

/**
 * Time Dilation 효과의 종류
 */
enum class ETimeDilationPriority : uint8
{
    Low = 0,      // 일반 Slomo (배경 효과)
    Normal = 10,  // 기본 우선순위
    High = 20,    // 중요한 효과
    Critical = 30 // Hit Stop (최우선)
};

/**
 * 활성화된 Time Dilation 효과 정보
 */
struct FTimeDilationEffect
{
    /** 효과 이름 (디버깅용) */
    FString Name;
    
    /** 시간 배율 */
    float TimeDilation;
    
    /** 남은 지속 시간 (초) */
    float RemainingTime;
    
    /** 우선순위 */
    ETimeDilationPriority Priority;
    
    /** 효과 ID (고유 식별자) */
    uint32 EffectID;

    FTimeDilationEffect()
        : TimeDilation(1.0f)
        , RemainingTime(0.0f)
        , Priority(ETimeDilationPriority::Normal)
        , EffectID(0)
    {}

    FTimeDilationEffect(const FString& InName, float InDilation, float InDuration, ETimeDilationPriority InPriority, uint32 InID)
        : Name(InName)
        , TimeDilation(InDilation)
        , RemainingTime(InDuration)
        , Priority(InPriority)
        , EffectID(InID)
    {}

    /** 우선순위 비교 (내림차순: 높은 우선순위가 앞에) */
    bool operator<(const FTimeDilationEffect& Other) const
    {
        return static_cast<uint8>(Priority) > static_cast<uint8>(Other.Priority);
    }
};

/**
 * UDeltaTimeManager
 * 
 * World의 시간 흐름(DeltaTime)을 관리하는 클래스입니다.
 * 우선순위 기반 스택으로 여러 Time Dilation 효과를 동시에 관리합니다.
 */
class UDeltaTimeManager : public UObject
{
public:
    DECLARE_CLASS(UDeltaTimeManager, UObject)

    UDeltaTimeManager();
    virtual ~UDeltaTimeManager() override = default;

    // ═══════════════════════════════════════════════
    // Time Dilation 효과 적용
    // ═══════════════════════════════════════════════

    /**
     * Hit Stop 효과 (짧은 시간 정지)
     * @param Duration - 정지 시간 (실제 시간 기준, 초)
     * @return 효과 ID (취소 시 사용)
     */
    uint32 ApplyHitStop(float Duration, ETimeDilationPriority Priority);

    /**
     * Slomo 효과 (슬로우 모션)
     * @param Duration - 슬로우 모션 지속 시간 (실제 시간 기준, 초)
     * @param TimeDilation - 시간 배율 (예: 0.3 = 30% 속도)
     * @param Priority - 효과 우선순위
     * @return 효과 ID (취소 시 사용)
     */
    uint32 ApplySlomoEffect(float Duration, float TimeDilation, 
                           ETimeDilationPriority Priority);

    /**
     * 특정 효과를 취소합니다.
     * @param EffectID - 취소할 효과 ID
     */
    void CancelEffect(uint32 EffectID);

    /**
     * 모든 Time Dilation 효과를 즉시 취소합니다.
     */
    void CancelAllEffects();

    // ═══════════════════════════════════════════════
    // 전역 Time Dilation (기본 배율)
    // ═══════════════════════════════════════════════

    /**
     * 기본 시간 배율을 설정합니다 (효과가 없을 때 적용되는 배율).
     * @param NewDilation - 시간 배율 (0.0 = 정지, 1.0 = 정상, 2.0 = 2배속)
     */
    void SetBaseTimeDilation(float NewDilation);

    /**
     * 기본 시간 배율을 반환합니다.
     */
    float GetBaseTimeDilation() const { return BaseTimeDilation; }

    /**
     * 현재 적용 중인 시간 배율을 반환합니다 (최우선 효과의 배율).
     */
    float GetCurrentTimeDilation() const;

    // ═══════════════════════════════════════════════
    // DeltaTime 계산
    // ═══════════════════════════════════════════════

    /**
     * 조정된 DeltaTime을 반환합니다 (현재 TimeDilation 적용됨)
     * @param RealDeltaTime - 실제 프레임 시간
     * @return 시간 배율이 적용된 DeltaTime
     */
    float GetScaledDeltaTime(float RealDeltaTime) const
    {
        return RealDeltaTime * GetCurrentTimeDilation();
    }

    /**
     * Time Dilation 효과 업데이트 (World::Tick에서 호출)
     * @param RealDeltaTime - 실제 프레임 시간
     */
    void Update(float RealDeltaTime);

    // ═══════════════════════════════════════════════
    // 상태 쿼리
    // ═══════════════════════════════════════════════

    /**
     * 현재 활성화된 효과가 있는지 확인합니다.
     */
    bool HasActiveEffects() const { return !ActiveEffects.empty(); }

    /**
     * 활성화된 효과 개수를 반환합니다.
     */
    int32 GetActiveEffectCount() const { return static_cast<int32>(ActiveEffects.size()); }

    /**
     * 최우선 효과의 남은 시간을 반환합니다.
     */
    float GetRemainingTime() const;

    /**
     * 디버그 정보를 출력합니다.
     */
    void PrintDebugInfo() const;

private:
    // ═══════════════════════════════════════════════
    // 내부 헬퍼
    // ═══════════════════════════════════════════════

    /**
     * 효과 스택을 우선순위에 따라 정렬합니다.
     */
    void SortEffectsByPriority();

    /**
     * 고유 효과 ID를 생성합니다.
     */
    uint32 GenerateEffectID() { return NextEffectID++; }

    // ═══════════════════════════════════════════════
    // 멤버 변수
    // ═══════════════════════════════════════════════

    /** 기본 시간 배율 (효과가 없을 때 사용) */
    float BaseTimeDilation = 1.0f;

    /** 활성화된 Time Dilation 효과 스택 (우선순위 높은 순) */
    TArray<FTimeDilationEffect> ActiveEffects;

    /** 다음 효과 ID */
    uint32 NextEffectID = 1;
};