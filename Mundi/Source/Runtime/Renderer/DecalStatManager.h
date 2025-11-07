#pragma once

#include <cstdint>

/**
 * @class FDecalStatManager
 * @brief 데칼 렌더링과 관련된 통계 데이터를 수집하고 제공하는 싱글톤 클래스입니다.
 */
class FDecalStatManager
{
public:
	/**
	 * @brief FDecalStatManager의 싱글톤 인스턴스를 반환합니다.
	 */
	static FDecalStatManager& GetInstance()
	{
		static FDecalStatManager Instance;
		return Instance;
	}

	/**
	 * @brief 매 프레임 렌더링 시작 시 호출하여 프레임 단위 통계 데이터를 초기화합니다.
	 */
	void ResetFrameStats()
	{
		TotalDecalCount = 0;
		VisibleDecalCount = 0;
		AffectedMeshCount = 0;
		DecalPassTimeMS = 0.0;
	}

	// --- Getters ---

	/** @return 씬 전체의 데칼 수 */
	uint32_t GetTotalDecalCount() const { return TotalDecalCount; }

	/** @return 그릴 데칼 수 (Frustum Culling 통과) */
	uint32_t GetVisibleDecalCount() const { return VisibleDecalCount; }

	/** @return 데칼과 충돌한 메시 수 (그릴 실제 메시 수) */
	uint32_t GetAffectedMeshCount() const { return AffectedMeshCount; }

	/** @return 데칼 전체 소요 시간 (ms) */
	double GetDecalPassTimeMS() const { return DecalPassTimeMS; }

	/**
	 * @brief 가시적인 데칼 1개가 렌더링되는 데 기여한 평균 소요 시간 (ms)을 계산하여 반환합니다.
	 * @return (전체 소요 시간) / (그릴 데칼 수)
	 */
	double GetAverageTimePerDecalMS() const
	{
		if (VisibleDecalCount == 0)
		{
			return 0.0;
		}
		return DecalPassTimeMS / static_cast<double>(VisibleDecalCount);
	}

	/**
	 * @brief 데칼-메시 쌍 하나를 그리는 데(Draw Call) 걸리는 평균 시간 (ms)을 계산하여 반환합니다.
	 * @return (전체 소요 시간) / (데칼과 충돌한 메시 수)
	 */
	double GetAverageTimePerDrawMS() const // 또는 GetAverageTimePerAffectedMeshMS()
	{
		if (AffectedMeshCount == 0)
		{
			return 0.0;
		}
		return DecalPassTimeMS / static_cast<double>(AffectedMeshCount);
	}

	// --- Setters / Incrementers ---

	// NOTE: 추후 데칼 생성/소멸 시 호출하여 실제 컴포넌트 수만큼만 표시
	/** @brief 씬의 전체 데칼 수를 1 증가시킵니다 */
	void AddTotalDecalCount(uint32_t InCount) { TotalDecalCount += InCount; }

	/** @brief 그릴 데칼 수를 더합니다. (Gather 단계 이후 호출) */
	void AddVisibleDecalCount(uint32_t InCount) { VisibleDecalCount += InCount; }

	/** @brief 데칼이 메시에 그려질 때마다 호출하여 카운트를 1 증가시킵니다. */
	void IncrementAffectedMeshCount() { ++AffectedMeshCount; }

	// NOTE: 추후 Scoped Timer 같은 타이머에서 시간을 기록할 수 있도록 참조자로 반환
	/** @brief 데칼 패스의 전체 소요 시간을 직접 기록할 수 있도록 변수의 참조를 반환합니다. */
	double& GetDecalPassTimeSlot() { return DecalPassTimeMS; }

private:
	FDecalStatManager() = default;
	~FDecalStatManager() = default;

	// 싱글톤 패턴을 위해 복사 및 대입을 금지합니다.
	FDecalStatManager(const FDecalStatManager&) = delete;
	FDecalStatManager& operator=(const FDecalStatManager&) = delete;

private:
	// --- 통계 데이터 멤버 변수 ---

	// 매 프레임 초기화되는 데이터
	uint32_t TotalDecalCount = 0;
	uint32_t VisibleDecalCount = 0;
	uint32_t AffectedMeshCount = 0;
	double DecalPassTimeMS = 0.0;
};