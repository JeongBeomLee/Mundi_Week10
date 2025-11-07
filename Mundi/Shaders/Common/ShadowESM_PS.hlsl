//================================================================================================
// Filename:      ShadowESM_PS.hlsl
// Description:   ESM (Exponential Shadow Maps) 섀도우 맵 생성용 픽셀 쉐이더
//                exp(c * depth)를 R32_FLOAT 포맷으로 출력
//================================================================================================

// b12: Shadow Filter Buffer
cbuffer ShadowFilterBuffer : register(b12)
{
    uint FilterType;
    uint PCFSampleCount;
    uint PCFCustomSampleCount;
    float DirectionalLightResolution;

    float SpotLightResolution;
    float PointLightResolution;
    float VSMLightBleedingReduction;
    float VSMMinVariance;

    float ESMExponent;  // ESM exponential 계수
    float EVSMPositiveExponent;
    float EVSMNegativeExponent;
    float EVSMLightBleedingReduction;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;  // 화면 공간 위치
    float Depth : TEXCOORD0;        // 라이트 공간 depth (0~1)
};

// ESM 출력: exp(c * depth)
float mainPS(PS_INPUT input) : SV_TARGET
{
    float depth = input.Position.z;

    depth = saturate(depth);
    float expDepth = exp(ESMExponent * depth);

    return expDepth;
}
