// b0: ModelBuffer (VS) - Matches ModelBufferType exactly (128 bytes)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix; // 64 bytes
    row_major float4x4 WorldInverseTranspose; // 64 bytes - For correct normal transformation
};

// b1: ViewProjBuffer (VS) - Matches ViewProjBufferType
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

cbuffer ColorId : register(b3)
{
    float4 Color; // 16 bytes
    uint UUID; // 4 bytes
    float SpriteRows; // 4 bytes - 스프라이트 시트 행 수 (0이면 일반 빌보드)
    float SpriteColumns; // 4 bytes - 스프라이트 시트 열 수
    float CurrentFrame; // 4 bytes - 현재 프레임 (0 ~ TotalFrames)
    // 총 32 bytes
}
struct VS_INPUT
{
    float3 localPos : POSITION; // quad local offset (-0.5~0.5)
    float2 uv : TEXCOORD0; // per-vertex UV
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    uint UUID : SV_Target1;
};

Texture2D BillboardTex : register(t0);
SamplerState LinearSamp : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT o;
    
    // 1. 뷰별 데이터(b1)에서 InverseViewMatrix 읽기
    // LocalPos는 어차피 float3라서 Translation 안되고 스케일만 됨, 스케일 적용
    float3 posAligned = mul(mul(float4(input.localPos, 0.0f), WorldMatrix), InverseViewMatrix).xyz;
    
    // 2. 객체별 데이터(b0)의 WorldMatrix에서 월드 위치 추출
    // (row_major 행렬의 4번째 행이 위치(translation) 정보)
    float3 worldCenterPos = WorldMatrix[3].xyz;
    float3 worldPos = worldCenterPos + posAligned;

    // 3. 뷰별 데이터(b1)에서 View/Projection 행렬 읽기
    o.pos = mul(float4(worldPos, 1.0f), mul(ViewMatrix, ProjectionMatrix));
    o.uv = input.uv;
    return o;
}

PS_OUTPUT mainPS(PS_INPUT i)
{
    PS_OUTPUT Output;

    float2 uv = i.uv;

    // 스프라이트 애니메이션 적용 (SpriteRows > 0이면 스프라이트 시트 사용)
    if (SpriteRows > 0.0f && SpriteColumns > 0.0f)
    {
        int totalFrames = (int) (SpriteRows * SpriteColumns);
        int frameIndex = (int) CurrentFrame % totalFrames;
        int row = frameIndex / (int) SpriteColumns;
        int col = frameIndex % (int) SpriteColumns;
        float frameWidth = 1.0f / SpriteColumns;
        float frameHeight = 1.0f / SpriteRows;

        // UV 좌표를 현재 프레임으로 변환
        float2 frameOffset = float2(col * frameWidth, row * frameHeight);
        uv = i.uv * float2(frameWidth, frameHeight) + frameOffset;
    }

    float4 c = BillboardTex.Sample(LinearSamp, uv);
    if (c.a < 0.1f)
        discard;
    c = c * Color;
    Output.Color = c;
    Output.UUID = UUID;
    return Output;
}
