Texture2D g_SceneColorTex : register(t0);
SamplerState g_LinearClampSample : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer FadeCB : register(b2)
{
    float FadeAlpha;       // 0 ~ 1 범위, 0 = 완전 Fade 색상, 1 = 원본 씬 색상
    float3 FadeColor;      // Fade 목표 색상 (RGB)
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 원본 씬 색상 샘플링 (SceneColorSource SRV = 이전 렌더 패스 결과)
    float4 sceneColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);

    // FadeAlpha를 사용하여 SceneColor와 FadeColor를 보간
    // FadeAlpha = 0 → 완전히 FadeColor (Fade Out)
    // FadeAlpha = 1 → 완전히 SceneColor (Fade In)
    float3 finalColor = lerp(FadeColor, sceneColor.rgb, FadeAlpha);

    // Alpha 채널은 유지
    return float4(finalColor, sceneColor.a);
}
