//================================================================================================
// Filename:      Decal.hlsl
// Description:   Decal projection shader with lighting support
//                Supports GOURAUD, LAMBERT, PHONG lighting models
//================================================================================================

// --- 조명 모델 선택 ---
// ViewMode에서 동적으로 설정됨 (SceneRenderer::RenderDecalPass)
// 가능한 매크로:
// - LIGHTING_MODEL_GOURAUD
// - LIGHTING_MODEL_LAMBERT
// - LIGHTING_MODEL_PHONG
// - (매크로 없음 = Unlit)

// --- 공통 조명 시스템 include ---
#include "../Common/LightStructures.hlsl"
#include "../Common/LightingBuffers.hlsl"
#include "../Common/LightingCommon.hlsl"

// --- Decal 전용 상수 버퍼 ---
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose;
}

// ViewProjBuffer는 LightingBuffers.hlsl에서 공통으로 정의됨 (register b1)

cbuffer PSScrollCB : register(b5)
{
    float2 UVScrollSpeed;
    float UVScrollTime;
    float _pad_scrollcb;
}

cbuffer DecalBuffer : register(b6)
{
    row_major float4x4 DecalMatrix;
    float DecalOpacity;
}

// --- 텍스처 리소스 ---
Texture2D g_DecalTexColor : register(t0);
SamplerState g_Sample : register(s0);

// --- 입출력 구조체 ---
struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL0;
    float2 texCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 decalPos : POSITION0;    // Decal projection 좌표

#if defined(LIGHTING_MODEL_GOURAUD) || defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    float3 worldPos : POSITION1;    // 조명 계산용
    float3 normal : NORMAL0;        // 조명 계산용

    #ifdef LIGHTING_MODEL_GOURAUD
        float4 litColor : COLOR0;   // Pre-calculated lighting (Gouraud)
    #endif
#endif
};

//================================================================================================
// 버텍스 셰이더
//================================================================================================
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // World position
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);

    // Decal projection
    output.decalPos = mul(worldPos, DecalMatrix);

    // Screen position
    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    output.position = mul(worldPos, VP);

#if defined(LIGHTING_MODEL_GOURAUD) || defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    // 조명 계산을 위한 데이터
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(input.normal, (float3x3) WorldInverseTranspose));

    #ifdef LIGHTING_MODEL_GOURAUD
        // Gouraud: Vertex shader에서 조명 계산
        // Note: 여기서는 texture를 샘플링할 수 없으므로 white를 base color로 사용
        // Pixel shader에서 texture를 곱함
        float4 baseColor = float4(1, 1, 1, 1);
        float3 viewDir = normalize(CameraPosition - output.worldPos);
        float specPower = 32.0f;

        float3 litColor = CalculateAllLights(
            output.worldPos,
            output.normal,
            viewDir,
            baseColor,
            specPower,
            output.position
        );

        output.litColor = float4(litColor, 1.0f);
    #endif
#endif

    return output;
}

//================================================================================================
// 픽셀 셰이더
//================================================================================================
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 부동 소수점 오차 무시를 위해 Epsilon 사용
    static const float Epsilon = 1e-6f; // 0.000001f
    
    // 1. Decal projection 범위 체크
    float3 ndc = input.decalPos.xyz / input.decalPos.w;

    // decal의 forward가 +x임 -> x방향 projection
    if (ndc.x < 0.0f - Epsilon || 1.0f + Epsilon < ndc.x ||
        ndc.y < -1.0f - Epsilon || 1.0f + Epsilon < ndc.y ||
        ndc.z < -1.0f - Epsilon || 1.0f + Epsilon < ndc.z)
    {
        discard;
    }

    // 2. UV 계산 및 텍스처 샘플링
    float2 uv = (ndc.yz + 1.0f) / 2.0f;
    uv.y = 1.0f - uv.y;
    uv += UVScrollSpeed * UVScrollTime;

    float4 decalTexture = g_DecalTexColor.Sample(g_Sample, uv);

    // 3. 조명 계산 (매크로에 따라)
#ifdef LIGHTING_MODEL_GOURAUD
    // Gouraud: VS에서 계산한 조명 결과 사용
    float4 finalColor = input.litColor;
    finalColor.rgb *= decalTexture.rgb;  // Texture modulation
    finalColor.a = decalTexture.a * DecalOpacity;
    return finalColor;

#elif defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    // Lambert/Phong: PS에서 조명 계산
    float3 normal = normalize(input.normal);
    float4 baseColor = decalTexture;
    float specPower = 32.0f;

    #ifdef LIGHTING_MODEL_PHONG
        float3 viewDir = normalize(CameraPosition - input.worldPos);
    #else
        float3 viewDir = float3(0, 0, 0);  // Lambert는 사용 안 함
    #endif

    float3 litColor = CalculateAllLights(
        input.worldPos,
        normal,
        viewDir,
        baseColor,
        specPower,
        input.position
    );

    float4 finalColor = float4(litColor, decalTexture.a * DecalOpacity);
    return finalColor;

#else
    // No lighting model - 기존 방식 (단순 텍스처)
    float4 finalColor = decalTexture;
    finalColor.a *= DecalOpacity;
    return finalColor;
#endif
}
