//================================================================================================
// Filename:      LightStructures.hlsl
// Description:   조명 구조체 정의
//                모든 조명 기반 셰이더에서 공유 (UberLit, Decal 등)
//================================================================================================

// --- 전역 상수 정의 ---
#define NUM_POINT_LIGHT_MAX 16
#define NUM_SPOT_LIGHT_MAX 16

// --- 조명 정보 구조체 (LightInfo.h와 완전히 일치) ---
// 주의: Color는 이미 Intensity와 Temperature가 포함됨 (C++에서 계산)
// 최소 메모리 사용을 위한 최적화된 패딩

struct FAmbientLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
};

// CSM Cascade Tier Info (Shader측)
struct FCascadeTierInfo
{
    uint TierIndex;      // 0=Low, 1=Medium, 2=High
    uint SliceIndex;     // 해당 티어 내 슬라이스 인덱스
    float Resolution;    // 실제 해상도 (PCF texelSize 계산용)
    float Padding;       // 16-byte 정렬
};

struct FDirectionalLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)

    float3 Direction;   // 12 bytes - FVector
    uint bCastShadow;   // 4 bytes - 섀도우 캐스팅 여부

    uint ShadowMapIndex; // 4 bytes - 섀도우 맵 base 인덱스 (CSM의 경우 첫 번째 캐스케이드 인덱스)
    uint bUseCSM;        // 4 bytes - CSM 사용 여부 (true = CSM, false = single shadow map)
    uint NumCascades;    // 4 bytes - CSM 캐스케이드 수 (3~6)
    float Padding;       // 4 bytes - 패딩

    row_major float4x4 LightViewProjection; // 64 bytes - Legacy: 단일 섀도우 맵용

    // CSM (Cascaded Shadow Maps) Data
    row_major float4x4 CascadeViewProjection[4]; // 256 bytes (64 bytes × 4) - 각 캐스케이드의 VP 행렬
    float4 CascadeSplitDistances;                // 16 bytes - 각 캐스케이드의 Far 거리 (float4로 패킹)

    // CSM Tier Info (3-Tier System)
    FCascadeTierInfo CascadeTierInfos[4];        // 64 bytes (16 bytes × 4) - 각 캐스케이드의 티어 정보

    // Total: 388 + 64 = 452 bytes
};

struct FPointLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (Intensity + Temperature 포함)

    float3 Position;        // 12 bytes - FVector
    float AttenuationRadius; // 4 bytes (슬롯 채우기 위해 위로 이동)

    float FalloffExponent;  // 4 bytes - 예술적 제어를 위한 감쇠 지수
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = 물리 기반, false = 지수 기반)
    uint bCastShadow;       // 4 bytes - 섀도우 캐스팅 여부
    uint ShadowMapIndex;    // 4 bytes - 섀도우 맵 인덱스 (-1이면 섀도우 없음)

    float4 Padding;         // 16 bytes - 16-byte 정렬을 위한 패딩 (float4x4 배열 요구사항)

    row_major float4x4 LightViewProjection[6]; // 384 bytes (64 bytes × 6) - 큐브맵 각 면의 View-Projection 행렬
    // Total: 448 bytes
};

struct FSpotLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (Intensity + Temperature 포함)

    float3 Position;        // 12 bytes - FVector
    float InnerConeAngle;   // 4 bytes - 내부 원뿔 각도

    float3 Direction;       // 12 bytes - FVector
    float OuterConeAngle;   // 4 bytes - 외부 원뿔 각도

    float AttenuationRadius; // 4 bytes - 감쇠 반경
    float FalloffExponent;  // 4 bytes - 예술적 제어를 위한 감쇠 지수
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = 물리 기반, false = 지수 기반)
    uint bCastShadow;       // 4 bytes - 섀도우 캐스팅 여부

    uint ShadowMapIndex;    // 4 bytes - 섀도우 맵 인덱스 (-1이면 섀도우 없음)
    float3 Padding;          // 12 bytes - 패딩

    row_major float4x4 LightViewProjection; // 64 bytes - 라이트 공간 변환 행렬
};
