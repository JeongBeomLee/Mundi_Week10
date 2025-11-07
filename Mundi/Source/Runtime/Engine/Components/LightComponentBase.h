#pragma once
#include "SceneComponent.h"
#include "Color.h"
#include "ObjectMacros.h"

// 모든 라이트 컴포넌트의 베이스 클래스
class ULightComponentBase : public USceneComponent
{
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	GENERATED_REFLECTION_BODY()

	ULightComponentBase();
	virtual ~ULightComponentBase() override;

public:
	//// Light Properties
	//void SetEnabled(bool bInEnabled) { bIsEnabled = bInEnabled; }
	//bool IsEnabled() const { return bIsEnabled; }

	void SetIntensity(float InIntensity) { Intensity = InIntensity;  }
	float GetIntensity() const { return Intensity; }

	void SetCastShadow(bool InValue) { bIsCastShadows = InValue; }
	bool GetIsCastShadows() const { return bIsCastShadows; }

	// Shadow mapping, 인덱스 값이 -1이면 비활성화를 뜻합니다.
	void SetShadowMapIndex(int32 Index) { ShadowMapIndex = Index; }
	int32 GetShadowMapIndex() const { return ShadowMapIndex; }

	void SetLightColor(const FLinearColor& InColor) { LightColor = InColor; }
	const FLinearColor& GetLightColor() const { return LightColor; }

	// Virtual Interface
	virtual void UpdateLightData();

	// Serialization & Duplication
	virtual void OnSerialized() override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULightComponentBase)

protected:
	//bool bIsEnabled = true;
	float Intensity = 1.0f;
	bool bIsCastShadows = true;
	int32 ShadowMapIndex = -1;
	FLinearColor LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};
