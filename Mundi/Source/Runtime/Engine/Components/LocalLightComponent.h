#pragma once
#include "LightComponent.h"

// 위치 기반 로컬 라이트의 공통 베이스 (Point, Spot)
class ULocalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)
	GENERATED_REFLECTION_BODY()

	ULocalLightComponent();
	virtual ~ULocalLightComponent() override;

public:
	// Attenuation Properties
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }
	float GetAttenuationRadius() const { return AttenuationRadius; }

	void SetFalloffExponent(float InExponent) { FalloffExponent = InExponent; }
	float GetFalloffExponent() const { return FalloffExponent; }

	// 감쇠 방식 선택 (Unreal Engine style)
	// true: Inverse Square Falloff (물리적으로 정확한 역제곱 감쇠)
	// false: Exponent Falloff (예술적 제어를 위한 지수 기반 감쇠)
	void SetUseInverseSquareFalloff(bool bInUse) { bUseInverseSquareFalloff = bInUse; }
	bool IsUsingInverseSquareFalloff() const { return bUseInverseSquareFalloff; }

	// 거리 기반 감쇠 계산
	// 반환값: 0.0 (영향 없음) ~ 1.0 (최대 영향)
	// - 0.0: AttenuationRadius 밖, 라이트 영향 없음
	// - 0.0 < x < 1.0: 부분적 영향 (거리에 따라 감쇠)
	// - 1.0: 라이트 중심, 최대 영향
	virtual float GetAttenuationFactor(const FVector& WorldPosition) const;

	// Serialization & Duplication
	virtual void OnSerialized() override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULocalLightComponent)

protected:
	float AttenuationRadius = 5.0f;			// 감쇠 반경
	float FalloffExponent = 2.0f;			// 감쇠 지수 (bUseInverseSquareFalloff = false일 때 사용)
	bool bUseInverseSquareFalloff = true;	// true: Inverse Square Falloff (물리 법칙), false: Exponent Falloff (예술적 제어)
};
