//================================================================================================
// Filename:      ShadowVSM_PS.hlsl
// Description:   VSM (Variance Shadow Maps) 섀도우 맵 생성용 픽셀 쉐이더
//                depth와 depth^2를 R32G32_FLOAT 포맷으로 출력
//================================================================================================

struct PS_INPUT
{
    float4 Position : SV_POSITION;  // 화면 공간 위치
    float Depth : TEXCOORD0;        // 라이트 공간 depth (0~1)
};

// VSM 출력: (depth, depth^2)
float2 mainPS(PS_INPUT input) : SV_TARGET
{
    float depth = input.Position.z;
    depth = saturate(depth);
    float depth2 = depth * depth;

    return float2(depth, depth2);
}
