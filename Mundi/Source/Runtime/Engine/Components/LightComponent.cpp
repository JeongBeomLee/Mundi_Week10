#include "pch.h"
#include "LightComponent.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(ULightComponent)

BEGIN_PROPERTIES(ULightComponent)
	ADD_PROPERTY_RANGE(float, Temperature, "Light", 1000.0f, 15000.0f, true,
		"조명의 색온도를 켈빈(K) 단위로 설정합니다\n(1000K: 주황색, 6500K: 주광색, 15000K: 푸른색)")
	ADD_PROPERTY_RANGE(float, ShadowBias, "Shadow", 0.0f, 0.01f, true,
		"섀도우 바이어스 값입니다\n(섀도우 아크네를 제거하기 위한 깊이 오프셋)")
	ADD_PROPERTY_RANGE(float, ShadowSlopeBias, "Shadow", 0.0f, 4.0f, true,
		"섀도우 슬로프 바이어스 값입니다\n(경사면에서의 섀도우 아크네 제거)")
	ADD_PROPERTY_RANGE(float, ShadowSharpen, "Shadow", 0.0f, 4.0f, true,
		"섀도우 선명도입니다\n")
END_PROPERTIES()

// Color Temperature to RGB conversion using blackbody radiation approximation
// Based on Tanner Helland's algorithm
static FLinearColor ColorTemperatureToRGB(float InTemperature)
{
	// Neutral white (D65) is 6500K - if temperature is this value, return white
	if (FMath::Abs(InTemperature - 6500.0f) < 0.1f)
	{
		return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Clamp temperature to reasonable range (1000K - 40000K)
	float Temp = FMath::Clamp(InTemperature, 1000.0f, 40000.0f);
	Temp /= 100.0f;

	float Red, Green, Blue;

	// Calculate Red
	if (Temp <= 66.0f)
	{
		Red = 255.0f;
	}
	else
	{
		Red = 329.698727446f * std::pow(Temp - 60.0f, -0.1332047592f);
	}

	// Calculate Green
	if (Temp <= 66.0f)
	{
		Green = 99.4708025861f * std::log(Temp) - 161.1195681661f;
	}
	else
	{
		Green = 288.1221695283f * std::pow(Temp - 60.0f, -0.0755148492f);
	}

	// Calculate Blue
	if (Temp >= 66.0f)
	{
		Blue = 255.0f;
	}
	else if (Temp <= 19.0f)
	{
		Blue = 0.0f;
	}
	else
	{
		Blue = 138.5177312231f * std::log(Temp - 10.0f) - 305.0447927307f;
	}

	// Normalize to [0, 1] range and clamp
	return FLinearColor(
		FMath::Clamp(Red / 255.0f, 0.0f, 1.0f),
		FMath::Clamp(Green / 255.0f, 0.0f, 1.0f),
		FMath::Clamp(Blue / 255.0f, 0.0f, 1.0f),
		1.0f
	);
}

ULightComponent::ULightComponent()
{
	Temperature = 6500.0f;
	ShadowBias = 0.001f;
	ShadowSlopeBias = 0.0f;
	ShadowSharpen = 1.0f;
}

ULightComponent::~ULightComponent()
{
}

FLinearColor ULightComponent::GetLightColorWithIntensity() const
{
	// Get base light color
	FLinearColor FinalColor = LightColor;

	// Apply color temperature (6500K is neutral white)
	FLinearColor TempColor = ColorTemperatureToRGB(Temperature);
	FinalColor = FinalColor * TempColor;

	// Apply intensity
	FinalColor.R *= Intensity;
	FinalColor.G *= Intensity;
	FinalColor.B *= Intensity;
	FinalColor.A = 1.0f;

	return FinalColor;
}

void ULightComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);
	UE_LOG("LightComponent::OnRegister called");
	if (!SpriteComponent && !InWorld->bPie)
	{
		UE_LOG("Creating SpriteComponent (Billboard)...");
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		UE_LOG("SpriteComponent created successfully");
	}
	else
	{
		UE_LOG("SpriteComponent already exists, skipping creation");
	}
}

void ULightComponent::OnSerialized()
{
	Super::OnSerialized();

}

void ULightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// Critical: Nullify editor component pointers
	// These will be recreated in OnRegister() when actor is registered to world
}
