#include "pch.h"
#include "ShadowManager.h"
#include "ShadowViewProjection.h"
#include "SpotLightComponent.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "D3D11RHI.h"
#include "ResourceManager.h"
#include "Shader.h"

FShadowManager::FShadowManager()
	: RHIDevice(nullptr)
	, ShadowVSM_PS(nullptr)
	, ShadowESM_PS(nullptr)
	, ShadowEVSM_PS(nullptr)
{
}

FShadowManager::~FShadowManager()
{
	Release();
}

void FShadowManager::Initialize(D3D11RHI* RHI, const FShadowConfiguration& InConfig)
{
	RHIDevice = RHI;
	Config = InConfig;

	// Shadow Map Array 초기화 (광원 타입별로 각각의 해상도 사용, 필터 타입 전달)
	SpotLightShadowMap.Initialize(RHI, Config.SpotLightResolution, Config.SpotLightResolution, Config.MaxSpotLights, false, Config.FilterType);

	// DirectionalLight Non-CSM (legacy, Default ShadowMapType)
	constexpr uint32 MaxCascadesPerLight = 6;
	uint32 DirectionalArraySize = Config.MaxDirectionalLights * MaxCascadesPerLight;
	DirectionalLightShadowMap.Initialize(RHI, Config.DirectionalLightResolution, Config.DirectionalLightResolution, DirectionalArraySize, false, Config.FilterType);

	// DirectionalLight CSM 3-Tier Arrays
	DirectionalLightShadowMapTiers[0].Initialize(RHI, Config.CSMTierLow.Resolution, Config.CSMTierLow.Resolution, Config.CSMTierLow.MaxSlices, false, Config.FilterType);
	DirectionalLightShadowMapTiers[1].Initialize(RHI, Config.CSMTierMedium.Resolution, Config.CSMTierMedium.Resolution, Config.CSMTierMedium.MaxSlices, false, Config.FilterType);
	DirectionalLightShadowMapTiers[2].Initialize(RHI, Config.CSMTierHigh.Resolution, Config.CSMTierHigh.Resolution, Config.CSMTierHigh.MaxSlices, false, Config.FilterType);

	PointLightCubeShadowMap.Initialize(RHI, Config.PointLightResolution, Config.PointLightResolution, Config.MaxPointLights, true, Config.FilterType);

	// CSM Cascade Allocation 초기화
	uint32 MaxGlobalCascades = Config.MaxDirectionalLights * MaxCascadesPerLight;
	CascadeAllocations.SetNum(MaxGlobalCascades);
	TierSlotUsage[0] = 0;
	TierSlotUsage[1] = 0;
	TierSlotUsage[2] = 0;

	// VSM/ESM/EVSM용 쉐이더 로드
	if (!ShadowVSM_PS)
	{
		ShadowVSM_PS = UResourceManager::GetInstance().Load<UShader>("Shaders/Common/ShadowVSM_PS.hlsl");
	}
	if (!ShadowESM_PS)
	{
		ShadowESM_PS = UResourceManager::GetInstance().Load<UShader>("Shaders/Common/ShadowESM_PS.hlsl");
	}
	if (!ShadowEVSM_PS)
	{
		ShadowEVSM_PS = UResourceManager::GetInstance().Load<UShader>("Shaders/Common/ShadowEVSM_PS.hlsl");
	}

	bIsInitialized = true;
}

void FShadowManager::Release()
{
	if (!bIsInitialized)
	{
		return;  // 초기화되지 않았으면 스킵
	}

	SpotLightShadowMap.Release();
	DirectionalLightShadowMap.Release();
	DirectionalLightShadowMapTiers[0].Release();
	DirectionalLightShadowMapTiers[1].Release();
	DirectionalLightShadowMapTiers[2].Release();
	PointLightCubeShadowMap.Release();

	bIsInitialized = false;
}

void FShadowManager::SetDirectionalLightResolution(uint32 NewResolution)
{
	// 초기화되지 않았으면 설정만 변경
	if (!bIsInitialized)
	{
		Config.DirectionalLightResolution = NewResolution;
		return;
	}

	// 이미 초기화된 경우 재초기화
	Config.DirectionalLightResolution = NewResolution;
	Release();
	Initialize(RHIDevice, Config);
}

void FShadowManager::SetSpotLightResolution(uint32 NewResolution)
{
	// 초기화되지 않았으면 설정만 변경
	if (!bIsInitialized)
	{
		Config.SpotLightResolution = NewResolution;
		return;
	}

	// 이미 초기화된 경우 재초기화
	Config.SpotLightResolution = NewResolution;
	Release();
	Initialize(RHIDevice, Config);
}

void FShadowManager::SetPointLightResolution(uint32 NewResolution)
{
	// 초기화되지 않았으면 설정만 변경
	if (!bIsInitialized)
	{
		Config.PointLightResolution = NewResolution;
		return;
	}

	// 이미 초기화된 경우 재초기화
	Config.PointLightResolution = NewResolution;
	Release();
	Initialize(RHIDevice, Config);
}

void FShadowManager::SetFilterType(EShadowFilterType NewFilterType)
{
	// 초기화되지 않았으면 설정만 변경
	if (!bIsInitialized)
	{
		Config.FilterType = NewFilterType;
		return;
	}

	// 이미 초기화된 경우 재초기화
	Config.FilterType = NewFilterType;
	Release();
	Initialize(RHIDevice, Config);
}

void FShadowManager::AssignShadowMapIndices(D3D11RHI* RHI, const FShadowCastingLights& InLights)
{
	// Lazy initialization: 최초 호출 시 ShadowMap 초기화
	if (!bIsInitialized)
	{
		Initialize(RHI, FShadowConfiguration::GetPlatformDefault());
	}

	// CSM Tier 슬롯 사용량 초기화
	TierSlotUsage[0] = 0;
	TierSlotUsage[1] = 0;
	TierSlotUsage[2] = 0;

	// RemappedSliceSRV 캐시 클리어 (매 프레임 Shadow Map이 갱신되므로 캐시도 무효화)
	SpotLightShadowMap.ClearRemappedSliceCache();
	DirectionalLightShadowMap.ClearRemappedSliceCache();
	DirectionalLightShadowMapTiers[0].ClearRemappedSliceCache();
	DirectionalLightShadowMapTiers[1].ClearRemappedSliceCache();
	DirectionalLightShadowMapTiers[2].ClearRemappedSliceCache();
	PointLightCubeShadowMap.ClearRemappedSliceCache();

	// 매 프레임 초기화 (Dynamic 환경) - 광원별로 독립적인 인덱스 사용
	uint32 DirectionalLightIndex = 0;
	uint32 SpotLightIndex = 0;
	uint32 PointLightIndex = 0;

	// 1. DirectionalLight 처리
	uint32 DirectionalLightCount = 0;
	for (UDirectionalLightComponent* DirLight : InLights.DirectionalLights)
	{
		// 유효성 검사
		if (!DirLight ||
			!DirLight->IsVisible() ||
			!DirLight->GetOwner()->IsActorVisible() ||
			!DirLight->GetIsCastShadows())
		{
			if (DirLight) { DirLight->SetShadowMapIndex(-1); }
			continue;
		}

		// 방향광 최대 개수 검사
		if (DirectionalLightIndex >= Config.MaxDirectionalLights)
		{
			DirLight->SetShadowMapIndex(-1);
			continue;
		}

		// CSM 사용 여부에 따라 Base Index 할당
		if (DirLight->GetShadowMapType() == EShadowMapType::CSM)
		{
			// CSM: 캐스케이드별로 티어 할당 (Base Index는 전역 캐스케이드 인덱스)
			constexpr uint32 MaxCascadesPerLight = 5;
			uint32 baseCascadeIndex = DirectionalLightIndex * MaxCascadesPerLight;
			DirLight->SetShadowMapIndex(baseCascadeIndex);

			// 각 캐스케이드를 티어에 할당
			int32 numCascades = DirLight->GetNumCascades();
			for (int32 cascadeIdx = 0; cascadeIdx < numCascades; ++cascadeIdx)
			{
				uint32 globalCascadeIdx = baseCascadeIndex + cascadeIdx;

				// 캐스케이드 거리 비율로 티어 결정 (0=가까움, 1=멀음)
				float ratio = (numCascades > 1) ? (float)cascadeIdx / (float)(numCascades - 1) : 0.0f;

				uint32 tierIndex = 0;
				if (ratio <= 0.33f)
					tierIndex = 2;  // High Tier (가까운 캐스케이드)
				else if (ratio <= 0.67f)
					tierIndex = 1;  // Medium Tier (중간 캐스케이드)
				else
					tierIndex = 0;  // Low Tier (먼 캐스케이드)

				// 티어 슬롯 할당
				uint32 sliceIndex = TierSlotUsage[tierIndex];
				TierSlotUsage[tierIndex]++;

				// 할당 정보 저장 (내부 배열)
				if (globalCascadeIdx < CascadeAllocations.Num())
				{
					CascadeAllocations[globalCascadeIdx].TierIndex = tierIndex;
					CascadeAllocations[globalCascadeIdx].SliceIndex = sliceIndex;
					CascadeAllocations[globalCascadeIdx].Resolution = DirectionalLightShadowMapTiers[tierIndex].GetWidth();
				}

				// DirectionalLightComponent에 티어 정보 캐싱 (GPU 업데이트용)
				DirLight->SetCascadeTierInfo(
					cascadeIdx,
					tierIndex,
					sliceIndex,
					(float)DirectionalLightShadowMapTiers[tierIndex].GetWidth()
				);
			}
		}
		else
		{
			// Non-CSM (Default): 기존 방식 유지
			DirLight->SetShadowMapIndex(DirectionalLightIndex);
		}

		DirectionalLightIndex++;
		DirectionalLightCount++;
	}

	// 2. PointLight 처리
	uint32 PointLightCount = 0;
	for (UPointLightComponent* PointLight : InLights.PointLights)
	{
		if (!PointLight ||
			!PointLight->IsVisible() ||
			!PointLight->GetOwner()->IsActorVisible() ||
			!PointLight->GetIsCastShadows())
		{
			if (PointLight) { PointLight->SetShadowMapIndex(-1); }
			continue;
		}

		// 포인트 라이트 최대 개수 검사
		if (PointLightIndex >= Config.MaxPointLights)
		{
			PointLight->SetShadowMapIndex(-1);
			continue;
		}

		// 섀도우맵 인덱스 할당 (PointLight는 별도 카운터 사용)
		PointLight->SetShadowMapIndex(PointLightCount);
		PointLightCount++;
	}

	// 3. SpotLight 처리
	uint32 SpotLightCount = 0;
	for (USpotLightComponent* SpotLight : InLights.SpotLights)
	{
		// 유효성 검사
		if (!SpotLight ||
			!SpotLight->IsVisible() ||
			!SpotLight->GetOwner()->IsActorVisible() ||
			!SpotLight->GetIsCastShadows())
		{
			if (SpotLight) { SpotLight->SetShadowMapIndex(-1); }
			continue;
		}

		// 스팟 라이트 최대 개수 검사
		if (SpotLightIndex >= Config.MaxSpotLights)
		{
			SpotLight->SetShadowMapIndex(-1);
			continue;
		}

		SpotLight->SetShadowMapIndex(SpotLightIndex);
		SpotLightIndex++;
		SpotLightCount++;
	}

	// 쉐도우 맵 통계 수집 및 업데이트
	FShadowStats Stats;
	//Stats.ShadowMapResolution = Config.ShadowMapResolution;
	Stats.MaxShadowCastingLights = Config.MaxShadowCastingLights;

	// CSM 사용 여부 확인 (DirectionalLight 중 하나라도 CSM을 사용하면 true)
	bool bAnyCSM = false;
	for (UDirectionalLightComponent* DirLight : InLights.DirectionalLights)
	{
		if (DirLight && DirLight->GetShadowMapType() == EShadowMapType::CSM)
		{
			bAnyCSM = true;
			break;
		}
	}
	Stats.bUsingCSM = bAnyCSM;

	// 광원별 해상도
	Stats.DirectionalLightResolution = DirectionalLightShadowMap.GetWidth();
	Stats.SpotLightResolution = SpotLightShadowMap.GetWidth();
	Stats.PointLightResolution = PointLightCubeShadowMap.GetWidth();

	// 실제 쉐도우 캐스팅 라이트 수
	Stats.DirectionalLightCount = DirectionalLightCount;
	Stats.SpotLightCount = SpotLightCount;
	Stats.PointLightCount = PointLightCount;

	// 각 쉐도우 맵의 실제 사용 중인 메모리 계산 (활성 라이트 수 기반)
	Stats.DirectionalLightUsedBytes = DirectionalLightShadowMap.GetUsedMemoryBytes(DirectionalLightCount);
	Stats.SpotLightUsedBytes = SpotLightShadowMap.GetUsedMemoryBytes(SpotLightCount);
	Stats.PointLightUsedBytes = PointLightCubeShadowMap.GetUsedMemoryBytes(PointLightCount * 6); // 큐브맵이므로 6개 면

	// CSM 티어별 통계 수집
	if (bAnyCSM)
	{
		for (int i = 0; i < 3; ++i)
		{
			Stats.CSMTierResolutions[i] = DirectionalLightShadowMapTiers[i].GetWidth();
			Stats.CSMTierCascadeCounts[i] = TierSlotUsage[i];  // 현재 사용 중인 슬롯 수
			Stats.CSMTierUsedBytes[i] = DirectionalLightShadowMapTiers[i].GetUsedMemoryBytes(TierSlotUsage[i]);
		}
	}

	// 총 사용 중인 메모리
	Stats.TotalUsedBytes = Stats.DirectionalLightUsedBytes + Stats.SpotLightUsedBytes + Stats.PointLightUsedBytes;
	if (bAnyCSM)
	{
		for (int i = 0; i < 3; ++i)
		{
			Stats.TotalUsedBytes += Stats.CSMTierUsedBytes[i];
		}
	}

	// 통계 매니저에 업데이트
	FShadowStatManager::GetInstance().UpdateStats(Stats);

	// 활성 라이트들의 평균 ShadowSharpen 계산 및 캐시
	CachedAverageShadowSharpen = CalculateAverageShadowSharpen(InLights);
}

bool FShadowManager::BeginShadowRender(D3D11RHI* RHI, USpotLightComponent* Light, FShadowRenderContext& OutContext)
{
	// 이 라이트가 Shadow Map을 할당받았는지 확인
	int32 Index = Light->GetShadowMapIndex();
	if (Index < 0)
	{
		return false;
	}

	// FShadowViewProjection 헬퍼 사용하여 VP 행렬 계산
	FShadowViewProjection ShadowVP = FShadowViewProjection::CreateForSpotLight(
		Light->GetWorldLocation(),
		Light->GetDirection(),
		Light->GetOuterConeAngle() * 2.0f,  // 전체 FOV
		Light->GetAttenuationRadius());

	// 출력 컨텍스트 설정
	OutContext.LightView = ShadowVP.View;
	OutContext.LightProjection = ShadowVP.Projection;
	OutContext.ShadowMapIndex = Index;
	OutContext.ShadowBias = Light->GetShadowBias();
	OutContext.ShadowSlopeBias = Light->GetShadowSlopeBias();

	// Shadow Map 렌더링 시작 (DSV 바인딩, Viewport 설정)
	SpotLightShadowMap.BeginRender(RHI, Index, OutContext.ShadowBias, OutContext.ShadowSlopeBias);

	return true;
}

bool FShadowManager::BeginShadowRender(D3D11RHI* RHI, UDirectionalLightComponent* Light,
	const FMatrix& CameraView, const FMatrix& CameraProjection, FShadowRenderContext& OutContext)
{
	// 이 라이트가 Shadow Map을 할당받았는지 확인
	int32 Index = Light->GetShadowMapIndex();
	if (Index < 0)
	{
		return false;
	}

	FShadowViewProjection ShadowVP;

	// ShadowProjectionType에 따라 LVP 또는 LiSPSM (Hybrid: TSM + OpenGL LiSPSM) 사용
	EShadowProjectionType ProjectionType = Light->GetShadowProjectionType();

	if (ProjectionType == EShadowProjectionType::LiSPSM)
	{
		// LiSPSM (Hybrid): 각도에 따라 TSM과 OpenGL LiSPSM을 자동 선택
		// - 수평(parallel): TSM 사용
		// - 수직(perpendicular): OpenGL LiSPSM 사용
		ShadowVP = FShadowViewProjection::CreateForDirectionalLight_LiSPSM(
			Light->GetLightDirection(),
			CameraView,
			CameraProjection);
	}
	else
	{
		// LVP (기본): 표준 직교 투영
		ShadowVP = FShadowViewProjection::CreateForDirectionalLight(
			Light->GetLightDirection(),
			CameraView,
			CameraProjection);
	}

	// 출력 컨텍스트 설정
	OutContext.LightView = ShadowVP.View;
	OutContext.LightProjection = ShadowVP.Projection;
	OutContext.ShadowMapIndex = Index;
	OutContext.ShadowBias = Light->GetShadowBias();

	// LiSPSM은 perspective warping으로 인해 slope가 변하므로 SlopeBias를 0으로 설정
	// LVP는 일반 orthographic이므로 원래 SlopeBias 사용
	if (ProjectionType == EShadowProjectionType::LiSPSM)
	{
		OutContext.ShadowSlopeBias = 0.0f;  // LiSPSM: SlopeBias 비활성화
	}
	else
	{
		OutContext.ShadowSlopeBias = Light->GetShadowSlopeBias();  // LVP: 원래 값 사용
	}

	// Light Component에 계산된 ViewProjection 저장 (Light Buffer 업데이트 시 사용)
	Light->SetLightViewProjection(ShadowVP.ViewProjection);

	// LiSPSM hybrid에서 실제 사용된 알고리즘 정보 저장 (Depth view 표시용)
	if (ProjectionType == EShadowProjectionType::LiSPSM)
	{
		Light->SetActualUsedTSM(ShadowVP.bUsedTSM);
	}

	// DirectionalLight Shadow Map 렌더링 시작 (DSV 바인딩, Viewport 설정)
	DirectionalLightShadowMap.BeginRender(RHI, Index, OutContext.ShadowBias, OutContext.ShadowSlopeBias);

	return true;
}

bool FShadowManager::BeginShadowRenderCSM(D3D11RHI* RHI, UDirectionalLightComponent* Light,
	const FMatrix& CameraView, const FMatrix& CameraProjection,
	int CascadeIndex, float SplitNear, float SplitFar, FShadowRenderContext& OutContext)
{
	// DirectionalLight의 NumCascades 가져오기
	const int32 NumCascades = Light->GetNumCascades();

	// 캐스케이드 인덱스 유효성 검사
	if (CascadeIndex < 0 || CascadeIndex >= NumCascades)
	{
		return false;
	}

	// 라이트의 Shadow Map Index 확인
	int32 LightShadowIndex = Light->GetShadowMapIndex();
	if (LightShadowIndex < 0)
	{
		return false;
	}

	// 1. 부분 Frustum 코너 계산
	TArray<FVector> CascadeFrustumCorners;
	GetFrustumCornersWorldSpace_Partial(
		CameraView,
		CameraProjection,
		SplitNear,
		SplitFar,
		CascadeFrustumCorners);

	// 2. Light VP 계산 (ShadowProjectionType에 따라 LVP 또는 LiSPSM (Hybrid: TSM + OpenGL LiSPSM))
	FShadowViewProjection ShadowVP;
	EShadowProjectionType ProjectionType = Light->GetShadowProjectionType();

	if (ProjectionType == EShadowProjectionType::LiSPSM)
	{
		// LiSPSM (Hybrid): 각도에 따라 TSM과 OpenGL LiSPSM을 자동 선택
		// - 수평(parallel): TSM 사용
		// - 수직(perpendicular): OpenGL LiSPSM 사용
		ShadowVP = FShadowViewProjection::CreateForDirectionalLight_LiSPSM_FromCorners(
			Light->GetLightDirection(),
			CascadeFrustumCorners,
			CameraView,
			CameraProjection);
	}
	else
	{
		// LVP (기본): 표준 직교 투영
		ShadowVP = FShadowViewProjection::CreateForDirectionalLight_FromCorners(
			Light->GetLightDirection(),
			CascadeFrustumCorners);
	}

	// 3. 전역 캐스케이드 인덱스 계산
	constexpr int32 MaxCascades = 6;
	uint32 GlobalCascadeIndex = LightShadowIndex + CascadeIndex;

	// 4. 티어 할당 정보 가져오기
	if (GlobalCascadeIndex >= CascadeAllocations.Num())
	{
		return false;
	}
	const FCascadeAllocation& allocation = CascadeAllocations[GlobalCascadeIndex];

	// 5. 출력 컨텍스트 설정
	OutContext.LightView = ShadowVP.View;
	OutContext.LightProjection = ShadowVP.Projection;
	OutContext.ShadowMapIndex = allocation.SliceIndex;  // 티어 내 슬라이스 인덱스
	OutContext.ShadowBias = Light->GetShadowBias();

	// LiSPSM은 perspective warping으로 인해 slope가 변하므로 SlopeBias를 0으로 설정
	if (ProjectionType == EShadowProjectionType::LiSPSM)
	{
		OutContext.ShadowSlopeBias = 0.0f;  // LiSPSM: SlopeBias 비활성화
	}
	else
	{
		OutContext.ShadowSlopeBias = Light->GetShadowSlopeBias();  // LVP: 원래 값 사용
	}

	// 6. Light Component에 VP 저장
	Light->SetCascadeViewProjection(CascadeIndex, ShadowVP.ViewProjection);
	Light->SetCascadeSplitDistance(CascadeIndex, SplitFar);

	// LiSPSM hybrid에서 실제 사용된 알고리즘 정보 저장 (Depth view 표시용)
	if (ProjectionType == EShadowProjectionType::LiSPSM)
	{
		Light->SetActualUsedTSM(ShadowVP.bUsedTSM);
	}

	// 7. 해당 티어의 Shadow Map 렌더링 시작
	DirectionalLightShadowMapTiers[allocation.TierIndex].BeginRender(
		RHI,
		allocation.SliceIndex,
		OutContext.ShadowBias,
		OutContext.ShadowSlopeBias);

	return true;
}

TArray<float> FShadowManager::CalculateCSMSplits(float CameraNear, float CameraFar, int NumCascades, float Lambda) const
{
	TArray<float> Splits;
	Splits.Reserve(NumCascades);

	for (int i = 0; i < NumCascades; ++i)
	{
		float p = (i + 1) / (float)NumCascades;

		// 로그 분할
		float LogSplit = CameraNear * pow(CameraFar / CameraNear, p);

		// 선형 분할
		float LinearSplit = CameraNear + (CameraFar - CameraNear) * p;

		// PSSM 혼합
		float Split = Lambda * LogSplit + (1.0f - Lambda) * LinearSplit;
		Splits.Add(Split);
	}

	return Splits;
}

bool FShadowManager::BeginShadowRenderCube(D3D11RHI* RHI, UPointLightComponent* Light, uint32 CubeFaceIndex, const FShadowViewProjection& ShadowVP, FShadowRenderContext& OutContext)
{
	// 이 라이트가 Shadow Map을 할당받았는지 확인
	int32 Index = Light->GetShadowMapIndex();
	if (Index < 0)
	{
		return false;
	}

	// Cube Face 인덱스 유효성 검사
	if (CubeFaceIndex >= 6)
	{
		return false;
	}

	// 미리 계산된 VP 행렬 사용 (중복 계산 제거)
	// 출력 컨텍스트 설정
	OutContext.LightView = ShadowVP.View;
	OutContext.LightProjection = ShadowVP.Projection;
	OutContext.ShadowMapIndex = Index;
	OutContext.ShadowBias = Light->GetShadowBias();
	OutContext.ShadowSlopeBias = Light->GetShadowSlopeBias();

	// Cube Shadow Map 렌더링 시작
	// ArrayIndex = (LightIndex * 6) + CubeFaceIndex
	uint32 ArrayIndex = (Index * 6) + CubeFaceIndex;
	PointLightCubeShadowMap.BeginRender(RHI, ArrayIndex, OutContext.ShadowBias, OutContext.ShadowSlopeBias);

	return true;
}

void FShadowManager::EndShadowRender(D3D11RHI* RHI)
{
	SpotLightShadowMap.EndRender(RHI);
	DirectionalLightShadowMap.EndRender(RHI);
	DirectionalLightShadowMapTiers[0].EndRender(RHI);
	DirectionalLightShadowMapTiers[1].EndRender(RHI);
	DirectionalLightShadowMapTiers[2].EndRender(RHI);
	PointLightCubeShadowMap.EndRender(RHI);
}

void FShadowManager::BindShadowResources(D3D11RHI* RHI)
{
	ID3D11ShaderResourceView* SpotShadowMapSRV = SpotLightShadowMap.GetSRV();
	ID3D11ShaderResourceView* DirShadowMapSRV = DirectionalLightShadowMap.GetSRV();
	ID3D11ShaderResourceView* PointCubeShadowMapSRV = PointLightCubeShadowMap.GetSRV();

	// CSM 3-Tier Arrays
	ID3D11ShaderResourceView* CSMTierLowSRV = DirectionalLightShadowMapTiers[0].GetSRV();
	ID3D11ShaderResourceView* CSMTierMediumSRV = DirectionalLightShadowMapTiers[1].GetSRV();
	ID3D11ShaderResourceView* CSMTierHighSRV = DirectionalLightShadowMapTiers[2].GetSRV();

	// FilterType에 따라 다른 슬롯에 바인딩
	if (Config.FilterType == EShadowFilterType::NONE || Config.FilterType == EShadowFilterType::PCF)
	{
		// NONE/PCF: Depth 포맷 텍스처
		RHI->GetDeviceContext()->PSSetShaderResources(5, 1, &SpotShadowMapSRV);              // t5: SpotLight
		RHI->GetDeviceContext()->PSSetShaderResources(6, 1, &DirShadowMapSRV);               // t6: DirectionalLight (Non-CSM)
		RHI->GetDeviceContext()->PSSetShaderResources(7, 1, &PointCubeShadowMapSRV);         // t7: PointLight
		// CSM 3-Tier (t16, t17, t18)
		RHI->GetDeviceContext()->PSSetShaderResources(16, 1, &CSMTierLowSRV);                // t16: CSM Low Tier
		RHI->GetDeviceContext()->PSSetShaderResources(17, 1, &CSMTierMediumSRV);             // t17: CSM Medium Tier
		RHI->GetDeviceContext()->PSSetShaderResources(18, 1, &CSMTierHighSRV);               // t18: CSM High Tier
	}
	else // VSM, ESM, EVSM
	{
		// VSM/ESM/EVSM: Float 포맷 텍스처
		RHI->GetDeviceContext()->PSSetShaderResources(8, 1, &SpotShadowMapSRV);              // t8: SpotLight
		RHI->GetDeviceContext()->PSSetShaderResources(9, 1, &DirShadowMapSRV);               // t9: DirectionalLight (Non-CSM)
		RHI->GetDeviceContext()->PSSetShaderResources(10, 1, &PointCubeShadowMapSRV);        // t10: PointLight
		// CSM 3-Tier (t19, t20, t21)
		RHI->GetDeviceContext()->PSSetShaderResources(19, 1, &CSMTierLowSRV);                // t19: CSM Low Tier
		RHI->GetDeviceContext()->PSSetShaderResources(20, 1, &CSMTierMediumSRV);             // t20: CSM Medium Tier
		RHI->GetDeviceContext()->PSSetShaderResources(21, 1, &CSMTierHighSRV);               // t21: CSM High Tier
	}

	// Shadow Comparison Sampler를 슬롯 s2에 바인딩
	ID3D11SamplerState* ShadowSampler = RHI->GetShadowComparisonSamplerState();
	RHI->GetDeviceContext()->PSSetSamplers(2, 1, &ShadowSampler);

	// Linear Sampler를 슬롯 s3에 바인딩 (VSM/ESM/EVSM용)
	ID3D11SamplerState* LinearSampler = RHI->GetLinearSamplerState();
	RHI->GetDeviceContext()->PSSetSamplers(3, 1, &LinearSampler);
}

void FShadowManager::UnbindShadowResources(D3D11RHI* RHI)
{
	// Shadow Map 언바인딩 (모든 슬롯 해제)
	ID3D11ShaderResourceView* NullSRVs[17] = { nullptr };

	// t5~t21 모두 언바인딩
	// t5-7: SpotLight, DirectionalLight(Non-CSM), PointLight
	// t8-10: VSM/ESM/EVSM Float 버전
	// t16-18: CSM 3-Tier (NONE/PCF)
	// t19-21: CSM 3-Tier (VSM/ESM/EVSM)
	RHI->GetDeviceContext()->PSSetShaderResources(5, 17, NullSRVs);

	// Shadow Sampler 언바인딩
	ID3D11SamplerState* NullSampler = nullptr;
	RHI->GetDeviceContext()->PSSetSamplers(2, 1, &NullSampler);
}

int32 FShadowManager::GetShadowMapIndex(USpotLightComponent* Light) const
{
	return Light ? Light->GetShadowMapIndex() : -1;
}

UShader* FShadowManager::GetShadowPixelShaderForFilterType(EShadowFilterType FilterType) const
{
	switch (FilterType)
	{
	case EShadowFilterType::VSM:
		return ShadowVSM_PS;
	case EShadowFilterType::ESM:
		return ShadowESM_PS;
	case EShadowFilterType::EVSM:
		return ShadowEVSM_PS;
	case EShadowFilterType::NONE:
	case EShadowFilterType::PCF:
	default:
		return nullptr;  // Depth-only rendering (픽셀 쉐이더 불필요)
	}
}

void FShadowManager::UpdateShadowFilterBuffer(D3D11RHI* RHI)
{
	// ShadowSharpen 값으로 VSM 파라미터 동적 계산 (0.0 ~ 4.0 -> 0.0 ~ 1.0)
	float normalizedSharpen = FMath::Clamp(CachedAverageShadowSharpen / 4.0f, 0.0f, 1.0f);

	// 비선형 매핑: 중간값에서 더 부드러운 조정
	float adjustedSharpen = pow(normalizedSharpen, 0.8f);

	// VSM 파라미터 계산
	float dynamicLightBleedingReduction = FMath::Clamp(adjustedSharpen, 0.0f, 1.0f);
	float dynamicMinVariance = FMath::Lerp(0.001f, 0.00001f, normalizedSharpen);

	// 섀도우 필터링 설정을 상수 버퍼에 업데이트
	FShadowFilterBufferType ShadowFilterBuffer;
	ShadowFilterBuffer.FilterType = static_cast<uint32>(Config.FilterType);
	ShadowFilterBuffer.PCFSampleCount = static_cast<uint32>(Config.PCFSampleCount);
	ShadowFilterBuffer.PCFCustomSampleCount = Config.PCFCustomSampleCount;
	ShadowFilterBuffer.DirectionalLightResolution = static_cast<float>(Config.DirectionalLightResolution);
	ShadowFilterBuffer.SpotLightResolution = static_cast<float>(Config.SpotLightResolution);
	ShadowFilterBuffer.PointLightResolution = static_cast<float>(Config.PointLightResolution);

	// 동적으로 계산된 VSM 파라미터 사용
	ShadowFilterBuffer.VSMLightBleedingReduction = dynamicLightBleedingReduction;
	ShadowFilterBuffer.VSMMinVariance = dynamicMinVariance;

	ShadowFilterBuffer.ESMExponent = Config.ESMExponent;
	ShadowFilterBuffer.EVSMPositiveExponent = Config.EVSMPositiveExponent;
	ShadowFilterBuffer.EVSMNegativeExponent = Config.EVSMNegativeExponent;
	ShadowFilterBuffer.EVSMLightBleedingReduction = Config.EVSMLightBleedingReduction;

	RHI->SetAndUpdateConstantBuffer(ShadowFilterBuffer);
}

float FShadowManager::CalculateAverageShadowSharpen(const FShadowCastingLights& Lights) const
{
	float TotalSharpen = 0.0f;
	uint32 ValidLightCount = 0;

	// DirectionalLight
	for (UDirectionalLightComponent* Light : Lights.DirectionalLights)
	{
		if (Light && Light->IsVisible() &&
			Light->GetOwner()->IsActorVisible() &&
			Light->GetIsCastShadows())
		{
			TotalSharpen += Light->GetShadowSharpen();
			ValidLightCount++;
		}
	}

	// SpotLight
	for (USpotLightComponent* Light : Lights.SpotLights)
	{
		if (Light && Light->IsVisible() &&
			Light->GetOwner()->IsActorVisible() &&
			Light->GetIsCastShadows())
		{
			TotalSharpen += Light->GetShadowSharpen();
			ValidLightCount++;
		}
	}

	// PointLight
	for (UPointLightComponent* Light : Lights.PointLights)
	{
		if (Light && Light->IsVisible() &&
			Light->GetOwner()->IsActorVisible() &&
			Light->GetIsCastShadows())
		{
			TotalSharpen += Light->GetShadowSharpen();
			ValidLightCount++;
		}
	}

	// 평균값 반환 (라이트가 없으면 기본값 1.0f)
	return ValidLightCount > 0 ? (TotalSharpen / ValidLightCount) : 1.0f;
}
