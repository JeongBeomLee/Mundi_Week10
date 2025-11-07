#include "UCurveFloat.h"

class UPerlinNoiseFloat : public UCurveFloat
{
    DECLARE_CLASS(UPerlinNoiseFloat, UCurveFloat)
    GENERATED_REFLECTION_BODY()
public:
    /* Constructor & Desturctor */
    UPerlinNoiseFloat(
        const float InTimeRange,
        const FVector2D& InValueRange,
        const int32 InNumSamples
    );

    UPerlinNoiseFloat() = default;
    ~UPerlinNoiseFloat() = default;

    /* Handle Curve */
    void RenewCurve() override;
};