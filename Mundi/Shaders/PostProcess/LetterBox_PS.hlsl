// LetterBox Post Process Pixel Shader
// 화면 상하단에 검은 띠(레터박스)를 렌더링하여 시네마틱 효과 제공

Texture2D g_SceneTex : register(t0);
SamplerState g_LinearClampSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

cbuffer LetterBoxCB : register(b2)
{
    float LetterBoxSize; // 레터박스 크기 (0.0 ~ 1.0)
    float LetterBoxOpacity; // 레터박스 불투명도 (0.0 ~ 1.0)
    float2 Padding;
};

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 원본 씬 컬러 샘플링
    float4 sceneColor = g_SceneTex.Sample(g_LinearClampSampler, input.TexCoord);
    
    // LetterBoxSize가 0이면 레터박스 없음
    if (LetterBoxSize <= 0.0)
    {
        return sceneColor;
    }
    
    // UV 좌표는 (0,0)이 좌상단, (1,1)이 우하단
    float v = input.TexCoord.y;
    
    // 상단 레터박스 영역: v < LetterBoxSize
    // 하단 레터박스 영역: v > (1.0 - LetterBoxSize)
    bool isInLetterBoxArea = (v < LetterBoxSize) || (v > (1.0 - LetterBoxSize));
    
    // 레터박스 영역이면 검은색과 블렌딩, 아니면 원본 씬 컬러 유지
    if (isInLetterBoxArea)
    {
        float3 letterBoxColor = float3(0.0, 0.0, 0.0);
        float3 finalColor = lerp(sceneColor.rgb, letterBoxColor, LetterBoxOpacity);
        return float4(finalColor, sceneColor.a);
    }
    else
    {
        return sceneColor;
    }
}