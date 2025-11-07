//--------------------------------------------------------------------------------------
// Gamma Correction Pixel Shader
// 최종 렌더링 결과에 감마 보정을 적용합니다.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// 상수 버퍼 (Constant Buffer)
// CPU에서 GPU로 넘겨주는 데이터
//--------------------------------------------------------------------------------------
cbuffer GammaCorrectionCB : register(b2)
{
    float Gamma;           // 감마 값 (일반적으로 2.2가 표준)
    float InvGamma;        // 1.0 / Gamma (미리 계산해서 넘겨주면 성능 향상)
    float Brightness;      // 추가 밝기 조정 (1.0 = 변화 없음)
    float Saturation;      // 채도 조정 (1.0 = 변화 없음)
};

//--------------------------------------------------------------------------------------
// 텍스처와 샘플러
//--------------------------------------------------------------------------------------
Texture2D g_SceneTexture : register(t0);
SamplerState g_SamplerLinear : register(s0);

//--------------------------------------------------------------------------------------
// RGB 색상에서 휘도(Luminance)를 계산하는 함수
//--------------------------------------------------------------------------------------
float RGBToLuminance(float3 InColor)
{
    // ITU-R BT.709 표준에 따른 휘도 공식
    return dot(InColor, float3(0.2126, 0.7152, 0.0722));
}

//--------------------------------------------------------------------------------------
// Gamma Correction Pixel Shader
//--------------------------------------------------------------------------------------
float4 mainPS(float4 Pos : SV_Position, float2 TexCoord : TEXCOORD0) : SV_Target
{
    // 1. 씬 텍스처에서 색상 샘플링
    float3 sceneColor = g_SceneTexture.Sample(g_SamplerLinear, TexCoord).rgb;

    // 2. 밝기 조정 (선택적)
    sceneColor *= Brightness;

    // 3. 채도 조정 (선택적)
    if (Saturation != 1.0)
    {
        float luminance = RGBToLuminance(sceneColor);
        sceneColor = lerp(float3(luminance, luminance, luminance), sceneColor, Saturation);
    }

    // 4. Gamma Correction 적용
    // Linear -> sRGB 변환: Output = pow(Input, 1/gamma)
    // 음수 값 방지를 위해 max 사용
    float3 gammaCorrectedColor = pow(max(sceneColor, 0.0), InvGamma);

    return float4(gammaCorrectedColor, 1.0);
}
