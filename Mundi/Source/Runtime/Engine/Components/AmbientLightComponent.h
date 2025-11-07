#pragma once
#include "LightComponent.h"
#include "LightManager.h"

// 환경광 (전역 조명)
class UAmbientLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	GENERATED_REFLECTION_BODY()

	UAmbientLightComponent();
	virtual ~UAmbientLightComponent() override;

public:
	// Light Info
	FAmbientLightInfo GetLightInfo() const;

	// Virtual Interface
	virtual void UpdateLightData() override;
	void OnTransformUpdated() override;
	void OnRegister(UWorld* InWorld) override;
	void OnUnregister() override;

	// Serialization & Duplication
	virtual void OnSerialized() override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UAmbientLightComponent)
};
