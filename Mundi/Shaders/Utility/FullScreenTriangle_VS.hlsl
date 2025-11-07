// 화면을 가득 채우는 사각형(삼각형 2개)을 만드는 VS
// C++ 코드에서 DeviceContext->Draw(6, 0); 으로 호출해야 합니다.

// C++의 FViewportConstants 구조체와 데이터가 매핑됩니다.
cbuffer ViewportConstants : register(b10)
{
    // x: Viewport TopLeftX, y: Viewport TopLeftY
    // z: Viewport Width,   w: Viewport Height
    float4 ViewportRect;
    
    // x: Screen Width      (전체 렌더 타겟 너비)
    // y: Screen Height     (전체 렌더 타겟 높이)
    // z: 1.0f / Screen W,  w: 1.0f / Screen H
    float4 ScreenSize;
}

// 셰이더의 출력 구조체 정의
struct VS_OUTPUT
{
    float4 Position : SV_POSITION; // 최종 클립 공간 좌표
    float2 TexCoord : TEXCOORD0; // 뷰포트에 매핑된 UV 좌표
};

// 정점 버퍼 입력 없이, SV_VertexID를 사용하여 화면 전체를 덮는 사각형을 생성합니다.
VS_OUTPUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Out;

    // 화면을 채우는 사각형의 정점 위치와 로컬 UV 좌표 (0.0 ~ 1.0)
    // C++에서 Draw(6, 0) 호출과 일치하는 삼각형 리스트 구성
    const float2 Positions[6] =
    {
        float2(-1, 1), float2(1, 1), float2(-1, -1), // 첫 번째 삼각형
        float2(-1, -1), float2(1, 1), float2(1, -1) // 두 번째 삼각형
    };
    const float2 LocalUVs[6] =
    {
        float2(0, 0), float2(1, 0), float2(0, 1), // 첫 번째 삼각형
        float2(0, 1), float2(1, 0), float2(1, 1) // 두 번째 삼각형
    };

    // 1. 클립 공간 위치는 항상 전체 화면(-1 ~ 1)을 채우도록 설정
    Out.Position = float4(Positions[VertexID], 0.0f, 1.0f);
    float2 LocalUV = LocalUVs[VertexID];

    // 2. 뷰포트의 위치와 크기를 사용하여 최종 UV 좌표 계산
    // 뷰포트의 시작 UV 좌표 (전체 화면 기준)
    float2 ViewportStartUV = ViewportRect.xy * ScreenSize.zw;
    // 뷰포트의 UV 공간 크기 (전체 화면 기준)
    float2 ViewportUVSpan = ViewportRect.zw * ScreenSize.zw;

    // 로컬 UV를 뷰포트 영역에 맞게 스케일링하고 오프셋을 더해 최종 UV를 구함
    Out.TexCoord = ViewportStartUV + (LocalUV * ViewportUVSpan);
    
    return Out;
}