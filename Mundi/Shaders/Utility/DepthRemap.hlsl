//================================================================================================
// Filename:      DepthRemap.hlsl
// Description:   Depth 값을 remap해서 시각화하는 셰이더
//================================================================================================

// Constant Buffer
cbuffer DepthRemapParams : register(b0)
{
    float MinDepth;  // 시작 범위 (예: 0.99)
    float MaxDepth;  // 끝 범위 (예: 1.0)
    float2 Padding;
};

// Input texture
Texture2D<float> DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

// Vertex Shader - Fullscreen Quad
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}

// Pixel Shader - Remap depth range
float4 PS(PS_INPUT input) : SV_TARGET
{
    // Sample depth value
    float depth = DepthTexture.Sample(PointSampler, input.TexCoord);

    // Remap: [MinDepth, MaxDepth] -> [0, 1]
    float remapped = saturate((depth - MinDepth) / (MaxDepth - MinDepth));

    // Output as grayscale
    return float4(remapped, 0.0f, 0.0f, 1.0f);
}
