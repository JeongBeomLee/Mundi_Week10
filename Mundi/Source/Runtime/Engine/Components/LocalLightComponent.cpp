#include "pch.h"
#include "LocalLightComponent.h"

IMPLEMENT_CLASS(ULocalLightComponent)

BEGIN_PROPERTIES(ULocalLightComponent)
	ADD_PROPERTY_RANGE(float, AttenuationRadius, "Light", 0.0f, 10000.0f, true, "감쇠 반경입니다.")
	ADD_PROPERTY(bool, bUseInverseSquareFalloff, "Light", true, "감쇠 방식 선택: true = Inverse Square Falloff (물리 법칙), false = Exponent Falloff (예술적 제어).")
	ADD_PROPERTY_RANGE(float, FalloffExponent, "Light", 0.1f, 10.0f, true, "감쇠 지수입니다 (bUseInverseSquareFalloff = false일 때 사용).")
END_PROPERTIES()

ULocalLightComponent::ULocalLightComponent()
{
}

ULocalLightComponent::~ULocalLightComponent()
{
}

float ULocalLightComponent::GetAttenuationFactor(const FVector& WorldPosition) const
{
	FVector LightPosition = GetWorldLocation();
	float Distance = FVector::Distance(LightPosition, WorldPosition);

	if (Distance >= AttenuationRadius)
	{
		return 0.0f;
	}

	// 거리 기반 감쇠 계산
	float NormalizedDistance = Distance / AttenuationRadius;
	float Attenuation = 1.0f - pow(NormalizedDistance, FalloffExponent);

	return FMath::Max(0.0f, Attenuation);
}

void ULocalLightComponent::OnSerialized()
{
	Super::OnSerialized();

}

void ULocalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
