#pragma once

#include "ShadowConfiguration.h"
#include "ShadowMap.h"
#include "ShadowStats.h"
#include "ShadowViewProjection.h"

// Forward Declarations
class D3D11RHI;
class USpotLightComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
struct FMatrix;

/**
 * @brief 쉐도우 캐스팅이 가능한 라이트들을 타입별로 그룹화한 구조체
 */
struct FShadowCastingLights
{
	const TArray<UDirectionalLightComponent*>& DirectionalLights;
	const TArray<USpotLightComponent*>& SpotLights;
	const TArray<UPointLightComponent*>& PointLights;

	FShadowCastingLights(
		const TArray<UDirectionalLightComponent*>& InDirectionalLights,
		const TArray<USpotLightComponent*>& InSpotLights,
		const TArray<UPointLightComponent*>& InPointLights)
		: DirectionalLights(InDirectionalLights)
		, SpotLights(InSpotLights)
		, PointLights(InPointLights)
	{
	}
};

// Shadow 렌더링 컨텍스트
// BeginShadowRender 호출 시 반환되는 정보
struct FShadowRenderContext
{
	FMatrix LightView;
	FMatrix LightProjection;
	int32 ShadowMapIndex;
	float ShadowBias;
	float ShadowSlopeBias;

	FShadowRenderContext()
		: LightView(FMatrix::Identity())
		, LightProjection(FMatrix::Identity())
		, ShadowMapIndex(-1)
		, ShadowBias(0.001f)
		, ShadowSlopeBias(0.0f)
	{}
};

// Shadow 시스템 관리자
// FLightManager로부터 Shadow 관련 책임을 분리
// SRP (Single Responsibility Principle) 준수
class FShadowManager
{
public:
	FShadowManager();
	~FShadowManager();

	// Shadow 시스템 초기화
	// @param RHI - D3D11 RHI 디바이스
	// @param InConfig - Shadow 설정
	void Initialize(D3D11RHI* RHI, const FShadowConfiguration& InConfig = FShadowConfiguration::GetPlatformDefault());

	// Shadow 시스템 해제
	void Release();

    /**
    * @brief 매 프레임에 활성화된 라이트 목록을 기반으로 섀도우 맵 인덱스를 할당합니다.
    * @param ShadowLights - 타입별로 그룹화된 섀도우 캐스팅 라이트 구조체
    */
    void AssignShadowMapIndices(D3D11RHI* RHI, const FShadowCastingLights& InLights);

	// Shadow 렌더링 시작 - SpotLight
	// @param Light - 렌더링할 SpotLight
	// @param OutContext - (출력) Shadow 렌더링 컨텍스트
	// @return 성공 여부
	bool BeginShadowRender(D3D11RHI* RHI, USpotLightComponent* Light, FShadowRenderContext& OutContext);

	// Shadow 렌더링 시작 - DirectionalLight
	// @param Light - 렌더링할 DirectionalLight
	// @param CameraView - 카메라의 View 행렬
	// @param CameraProjection - 카메라의 Projection 행렬
	// @param OutContext - (출력) Shadow 렌더링 컨텍스트
	// @return 성공 여부
	bool BeginShadowRender(D3D11RHI* RHI, UDirectionalLightComponent* Light,
		const FMatrix& CameraView, const FMatrix& CameraProjection, FShadowRenderContext& OutContext);

	// CSM Shadow 렌더링 시작 - DirectionalLight (Cascaded Shadow Maps)
	// @param CascadeIndex - 캐스케이드 인덱스 (0~3)
	// @param SplitNear - 이 캐스케이드의 Near 거리
	// @param SplitFar - 이 캐스케이드의 Far 거리
	bool BeginShadowRenderCSM(D3D11RHI* RHI, UDirectionalLightComponent* Light,
		const FMatrix& CameraView, const FMatrix& CameraProjection,
		int CascadeIndex, float SplitNear, float SplitFar, FShadowRenderContext& OutContext);

	// CSM Split 거리 계산 (PSSM)
	TArray<float> CalculateCSMSplits(float CameraNear, float CameraFar, int NumCascades, float Lambda) const;

	// Shadow 렌더링 시작 - PointLight (Cube Map용)
	// @param Light - 렌더링할 PointLight
	// @param CubeFaceIndex - 큐브맵 면 인덱스 (0~5: +X, -X, +Y, -Y, +Z, -Z)
	// @param ShadowVP - 해당 큐브맵 면의 View-Projection 행렬 (미리 계산된 값)
	// @param OutContext - (출력) Shadow 렌더링 컨텍스트
	// @return 성공 여부
	bool BeginShadowRenderCube(D3D11RHI* RHI, UPointLightComponent* Light, uint32 CubeFaceIndex, const FShadowViewProjection& ShadowVP, FShadowRenderContext& OutContext);

	// Shadow 렌더링 종료
	void EndShadowRender(D3D11RHI* RHI);

	// Shadow Map 텍스처를 셰이더에 바인딩
	void BindShadowResources(D3D11RHI* RHI);

	// Shadow Map 텍스처 언바인딩
	void UnbindShadowResources(D3D11RHI* RHI);

	// 섀도우 필터링 설정을 상수 버퍼에 업데이트
	void UpdateShadowFilterBuffer(D3D11RHI* RHI);

	// 디렉셔널 라이트 쉐도우 맵 해상도 변경
	void SetDirectionalLightResolution(uint32 NewResolution);

	// 스팟 라이트 쉐도우 맵 해상도 변경
	void SetSpotLightResolution(uint32 NewResolution);

	// 포인트 라이트 쉐도우 맵 해상도 변경
	void SetPointLightResolution(uint32 NewResolution);

	// 섀도우 필터 타입 변경
	void SetFilterType(EShadowFilterType NewFilterType);

	// Query 메서드들
	const FShadowConfiguration& GetShadowConfiguration() const { return Config; }
	FShadowConfiguration& GetShadowConfiguration() { return Config; }
	int32 GetShadowMapIndex(USpotLightComponent* Light) const;
	FShadowMap& GetSpotLightShadowMap() { return SpotLightShadowMap; }
	const FShadowMap& GetSpotLightShadowMap() const { return SpotLightShadowMap; }
	FShadowMap& GetDirectionalLightShadowMap() { return DirectionalLightShadowMap; }
	const FShadowMap& GetDirectionalLightShadowMap() const { return DirectionalLightShadowMap; }
	FShadowMap& GetPointLightCubeShadowMap() { return PointLightCubeShadowMap; }
	const FShadowMap& GetPointLightCubeShadowMap() const { return PointLightCubeShadowMap; }

	// CSM Tier 접근 메서드 (0=Low 512, 1=Medium 1024, 2=High 2048)
	FShadowMap& GetDirectionalLightShadowMapTier(uint32 TierIndex)
	{
		return DirectionalLightShadowMapTiers[TierIndex];
	}
	const FShadowMap& GetDirectionalLightShadowMapTier(uint32 TierIndex) const
	{
		return DirectionalLightShadowMapTiers[TierIndex];
	}

	// 필터 타입에 따라 적절한 픽셀 셰이더 반환
	class UShader* GetShadowPixelShaderForFilterType(EShadowFilterType FilterType) const;

	// CSM Split 거리 계산 (PSSM)
	//TArray<float> CalculateCSMSplits(float CameraNear, float CameraFar, int NumCascades, float Lambda) const;

private:
	// RHI 디바이스 참조
	D3D11RHI* RHIDevice;

	// Shadow 설정
	FShadowConfiguration Config;

	// 초기화 플래그
	bool bIsInitialized = false;

	// Shadow Map 리소스
	FShadowMap SpotLightShadowMap;
	FShadowMap DirectionalLightShadowMap;      // Non-CSM용 (legacy, Default ShadowMapType)
	FShadowMap DirectionalLightShadowMapTiers[3]; // CSM 3-Tier Arrays (Low, Medium, High)
	FShadowMap PointLightCubeShadowMap;       // PointLight Cube Map (6 faces per light)

	// CSM Cascade Allocation Tracking
	struct FCascadeAllocation
	{
		uint32 TierIndex;        // 0=Low, 1=Medium, 2=High
		uint32 SliceIndex;       // 해당 티어 내 슬라이스 인덱스
		uint32 Resolution;       // 실제 해상도
	};
	TArray<FCascadeAllocation> CascadeAllocations; // 전역 캐스케이드 인덱스 → 할당 정보
	uint32 TierSlotUsage[3];  // 티어별 사용 중인 슬롯 수

	// VSM/ESM/EVSM용 쉐이더 (필터 타입에 따라 사용)
	class UShader* ShadowVSM_PS;      // VSM 픽셀 쉐이더
	class UShader* ShadowESM_PS;      // ESM 픽셀 쉐이더
	class UShader* ShadowEVSM_PS;     // EVSM 픽셀 쉐이더

	// 캐시된 평균 ShadowSharpen 값 (활성 라이트들의 평균)
	float CachedAverageShadowSharpen = 1.0f;

	// 활성 라이트들의 평균 ShadowSharpen 값 계산
	float CalculateAverageShadowSharpen(const FShadowCastingLights& Lights) const;
};
