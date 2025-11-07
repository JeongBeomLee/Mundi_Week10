Texture2D SourceTexture : register(t0);
SamplerState PointSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT In) : SV_TARGET
{
    // 입력 텍스처의 UV 좌표 위치에서 색상을 그대로 샘플링하여 반환
    return SourceTexture.Sample(PointSampler, In.TexCoord);
}