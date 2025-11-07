//================================================================================================
// Filename:      LightingBuffers.hlsl
// Description:   조명 상수 버퍼 및 리소스
//                모든 조명 기반 셰이더에서 공유
//================================================================================================

// 주의: 이 파일은 LightStructures.hlsl 이후에 include 되어야 함

// b1: ViewProjBuffer (VS+PS) - View, Projection 행렬 (CSM, 조명 계산에 필요)
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

// b7: CameraBuffer (VS+PS) - 카메라 속성
cbuffer CameraBuffer : register(b7)
{
    float3 CameraPosition;
};

// b8: LightBuffer (VS+PS) - ConstantBufferType.h의 FLightBufferType과 일치
cbuffer LightBuffer : register(b8)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
    uint PointLightCount;
    uint SpotLightCount;
};

// --- 타일 기반 라이트 컬링 리소스 ---
// t2: 타일별 라이트 인덱스 Structured Buffer
// 구조:  [TileIndex * MaxLightsPerTile] = LightCount
//        [TileIndex * MaxLightsPerTile + 1 ~ ...] = LightIndices (상위 16비트: 타입, 하위 16비트: 인덱스)
StructuredBuffer<uint> g_TileLightIndices : register(t2);

// PointLight, SpotLight Structured Buffer
StructuredBuffer<FPointLightInfo> g_PointLightList : register(t3);
StructuredBuffer<FSpotLightInfo> g_SpotLightList : register(t4);

// --- Shadow Map Resources ---
// NONE/PCF (Depth Format)
// t5: SpotLight Shadow map texture array
Texture2DArray g_SpotLightShadowMaps : register(t5);

// t6: DirectionalLight Shadow map texture array (Non-CSM)
Texture2DArray g_DirectionalLightShadowMaps : register(t6);

// t7: PointLight Cube Shadow map texture cube array
TextureCubeArray g_PointLightShadowCubeMaps : register(t7);

// t16-18: DirectionalLight CSM 3-Tier Arrays (NONE/PCF Depth Format)
Texture2DArray g_DirectionalShadowMaps_CSM_LowTier    : register(t16); // 256~512
Texture2DArray g_DirectionalShadowMaps_CSM_MediumTier : register(t17); // 1024~2048
Texture2DArray g_DirectionalShadowMaps_CSM_HighTier   : register(t18); // 4096+

// VSM/ESM/EVSM용 (Float 포맷 R32_FLOAT/R32G32_FLOAT)
// t8: SpotLight Shadow map texture array (Float)
Texture2DArray g_SpotLightShadowMaps_Float : register(t8);

// t9: DirectionalLight Shadow map texture array (Float, Non-CSM)
Texture2DArray g_DirectionalLightShadowMaps_Float : register(t9);

// t10: PointLight Cube Shadow map texture cube array (Float)
TextureCubeArray g_PointLightShadowCubeMaps_Float : register(t10);

// t19-21: DirectionalLight CSM 3-Tier Arrays (VSM/ESM/EVSM Float Format)
Texture2DArray g_DirectionalShadowMaps_CSM_LowTier_Float    : register(t19);
Texture2DArray g_DirectionalShadowMaps_CSM_MediumTier_Float : register(t20);
Texture2DArray g_DirectionalShadowMaps_CSM_HighTier_Float   : register(t21);

// Shadow map sampler - comparison sampler for NONE, PCF
SamplerComparisonState g_ShadowSampler : register(s2);

// Linear sampler for VSM/ESM/EVSM
SamplerState g_LinearSampler : register(s3);

// b11: 타일 컬링 설정 상수 버퍼
cbuffer TileCullingBuffer : register(b11)
{
    uint TileSize;          // 타일 크기 (픽셀, 기본 16)
    uint TileCountX;        // 가로 타일 개수
    uint TileCountY;        // 세로 타일 개수
    uint bUseTileCulling;   // 타일 컬링 활성화 여부 (0=비활성화, 1=활성화)
};

// b12: 섀도우 필터링 설정 상수 버퍼
cbuffer ShadowFilterBuffer : register(b12)
{
    uint FilterType;                  // 필터링 타입 (0=NONE, 1=PCF, 2=VSM, 3=ESM, 4=EVSM)
    uint PCFSampleCount;              // PCF 샘플 수 (3, 4, 5)
    uint PCFCustomSampleCount;        // PCF Custom 샘플 수
    float DirectionalLightResolution; // Directional Light 섀도우 맵 해상도

    float SpotLightResolution;        // Spot Light 섀도우 맵 해상도
    float PointLightResolution;       // Point Light 섀도우 맵 해상도
    float VSMLightBleedingReduction;  // VSM Light bleeding 감소 (0.0 ~ 1.0)
    float VSMMinVariance;             // VSM 최소 분산 값

    float ESMExponent;                // ESM Exponential 계수
    float EVSMPositiveExponent;       // EVSM Positive exponent
    float EVSMNegativeExponent;       // EVSM Negative exponent
    float EVSMLightBleedingReduction; // EVSM Light bleeding 감소
};
