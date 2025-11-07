// ShadowDepthCube.hlsl
// Linear depth shader for PointLight cube map shadow maps
// Paraboloid와 유사하지만 projection 없이 단순 선형 깊이 계산

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

// b9: Cube Shadow 전용 상수 버퍼
cbuffer CubeShadowBuffer : register(b9)
{
    float AttenuationRadius;  // 라이트 감쇠 반경
    float NearPlane;          // Near clipping plane
    float2 Padding;           // 16바이트 정렬을 위한 패딩
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

//-----------------------------------------------------------------------------
// Vertex Shader - Linear Depth for Cube Map
//-----------------------------------------------------------------------------
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 1. World Space로 변환
    float4 worldPos = mul(float4(input.Position, 1.0f), WorldMatrix);

    // 2. View Space로 변환 (라이트 중심 기준)
    float4 viewPos = mul(worldPos, ViewMatrix);

    // 3. Projection Space로 변환 (clip space 계산)
    output.Position = mul(viewPos, ProjectionMatrix);

    // 4. 선형 깊이로 덮어쓰기
    // Paraboloid와 동일한 방식: (distance - nearPlane) / (attenuationRadius - nearPlane)
    float distance = length(viewPos.xyz);
    output.Position.z = saturate((distance - NearPlane) / (AttenuationRadius - NearPlane));

    // 5. w는 1.0으로 유지 (perspective divide 방지)
    output.Position.w = 1.0f;

    return output;
}

//-----------------------------------------------------------------------------
// Pixel Shader
//-----------------------------------------------------------------------------
void mainPS(PS_INPUT input)
{
    // 깊이만 출력
    // SV_POSITION.z가 자동으로 depth buffer에 기록됨
}
