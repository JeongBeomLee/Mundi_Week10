//--------------------------------------------------------------------------------------
// 상수 버퍼 (Constant Buffer)
// CPU에서 GPU로 넘겨주는 데이터
//--------------------------------------------------------------------------------------
cbuffer FXAAParametersCB : register(b2)
{
    float2 ScreenSize; // 화면 해상도 (e.g., float2(1920.0f, 1080.0f))
    float2 InvScreenSize; // 1.0f / ScreenSize (픽셀 하나의 크기)
    float EdgeThresholdMin; // 엣지 감지 최소 휘도 차이 (0.0833f 권장 -> 1/12)
    float EdgeThresholdMax; // 엣지 감지 최대 휘도 차이 (0.166f 권장 -> 1/6)
    float QualitySubPix; // 서브픽셀 품질 (낮을수록 부드러움, 0.75 권장)
    int QualityIterations; // 엣지 탐색 반복 횟수 (12 권장)
};

//--------------------------------------------------------------------------------------
// 텍스처와 샘플러
//--------------------------------------------------------------------------------------
Texture2D g_SceneTexture : register(t0);
SamplerState g_SamplerLinear : register(s0);

//--------------------------------------------------------------------------------------
// RGB 색상에서 휘도(Luminance)를 계산하는 함수
//--------------------------------------------------------------------------------------
float RGBToLuminance(float3 InColor)
{
    // ITU-R BT.709 표준에 따른 휘도 공식 (가장 일반적)
    return dot(InColor, float3(0.2126, 0.7152, 0.0722));
}

//--------------------------------------------------------------------------------------
// FXAA Pixel Shader
//--------------------------------------------------------------------------------------
float4 mainPS(float4 Pos : SV_Position, float2 TexCoord : TEXCOORD0) : SV_Target
{
    // 1. 중심 및 주변 휘도 샘플
    float3 colorCenter = g_SceneTexture.Sample(g_SamplerLinear, TexCoord).rgb;
    float lumaCenter = RGBToLuminance(colorCenter);

    float lumaUp = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(0.0, -InvScreenSize.y)).rgb);
    float lumaDown = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(0.0, InvScreenSize.y)).rgb);
    float lumaLeft = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(-InvScreenSize.x, 0.0)).rgb);
    float lumaRight = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(InvScreenSize.x, 0.0)).rgb);

    // 2. 지역 대비 계산
    float lumaMin = min(lumaCenter, min(min(lumaUp, lumaDown), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaUp, lumaDown), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    // 3. 엣지 감지 (조기 종료)
    if (lumaRange < max(EdgeThresholdMin, lumaMax * EdgeThresholdMax))
    {
        return float4(colorCenter, 1.0f);
    }

    // 4. 엣지 방향 판단 (수평 vs 수직)
    float lumaDownLeft = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(-InvScreenSize.x, InvScreenSize.y)).rgb);
    float lumaUpRight = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(InvScreenSize.x, -InvScreenSize.y)).rgb);
    float lumaUpLeft = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(-InvScreenSize.x, -InvScreenSize.y)).rgb);
    float lumaDownRight = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + float2(InvScreenSize.x, InvScreenSize.y)).rgb);

    float edgeH = abs((lumaUpLeft + lumaUpRight) - (2.0 * lumaUp))
                + abs((lumaDownLeft + lumaDownRight) - (2.0 * lumaDown));
    float edgeV = abs((lumaUpLeft + lumaDownLeft) - (2.0 * lumaLeft))
                + abs((lumaUpRight + lumaDownRight) - (2.0 * lumaRight));

    bool isHorizontal = (edgeH >= edgeV);

    // 5. 탐색 방향 설정
    float2 dir = isHorizontal ? float2(InvScreenSize.x, 0.0) : float2(0.0, InvScreenSize.y);
    float dirSign = 0.0;

    if (isHorizontal)
    {
        dirSign = (lumaLeft > lumaRight) ? -1.0 : 1.0;
    }
    else
    {
        dirSign = (lumaUp > lumaDown) ? -1.0 : 1.0;
    }

    dir *= dirSign;

    // 6. 엣지 탐색 (min/max 갱신 포함)
    float2 posNeg = TexCoord - dir;
    float2 posPos = TexCoord + dir;
    float lumaNeg = lumaCenter;
    float lumaPos = lumaCenter;

    for (int i = 0; i < 12; i++)
    {
        lumaNeg = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, posNeg).rgb);
        lumaPos = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, posPos).rgb);

        lumaMin = min(lumaMin, min(lumaNeg, lumaPos));
        lumaMax = max(lumaMax, max(lumaNeg, lumaPos));

        if (abs(lumaNeg - lumaCenter) >= lumaRange * 0.9f &&
            abs(lumaPos - lumaCenter) >= lumaRange * 0.9f)
        {
            break;
        }

        posNeg -= dir;
        posPos += dir;
    }

    // 7. 엣지 중심 계산
    float distNeg = isHorizontal ? (TexCoord.x - posNeg.x) : (TexCoord.y - posNeg.y);
    float distPos = isHorizontal ? (posPos.x - TexCoord.x) : (posPos.y - TexCoord.y);
    float edgeDist = distNeg + distPos;
    float pixelOffset = (distPos - distNeg) / edgeDist - 0.5f;

    // 8. 서브픽셀 보정
    pixelOffset = clamp(pixelOffset * QualitySubPix, -0.5, 0.5);

    float2 offsetDir = isHorizontal ? float2(0.0, 1.0) : float2(1.0, 0.0);
    float2 finalTexCoord = TexCoord + offsetDir * pixelOffset * InvScreenSize;

    // 9. 최종 색상 샘플
    float3 finalColor = g_SceneTexture.Sample(g_SamplerLinear, finalTexCoord).rgb;
    return float4(finalColor, 1.0f);
}