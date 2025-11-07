Texture2D g_SceneColorTex : register(t0);

SamplerState g_LinearClampSample : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer VignettingCB : register(b2)
{
    float4 VignettingColor;      // Fade 목표 색상 (RGB)

    float Radius;
    float Softness;
    float AspectRatio;
    float Padding;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.texCoord;

    // uv -> 중심 좌표 변환
    float2 position = uv * 2.0f - 1.0f;
    position.x *= AspectRatio;  // 화면 종횡비 보정
    float dist = length(position);

    // 페이드/감쇠 커브 계산
    // vignette: 중심부=1 (원본 유지), 가장자리=0 (VignettingColor)
    float vignette = smoothstep(Radius, Radius - Softness, dist);

    // 원본 색상
    float3 sceneColor = g_SceneColorTex.Sample(g_LinearClampSample, uv).rgb;

    // Alpha를 강도로 사용: Alpha가 0이면 효과 없음, 1이면 최대 효과
    float strength = VignettingColor.a;

    // 가장자리 블렌딩 양 계산: (1-vignette)는 가장자리에서 1, 중심에서 0
    // strength를 곱해서 전체 효과 강도 조절
    float blendAmount = (1.0 - vignette) * strength;

    // 최종 색상: sceneColor에서 VignettingColor로 블렌딩
    // blendAmount=0 (중심): sceneColor
    // blendAmount=1 (가장자리, strength=1): VignettingColor
    float3 finalColor = lerp(sceneColor, VignettingColor.rgb, blendAmount);

    return float4(finalColor, 1.0f);
}

