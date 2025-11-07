#include "pch.h"
#include "ShadowConfiguration.h"

FShadowConfiguration FShadowConfiguration::FromQuality(EShadowQuality InQuality)
{
	FShadowConfiguration Config;
	Config.Quality = InQuality;

	switch (InQuality)
	{
	case EShadowQuality::Low:
		Config.DirectionalLightResolution = 1024;
		Config.SpotLightResolution = 512;
		Config.PointLightResolution = 512;
		Config.MaxDirectionalLights = 1;
		Config.MaxSpotLights = 100;
		Config.MaxPointLights = 50;
		Config.MaxShadowCastingLights = 151;
		// CSM Tiers (Low Quality)
		Config.CSMTierLow = FCSMTierConfig(256, 16);
		Config.CSMTierMedium = FCSMTierConfig(512, 12);
		Config.CSMTierHigh = FCSMTierConfig(1024, 8);
		break;

	case EShadowQuality::Medium:
		Config.DirectionalLightResolution = 2048;
		Config.SpotLightResolution = 1024;
		Config.PointLightResolution = 1024;
		Config.MaxDirectionalLights = 1;
		Config.MaxSpotLights = 200;
		Config.MaxPointLights = 100;
		Config.MaxShadowCastingLights = 301;
		// CSM Tiers (Medium Quality)
		Config.CSMTierLow = FCSMTierConfig(512, 16);
		Config.CSMTierMedium = FCSMTierConfig(1024, 12);
		Config.CSMTierHigh = FCSMTierConfig(2048, 8);
		break;

	case EShadowQuality::High:
		Config.DirectionalLightResolution = 4096;
		Config.SpotLightResolution = 2048;
		Config.PointLightResolution = 1024;
		Config.MaxDirectionalLights = 1;
		Config.MaxSpotLights = 300;
		Config.MaxPointLights = 150;
		Config.MaxShadowCastingLights = 451;
		// CSM Tiers (High Quality)
		Config.CSMTierLow = FCSMTierConfig(512, 16);
		Config.CSMTierMedium = FCSMTierConfig(2048, 12);
		Config.CSMTierHigh = FCSMTierConfig(4096, 8);
		break;

	case EShadowQuality::Custom:
	default:
		// 현재 값 유지
		break;
	}

	return Config;
}

FShadowConfiguration FShadowConfiguration::GetPlatformDefault()
{
	return FromQuality(EShadowQuality::Custom);
}

bool FShadowConfiguration::IsValid() const
{
	// 각 라이트 타입별 Shadow map 해상도는 2의 제곱수이고 적절한 범위 내에 있어야 함
	auto IsResolutionValid = [](uint32 Resolution) -> bool
		{
			if (Resolution < 256 || Resolution > 8192)
				return false;
			// 2의 제곱수 확인
			if ((Resolution & (Resolution - 1)) != 0)
				return false;
			return true;
		};

	// 2의 제곱수 확인
	if (!IsResolutionValid(DirectionalLightResolution))
		return false;
	if (!IsResolutionValid(SpotLightResolution))
		return false;
	if (!IsResolutionValid(PointLightResolution))
		return false;

	// 최대 라이트 수는 적절한 범위 내
	if (MaxShadowCastingLights < 1)
		return false;

	return true;
}
