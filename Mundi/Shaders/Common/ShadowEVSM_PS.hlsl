//================================================================================================
// Filename:      ShadowEVSM_PS.hlsl
// Description:   EVSM (Exponential Variance Shadow Maps) 섀도우 맵 생성용 픽셀 쉐이더
//                (exp(c+ * depth), exp(-c- * depth))를 R32G32_FLOAT 포맷으로 출력
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

    float ESMExponent;
    float EVSMPositiveExponent;   // c+ (positive exponent)
    float EVSMNegativeExponent;   // c- (negative exponent)
    float EVSMLightBleedingReduction;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;  // 화면 공간 위치
    float Depth : TEXCOORD0;        // 라이트 공간 depth (0~1)
};

// EVSM 출력: (exp(c+ * depth), exp(-c- * depth))
float2 mainPS(PS_INPUT input) : SV_TARGET
{
    float depth = input.Position.z;
    depth = saturate(depth);

    // Positive와 Negative exponential depth 계산
    float posExp = exp(EVSMPositiveExponent * depth);
    float negExp = exp(-EVSMNegativeExponent * depth);

    return float2(posExp, negExp);
}
