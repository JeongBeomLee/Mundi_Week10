#pragma once

// Shadow 품질 프리셋 레벨
enum class EShadowQuality : uint8
{
	Low,        // 512x512, 4 lights
	Medium,     // 1024x1024, 10 lights
	High,       // 2048x2048, 16 lights
	Custom      // 사용자 정의
};

// 섀도우 필터링 타입
enum class EShadowFilterType : uint8
{
	NONE,       // 필터링 없음 (하드웨어 기본 비교 샘플링)
	PCF,        // Percentage Closer Filtering (소프트웨어)
	VSM,        // Variance Shadow Maps
	ESM,        // Exponential Shadow Maps
	EVSM        // Exponential Variance Shadow Maps
};

// PCF 샘플 수
enum class EPCFSampleCount : uint8
{
	PCF_3x3 = 3,    // 9 샘플
	PCF_4x4 = 4,    // 16 샘플
	PCF_5x5 = 5,    // 25 샘플
	Custom = 0      // 사용자 정의
};

// CSM Resolution Tier (3-Tier System)
enum class ECSMResolutionTier : uint8
{
	Low = 0,    // 256 ~ 512 (먼 캐스케이드)
	Medium = 1, // 1024 ~ 2048 (중간 캐스케이드)
	High = 2    // 4096+ (가까운 캐스케이드)
};

// CSM Tier Configuration
struct FCSMTierConfig
{
	uint32 Resolution;   // 이 티어의 텍스처 해상도
	uint32 MaxSlices;    // 최대 캐스케이드 슬롯 수

	FCSMTierConfig(uint32 InResolution = 1024, uint32 InMaxSlices = 16)
		: Resolution(InResolution), MaxSlices(InMaxSlices)
	{}
};

// Shadow 시스템 설정
// 모든 shadow 관련 설정을 중앙화하여 magic number 제거
struct FShadowConfiguration
{
	EShadowQuality Quality = EShadowQuality::Medium;

	// 라이트 타입별 쉐도우 맵 해상도
	uint32 DirectionalLightResolution = 4096;  // Non-CSM용 (legacy)
	uint32 SpotLightResolution = 1024;
	uint32 PointLightResolution = 512;

	// CSM 3-Tier 해상도 설정 (DirectionalLight CSM 전용)
	FCSMTierConfig CSMTierLow = FCSMTierConfig(512, 16);      // Tier 0: 256~512
	FCSMTierConfig CSMTierMedium = FCSMTierConfig(2048, 12);  // Tier 1: 1024~2048
	FCSMTierConfig CSMTierHigh = FCSMTierConfig(4096, 8);     // Tier 2: 4096+

	// 광원 타입별 최대 쉐도우 캐스팅 라이트 수
	uint32 MaxDirectionalLights = 1;
	uint32 MaxSpotLights = 100;
	uint32 MaxPointLights = 50;

	// 전체 쉐도우 캐스팅 라이트 수 (통계/검증용)
	uint32 MaxShadowCastingLights = 16;

	// 섀도우 필터링 설정
	EShadowFilterType FilterType = EShadowFilterType::PCF;   // 필터링 타입

	// PCF 설정
	EPCFSampleCount PCFSampleCount = EPCFSampleCount::PCF_3x3;  // PCF 샘플 수
	uint32 PCFCustomSampleCount = 3;                             // Custom 샘플 수 (PCFSampleCount가 Custom일 때)

	// VSM 설정
	float VSMLightBleedingReduction = 0.3f;   // Light bleeding 감소 (0.0 ~ 1.0)
	float VSMMinVariance = 0.00001f;          // 최소 분산 값

	// ESM 설정
	float ESMExponent = 80.0f;                // Exponential 계수 (40.0 ~ 100.0 권장)

	// EVSM 설정
	float EVSMPositiveExponent = 40.0f;       // Positive exponent
	float EVSMNegativeExponent = 40.0f;       // Negative exponent
	float EVSMLightBleedingReduction = 0.3f;  // Light bleeding 감소

	// 품질 프리셋으로부터 설정 로드
	static FShadowConfiguration FromQuality(EShadowQuality InQuality);

	// 현재 플랫폼의 기본 설정 가져오기
	static FShadowConfiguration GetPlatformDefault();

	// 설정 값 유효성 검증
	bool IsValid() const;
};
