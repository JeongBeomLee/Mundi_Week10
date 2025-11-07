//================================================================================================
// Filename:      Gizmo.hlsl (Gizmo Shader)
// Description:   Simplified shader for Gizmo rendering (Arrow, Rotate, Scale components)
//                - No lighting calculations (unlit)
//                - No material/texture support
//                - Custom color per gizmo with highlight support
//================================================================================================

// --- Constant Buffers ---

// b0: ModelBuffer (VS) - World transform
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose; // Not used for gizmos, kept for compatibility
}

// b1: ViewProjBuffer (VS) - Camera matrices
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

// b3: ColorBuffer (PS) - UUID for object picking
cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;
    uint UUID;          // Object ID for picking
}

// --- Input/Output Structures ---

struct VS_INPUT
{
    float3 position : POSITION;     // Vertex position
};

struct PS_INPUT
{
    float4 position : SV_POSITION;  // Clip-space position
    float4 color : COLOR;           // Gizmo color (from vertex shader)
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;      // Final color output
    uint UUID : SV_Target1;         // Object ID for picking
};

//================================================================================================
// Vertex Shader
//================================================================================================
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // Transform vertex position: Model -> World -> View -> Clip space
    float4x4 MVP = mul(mul(WorldMatrix, ViewMatrix), ProjectionMatrix);
    output.position = mul(float4(input.position, 1.0f), MVP);
    output.color = LerpColor;

    return output;
}

//================================================================================================
// Pixel Shader
//================================================================================================
PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT Output;

    // Gizmo is unlit - just pass through the color from vertex shader
    Output.Color = input.color;
    Output.UUID = UUID;

    return Output;
}

