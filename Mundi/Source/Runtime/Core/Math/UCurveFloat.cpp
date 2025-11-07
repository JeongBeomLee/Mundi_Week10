#include "pch.h"
#include "UCurveFloat.h"

#include <random>

IMPLEMENT_CLASS(UCurveFloat)

BEGIN_PROPERTIES(UCurveFloat)
	ADD_PROPERTY_RANGE(float, TimeRange, "Curve", 0.0f, 100.0f, false, "커브의 시간 범위")
	ADD_PROPERTY(FVector2D, ValueRange, "Curve", false, "커브의 값 범위 (Min, Max)")
	ADD_PROPERTY_RANGE(int32, NumSamples, "Curve", 2, 1000, false, "커브 샘플 개수")
END_PROPERTIES()

UCurveFloat::UCurveFloat(
    const float InTimeRange,
    const FVector2D& InValueRange,
    const int32 InNumSamples
) :
    TimeRange(InTimeRange),
    ValueRange(InValueRange),
    NumSamples(InNumSamples)
{
    // 최초 1회 펄린 노이즈 곡선 초기화
    RenewCurve();
}

void UCurveFloat::RenewCurve()
{
    Curve.Keys = TArray<FRichCurveKey>(NumSamples, FRichCurveKey());
    Curve.InterpMode = ERichCurveInterpMode::RCIM_Linear;
}

// Getters
const FRichCurve& UCurveFloat::GetCurve() const
{
    return Curve;
}

float UCurveFloat::GetTimeRange() const
{
    return TimeRange;
}

const FVector2D& UCurveFloat::GetValueRange() const
{
    return ValueRange;
}

int32 UCurveFloat::GetNumSamples() const
{
    return NumSamples;
}

// Setters
void UCurveFloat::SetTimeRange(float InTimeRange)
{
    TimeRange = InTimeRange;
}

void UCurveFloat::SetValueRange(const FVector2D& InValueRange)
{
    ValueRange = InValueRange;
}

void UCurveFloat::SetNumSamples(int32 InNumSamples)
{
    NumSamples = InNumSamples;
}

