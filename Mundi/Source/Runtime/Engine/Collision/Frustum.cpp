#include "pch.h"

#include "Vector.h"
#include "Frustum.h"
#include "CameraComponent.h"
#include <immintrin.h> // For SSE, AVX, FMA instructions

inline float     Dot3(const FVector4& A, const FVector4& B)
{
    // Use SSE4.1's dot product instruction. It's faster than manual shuffling.
    // 0x71 = (0111 0001b)
    //   - Low 4 bits (0111): Multiply and sum the first three components (X, Y, Z).
    //   - High 4 bits (0001): Store the result in the first component of the destination.
    __m128 dp = _mm_dp_ps(A.SimdData, B.SimdData, 0x71);
    float result;
    _mm_store_ss(&result, dp);
    return result;
}
inline FVector4  Cross3(const FVector4& A, const FVector4& B)
{
    // Use shuffles and FMA (Fused Multiply-Add) for an efficient cross product.
    // Formula:
    // C.x = A.y * B.z - A.z * B.y
    // C.y = A.z * B.x - A.x * B.z
    // C.z = A.x * B.y - A.y * B.x

    // Shuffle A and B to align components for multiplication
    __m128 a_yzx = _mm_shuffle_ps(A.SimdData, A.SimdData, _MM_SHUFFLE(3, 0, 2, 1)); // (Ay, Az, Ax, Aw)
    __m128 b_zxy = _mm_shuffle_ps(B.SimdData, B.SimdData, _MM_SHUFFLE(3, 1, 0, 2)); // (Bz, Bx, By, Bw)
    __m128 a_zxy = _mm_shuffle_ps(A.SimdData, A.SimdData, _MM_SHUFFLE(3, 1, 0, 2)); // (Az, Ax, Ay, Aw)
    __m128 b_yzx = _mm_shuffle_ps(B.SimdData, B.SimdData, _MM_SHUFFLE(3, 0, 2, 1)); // (By, Bz, Bx, Bw)

    // term1 = (Ay*Bz, Az*Bx, Ax*By, ...)
    __m128 term1 = _mm_mul_ps(a_yzx, b_zxy);

    // result = term1 - (a_zxy * b_yzx) using FMA
    // _mm_fnmadd_ps(a, b, c) computes c - (a * b)
    __m128 sub = _mm_fnmadd_ps(a_zxy, b_yzx, term1);

    FVector4 result(sub);
    result.W = 0.0f;
    return result;
}
inline float     Length3(const FVector4& V)
{
    float dot = Dot3(V, V);
    return std::sqrt(dot);
}
inline FVector4  Normalize3(const FVector4& V)
{
    float len = Length3(V);
    if (len > 0.0f)
    {
        __m128 v = _mm_load_ps(&V.X);
        __m128 len_v = _mm_set1_ps(len);
        __m128 result_v = _mm_div_ps(v, len_v);
        FVector4 result;
        _mm_store_ps(&result.X, result_v);
        result.W = 0.0f;
        return result;
    }
    return FVector4(0, 0, 0, 0);
}
// ------------------------------------------------------------
// 절두체(Frustum)/평면 유틸
//  - 평면 식:  dot(N, X) - D = 0
//  - 본 코드는 "절두체 안쪽이 >= 0" 이 되도록 법선을 맞춘다(가시성 테스트에 유리).
//  - 좌표계가 LH여도 외적(Cross)은 RH 정의를 사용한다.
//    => 법선의 "방향(안/밖)"은 외적의 피연산자 "순서"로 결정한다.
// ------------------------------------------------------------

namespace
{
    // 점(Point)과 법선(Normal)로 평면 생성
    //  - 입력 Normal은 정규화되지 않았을 수 있으므로, 반드시 정규화
    //  - Distance = dot(Normal, Point)  (평면 상의 임의의 점과 법선의 내적)
    FPlane MakePlane(const FVector4& Point, const FVector4& Normal)
    {
        const FVector4 UnitNormal = Normalize3(Normal); // 길이 1로 보정
        return FPlane
        {
            UnitNormal,
            Dot3(UnitNormal, Point)
        };
    }
    // (선택) 모든 평면을 "안쪽을 향하도록" 보정
    //  - 수치 불안정, 축/부호 실수, 미러링(det<0) 등에도 항상 안쪽 ≥ 0을 보장
    /*
    inline void EnsureFacingInwards(FPlane& P, const FVector& InsidePoint)
    {
        if (FVector::Dot(P.Normal, InsidePoint) - P.Distance < 0.0f)
        {
            P.Normal *= -1.0f;
            P.Distance *= -1.0f;
        }
    }
    */
}

FFrustum CreateFrustumFromCamera(const UCameraComponent& Camera, float OverrideAspect)
{
    // 카메라 파라미터
    const float NearClip = Camera.GetNearClip();
    const float FarClip = Camera.GetFarClip();
    const float Aspect = OverrideAspect > 0.f ? OverrideAspect : Camera.GetAspectRatio();
    const float FovRad = DegreesToRadians(Camera.GetFOV());

    // 카메라 기준 좌표축(월드 공간)
    // Forward=+X, Right=+Y, Up=+Z
    const FVector4 Origin = FVector4::FromPoint(Camera.GetWorldLocation());
    const FVector4 Forward = FVector4::FromDirection(Camera.GetForward());
    const FVector4 Right = FVector4::FromDirection(Camera.GetRight());
    const FVector4 Up = FVector4::FromDirection(Camera.GetUp());

    // Far 평면에서의 절반 높이/너비
    const float HalfVSide = FarClip * tanf(FovRad * 0.5f); // 세로(Vertical) 반폭
    const float HalfHSide = HalfVSide * Aspect;            // 가로(Horizontal) 반폭
    const FVector4 FrontMultFar = FVector4(Forward.X * FarClip, Forward.Y * FarClip, Forward.Z * FarClip, 0.0f);        // Far까지의 전방 벡터

    FFrustum Result;

    // ------------------------------------------------------------
    // Near / Far
    //  - 평면 법선이 "프러스텀 내부"를 향하도록 잡는다.
    //    Near: +Forward,  Far: -Forward
    // ------------------------------------------------------------
    Result.NearFace = MakePlane(Origin + Forward * NearClip, Forward);
    Result.FarFace = MakePlane(Origin + FrontMultFar, Forward * -1.0f);


    // ------------------------------------------------------------
    // 측면 평면(Left/Right/Top/Bottom)
    //  - 외적은 RH 정의를 사용한다.
    //  - 피연산자 순서를 적절히 선택해 "내부"를 바라보는 법선을 만든다.
    // 
    //	- 현재 좌표축이 LH(Left-Handed) 기준이므로, 
    //  - Cross 연산 순서 또한 왼손으로 A 먼저, 그 후 B를 감았을 때 나오는 방향이 법선이 된다.
    // 
    //  - Right  : Cross( F*Far + R*HalfH,  U )  → (+X, -Y, 0)
    //  - Left   : Cross( U, F*Far - R*HalfH ) → (+X, +Y, 0)
    //  - Top    : Cross( R, F*Far + U*HalfV ) → (+X, 0, -Z)
    //  - Bottom : Cross( F*Far - U*HalfV,   R )  → (+X, 0, +Z)
    // ------------------------------------------------------------
    Result.RightFace = MakePlane(Origin, Cross3(FrontMultFar + Right * HalfHSide, Up));
    Result.LeftFace = MakePlane(Origin, Cross3(Up, FrontMultFar - Right * HalfHSide));
    Result.TopFace = MakePlane(Origin, Cross3(Right, FrontMultFar + Up * HalfVSide));
    Result.BottomFace = MakePlane(Origin, Cross3(FrontMultFar - Up * HalfVSide, Right));
    return Result;
}

// ------------------------------------------------------------
// AABB vs 프러스텀 판정
//  - 각 평면에 대해: 중심의 부호 + 박스의 "프로젝션 반경"으로 배제 테스트
//  - 규약: 안쪽 ≥ 0  ⇒  Distance + Radius >= 0 이면 그 평면을 통과(겹침)
//  - 하나라도 실패하면(음수) 절두체 밖 → 즉시 탈락
// ------------------------------------------------------------
bool Intersects(const FPlane& P, const FVector4& Center, const FVector4& Extents)
{
    // 평면과 박스사이의 거리 (양수면 평면의 법선 방향, 음수면 반대 방향)
    const float Distance = Dot3(P.Normal, Center) - P.Distance;
    // AABB를 평면 법선 방향으로 투영했을 때의 최대 반경
    // Radius = abs(Normal.X) * Extents.X + abs(Normal.Y) * Extents.Y + abs(Normal.Z) * Extents.Z
    __m128 abs_normal = _mm_andnot_ps(_mm_set1_ps(-0.0f), P.Normal.SimdData); // abs for all components
    __m128 dp = _mm_dp_ps(abs_normal, Extents.SimdData, 0x71);
    float radius;
    _mm_store_ss(&radius, dp);
    //  최대 반경은 항상 양수이므로, Distance + Radius < 0 이면 절두체의 바깥
    return Distance + radius >= 0.0f;
}

bool IsAABBVisible(const FFrustum& Frustum, const FAABB& Bound)
{
    // AABB 중심/반길이
    const FVector Center3 = (Bound.Min + Bound.Max) * 0.5f;
    const FVector Extents3 = (Bound.Max - Bound.Min) * 0.5f; // 항상 양수
    const FVector4 Center = FVector4::FromPoint(Center3);
    const FVector4 Extents = FVector4::FromDirection(Extents3); // 항상 양수
    // 6면 모두 통과해야 절두체의 안쪽이므로 "보이는 것"
    return Intersects(Frustum.LeftFace, Center, Extents) &&
        Intersects(Frustum.RightFace, Center, Extents) &&
        Intersects(Frustum.TopFace, Center, Extents) &&
        Intersects(Frustum.BottomFace, Center, Extents) &&
        Intersects(Frustum.NearFace, Center, Extents) &&
        Intersects(Frustum.FarFace, Center, Extents);
}

bool IsAABBIntersects(const FFrustum& F, const FAABB& B)
{
    // 부분 교차(Intersect)만 true. 완전 내부/완전 외부는 false.
    const FVector Center3 = (B.Min + B.Max) * 0.5f;
    const FVector Extents3 = (B.Max - B.Min) * 0.5f;
    const FVector4 Center = FVector4::FromPoint(Center3);
    const FVector4 Extents = FVector4::FromDirection(Extents3);

    const FPlane planes[6] = { F.LeftFace, F.RightFace, F.TopFace, F.BottomFace, F.NearFace, F.FarFace };

    bool fullyInside = true;
    for (int i = 0; i < 6; ++i)
    {
        const FPlane& P = planes[i];
        const float Distance = Dot3(P.Normal, Center) - P.Distance;
        const float Radius = std::abs(P.Normal.X) * Extents.X + std::abs(P.Normal.Y) * Extents.Y + std::abs(P.Normal.Z) * Extents.Z;

        if (Distance + Radius < 0.0f)
        {
            // 완전 외부 → 교차 아님
            return false;
        }
        if (Distance - Radius < 0.0f)
        {
            // 이 평면 기준으로는 완전 내부가 아님 → 교차 가능성
            fullyInside = false;
        }
    }
    // 모든 평면 기준으로 완전 내부면 false, 일부 평면에서만 내부가 아니면 true
    return !fullyInside;
}

// 추후에 절두체를 VP 행렬에서 바로 추출하는 방법도 필요하다면 아래를 참고.
// ---------- VP(=View*Proj)에서 평면 추출 ----------
// row-vector 규약(p' = p * M)에서 클립 경계는
// Left:   R3 + R0
// Right:  R3 - R0
// Bottom: R3 + R1
// Top:    R3 - R1
// Near:   R3 + R2
// Far:    R3 - R2
// 결합 결과 P=(a,b,c,d)에 대해:  a*x + b*y + c*z + d >= 0  (클립 내부)
// 우리의 평면식 dot(N,X) - D >= 0 과 맞추려면  N=(a,b,c), D=-d  을 사용.
// 정규화 스케일도 Distance에 나눠 반영해야 함.
/*
static inline FPlane MakePlaneFromRowCombo(const FVector4& R3, const FVector4& Ri, int Sign) // +1 or -1
{
    // P = R3 + Sign * Ri
    const FVector4 P(R3.X + Sign * Ri.X,
        R3.Y + Sign * Ri.Y,
        R3.Z + Sign * Ri.Z,
        R3.W + Sign * Ri.W);

    // 법선/거리 추출 및 정규화
    FPlane Out;
    const FVector4 N(P.X, P.Y, P.Z, 0.0f);
    const float    Len = Length3(N);

    if (Len > 0.0f)
    {
        Out.Normal = FVector4(N.X / Len, N.Y / Len, N.Z / Len, 0.0f);
        // dot(N,X) - D >= 0  와  a*x+b*y+c*z+d >= 0  을 맞추기 위해 D = -d/Len
        Out.Distance = -P.W / Len;
    }
    else
    {
        Out.Normal = FVector4(0, 1, 0, 0);
        Out.Distance = 0.0f;
    }
    return Out;
}

// 카메라에서 VP를 구할 수 있다면 직접 사용.
// 주의: row-vector 규약이면 반드시  VP = View * Proj  (이 순서)
static Frustum CreateFrustumFromVP(const FMatrix& View, const FMatrix& Proj)
{
    const FMatrix VP = View * Proj;

    // 행 접근 함수가 있다고 가정 (없다면 직접 구현)
    const FVector4 R0 = VP.GetRow(0);
    const FVector4 R1 = VP.GetRow(1);
    const FVector4 R2 = VP.GetRow(2);
    const FVector4 R3 = VP.GetRow(3);

    Frustum F;
    F.LeftFace = MakePlaneFromRowCombo(R3, R0, +1);
    F.RightFace = MakePlaneFromRowCombo(R3, R0, -1);
    F.BottomFace = MakePlaneFromRowCombo(R3, R1, +1);
    F.TopFace = MakePlaneFromRowCombo(R3, R1, -1);
    F.NearFace = MakePlaneFromRowCombo(R3, R2, +1);
    F.FarFace = MakePlaneFromRowCombo(R3, R2, -1);
    return F;
}

Frustum CreateFrustumFromCamera(const UCameraComponent& Camera, float)  // OverrideAspect
{
    // 카메라에서 뷰/프로젝션 행렬을 가져오는 API는 엔진에 맞춰 바꿔줘.
    // row-vector 규약: p' = p * View * Proj
    const FMatrix View = Camera.GetViewMatrix();         // 너의 엔진 API
    const FMatrix Proj = Camera.GetProjectionMatrix();   // 너의 엔진 API
    return CreateFrustumFromVP(View, Proj);
}

// ---------- AABB vs 평면 판정(기존 유지, std::abs 권장) ----------
bool Intersects(const FPlane& P, const FVector4& Center, const FVector4& Extents)
{
    const float Distance = Dot3(P.Normal, Center) - P.Distance;

    // AABB의 법선 방향 투영 반경
    const float Radius =
        std::abs(P.Normal.X) * Extents.X +
        std::abs(P.Normal.Y) * Extents.Y +
        std::abs(P.Normal.Z) * Extents.Z;

    return Distance + Radius >= 0.0f;
}

bool IsAABBVisible(const Frustum& F, const FBound& B)
{
    const FVector  Center3 = (B.Min + B.Max) * 0.5f;
    const FVector  Extents3 = (B.Max - B.Min) * 0.5f; // 양수

    const FVector4 Center = FVector4(Center3.X, Center3.Y, Center3.Z, 1.0f); // 점
    const FVector4 Extents = FVector4(Extents3.X, Extents3.Y, Extents3.Z, 0.0f); // 방향

    return Intersects(F.LeftFace, Center, Extents) &&
        Intersects(F.RightFace, Center, Extents) &&
        Intersects(F.TopFace, Center, Extents) &&
        Intersects(F.BottomFace, Center, Extents) &&
        Intersects(F.NearFace, Center, Extents) &&
        Intersects(F.FarFace, Center, Extents);
}

*/

// AVX-optimized culling for 8 AABBs
uint8_t AreAABBsVisible_8_AVX(const FFrustum& Frustum, const FAABB Bounds[8])
{
    // This function performs frustum culling for 8 AABBs simultaneously using AVX2.
    // It works by testing all 8 boxes against each of the 6 frustum planes.

    // 1. Transpose AoS to SoA using a combination of SSE and AVX instructions.
    // This is faster than a scalar loop. The FBound struct (6 floats) is not
    // naturally aligned for SIMD, so this process is complex but efficient.

    // --- Transpose Part 1: Boxes 0-3 ---
    __m128 r0 = _mm_loadu_ps(&Bounds[0].Min.X); // {m0x, m0y, m0z, M0x}
    __m128 r1 = _mm_loadu_ps(&Bounds[1].Min.X); // {m1x, m1y, m1z, M1x}
    __m128 r2 = _mm_loadu_ps(&Bounds[2].Min.X); // {m2x, m2y, m2z, M2x}
    __m128 r3 = _mm_loadu_ps(&Bounds[3].Min.X); // {m3x, m3y, m3z, M3x}
    _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
    __m128 b0_3_min_x = r0;
    __m128 b0_3_min_y = r1;
    __m128 b0_3_min_z = r2;
    __m128 b0_3_max_x = r3;

    __m128 myz01 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_load_sd((double*)&Bounds[0].Max.Y), _mm_load_sd((double*)&Bounds[1].Max.Y)));
    __m128 myz23 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_load_sd((double*)&Bounds[2].Max.Y), _mm_load_sd((double*)&Bounds[3].Max.Y)));
    __m128 b0_3_max_y = _mm_shuffle_ps(myz01, myz23, _MM_SHUFFLE(2, 0, 2, 0));
    __m128 b0_3_max_z = _mm_shuffle_ps(myz01, myz23, _MM_SHUFFLE(3, 1, 3, 1));

    // --- Transpose Part 2: Boxes 4-7 ---
    r0 = _mm_loadu_ps(&Bounds[4].Min.X);
    r1 = _mm_loadu_ps(&Bounds[5].Min.X);
    r2 = _mm_loadu_ps(&Bounds[6].Min.X);
    r3 = _mm_loadu_ps(&Bounds[7].Min.X);
    _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
    __m128 b4_7_min_x = r0;
    __m128 b4_7_min_y = r1;
    __m128 b4_7_min_z = r2;
    __m128 b4_7_max_x = r3;

    myz01 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_load_sd((double*)&Bounds[4].Max.Y), _mm_load_sd((double*)&Bounds[5].Max.Y)));
    myz23 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_load_sd((double*)&Bounds[6].Max.Y), _mm_load_sd((double*)&Bounds[7].Max.Y)));
    __m128 b4_7_max_y = _mm_shuffle_ps(myz01, myz23, _MM_SHUFFLE(2, 0, 2, 0));
    __m128 b4_7_max_z = _mm_shuffle_ps(myz01, myz23, _MM_SHUFFLE(3, 1, 3, 1));

    // --- Transpose Part 3: Combine into __m256 ---
    __m256 min_x = _mm256_set_m128(b4_7_min_x, b0_3_min_x);
    __m256 min_y = _mm256_set_m128(b4_7_min_y, b0_3_min_y);
    __m256 min_z = _mm256_set_m128(b4_7_min_z, b0_3_min_z);
    __m256 max_x = _mm256_set_m128(b4_7_max_x, b0_3_max_x);
    __m256 max_y = _mm256_set_m128(b4_7_max_y, b0_3_max_y);
    __m256 max_z = _mm256_set_m128(b4_7_max_z, b0_3_max_z);

    // 2. Calculate centers and extents
    const __m256 half = _mm256_set1_ps(0.5f);
    __m256 centers_x = _mm256_mul_ps(_mm256_add_ps(max_x, min_x), half);
    __m256 centers_y = _mm256_mul_ps(_mm256_add_ps(max_y, min_y), half);
    __m256 centers_z = _mm256_mul_ps(_mm256_add_ps(max_z, min_z), half);
    __m256 extents_x = _mm256_mul_ps(_mm256_sub_ps(max_x, min_x), half);
    __m256 extents_y = _mm256_mul_ps(_mm256_sub_ps(max_y, min_y), half);
    __m256 extents_z = _mm256_mul_ps(_mm256_sub_ps(max_z, min_z), half);

    // 3. Perform Culling (This part was correct before)
    const FPlane* planes = &Frustum.TopFace;
    uint32_t all_visible_mask = 0xFF;

    const __m256 sign_mask = _mm256_set1_ps(-0.0f);

    for (int i = 0; i < 6; ++i)
    {
        const FPlane& p = planes[i];
        __m256 plane_nx = _mm256_set1_ps(p.Normal.X);
        __m256 plane_ny = _mm256_set1_ps(p.Normal.Y);
        __m256 plane_nz = _mm256_set1_ps(p.Normal.Z);
        __m256 plane_d = _mm256_set1_ps(p.Distance);

        __m256 dist = _mm256_sub_ps(
            _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(centers_x, plane_nx), _mm256_mul_ps(centers_y, plane_ny)),
                _mm256_mul_ps(centers_z, plane_nz)
            ),
            plane_d
        );

        __m256 radius = _mm256_add_ps(
            _mm256_add_ps(
                _mm256_mul_ps(extents_x, _mm256_andnot_ps(sign_mask, plane_nx)),
                _mm256_mul_ps(extents_y, _mm256_andnot_ps(sign_mask, plane_ny))
            ),
            _mm256_mul_ps(extents_z, _mm256_andnot_ps(sign_mask, plane_nz))
        );

        __m256 comparison = _mm256_cmp_ps(_mm256_add_ps(dist, radius), _mm256_setzero_ps(), _CMP_GE_OQ);
        
        int plane_mask = _mm256_movemask_ps(comparison);
        all_visible_mask &= plane_mask;

        if (all_visible_mask == 0)
        {
            return 0;
        }
    }

    return static_cast<uint8_t>(all_visible_mask);
}

/* *
* @brief 해당 namespace로 래핑된 함수들은 "카메라 프러스텀 월드 공간"을 추출하는데 관련된 영역입니다.
*/
namespace
{
    // Z Near ~ Z Far의 뎁스값을 "비선형적"으로 변환합니다.
    float DepthToNDC(float Depth, float NearZ, float FarZ) { return (FarZ / (FarZ - NearZ)) * (1.0f - NearZ / Depth); }

    // NDC 코너를 월드 공간으로 변환하는 공통 로직
    // @param NDCCorners - NDC 공간의 8개 코너 점
    // @param InvViewProj - ViewProjection의 역행렬
    // @param OutCorners - 월드 공간 코너 출력
    void TransformNDCCornersToWorldSpace(const FVector4 NDCCorners[8], const FMatrix& InvViewProj, TArray<FVector>& OutCorners)
    {
        // 출력 배열 크기 확보
        OutCorners.Empty();
        OutCorners.Reserve(8);

        // 각 NDC 코너를 월드 공간으로 변환
        for (int i = 0; i < 8; ++i)
        {
            // Row-vector 방식: v' = v * M
            FVector4 WorldPosHomogeneous = NDCCorners[i] * InvViewProj;

            // Perspective divide (동차 좌표 → 데카르트 좌표)
            if (WorldPosHomogeneous.W != 0.0f)
            {
                float InvW = 1.0f / WorldPosHomogeneous.W;
                OutCorners.Add(FVector(
                    WorldPosHomogeneous.X * InvW,
                    WorldPosHomogeneous.Y * InvW,
                    WorldPosHomogeneous.Z * InvW
                ));
            }
            else
            {
                // W=0인 경우 (방향 벡터) - 일반적으로 발생하지 않지만 안전장치
                OutCorners.Add(FVector(
                    WorldPosHomogeneous.X,
                    WorldPosHomogeneous.Y,
                    WorldPosHomogeneous.Z
                ));
            }
        }
    }
}

void GetFrustumCornersWorldSpace(const FMatrix& InViewProjInverse, TArray<FVector>& OutCorners)
{
    // NDC 공간의 Frustum 8개 코너 점 정의
    // DirectX는 NDC에서 z 범위가 [0, 1]이므로 Near=0, Far=1
    static const FVector4 NDCCorners[8] = {
        // Near plane (z = 0 in D3D NDC)
        { -1, -1, 0, 1 },  // Left-Bottom-Near
        {  1, -1, 0, 1 },  // Right-Bottom-Near
        {  1,  1, 0, 1 },  // Right-Top-Near
        { -1,  1, 0, 1 },  // Left-Top-Near
        // Far plane (z = 1 in D3D NDC)
        { -1, -1, 1, 1 },  // Left-Bottom-Far
        {  1, -1, 1, 1 },  // Right-Bottom-Far
        {  1,  1, 1, 1 },  // Right-Top-Far
        { -1,  1, 1, 1 }   // Left-Top-Far
    };

    TransformNDCCornersToWorldSpace(NDCCorners, InViewProjInverse, OutCorners);
}

void GetFrustumCornersWorldSpace_Partial(const FMatrix& CameraView, const FMatrix& CameraProjection,
	float NearDistance, float FarDistance, TArray<FVector>& OutCorners)
{
	// 카메라 Projection 행렬에서 원본 Near/Far 추출
	// Perspective Projection: [2][2] = far/(far-near), [3][2] = -near*far/(far-near)
	float OriginalNear = -CameraProjection.M[3][2] / CameraProjection.M[2][2];
	float OriginalFar = -CameraProjection.M[3][2] / (CameraProjection.M[2][2] - 1.0f);

	// CSM을 위해 NearDistance와 FarDistance를 사용하여 NDC Z 계산
	// 원본 카메라 투영 행렬의 Near/Far가 아닌, 캐스케이드의 Near/Far를 사용
	float NearNDC = DepthToNDC(NearDistance, OriginalNear, OriginalFar);
	float FarNDC = DepthToNDC(FarDistance, OriginalNear, OriginalFar);

	// NDC 공간의 Frustum 8개 코너 점 정의 (Z값 수정)
	FVector4 NDCCorners[8] = {
		// Near plane
		{ -1, -1, NearNDC, 1 },  // Left-Bottom-Near
		{  1, -1, NearNDC, 1 },  // Right-Bottom-Near
		{  1,  1, NearNDC, 1 },  // Right-Top-Near
		{ -1,  1, NearNDC, 1 },  // Left-Top-Near
		// Far plane
		{ -1, -1, FarNDC, 1 },  // Left-Bottom-Far
		{  1, -1, FarNDC, 1 },  // Right-Bottom-Far
		{  1,  1, FarNDC, 1 },  // Right-Top-Far
		{ -1,  1, FarNDC, 1 }   // Left-Top-Far
	};

	// ViewProjection 역행렬 계산
	FMatrix InvView = CameraView.InverseAffine();
	FMatrix InvProj = CameraProjection.InversePerspectiveProjection();
	FMatrix InvViewProj = InvProj * InvView;

	TransformNDCCornersToWorldSpace(NDCCorners, InvViewProj, OutCorners);
}