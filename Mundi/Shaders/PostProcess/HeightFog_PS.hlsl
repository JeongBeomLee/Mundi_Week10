Texture2D g_DepthTex : register(t0);
Texture2D g_PrePathResultTex : register(t1);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer FogCB : register(b2)
{
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    
    float4 FogInscatteringColor;
    
    float FogMaxOpacity;
    float FogHeight; // fog base height
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = g_PrePathResultTex.Sample(g_LinearClampSample, input.texCoord);
    float depth = g_DepthTex.Sample(g_PointClampSample, input.texCoord).r;

    // -----------------------------
    // 1. Depth → Clip Space 복원
    // -----------------------------
    // DX11에서는 depth가 [0,1] 범위 → NDC z 그대로 사용 가능
    //float ndcZ = depth * 2.0f - 1.0f; // clip space 변환

    // NDC 좌표
    float ndcX = input.texCoord.x * 2.0f - 1.0f;
    float ndcY = 1.0f - input.texCoord.y * 2.0f; // y축 반전
    float4 ndcPos = float4(ndcX, ndcY, depth, 1.0f);

    // -----------------------------
    // 2. 월드 좌표 복원
    // -----------------------------
    float4 viewPos = mul(ndcPos, InverseProjectionMatrix);
    viewPos /= viewPos.w;
    float4 worldPos = mul(viewPos, InverseViewMatrix);
    worldPos /= worldPos.w;
    
    // 카메라 월드 좌표
    float4 cameraWorldPos = mul(float4(0, 0, 0, 1), InverseViewMatrix);

    // -----------------------------
    // 3. 광선 벡터 및 거리
    // -----------------------------
    float3 ray = worldPos.xyz - cameraWorldPos.xyz;
    float L = length(ray);
    float3 rayDir = ray / L;
    
    // -----------------------------
    // 4. 계산 시작
    // -----------------------------
    float Distance = max(0.0, L - StartDistance);
    if(Distance > (FogCutoffDistance - StartDistance))
        Distance = 0.0;
    
    float oy = cameraWorldPos.z;
    float dy = rayDir.z;
    
    float baseExp = exp(-FogHeightFalloff * (oy - FogHeight));

    float FogIntegral = 0.0;
    if (abs(dy) > 1e-6) {
        // d_y ≠ 0 인 경우
        float numerator = 1.0 - exp(-FogHeightFalloff * dy * Distance);
        float denominator = FogHeightFalloff * dy;
        FogIntegral = FogDensity * baseExp * (numerator / denominator);
    } else {
        // d_y = 0 (수평 광선)
        FogIntegral = FogDensity * baseExp * Distance;
    }
    
    float fogFactor = exp(-FogIntegral);
    // 최대 불투명도 적용
    fogFactor = clamp(fogFactor, 1 - FogMaxOpacity, 1.0);
    
    // 최종 색상
    //color.rgb = lerp(color.rgb, FogInscatteringColor.rgb, fogFactor);
    color.rgb = color.rgb * fogFactor + FogInscatteringColor.rgb * (1 - fogFactor);

    return color;
}
