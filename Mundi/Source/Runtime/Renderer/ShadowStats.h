#pragma once

#include <cstdint>

/**
 * @brief 쉐도우 맵 메모리 사용량 통계 구조체
 */
struct FShadowStats
{
	// 광원별 쉐도우 맵 해상도
	uint32 DirectionalLightResolution = 0;
	uint32 SpotLightResolution = 0;
	uint32 PointLightResolution = 0;

	// 광원별 쉐도우 캐스팅 라이트 수
	uint32 DirectionalLightCount = 0;
	uint32 SpotLightCount = 0;
	uint32 PointLightCount = 0;

	// 광원별 사용 중인 메모리 (바이트) - 실제 활성화된 라이트 수에 해당하는 크기
	uint64 DirectionalLightUsedBytes = 0;
	uint64 SpotLightUsedBytes = 0;
	uint64 PointLightUsedBytes = 0;

	// 총 사용 중인 메모리 (바이트)
	uint64 TotalUsedBytes = 0;

	// 최대 쉐도우 캐스팅 라이트 수
	uint32 MaxShadowCastingLights = 0;

	// CSM 사용 여부
	bool bUsingCSM = false;

	// CSM 티어별 정보 (Low, Medium, High)
	uint32 CSMTierResolutions[3] = { 0, 0, 0 };      // 각 티어의 해상도
	uint32 CSMTierCascadeCounts[3] = { 0, 0, 0 };    // 각 티어에 할당된 캐스케이드 수
	uint64 CSMTierUsedBytes[3] = { 0, 0, 0 };        // 각 티어의 사용 중인 메모리

	FShadowStats()
		: DirectionalLightResolution(0)
		, SpotLightResolution(0)
		, PointLightResolution(0)
		, DirectionalLightCount(0)
		, SpotLightCount(0)
		, PointLightCount(0)
		, DirectionalLightUsedBytes(0)
		, SpotLightUsedBytes(0)
		, PointLightUsedBytes(0)
		, TotalUsedBytes(0)
		, MaxShadowCastingLights(0)
		, bUsingCSM(false)
	{
	}

	/**
	 * @brief 모든 통계를 초기화합니다.
	 */
	void Reset()
	{
		DirectionalLightResolution = 0;
		SpotLightResolution = 0;
		PointLightResolution = 0;
		DirectionalLightCount = 0;
		SpotLightCount = 0;
		PointLightCount = 0;
		DirectionalLightUsedBytes = 0;
		SpotLightUsedBytes = 0;
		PointLightUsedBytes = 0;
		TotalUsedBytes = 0;
		bUsingCSM = false;
		for (int i = 0; i < 3; ++i)
		{
			CSMTierResolutions[i] = 0;
			CSMTierCascadeCounts[i] = 0;
			CSMTierUsedBytes[i] = 0;
		}
	}

	/**
	 * @brief 총 쉐도우 캐스팅 라이트 수를 반환합니다.
	 */
	uint32 GetTotalLightCount() const
	{
		return DirectionalLightCount + SpotLightCount + PointLightCount;
	}
};

/**
 * @brief 쉐도우 맵 통계 관리자 (싱글톤)
 */
class FShadowStatManager
{
public:
	static FShadowStatManager& GetInstance()
	{
		static FShadowStatManager Instance;
		return Instance;
	}

	/**
	 * @brief 현재 프레임의 쉐도우 통계를 반환합니다.
	 */
	const FShadowStats& GetStats() const { return Stats; }

	/**
	 * @brief 쉐도우 통계를 업데이트합니다.
	 */
	void UpdateStats(const FShadowStats& InStats)
	{
		Stats = InStats;
	}

private:
	FShadowStatManager() = default;
	~FShadowStatManager() = default;
	FShadowStatManager(const FShadowStatManager&) = delete;
	FShadowStatManager& operator=(const FShadowStatManager&) = delete;

	FShadowStats Stats;
};
