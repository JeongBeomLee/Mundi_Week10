// ShadowDepth.hlsl
// Simple depth-only shader for rendering shadow maps
// This shader only outputs depth, no color information needed

// b0: Object Transform
cbuffer ObjectBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
};

// b1: View/Projection for light's perspective
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float Depth : TEXCOORD0;
};

//-----------------------------------------------------------------------------
// Vertex Shader
//-----------------------------------------------------------------------------
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // Transform vertex to world space
    float4 worldPos = mul(float4(input.Position, 1.0f), WorldMatrix);

    // Transform to light's view space
    float4 viewPos = mul(worldPos, ViewMatrix);

    // Transform to light's clip space
    output.Position = mul(viewPos, ProjectionMatrix);
    output.Depth = output.Position.z / output.Position.w;
    
    return output;
}

//-----------------------------------------------------------------------------
// Pixel Shader
//-----------------------------------------------------------------------------
void mainPS(PS_INPUT input)
{
    // No output needed - depth is automatically written to depth buffer
    // This shader just ensures the pixel is not discarded

    // If you want to do alpha testing or custom depth modifications,
    // you can add code here
}
