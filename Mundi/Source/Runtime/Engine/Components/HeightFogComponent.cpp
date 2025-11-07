#include "pch.h"
#include "HeightFogComponent.h"

#include "Color.h"
#include "ResourceManager.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(UHeightFogComponent)

BEGIN_PROPERTIES(UHeightFogComponent)
	MARK_AS_COMPONENT("하이트 포그 컴포넌트", "하이트 기반 포그 효과를 생성합니다.")
	ADD_PROPERTY(FLinearColor, FogInscatteringColor, "Fog", "안개 산란 색상입니다.")
	ADD_PROPERTY_RANGE(float, FogDensity, "Fog", 0.0f, 10.0f, true, "안개 밀도입니다.")
	ADD_PROPERTY_RANGE(float, FogHeightFalloff, "Fog", 0.0f, 10.0f, true, "높이에 따른 안개 감쇠 정도입니다.")
	ADD_PROPERTY_RANGE(float, StartDistance, "Fog", 0.0f, 10000.0f, true, "안개가 시작되는 거리입니다.")
	ADD_PROPERTY_RANGE(float, FogCutoffDistance, "Fog", 0.0f, 100000.0f, true, "안개가 최대가 되는 거리입니다.")
	ADD_PROPERTY_RANGE(float, FogMaxOpacity, "Fog", 0.0f, 1.0f, true, "안개 최대 불투명도입니다.")
END_PROPERTIES()

UHeightFogComponent::UHeightFogComponent()
{
	// 사막 느낌
	FogInscatteringColor = FLinearColor(0.93f, 0.79f, 0.69f, 1.0f);
	HeightFogShader = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/HeightFog_PS.hlsl");

	
}

UHeightFogComponent::~UHeightFogComponent()
{
}

void UHeightFogComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);
	if (!SpriteComponent && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/S_AtmosphericHeightFog.dds");

	}

}

void UHeightFogComponent::RenderHeightFog(URenderer* Renderer)
{
}

void UHeightFogComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		// Load FogInscatteringColor
		if (InOutHandle.hasKey("FogInscatteringColor"))
		{
			FVector4 ColorVec;
			FJsonSerializer::ReadVector4(InOutHandle, "FogInscatteringColor", ColorVec);
			FogInscatteringColor = FLinearColor(ColorVec);
		}

		// Load HeightFogShader
		if (InOutHandle.hasKey("HeightFogShader"))
		{
			FString shaderPath = InOutHandle["HeightFogShader"].ToString();
			if (!shaderPath.empty())
			{
				HeightFogShader = UResourceManager::GetInstance().Load<UShader>(shaderPath.c_str());
			}
		}

		// FogDensity, FogHeightFalloff, StartDistance, FogCutoffDistance, FogMaxOpacity도 로드
		FJsonSerializer::ReadFloat(InOutHandle, "FogDensity", FogDensity, 0.2f);
		FJsonSerializer::ReadFloat(InOutHandle, "FogHeightFalloff", FogHeightFalloff, 0.2f);
		FJsonSerializer::ReadFloat(InOutHandle, "StartDistance", StartDistance, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "FogCutoffDistance", FogCutoffDistance, 6000.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "FogMaxOpacity", FogMaxOpacity, 1.0f);
	}
	else
	{
		// Save FogInscatteringColor
		if (FogInscatteringColor != FLinearColor::Zero())
		{
			InOutHandle["FogInscatteringColor"] = FJsonSerializer::Vector4ToJson(FogInscatteringColor.ToFVector4());
		}

		// Save HeightFogShader
		if (HeightFogShader != nullptr)
		{
			InOutHandle["HeightFogShader"] = HeightFogShader->GetFilePath();
		}
		else
		{
			InOutHandle["HeightFogShader"] = "";
		}

		// FogDensity, FogHeightFalloff, StartDistance, FogCutoffDistance, FogMaxOpacity도 저장
		InOutHandle["FogDensity"] = FogDensity;
		InOutHandle["FogHeightFalloff"] = FogHeightFalloff;
		InOutHandle["StartDistance"] = StartDistance;
		InOutHandle["FogCutoffDistance"] = FogCutoffDistance;
		InOutHandle["FogMaxOpacity"] = FogMaxOpacity;

	}
}
void UHeightFogComponent::OnSerialized()
{
	Super::OnSerialized();

	
}

void UHeightFogComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
