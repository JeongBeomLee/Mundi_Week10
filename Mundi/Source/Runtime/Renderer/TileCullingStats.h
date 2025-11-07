#pragma once
#include "UEContainer.h"

// 타일 기반 라이트 컬링 통계
// 성능 메트릭과 컬링 효율성을 추적
struct FTileCullingStats
{
	// 타일 그리드 차원
	uint32 TileCountX = 0;
	uint32 TileCountY = 0;
	uint32 TotalTileCount = 0;

	// 라이트 개수
	uint32 TotalPointLights = 0;
	uint32 TotalSpotLights = 0;
	uint32 TotalLights = 0;

	// 타일당 라이트 통계
	uint32 MinLightsPerTile = 0;
	uint32 MaxLightsPerTile = 0;
	float AvgLightsPerTile = 0.0f;

	// 컬링 효율성 메트릭
	float CullingEfficiency = 0.0f; // 컬링된 라이트 비율 (%)
	uint32 TotalLightTests = 0;     // 전체 라이트-타일 테스트 수
	uint32 TotalLightsPassed = 0;   // 컬링을 통과한 라이트 수

	// 성능 메트릭
	float ComputeShaderTimeMS = 0.0f;
	uint32 LightIndexBufferSizeBytes = 0;

	// 시각화 모드
	enum class EVisualizationMode : uint8
	{
		None = 0,
		Heatmap = 1,      // 히트맵 (타일별 라이트 개수)
		Grid = 2,         // 그리드 라인
		LightBounds = 4,  // 라이트 영향 범위
		All = Heatmap | Grid | LightBounds
	};

	EVisualizationMode VisualizationMode = EVisualizationMode::None;

	// 모든 통계를 0으로 리셋
	void Reset()
	{
		TileCountX = 0;
		TileCountY = 0;
		TotalTileCount = 0;
		TotalPointLights = 0;
		TotalSpotLights = 0;
		TotalLights = 0;
		MinLightsPerTile = 0;
		MaxLightsPerTile = 0;
		AvgLightsPerTile = 0.0f;
		CullingEfficiency = 0.0f;
		TotalLightTests = 0;
		TotalLightsPassed = 0;
		ComputeShaderTimeMS = 0.0f;
		LightIndexBufferSizeBytes = 0;
	}

	// 파생 통계 계산
	void CalculateStats()
	{
		TotalLights = TotalPointLights + TotalSpotLights;
		TotalTileCount = TileCountX * TileCountY;

		if (TotalTileCount > 0)
		{
			AvgLightsPerTile = static_cast<float>(TotalLightsPassed) / static_cast<float>(TotalTileCount);
		}

		if (TotalLightTests > 0)
		{
			uint32 LightsCulled = TotalLightTests - TotalLightsPassed;
			CullingEfficiency = (static_cast<float>(LightsCulled) / static_cast<float>(TotalLightTests)) * 100.0f;
		}
	}
};

inline FTileCullingStats::EVisualizationMode operator|(FTileCullingStats::EVisualizationMode a, FTileCullingStats::EVisualizationMode b)
{
	return static_cast<FTileCullingStats::EVisualizationMode>(static_cast<uint8>(a) | static_cast<uint8>(b));
}

inline FTileCullingStats::EVisualizationMode operator&(FTileCullingStats::EVisualizationMode a, FTileCullingStats::EVisualizationMode b)
{
	return static_cast<FTileCullingStats::EVisualizationMode>(static_cast<uint8>(a) & static_cast<uint8>(b));
}

inline bool operator!=(FTileCullingStats::EVisualizationMode a, int b)
{
	return static_cast<uint8>(a) != b;
}

// 타일 컬링 통계 전역 매니저 (싱글톤)
// UStatsOverlayD2D에서 접근할 수 있도록 전역 통계 제공
class FTileCullingStatManager
{
public:
	static FTileCullingStatManager& GetInstance()
	{
		static FTileCullingStatManager Instance;
		return Instance;
	}

	// 통계 업데이트
	void UpdateStats(const FTileCullingStats& InStats)
	{
		CurrentStats = InStats;
	}

	// 통계 조회
	const FTileCullingStats& GetStats() const
	{
		return CurrentStats;
	}

	// 통계 리셋
	void ResetStats()
	{
		CurrentStats.Reset();
	}

private:
	FTileCullingStatManager() = default;
	~FTileCullingStatManager() = default;
	FTileCullingStatManager(const FTileCullingStatManager&) = delete;
	FTileCullingStatManager& operator=(const FTileCullingStatManager&) = delete;

	FTileCullingStats CurrentStats;
};
