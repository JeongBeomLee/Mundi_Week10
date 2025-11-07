#pragma once

struct FVector;
struct FVector4;
struct FMatrix; // row-major, p' = p * M 가정(네 컨벤션대로)
struct FAABB; // AABB

struct FCandidateDrawable
{
    uint32_t ActorIndex;   // VisibleFlags 인덱스
    FAABB   Bound;        // 월드 AABB (Min/Max)
    FMatrix  WorldViewProj;// 행벡터 기준 WVP
    FMatrix  WorldView;    // ★ 추가: World-space * View  (여기서는 View만 주면 됨)
    float    ZNear;        // ★ 추가
    float    ZFar;         // ★ 추가
};

// 교체 (MaxZ 추가)
struct FOcclusionRect
{
    float MinX, MinY, MaxX, MaxY; // [0..1]
    float MinZ;                   // 가장 가까운
    float MaxZ;                   // 가장 먼      <<--- 추가
    uint32_t ActorIndex;
};

// 저해상도 깊이맵 + HZB(min) - CPU 전용
class FOcclusionGrid
{
public:
    void Initialize(int InWidth, int InHeight)
    {
        Width = InWidth; Height = InHeight;
        // 교체: 1.0f (Far)
        Depth.assign(size_t(Width * Height), 1.0f);
        BuildLevels.clear();
    }
    void Clear()
    {
        // Clear 도 동일하게 1.0f로
        std::fill(Depth.begin(), Depth.end(), 1.0f);
        BuildLevels.clear();
    }

    /*
        왜 R.MaxZ를 쓰는데 min 누적을 하나?

        오클루더 AABB가 두껍거나 경계 근사 때문에 일부 픽셀이 원래보다 더 멀게 잡힐 수 있다.

        레벨0은 per-pixel 가장 가까운 오클루더가 들어가야 하므로, 여러 오클루더가 겹칠 때 **min(current, MaxZ)**로 더 가까운 값으로만 낮춘다.

        이렇게 해야 상위 레벨의 MAX 피라미드가 보수적이 됨(=오버컬링 방지).
    */

    void RasterizeRectDepthMin(int MinPX, int MinPY, int MaxPX, int MaxPY, float MaxZ)
    {
        MinPX = std::max(0, MinPX); MinPY = std::max(0, MinPY);
        MaxPX = std::min(Width - 1, MaxPX); MaxPY = std::min(Height - 1, MaxPY);
        for (int y = MinPY; y <= MaxPY; ++y)
        {
            float* Row = &Depth[size_t(y) * Width];
            for (int x = MinPX; x <= MaxPX; ++x)
            {
                Row[x] = std::min(Row[x], MaxZ); // 가장 가까운 깊이로 갱신
            }
        }
    }

    void BuildHZB()
    {
        BuildLevels.clear();
        BuildLevels.push_back(Depth); // level 0

        int W = Width, H = Height;
        while (W > 1 || H > 1)
        {
            int NW = std::max(1, W >> 1);
            int NH = std::max(1, H >> 1);
            TArray<float> Next(size_t(NW * NH), +FLT_MAX);

            for (int y = 0; y < NH; ++y)
                for (int x = 0; x < NW; ++x)
                {
                    int sx = x * 2, sy = y * 2;
                    float a = SampleSafe(BuildLevels.back(), W, H, sx + 0, sy + 0);
                    float b = SampleSafe(BuildLevels.back(), W, H, sx + 1, sy + 0);
                    float c = SampleSafe(BuildLevels.back(), W, H, sx + 0, sy + 1);
                    float d = SampleSafe(BuildLevels.back(), W, H, sx + 1, sy + 1);
                    Next[size_t(y) * NW + x] = std::max(std::max(a, b), std::max(c, d));
                }
            BuildLevels.push_back(std::move(Next));
            W = NW; H = NH;
        }
    }

    int ChooseMip(float RectW01, float RectH01) const
    {
        float pxW = RectW01 * Width;
        float pxH = RectH01 * Height;
        float s = std::max(pxW, pxH);
        int mip = int(std::floor(std::log2(std::max(1.0f, s))));
        mip = std::max(0, std::min(mip, int(BuildLevels.size() - 1)));
        return mip;
    }

    float SampleMaxRect(float MinX01, float MinY01, float MaxX01, float MaxY01, int Mip) const
    {
        const TArray<float>& L = BuildLevels[size_t(Mip)];
        int W = std::max(1, Width >> Mip);
        int H = std::max(1, Height >> Mip);

        auto ToPx = [&](float u, float v)->std::pair<int, int> {
            int x = int(u * W + 0.5f);
            int y = int(v * H + 0.5f);
            x = std::max(0, std::min(W - 1, x));
            y = std::max(0, std::min(H - 1, y));
            return { x,y };
            };
        auto Smp = [&](float u, float v)->float {
            auto [x, y] = ToPx(u, v);
            return L[size_t(y) * W + x];
            };

        float cx = 0.5f * (MinX01 + MaxX01);
        float cy = 0.5f * (MinY01 + MaxY01);

        float s0 = Smp(cx, cy);
        float s1 = Smp(MinX01, MinY01);
        float s2 = Smp(MaxX01, MinY01);
        float s3 = Smp(MinX01, MaxY01);
        float s4 = Smp(MaxX01, MaxY01);

        return std::max(s0, std::max(s1, std::max(s2, std::max(s3, s4)))); // ★ MAX
    }
    // FOcclusionGrid 내부에 추가
    float SampleMaxRectAdaptive(float MinX01, float MinY01, float MaxX01, float MaxY01, int Mip) const
    {
        const TArray<float>& L = BuildLevels[size_t(Mip)];
        const int W = std::max(1, Width >> Mip);
        const int H = std::max(1, Height >> Mip);

        auto ToPx = [&](float u, float v)->std::pair<int, int> {
            int x = int(u * W + 0.5f);
            int y = int(v * H + 0.5f);
            x = std::max(0, std::min(W - 1, x));
            y = std::max(0, std::min(H - 1, y));
            return { x,y };
            };
        auto Smp = [&](float u, float v)->float {
            auto [x, y] = ToPx(u, v);
            return L[size_t(y) * W + x];
            };

        // 샘플 밀도: 화면 픽셀 크기에 비례 (최대 5x5)
        const int grid = ((MaxX01 - MinX01) * Width + (MaxY01 - MinY01) * Height > 80) ? 5 :
            ((MaxX01 - MinX01) * Width + (MaxY01 - MinY01) * Height > 30) ? 4 : 3;

        float best = 0.0f; // MAX 피라미드이므로 최댓값을 모음
        for (int j = 0; j < grid; j++)
        {
            float v = std::lerp(MinY01, MaxY01, (grid == 1) ? 0.5f : float(j) / (grid - 1));
            for (int i = 0; i < grid; i++)
            {
                float u = std::lerp(MinX01, MaxX01, (grid == 1) ? 0.5f : float(i) / (grid - 1));
                best = std::max(best, Smp(u, v));
            }
        }
        return best;
    }
    // FOcclusionGrid 내부에 추가 (레벨0 정밀 검사)
    bool FullyOccludedAtLevel0(float MinX01, float MinY01, float MaxX01, float MaxY01, float MinZ, float eps2) const
    {
        const TArray<float>& L0 = BuildLevels[0];
        const int W = Width, H = Height;

        int x0 = std::max(0, std::min(W - 1, int(MinX01 * W)));
        int y0 = std::max(0, std::min(H - 1, int(MinY01 * H)));
        int x1 = std::max(0, std::min(W - 1, int(MaxX01 * W)));
        int y1 = std::max(0, std::min(H - 1, int(MaxY01 * H)));

        // 너무 큰 박스는 샘플링 간격을 둠 (성능)
        const int maxScan = 128 * 128; // 임계 픽셀수
        int w = std::max(1, x1 - x0 + 1);
        int h = std::max(1, y1 - y0 + 1);
        int step = (w * h > maxScan) ? 2 : 1;

        for (int y = y0; y <= y1; y += step)
        {
            const float* row = &L0[size_t(y) * W];
            for (int x = x0; x <= x1; x += step)
            {
                float z = row[x];           // 레벨0의 MAX-기반 값(=멀리 기록한 것들의 min 축약 결과 아님!)
                // 주의: 우리는 base에 'MaxZ를 min으로 누적'시켰고, HZB는 최대화했음.
                // 레벨0에서도 보수성 유지: z + eps2 <= MinZ 일 때만 occluded 픽셀로 간주.
                if (z + eps2 > MinZ)
                    return false; // 한 픽셀이라도 덮여있지 않음 → 전체 가림 실패
            }
        }
        return true; // 전부 덮임
    }
    int GetWidth()  const { return Width; }
    int GetHeight() const { return Height; }


private:
    static float SampleSafe(const TArray<float>& L, int W, int H, int X, int Y)
    {
        X = std::max(0, std::min(W - 1, X));
        Y = std::max(0, std::min(H - 1, Y));
        return L[size_t(Y) * W + X];
    }

private:
    int Width = 0, Height = 0;
    TArray<float> Depth;                     // level 0
    TArray<TArray<float>> BuildLevels;  // [0..N-1], min chain


};

// CPU 오클루전 매니저
class FOcclusionCullingManagerCPU
{
public:
    void Initialize(int GridW, int GridH) { Grid.Initialize(GridW, GridH); }
    void Shutdown() {}

    // 1) 오클루더로 저해상도 Depth 채우기
    void BuildOccluderDepth(const TArray<FCandidateDrawable>& Occluders, int ViewW, int ViewH);

    // 2) CPU HZB
    void BuildHZB() { Grid.BuildHZB(); }

    // 3) 후보 가시성 판정
    void TestOcclusion(const TArray<FCandidateDrawable>& Candidates, int ViewW, int ViewH, TArray<uint8_t>& OutVisibleFlags);

    const FOcclusionGrid& GetGrid() const { return Grid; }

private:
    // AABB(Min/Max) → 화면 사각형 + MinZ (★이제 MinZ는 '선형 깊이 0..1')
    static bool ComputeRectAndMinZ(const FCandidateDrawable& D, int ViewW, int ViewH, FOcclusionRect& OutRect);

    // 행벡터: Out = In(1x4) * M(4x4)
    static inline void MulPointRow(const float In[4], const FMatrix& M, float Out[4])
    {
        Out[0] = In[0] * M.M[0][0] + In[1] * M.M[1][0] + In[2] * M.M[2][0] + In[3] * M.M[3][0];
        Out[1] = In[0] * M.M[0][1] + In[1] * M.M[1][1] + In[2] * M.M[2][1] + In[3] * M.M[3][1];
        Out[2] = In[0] * M.M[0][2] + In[1] * M.M[1][2] + In[2] * M.M[2][2] + In[3] * M.M[3][2];
        Out[3] = In[0] * M.M[0][3] + In[1] * M.M[1][3] + In[2] * M.M[2][3] + In[3] * M.M[3][3];
    }

private:
    FOcclusionGrid Grid;
    TArray<uint8_t> VisibleStreak;   // 연속 보임 프레임 수
    TArray<uint8_t> OccludedStreak;  // 연속 가림 프레임 수
    TArray<uint8_t> LastState;       // 0=occluded, 1=visible
};