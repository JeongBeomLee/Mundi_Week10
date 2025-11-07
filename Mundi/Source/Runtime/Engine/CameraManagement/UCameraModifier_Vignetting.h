#pragma once
#include "UCameraModifier.h"

class UCameraModifier_Vignetting : public UCameraModifier
{
    DECLARE_CLASS(UCameraModifier_Vignetting, UCameraModifier)
    GENERATED_REFLECTION_BODY()
public:
    UCameraModifier_Vignetting();
    ~UCameraModifier_Vignetting() = default;

    void ModifyPostProcess(
        float DeltaTime,
        float& PostProcessBlendWeight,
        FPostProcessSettings& PostProcessSettings
    ) override;

    float GetRadius() const;
    void SetRadius(float InRadius);
    
    float GetSoftness() const;
    void SetSoftness(float InSoftness);

    FVector4 GetVignettingColor() const;
    void SetVignettingColor(const FVector4& InVignettingColor);
private:
    float Radius;
    float Softness;
    FVector4 VignettingColor{};
};