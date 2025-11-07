Texture2D g_DepthTex : register(t0);

SamplerState g_PointClampSample : register(s1);

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
    int IsOrthographic; // 0: Perspective, 1: Orthographic
}

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float depth = g_DepthTex.Sample(g_PointClampSample, input.texCoord).r;

    float zView;
    if (IsOrthographic == 1)
    {
        // Orthographic
        zView = Near + depth * (Far - Near);
    }
    else
    {
        // Perspective
        zView = Near * Far / (Far - depth * (Far - Near));
    }
    //float zView = (2.0 * Near * Far) / (Far + Near - depth * (Far - Near));
    float NormalizedDepth = saturate((zView - Near) / (Far - Near));
    float4 FinalColor = float4(NormalizedDepth, NormalizedDepth, NormalizedDepth, 1.0f);

    return FinalColor;
}
