#include "Vector.h"

enum class ERichCurveInterpMode
{
    RCIM_Linear,
    // RCIM_Constant,
    // RCIM_Cubic,
    // RCIM_None
};

struct FRichCurveKey
{
    float Time = 0.0f;             // X축 (초 단위)
    float Value = 0.0f;            // Y축 (float 값)
    float ArriveTangent = 0.0f;    // 이전 구간에서 들어올 때 기울기
    float LeaveTangent = 0.0f;     // 다음 구간으로 나갈 때 기울기
};

struct FRichCurve
{
    TArray<FRichCurveKey> Keys;
    ERichCurveInterpMode InterpMode;
};

class UCurveFloat : public UObject
{
    DECLARE_CLASS(UCurveFloat, UObject)
    GENERATED_REFLECTION_BODY()
public:
    /* Constructor & Desturctor */
    UCurveFloat(
        const float InTimeRange,
        const FVector2D& InValueRange,
        const int32 InNumSamples
    );

    UCurveFloat() = default;
    ~UCurveFloat() = default;

    /* Handle Curve */
    virtual void RenewCurve();

    /* Getters */
    const FRichCurve& GetCurve() const;
    float GetTimeRange() const;
    const FVector2D& GetValueRange() const;
    int32 GetNumSamples() const;

    /* Setters */
    void SetTimeRange(float InTimeRange);
    void SetValueRange(const FVector2D& InValueRange);
    void SetNumSamples(int32 InNumSamples);

protected:
    FRichCurve Curve;

    // Curve을 정의하는 최소한의 요소들
    float TimeRange = 0.0f;
    FVector2D ValueRange{};

    // 시작점과 끝점을 포함한 샘플의 수
    int32 NumSamples = 0;
};
