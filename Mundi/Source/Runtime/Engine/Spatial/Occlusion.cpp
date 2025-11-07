#include "pch.h"
#include "Occlusion.h"
#include "Frustum.h"

// NDC Z가 [-1..1]인 프로젝션이면 아래 변환을 켜세요.
// static inline float To01(float z_ndc) { return z_ndc * 0.5f + 0.5f; }
static inline void Clamp01(float& v) { v = std::max(0.0f, std::min(1.0f, v)); }

// 헬퍼: 뷰공간 Z → [0..1] 선형
static inline float LinearizeZ01(float zView, float zNear, float zFar) {
	// LH 기준, 카메라 앞 z가 + 방향일 때
	float z = (zView - zNear) / (zFar - zNear);
	return std::max(0.f, std::min(1.f, z));
}

// FBound(Min/Max) → 8코너
static inline void MakeAabbCornersMinMax(const FAABB& B, FVector Corners[8])
{
	const FVector& mn = B.Min;
	const FVector& mx = B.Max;
	Corners[0] = { mn.X, mn.Y, mn.Z };
	Corners[1] = { mx.X, mn.Y, mn.Z };
	Corners[2] = { mn.X, mx.Y, mn.Z };
	Corners[3] = { mx.X, mx.Y, mn.Z };
	Corners[4] = { mn.X, mn.Y, mx.Z };
	Corners[5] = { mx.X, mn.Y, mx.Z };
	Corners[6] = { mn.X, mx.Y, mx.Z };
	Corners[7] = { mx.X, mx.Y, mx.Z };
}

bool FOcclusionCullingManagerCPU::ComputeRectAndMinZ(
	const FCandidateDrawable& D, int /*ViewW*/, int /*ViewH*/, FOcclusionRect& OutR)
{
	FVector C[8];
	MakeAabbCornersMinMax(D.Bound, C);

	float MinX = +1e9f, MinY = +1e9f, MaxX = -1e9f, MaxY = -1e9f;
	float MinZLin = +1e9f, MaxZLin = -1e9f; // ★ 선형 깊이(0..1)

	int used = 0;
	for (int i = 0; i < 8; i++)
	{
		const float p[4] = { C[i].X, C[i].Y, C[i].Z, 1.0f };

		// 1) 화면 사각형용: WVP → NDC
		float c[4];
		MulPointRow(p, D.WorldViewProj, c);
		if (c[3] <= 0.0f) continue;

		const float invW = 1.0f / c[3];
		const float ndcX = c[0] * invW;  // -1..1
		const float ndcY = c[1] * invW;  // -1..1
		// const float ndcZ = c[2] * invW; // 깊이는 쓰지 않음

		const float u = 0.5f * (ndcX + 1.0f);
		const float v = 0.5f * (ndcY + 1.0f);

		MinX = std::min(MinX, u); MinY = std::min(MinY, v);
		MaxX = std::max(MaxX, u); MaxY = std::max(MaxY, v);

		// 2) 깊이: WorldView로 뷰 공간 z → 선형 0..1
		float v4[4];
		MulPointRow(p, D.WorldView, v4);     // p_world * (World*View) == p_world * View (월드좌표니까 View만 와도 OK)
		const float zView = v4[2];          // LH: +Z 앞
		const float zLin01 = LinearizeZ01(zView, D.ZNear, D.ZFar);

		MinZLin = std::min(MinZLin, zLin01);
		MaxZLin = std::max(MaxZLin, zLin01);

		used++;
	}

	if (used == 0) return false;
	if (MaxX < 0 || MaxY < 0 || MinX > 1 || MinY > 1) return false;

	Clamp01(MinX); Clamp01(MinY); Clamp01(MaxX); Clamp01(MaxY);
	Clamp01(MinZLin); Clamp01(MaxZLin);

	OutR.MinX = MinX; OutR.MinY = MinY; OutR.MaxX = MaxX; OutR.MaxY = MaxY;
	OutR.MinZ = MinZLin;   // ★ 선형 깊이
	OutR.MaxZ = MaxZLin;
	OutR.ActorIndex = D.ActorIndex;
	return true;
}
void FOcclusionCullingManagerCPU::BuildOccluderDepth(
	const TArray<FCandidateDrawable>& Occluders, int ViewW, int ViewH)
{
	Grid.Clear();

	const int GW = Grid.GetWidth();
	const int GH = Grid.GetHeight();

	// --- 튜닝 파라미터 ---
	const float ErodeScale = 0.5f;     // 0.5배 축소
	const int   Dilate = 1;        // 경계 1px 팽창
	const int   MinPxEdge = 8;        // 너무 작은 사각형은 erosion 스킵
	const float MaxCover = 0.6f;     // 화면의 60% 이상 덮으면 erosion 스킵

	for (const auto& D : Occluders)
	{
		FOcclusionRect R;
		if (!ComputeRectAndMinZ(D, ViewW, ViewH, R))
			continue;

		int minPX = (int)std::floor(R.MinX * GW);
		int minPY = (int)std::floor(R.MinY * GH);
		int maxPX = (int)std::ceil(R.MaxX * GW) - 1;
		int maxPY = (int)std::ceil(R.MaxY * GH) - 1;

		// --- 화면 사각형 erosion(오클루더 영향 완화) ---
		int w = std::max(0, maxPX - minPX + 1);
		int h = std::max(0, maxPY - minPY + 1);
		const bool bigCover =
			(w >= int(MaxCover * GW)) || (h >= int(MaxCover * GH));
		if (ErodeScale < 1.0f && w >= MinPxEdge && h >= MinPxEdge && !bigCover)
		{
			const float cx = 0.5f * float(minPX + maxPX);
			const float cy = 0.5f * float(minPY + maxPY);
			const float hw = 0.5f * float(w) * ErodeScale;
			const float hh = 0.5f * float(h) * ErodeScale;

			minPX = int(std::floor(cx - hw));
			maxPX = int(std::ceil(cx + hw));
			minPY = int(std::floor(cy - hh));
			maxPY = int(std::ceil(cy + hh));
		}


		// 화면 경계 클램프
		minPX = std::max(0, std::min(GW - 1, minPX));
		minPY = std::max(0, std::min(GH - 1, minPY));
		maxPX = std::max(0, std::min(GW - 1, maxPX));
		maxPY = std::max(0, std::min(GH - 1, maxPY));
		if (minPX > maxPX || minPY > maxPY) continue;

		Grid.RasterizeRectDepthMin(minPX, minPY, maxPX, maxPY, R.MaxZ);
	}
}

// 2) 후보 가시성 판정(HZB 샘플)
void FOcclusionCullingManagerCPU::TestOcclusion(const TArray<FCandidateDrawable>& Candidates, int ViewW, int ViewH, TArray<uint8_t>& OutVisibleFlags)
{
	const float eps = 2e-3f;  // 1차 바이어스
	const float eps2 = 2 * eps;  // 레벨0 재검증 바이어스(조금 더 큼)
	// --- 크기 보장 ---
	uint32_t maxId = 0;
	for (auto& c : Candidates) maxId = std::max(maxId, c.ActorIndex);

	if (OutVisibleFlags.size() <= maxId) OutVisibleFlags.resize(maxId + 1, 1);
	if (VisibleStreak.size() <= maxId) VisibleStreak.resize(maxId + 1, 0);
	if (OccludedStreak.size() <= maxId) OccludedStreak.resize(maxId + 1, 0);
	if (LastState.size() <= maxId) LastState.resize(maxId + 1, 1); // 초기=보임

	for (const auto& D : Candidates)
	{
		uint32_t id = D.ActorIndex;

		FOcclusionRect R;
		if (!ComputeRectAndMinZ(D, ViewW, ViewH, R))
		{
			// 화면 밖(또는 w<=0 코너만) → 가려짐으로 처리
			OutVisibleFlags[id] = 0;
			// streak 업데이트 (선택): occluded 누적
			OccludedStreak[id] = std::min<uint8_t>(255, OccludedStreak[id] + 1);
			VisibleStreak[id] = 0;
			LastState[id] = 0;
			continue;
		}

		const float rw = std::max(0.0f, R.MaxX - R.MinX);
		const float rh = std::max(0.0f, R.MaxY - R.MinY);
		const float pxW = rw * GetGrid().GetWidth();
		const float pxH = rh * GetGrid().GetHeight();

		// --- 작은 사각형 가드: 한 변이라도 2px 미만이면 컬링하지 않음 ---
		if (std::min(pxW, pxH) < 2.0f)
		{
			OutVisibleFlags[id] = 1;
			VisibleStreak[id] = std::min<uint8_t>(255, VisibleStreak[id] + 1);
			OccludedStreak[id] = 0;
			LastState[id] = 1;
			continue;
		}

		// --- 보수적 mip 선택 ---
		int mip = std::max(0, Grid.ChooseMip(rw, rh) - 1);
		if (pxW < 48.0f || pxH < 48.0f)
			mip = std::max(0, mip - 1);


		// --- MAX HZB 적응형 샘플 ---
		const float hzbMax = Grid.SampleMaxRectAdaptive(R.MinX, R.MinY, R.MaxX, R.MaxY, mip);

		bool occluded = ((hzbMax + eps) <= R.MinZ);

		// --- 레벨0 정밀 재검증(occluded일 때만) ---
		if (occluded)
		{
			if (!Grid.FullyOccludedAtLevel0(R.MinX, R.MinY, R.MaxX, R.MaxY, R.MinZ, eps2))
				occluded = false;
		}

		// --- 양방향 히스테리시스(2~3프레임 연속일 때만 상태 전환) ---
		const int thresh = 2; // 2~3 추천

		if (occluded)
		{
			OccludedStreak[id] = std::min<uint8_t>(255, OccludedStreak[id] + 1);
			VisibleStreak[id] = 0;

			// 직전이 보임이면, thresh 미만 동안은 보임 유지
			if (LastState[id] == 1 && OccludedStreak[id] < thresh)
				occluded = false;
		}
		else
		{
			VisibleStreak[id] = std::min<uint8_t>(255, VisibleStreak[id] + 1);
			OccludedStreak[id] = 0;

			// 직전이 가려짐이면, thresh 미만 동안은 가려짐 유지
			if (LastState[id] == 0 && VisibleStreak[id] < thresh)
				occluded = true;
		}

		LastState[id] = occluded ? 0 : 1;
		OutVisibleFlags[id] = occluded ? 0 : 1;
	}
}