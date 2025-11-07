//================================================================================================
// Filename:      LightingCommon.hlsl
// Description:   조명 계산 공통 함수
//                매크로를 통해 GOURAUD, LAMBERT, PHONG 조명 모델 지원
//================================================================================================

// 주의: 이 파일은 다음 파일들 이후에 include 되어야 함:
// - LightStructures.hlsl
// - LightingBuffers.hlsl

//================================================================================================
// Specular 계산 방식 선택
//================================================================================================
// USE_BLINN_PHONG 정의됨: Half-vector 기반 Blinn-Phong (더 빠르고 부드러운 하이라이트)
// USE_BLINN_PHONG 주석 처리: Reflection vector 기반 전통적인 Phong (더 날카로운 하이라이트)
#define USE_BLINN_PHONG 1

//================================================================================================
// 유틸리티 함수
//================================================================================================

// 타일 인덱스 계산 (픽셀 위치로부터)
// SV_POSITION은 픽셀 중심 좌표 (0.5, 0.5 offset)
uint CalculateTileIndex(float4 screenPos)
{
    uint tileX = uint(screenPos.x) / TileSize;
    uint tileY = uint(screenPos.y) / TileSize;
    return tileY * TileCountX + tileX;
}

// 타일별 라이트 인덱스 데이터의 시작 오프셋 계산
uint GetTileDataOffset(uint tileIndex)
{
    // MaxLightsPerTile = 256 (TileLightCuller.h와 일치)
    const uint MaxLightsPerTile = 256;
    return tileIndex * MaxLightsPerTile;
}

//================================================================================================
// 기본 조명 계산 함수
//================================================================================================

// Ambient Light 계산 (OBJ/MTL 표준)
// 공식: Ambient = La × Ka
// 주의: light.Color (La)는 이미 Intensity와 Temperature가 포함됨
// ambientColor: 재질의 Ambient Color (Ka) - Diffuse Color가 아님!
float3 CalculateAmbientLight(FAmbientLightInfo light, float3 ambientColor)
{
    // OBJ/MTL 표준: La × Ka
    return light.Color.rgb * ambientColor;
}

// Diffuse Light 계산 (Lambert)
// 제공된 materialColor 사용 (텍스처가 있으면 포함됨)
// 주의: lightColor는 이미 Intensity가 포함됨 (C++에서 계산)
float3 CalculateDiffuse(float3 lightDir, float3 normal, float4 lightColor, float4 materialColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0f);
    // materialColor를 직접 사용 (이미 텍스처가 포함되어 있으면 포함됨)
    return lightColor.rgb * materialColor.rgb * NdotL;
}

// Specular 색상 결정 매크로
// 각 셰이더에서 include 전에 이 매크로를 정의하여 커스터마이즈 가능
// 기본값: 흰색 스페큘러 (1,1,1)
#ifndef SPECULAR_COLOR
    #define SPECULAR_COLOR float3(1.0f, 1.0f, 1.0f)
#endif

// Specular Light 계산 함수
// USE_BLINN_PHONG 매크로에 따라 Blinn-Phong 또는 전통적인 Phong 방식 사용
// 주의: lightColor는 이미 Intensity가 포함됨 (C++에서 계산)
// 주의: SPECULAR_COLOR 매크로로 specular 색상 제어 (기본: 흰색)
float3 CalculateSpecular(float3 lightDir, float3 normal, float3 viewDir, float4 lightColor, float specularPower)
{
    float3 specularColor = SPECULAR_COLOR;

#ifdef USE_BLINN_PHONG
    // Blinn-Phong 방식: Half-vector 기반 (더 빠르고 부드러운 하이라이트)
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfVec), 0.0f);
    float specular = pow(NdotH, specularPower);
    return lightColor.rgb * specularColor * specular;
#else
    // 전통적인 Phong 방식: Reflection vector 기반 (더 날카로운 하이라이트)
    float3 reflectDir = reflect(-lightDir, normal);
    float RdotV = max(dot(reflectDir, viewDir), 0.0f);
    float specular = pow(RdotV, specularPower);
    return lightColor.rgb * specularColor * specular;
#endif
}

//================================================================================================
// 감쇠(Attenuation) 함수
//================================================================================================

// Unreal Engine: Inverse Square Falloff (물리 기반)
// https://docs.unrealengine.com/en-US/BuildingWorlds/LightingAndShadows/PhysicalLightUnits/
float CalculateInverseSquareFalloff(float distance, float attenuationRadius)
{
    // Unreal Engine의 역제곱 감쇠 with 부드러운 윈도우 함수
    float distanceSq = distance * distance;
    float radiusSq = attenuationRadius * attenuationRadius;

    // 기본 역제곱 법칙: I = 1 / (distance^2)
    float basicFalloff = 1.0f / max(distanceSq, 0.01f * 0.01f); // 0으로 나누기 방지

    // attenuationRadius에서 0에 도달하는 부드러운 윈도우 함수 적용
    // 반경 경계에서 급격한 차단을 방지
    float distanceRatio = saturate(distance / attenuationRadius);
    float windowAttenuation = pow(1.0f - pow(distanceRatio, 4.0f), 2.0f);

    return basicFalloff * windowAttenuation;
}

// Unreal Engine: Light Falloff Exponent (예술적 제어)
// 물리적으로 정확하지 않지만 예술적 제어 제공
float CalculateExponentFalloff(float distance, float attenuationRadius, float falloffExponent)
{
    // 정규화된 거리 (라이트 중심에서 0, 반경 경계에서 1)
    float distanceRatio = saturate(distance / attenuationRadius);

    // 단순 지수 기반 감쇠: attenuation = (1 - ratio)^exponent
    // falloffExponent가 곡선을 제어:
    // - exponent = 1: 선형 감쇠
    // - exponent > 1: 더 빠른 감쇠 (더 날카로움)
    // - exponent < 1: 더 느린 감쇠 (더 부드러움)
    float attenuation = pow(1.0f - pow(distanceRatio, 2.0f), max(falloffExponent, 0.1f));

    return attenuation;
}

//================================================================================================
// Shadow Mapping Functions
//================================================================================================

// Poisson Disk 샘플링 패턴 (8x8 = 64 샘플)
// 고품질 PCF를 위한 랜덤 분포 패턴
static const float2 PoissonDisk64[64] =
{
    float2(-0.613392f, 0.617481f), float2(0.170019f, -0.040254f), float2(-0.299417f, 0.791925f), float2(0.645680f, 0.493210f),
    float2(-0.651784f, 0.717887f), float2(0.421003f, 0.027070f), float2(-0.817194f, -0.271096f), float2(-0.705374f, -0.668203f),
    float2(0.977050f, -0.108615f), float2(0.063326f, 0.142369f), float2(0.203528f, 0.214331f), float2(-0.667531f, 0.326090f),
    float2(-0.098422f, -0.295755f), float2(-0.885922f, 0.215369f), float2(0.566637f, 0.605213f), float2(0.039766f, -0.396100f),
    float2(0.751946f, 0.453352f), float2(0.078707f, -0.715323f), float2(-0.075838f, -0.529344f), float2(0.724479f, -0.580798f),
    float2(0.222999f, -0.215125f), float2(-0.467574f, -0.405438f), float2(-0.248268f, -0.814753f), float2(0.354411f, -0.887570f),
    float2(0.175817f, 0.382366f), float2(0.487472f, -0.063082f), float2(-0.084078f, 0.898312f), float2(0.488876f, -0.783441f),
    float2(0.470016f, 0.217933f), float2(-0.696890f, -0.549791f), float2(-0.149693f, 0.605762f), float2(0.034211f, 0.979980f),
    float2(0.503098f, -0.308878f), float2(-0.016205f, -0.872921f), float2(0.385784f, -0.393902f), float2(-0.146886f, -0.859249f),
    float2(0.643361f, 0.164098f), float2(0.634388f, -0.049471f), float2(-0.688894f, 0.007843f), float2(0.464034f, -0.188818f),
    float2(-0.440840f, 0.137486f), float2(0.364483f, 0.511704f), float2(0.034028f, 0.325968f), float2(0.099094f, -0.308023f),
    float2(0.693960f, -0.366253f), float2(0.678884f, -0.204688f), float2(0.001801f, 0.780328f), float2(0.145177f, -0.898984f),
    float2(0.062655f, -0.611866f), float2(0.315226f, -0.604297f), float2(-0.780145f, 0.486251f), float2(-0.371868f, 0.882138f),
    float2(0.200476f, 0.494430f), float2(-0.494552f, -0.711051f), float2(0.612476f, 0.705252f), float2(-0.578845f, -0.768792f),
    float2(-0.772454f, -0.090976f), float2(0.504440f, 0.372295f), float2(0.155736f, 0.065157f), float2(0.391522f, 0.849605f),
    float2(-0.620106f, -0.328104f), float2(0.789239f, -0.419965f), float2(-0.545396f, 0.538133f), float2(-0.178564f, -0.596057f)
};

/**
 * PCF (Percentage Closer Filtering) 샘플링
 * 소프트웨어 기반 다중 샘플링으로 부드러운 그림자 생성
 *
 * @param shadowMap - 샘플링할 섀도우 맵
 * @param shadowTexCoord - 섀도우 맵 텍스처 좌표
 * @param shadowMapIndex - 섀도우 맵 배열 인덱스
 * @param currentDepth - 현재 깊이 값
 * @param sampleCount - PCF 샘플 수 (3, 4, 5 등)
 * @param texelSize - 섀도우 맵의 텍셀 크기 (1.0 / resolution)
 * @return 섀도우 값 (0.0 = 완전한 그림자, 1.0 = 완전히 밝음)
 */
float SamplePCF_2DArray(Texture2DArray shadowMap, float2 shadowTexCoord, uint shadowMapIndex, float currentDepth, uint sampleCount, float texelSize)
{
    float shadow = 0.0f;
    uint totalSamples = sampleCount * sampleCount;
    float halfSample = float(sampleCount) * 0.5f;

    // Poisson disk 인덱스 오프셋 (sampleCount에 따라 다른 패턴 사용)
    uint poissonStart = min(sampleCount * sampleCount, 64) % 64;

    for (uint i = 0; i < totalSamples; ++i)
    {
        // Poisson disk 샘플 가져오기
        uint poissonIndex = (poissonStart + i) % 64;
        float2 offset = PoissonDisk64[poissonIndex] * texelSize * halfSample;

        float2 sampleCoord = shadowTexCoord + offset;
        float3 samplePos = float3(sampleCoord, shadowMapIndex);

        // Comparison sampler를 사용하여 깊이 비교
        shadow += shadowMap.SampleCmpLevelZero(g_ShadowSampler, samplePos, currentDepth);
    }

    return shadow / float(totalSamples);
}

/**
 * PCF (Percentage Closer Filtering) 샘플링 - TextureCubeArray용
 *
 * @param shadowCubeMap - 샘플링할 큐브 섀도우 맵
 * @param direction - 샘플링 방향
 * @param shadowMapIndex - 섀도우 맵 배열 인덱스
 * @param currentDepth - 현재 깊이 값
 * @param sampleCount - PCF 샘플 수
 * @param texelSize - 섀도우 맵의 텍셀 크기
 * @return 섀도우 값
 */
float SamplePCF_CubeArray(TextureCubeArray shadowCubeMap, float3 direction, uint shadowMapIndex, float currentDepth, uint sampleCount, float texelSize)
{
    float shadow = 0.0f;
    uint totalSamples = sampleCount * sampleCount;
    float halfSample = float(sampleCount) * 0.5f;

    // 큐브맵 샘플링을 위한 tangent/bitangent 계산
    float3 absDir = abs(direction);
    float3 tangent, bitangent;

    if (absDir.x > absDir.y && absDir.x > absDir.z)
    {
        tangent = float3(0.0f, 1.0f, 0.0f);
    }
    else if (absDir.y > absDir.z)
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }

    bitangent = normalize(cross(direction, tangent));
    tangent = normalize(cross(bitangent, direction));

    uint poissonStart = min(sampleCount * sampleCount, 64) % 64;

    for (uint i = 0; i < totalSamples; ++i)
    {
        uint poissonIndex = (poissonStart + i) % 64;
        float2 diskOffset = PoissonDisk64[poissonIndex] * texelSize * halfSample;

        // 큐브맵 방향 오프셋 적용
        float3 sampleDir = normalize(direction + tangent * diskOffset.x + bitangent * diskOffset.y);
        float4 samplePos = float4(sampleDir, shadowMapIndex);

        shadow += shadowCubeMap.SampleCmpLevelZero(g_ShadowSampler, samplePos, currentDepth);
    }

    return shadow / float(totalSamples);
}

//================================================================================================
// VSM/ESM/EVSM 헬퍼 함수들
//================================================================================================

/**
 * Gaussian 가중치 계산
 * @param distance - 중심으로부터의 거리
 * @param sigma - Gaussian 분포의 표준편차
 * @return Gaussian 가중치 값
 */
float GaussianWeight(float distance, float sigma)
{
    float variance = sigma * sigma;
    return exp(-(distance * distance) / (2.0f * variance));
}

/**
 * VSM (Variance Shadow Maps) 샘플링
 * Chebyshev 부등식을 사용하여 섀도우 확률 계산
 * @param moments - float2(mean, variance) 또는 float2(depth, depth^2)
 * @param depth - 현재 픽셀의 depth
 * @param minVariance - 최소 분산값 (수치 불안정성 방지)
 * @param lightBleedingReduction - Light bleeding 감소 파라미터 (0~1)
 */
float ChebyshevUpperBound(float2 moments, float depth, float minVariance, float lightBleedingReduction)
{
    // depth가 평균보다 작으면 완전히 밝음
    float mean = moments.x;
    if (depth <= mean)
        return 1.0f;

    // 분산 계산
    float variance = moments.y - (mean * mean);
    variance = max(variance, minVariance); // 수치 불안정성 방지

    // Chebyshev 부등식: P(x >= t) <= variance / (variance + (t - mean)^2)
    float d = depth - mean;
    float p_max = variance / (variance + d * d);

    // Light bleeding reduction 적용
    p_max = saturate((p_max - lightBleedingReduction) / (1.0f - lightBleedingReduction));

    return p_max;
}

/**
 * ESM (Exponential Shadow Maps) 샘플링
 * @param expDepth - exp(c * occluder_depth)
 * @param depth - 현재 픽셀의 depth
 * @param exponent - ESM exponential 계수
 */
float SampleESM(float expDepth, float depth, float exponent)
{
    // shadow = exp(-c * (depth - occluder_depth))
    //        = exp(-c * depth) * exp(c * occluder_depth)
    float shadow = expDepth * exp(-exponent * depth);
    return saturate(shadow);
}

/**
 * EVSM (Exponential Variance Shadow Maps) 샘플링
 * Positive와 Negative exponential의 조합
 * @param moments - float2(exp(c+ * depth), exp(-c- * depth))
 * @param depth - 현재 픽셀의 depth
 * @param positiveExponent - c+ (positive exponent)
 * @param negativeExponent - c- (negative exponent)
 * @param lightBleedingReduction - Light bleeding 감소 파라미터
 */
float SampleEVSM(float2 moments, float depth, float positiveExponent, float negativeExponent, float lightBleedingReduction)
{
    // Positive와 Negative 두 가지 exponential depth 사용
    float pos = exp(positiveExponent * depth);
    float neg = exp(-negativeExponent * depth);
    
    // Occluder moments 구성 (섀도우 맵에서 읽은 exp 값으로 moments 생성)
    // blur 없이는 variance = 0이지만, 필터링 후에는 유효한 분산 생성
    float2 posOccluderMoments = float2(moments.x, moments.x * moments.x);
    float2 negOccluderMoments = float2(moments.y, moments.y * moments.y);

    // Chebyshev for positive (minVariance 증가로 테두리 부드럽게)
    float posContrib = ChebyshevUpperBound(posOccluderMoments, pos, 0.001f, lightBleedingReduction);

    // Chebyshev for negative (minVariance 증가로 테두리 부드럽게)
    float negContrib = ChebyshevUpperBound(negOccluderMoments, neg, 0.001f, lightBleedingReduction);

    // 두 결과의 최솟값
    return min(posContrib, negContrib);
}

/**
 * Gaussian Multi-tap 샘플링 (Texture2DArray용)
 * 주변 픽셀들을 Gaussian 가중치로 샘플링하여 평균화
 * VSM, ESM, EVSM 모두에 사용 가능
 *
 * @param shadowMap - 섀도우 맵 (R32_FLOAT 또는 R32G32_FLOAT)
 * @param shadowTexCoord - 기본 텍스처 좌표
 * @param shadowMapIndex - 배열 인덱스
 * @param texelSize - 1.0 / shadowMapResolution
 * @param sampleRadius - 샘플링 반경 (픽셀 단위, 기본 2.0)
 * @param sigma - Gaussian 표준편차 (기본 1.5)
 * @return float2 - 가중 평균된 moments
 */
float2 SampleGaussian_2DArray(Texture2DArray shadowMap, float2 shadowTexCoord, uint shadowMapIndex, float texelSize, float sampleRadius, float sigma)
{
    float2 moments = float2(0.0f, 0.0f);
    float totalWeight = 0.0f;

    const uint sampleCount = 32;

    for (uint i = 0; i < sampleCount; ++i)
    {
        // Poisson Disk 오프셋 가져오기
        float2 offset = PoissonDisk64[i] * texelSize * sampleRadius;

        // 오프셋 거리 계산
        float distance = length(offset / texelSize);

        // Gaussian 가중치 계산
        float weight = GaussianWeight(distance, sigma);

        // 샘플 좌표 계산
        float2 sampleCoord = shadowTexCoord + offset;
        float3 samplePos = float3(sampleCoord, shadowMapIndex);

        // VSM moments 샘플링
        float2 sampleMoments = shadowMap.SampleLevel(g_LinearSampler, samplePos, 0).rg;

        // 가중치 적용하여 누적
        moments += sampleMoments * weight;
        totalWeight += weight;
    }

    // 정규화 (총 가중치로 나누기)
    moments /= totalWeight;

    return moments;
}

/**
 * PCF-like Multi-tap EVSM 샘플링 (Texture2DArray용)
 * 그림자 경계를 부드럽게 만들기 위해 여러 위치에서 EVSM을 계산하고 평균화
 *
 * @param shadowMap - EVSM 섀도우 맵
 * @param shadowTexCoord - 텍스처 좌표
 * @param shadowMapIndex - 배열 인덱스
 * @param currentDepth - 현재 깊이
 * @param positiveExponent - EVSM positive exponent
 * @param negativeExponent - EVSM negative exponent
 * @param lightBleedingReduction - Light bleeding 감소 파라미터
 * @param texelSize - 1.0 / resolution
 * @param sampleRadius - 샘플링 반경 (픽셀 단위)
 * @return 평균화된 섀도우 값
 */
float SampleEVSM_PCF_2DArray(
    Texture2DArray shadowMap,
    float2 shadowTexCoord,
    uint shadowMapIndex,
    float currentDepth,
    float positiveExponent,
    float negativeExponent,
    float lightBleedingReduction,
    float texelSize,
    float sampleRadius)
{
    float shadowSum = 0.0f;
    const uint sampleCount = 32;

    for (uint i = 0; i < sampleCount; ++i)
    {
        // Poisson Disk 패턴으로 오프셋 계산
        float2 offset = PoissonDisk64[i] * texelSize * sampleRadius;
        float2 sampleCoord = shadowTexCoord + offset;
        float3 samplePos = float3(sampleCoord, shadowMapIndex);

        // 해당 위치의 EVSM moments 샘플링
        float2 moments = shadowMap.SampleLevel(g_LinearSampler, samplePos, 0).rg;

        // 각 샘플 위치에서 EVSM 계산
        float sampleShadow = SampleEVSM(moments, currentDepth, positiveExponent, negativeExponent, lightBleedingReduction);

        // 누적
        shadowSum += sampleShadow;
    }

    // 평균 반환
    return shadowSum / float(sampleCount);
}

/**
 * Gaussian Multi-tap 샘플링 (TextureCubeArray용)
 * 주변 픽셀들을 Gaussian 가중치로 샘플링하여 평균화
 * VSM, ESM, EVSM 모두에 사용 가능
 *
 * @param shadowCubeMap - 섀도우 큐브맵 (R32_FLOAT 또는 R32G32_FLOAT)
 * @param direction - 샘플링 방향
 * @param shadowMapIndex - 배열 인덱스
 * @param texelSize - 1.0 / shadowMapResolution
 * @param sampleRadius - 샘플링 반경 (픽셀 단위, 기본 2.0)
 * @param sigma - Gaussian 표준편차 (기본 1.5)
 * @return float2 - 가중 평균된 moments
 */
float2 SampleGaussian_CubeArray(TextureCubeArray shadowCubeMap, float3 direction, uint shadowMapIndex, float texelSize, float sampleRadius, float sigma)
{
    float2 moments = float2(0.0f, 0.0f);
    float totalWeight = 0.0f;

    // 큐브맵 샘플링을 위한 tangent/bitangent 계산
    float3 absDir = abs(direction);
    float3 tangent, bitangent;

    if (absDir.x > absDir.y && absDir.x > absDir.z)
    {
        tangent = float3(0.0f, 1.0f, 0.0f);
    }
    else if (absDir.y > absDir.z)
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }

    bitangent = normalize(cross(direction, tangent));
    tangent = normalize(cross(bitangent, direction));

    const uint sampleCount = 32;

    for (uint i = 0; i < sampleCount; ++i)
    {
        // Poisson Disk 오프셋 가져오기
        float2 diskOffset = PoissonDisk64[i] * texelSize * sampleRadius;

        // 오프셋 거리 계산
        float distance = length(diskOffset);

        // Gaussian 가중치 계산
        float weight = GaussianWeight(distance, sigma);

        // 큐브맵 방향 오프셋 적용
        float3 sampleDir = normalize(direction + tangent * diskOffset.x + bitangent * diskOffset.y);
        float4 samplePos = float4(sampleDir, shadowMapIndex);

        // VSM moments 샘플링
        float2 sampleMoments = shadowCubeMap.SampleLevel(g_LinearSampler, samplePos, 0).rg;

        // 가중치 적용하여 누적
        moments += sampleMoments * weight;
        totalWeight += weight;
    }

    // 정규화 (총 가중치로 나누기)
    moments /= totalWeight;

    return moments;
}

/**
 * PCF-like Multi-tap EVSM 샘플링 (TextureCubeArray용)
 * 그림자 경계를 부드럽게 만들기 위해 여러 위치에서 EVSM을 계산하고 평균화
 *
 * @param shadowCubeMap - EVSM 섀도우 큐브맵
 * @param direction - 샘플링 방향
 * @param shadowMapIndex - 배열 인덱스
 * @param currentDepth - 현재 깊이
 * @param positiveExponent - EVSM positive exponent
 * @param negativeExponent - EVSM negative exponent
 * @param lightBleedingReduction - Light bleeding 감소 파라미터
 * @param texelSize - 1.0 / resolution
 * @param sampleRadius - 샘플링 반경 (픽셀 단위)
 * @return 평균화된 섀도우 값
 */
float SampleEVSM_PCF_CubeArray(
    TextureCubeArray shadowCubeMap,
    float3 direction,
    uint shadowMapIndex,
    float currentDepth,
    float positiveExponent,
    float negativeExponent,
    float lightBleedingReduction,
    float texelSize,
    float sampleRadius)
{
    float shadowSum = 0.0f;
    const uint sampleCount = 32; // 16개 샘플로 부드러운 경계 생성

    // 큐브맵 샘플링을 위한 tangent/bitangent 계산
    float3 absDir = abs(direction);
    float3 tangent, bitangent;

    if (absDir.x > absDir.y && absDir.x > absDir.z)
    {
        tangent = float3(0.0f, 1.0f, 0.0f);
    }
    else if (absDir.y > absDir.z)
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }

    bitangent = normalize(cross(direction, tangent));
    tangent = normalize(cross(bitangent, direction));

    for (uint i = 0; i < sampleCount; ++i)
    {
        // Poisson Disk 패턴으로 오프셋 계산
        float2 diskOffset = PoissonDisk64[i] * texelSize * sampleRadius;

        // 큐브맵 방향 오프셋 적용
        float3 sampleDir = normalize(direction + tangent * diskOffset.x + bitangent * diskOffset.y);
        float4 samplePos = float4(sampleDir, shadowMapIndex);

        // 해당 위치의 EVSM moments 샘플링
        float2 moments = shadowCubeMap.SampleLevel(g_LinearSampler, samplePos, 0).rg;

        // 각 샘플 위치에서 EVSM 계산
        float sampleShadow = SampleEVSM(moments, currentDepth, positiveExponent, negativeExponent, lightBleedingReduction);

        // 누적
        shadowSum += sampleShadow;
    }

    // 평균 반환
    return shadowSum / float(sampleCount);
}

//================================================================================================
// Shadow Map 샘플링 함수들
//================================================================================================

/**
 * Sample SpotLight shadow map
 * Returns 0.0 if in shadow, 1.0 if lit
 *
 * @param shadowMapIndex - Index into shadow map array
 * @param lightSpacePos - Position in light's clip space
 * @param worldNormal - World space normal for slope-scaled bias
 * @param lightDir - Light direction for slope calculation
 * @param shadowMapResolution - 섀도우 맵 해상도 (PCF texelSize 계산용)
 */
float SampleSpotLightShadowMap(uint shadowMapIndex, float4 lightSpacePos, float3 worldNormal, float3 lightDir, float shadowMapResolution)
{
    // Perspective divide to get NDC coordinates
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Convert NDC [-1, 1] to texture coordinates [0, 1]
    float2 shadowTexCoord;
    shadowTexCoord.x = projCoords.x * 0.5f + 0.5f;
    shadowTexCoord.y = -projCoords.y * 0.5f + 0.5f; // Flip Y for D3D

    // Check if position is outside shadow map bounds
    if (shadowTexCoord.x < 0.0f || shadowTexCoord.x > 1.0f ||
        shadowTexCoord.y < 0.0f || shadowTexCoord.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f; // Outside shadow map = not shadowed
    }

    // Current depth in light space
    float currentDepth = projCoords.z;

    // 필터링 타입에 따라 분기
    float shadow = 1.0f;

    [branch]
    if (FilterType == 0) // NONE
    {
        // 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;
        
        // 하드웨어 기본 비교 샘플링
        float3 shadowSampleCoord = float3(shadowTexCoord, shadowMapIndex);
        shadow = g_SpotLightShadowMaps.SampleCmpLevelZero(g_ShadowSampler, shadowSampleCoord, biasedDepth);
    }
    else if (FilterType == 1) // PCF
    {
        // 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;
        
        // 소프트웨어 PCF
        uint sampleCount = (PCFSampleCount == 0) ? PCFCustomSampleCount : PCFSampleCount;
        float texelSize = 1.0f / shadowMapResolution;
        shadow = SamplePCF_2DArray(g_SpotLightShadowMaps, shadowTexCoord, shadowMapIndex, biasedDepth, sampleCount, texelSize);
    }
    else if (FilterType == 2) // VSM
    {
        // VSM: Gaussian Multi-tap 샘플링
        float texelSize = 1.0f / shadowMapResolution;
        float2 moments = SampleGaussian_2DArray(
            g_SpotLightShadowMaps_Float,
            shadowTexCoord,
            shadowMapIndex,
            texelSize,
            5.1f,  // sampleRadius
            2.0f   // sigma
        );
        shadow = ChebyshevUpperBound(moments, currentDepth, VSMMinVariance, VSMLightBleedingReduction);
    }
    else if (FilterType == 3) // ESM
    {
        // ESM: Gaussian Multi-tap 샘플링
        float texelSize = 1.0f / shadowMapResolution;
        float2 moments = SampleGaussian_2DArray(
            g_SpotLightShadowMaps_Float,
            shadowTexCoord,
            shadowMapIndex,
            texelSize,
            5.1f,  // sampleRadius
            2.0f   // sigma
        );
        // moments.r에 exp(c * depth)가 저장되어 있음
        shadow = SampleESM(moments.r, currentDepth, ESMExponent);
    }
    else if (FilterType == 4) // EVSM
    {
        // 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.0001f, 0.0005f, NdotL);
        float biasedDepth = currentDepth - bias;

        // EVSM: PCF-like Multi-tap 샘플링으로 테두리 부드럽게
        float texelSize = 1.0f / shadowMapResolution;
        shadow = SampleEVSM_PCF_2DArray(
            g_SpotLightShadowMaps_Float,
            shadowTexCoord,
            shadowMapIndex,
            biasedDepth,
            EVSMPositiveExponent,
            EVSMNegativeExponent,
            EVSMLightBleedingReduction,
            texelSize,
            2.5f  // sampleRadius: 부드러운 경계를 위한 반경
        );
    }

    return shadow;
}

/**
 * Sample DirectionalLight shadow map
 * Returns 0.0 if in shadow, 1.0 if lit
 *
 * @param shadowMapIndex - Index into shadow map array
 * @param lightSpacePos - Position in light's clip space
 * @param worldNormal - World space normal for slope-scaled bias
 * @param lightDir - Light direction for slope calculation
 * @param shadowMapResolution - 섀도우 맵 해상도 (PCF texelSize 계산용)
 */
float SampleDirectionalLightShadowMap(uint shadowMapIndex, float4 lightSpacePos, float3 worldNormal, float3 lightDir, float shadowMapResolution)
{
    // Perspective divide to get NDC coordinates
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Convert NDC [-1, 1] to texture coordinates [0, 1]
    float2 shadowTexCoord;
    shadowTexCoord.x = projCoords.x * 0.5f + 0.5f;
    shadowTexCoord.y = -projCoords.y * 0.5f + 0.5f; // Flip Y for D3D

    // Check if position is outside shadow map bounds
    if (shadowTexCoord.x < 0.0f || shadowTexCoord.x > 1.0f ||
        shadowTexCoord.y < 0.0f || shadowTexCoord.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f; // Outside shadow map = not shadowed
    }

    // Current depth in light space
    float currentDepth = projCoords.z;

    // 필터링 타입에 따라 분기
    float shadow = 1.0f;

    [branch]
    if (FilterType == 0) // NONE
    {
        // 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;
        
        // 하드웨어 기본 비교 샘플링
        float3 shadowSampleCoord = float3(shadowTexCoord, shadowMapIndex);
        shadow = g_DirectionalLightShadowMaps.SampleCmpLevelZero(g_ShadowSampler, shadowSampleCoord, biasedDepth);
    }
    else if (FilterType == 1) // PCF
    {
        // 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;
        
        // 소프트웨어 PCF
        uint sampleCount = (PCFSampleCount == 0) ? PCFCustomSampleCount : PCFSampleCount;
        float texelSize = 1.0f / shadowMapResolution;
        shadow = SamplePCF_2DArray(g_DirectionalLightShadowMaps, shadowTexCoord, shadowMapIndex, biasedDepth, sampleCount, texelSize);
    }
    else if (FilterType == 2) // VSM
    {
        // VSM: Gaussian Multi-tap 샘플링
        float texelSize = 1.0f / shadowMapResolution;
        float2 moments = SampleGaussian_2DArray(
            g_DirectionalLightShadowMaps_Float,
            shadowTexCoord,
            shadowMapIndex,
            texelSize,
            5.1f,  // sampleRadius
            2.0f   // sigma
        );
        shadow = ChebyshevUpperBound(moments, currentDepth, VSMMinVariance, VSMLightBleedingReduction);
    }
    else if (FilterType == 3) // ESM
    {
        // ESM: Gaussian Multi-tap 샘플링
        float texelSize = 1.0f / shadowMapResolution;
        float2 moments = SampleGaussian_2DArray(
            g_DirectionalLightShadowMaps_Float,
            shadowTexCoord,
            shadowMapIndex,
            texelSize,
            5.1f,  // sampleRadius
            2.0f   // sigma
        );
        shadow = SampleESM(moments.r, currentDepth, ESMExponent);
    }
    else if (FilterType == 4) // EVSM
    {
        // EVSM: PCF-like Multi-tap 샘플링으로 테두리 부드럽게
        float texelSize = 1.0f / shadowMapResolution;
        shadow = SampleEVSM_PCF_2DArray(
            g_DirectionalLightShadowMaps_Float,
            shadowTexCoord,
            shadowMapIndex,
            currentDepth,
            EVSMPositiveExponent,
            EVSMNegativeExponent,
            EVSMLightBleedingReduction,
            texelSize,
            2.5f  // sampleRadius: 부드러운 경계를 위한 반경
        );
    }

    return shadow;
}

/**
 * Sample PointLight shadow map using LightViewProjection matrices
 * Returns 0.0 if in shadow, 1.0 if lit
 *
 * @param light - Point light information (includes 6 VP matrices)
 * @param worldPos - World position
 * @param worldNormal - World space normal for slope-scaled bias
 * @param shadowMapResolution - 섀도우 맵 해상도 (PCF texelSize 계산용)
 */
float SamplePointLightShadowMap(FPointLightInfo light, float3 worldPos, float3 worldNormal, float shadowMapResolution)
{
    // 1. 라이트에서 픽셀로의 방향 벡터 계산
    float3 lightToPixel = worldPos - light.Position;
    float3 lightDir = normalize(-lightToPixel); // 픽셀에서 라이트로의 방향

    // 2. 방향에 따라 큐브맵 면 선택 (0-5)
    // Cube Map Face 순서: +X(0), -X(1), +Y(2), -Y(3), +Z(4), -Z(5)
    float3 absDir = abs(lightToPixel);
    uint faceIndex = 0;

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        // X축이 가장 큼
        faceIndex = lightToPixel.x > 0.0f ? 0 : 1; // +X or -X
    }
    else if (absDir.y >= absDir.z)
    {
        // Y축이 가장 큼
        faceIndex = lightToPixel.y > 0.0f ? 2 : 3; // +Y or -Y
    }
    else
    {
        // Z축이 가장 큼
        faceIndex = lightToPixel.z > 0.0f ? 4 : 5; // +Z or -Z
    }

    // 3. 선택된 LightViewProjection으로 LightSpacePos 계산
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), light.LightViewProjection[faceIndex]);

    // 4. Perspective divide to get NDC coordinates
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // 5. Convert NDC [-1, 1] to texture coordinates [0, 1]
    float2 shadowTexCoord;
    shadowTexCoord.x = projCoords.x * 0.5f + 0.5f;
    shadowTexCoord.y = -projCoords.y * 0.5f + 0.5f; // Flip Y for D3D

    // 6. Check if position is outside shadow map bounds
    if (shadowTexCoord.x < 0.0f || shadowTexCoord.x > 1.0f ||
        shadowTexCoord.y < 0.0f || shadowTexCoord.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f; // Outside shadow map = not shadowed
    }

    // 7. Current depth in light space
    float currentDepth = projCoords.z;

    // 9. 필터링 타입에 따라 분기
    float shadow = 1.0f;

    [branch]
    if (FilterType == 0) // NONE
    {
        // Slope-scaled bias: 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;
        
        // 하드웨어 기본 비교 샘플링
        float4 cubeSampleCoord = float4(lightToPixel, light.ShadowMapIndex);
        shadow = g_PointLightShadowCubeMaps.SampleCmpLevelZero(g_ShadowSampler, cubeSampleCoord, biasedDepth);
    }
    else if (FilterType == 1) // PCF
    {
        // Slope-scaled bias: 표면 기울기에 따라 bias 자동 조절
        float NdotL = saturate(dot(worldNormal, lightDir));
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;
        
        // 소프트웨어 PCF
        uint sampleCount = (PCFSampleCount == 0) ? PCFCustomSampleCount : PCFSampleCount;
        float texelSize = 1.0f / shadowMapResolution;
        shadow = SamplePCF_CubeArray(g_PointLightShadowCubeMaps, lightToPixel, light.ShadowMapIndex, biasedDepth, sampleCount, texelSize);
    }
    else if (FilterType == 2) // VSM
    {
        // VSM: Gaussian Multi-tap 샘플링
        float texelSize = 1.0f / shadowMapResolution;
        float2 moments = SampleGaussian_CubeArray(
            g_PointLightShadowCubeMaps_Float,
            lightToPixel,
            light.ShadowMapIndex,
            texelSize,
            5.1f,  // sampleRadius
            2.0f   // sigma
        );
        shadow = ChebyshevUpperBound(moments, currentDepth, VSMMinVariance, VSMLightBleedingReduction);
    }
    else if (FilterType == 3) // ESM
    {
        // ESM: Gaussian Multi-tap 샘플링
        float texelSize = 1.0f / shadowMapResolution;
        float2 moments = SampleGaussian_CubeArray(
            g_PointLightShadowCubeMaps_Float,
            lightToPixel,
            light.ShadowMapIndex,
            texelSize,
            5.1f,  // sampleRadius
            2.0f   // sigma
        );
        shadow = SampleESM(moments.r, currentDepth, ESMExponent);
    }
    else if (FilterType == 4) // EVSM
    {
        // EVSM: PCF-like Multi-tap 샘플링으로 테두리 부드럽게
        float texelSize = 1.0f / shadowMapResolution;
        shadow = SampleEVSM_PCF_CubeArray(
            g_PointLightShadowCubeMaps_Float,
            lightToPixel,
            light.ShadowMapIndex,
            currentDepth,
            EVSMPositiveExponent,
            EVSMNegativeExponent,
            EVSMLightBleedingReduction,
            texelSize,
            2.5f  // sampleRadius: 부드러운 경계를 위한 반경
        );
    }

    return shadow;
}

//================================================================================================
// 통합 조명 계산 함수
//================================================================================================

float SampleDirectionalLightShadowMapCSM(FCascadeTierInfo tierInfo, float4 lightSpacePos, float3 worldNormal, float3 lightDir)
{
    // Perspective divide to get NDC coordinates
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    // Convert NDC [-1, 1] to texture coordinates [0, 1]
    float2 shadowTexCoord;
    shadowTexCoord.x = projCoords.x * 0.5f + 0.5f;
    shadowTexCoord.y = -projCoords.y * 0.5f + 0.5f;

    // Check if position is outside shadow map bounds
    if (shadowTexCoord.x < 0.0f || shadowTexCoord.x > 1.0f ||
        shadowTexCoord.y < 0.0f || shadowTexCoord.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    // Current depth in light space
    float currentDepth = projCoords.z;
    float shadow = 1.0f;

    // Get data from tierInfo
    uint shadowMapIndex = tierInfo.SliceIndex;
    float shadowMapResolution = tierInfo.Resolution;
    float texelSize = 1.0f / shadowMapResolution;

    // Slope-scaled bias
    float NdotL = saturate(dot(worldNormal, lightDir));
    
    [branch]
    if (FilterType == 0 || FilterType == 1) // NONE, PCF
    {
        float bias = lerp(0.00001f, 0.00005f, NdotL);
        float biasedDepth = currentDepth - bias;

        if (FilterType == 0) // NONE
        {
            float3 shadowSampleCoord = float3(shadowTexCoord, shadowMapIndex);
            [branch]
            if (tierInfo.TierIndex == 0)
                shadow = g_DirectionalShadowMaps_CSM_LowTier.SampleCmpLevelZero(g_ShadowSampler, shadowSampleCoord, biasedDepth);
            else if (tierInfo.TierIndex == 1)
                shadow = g_DirectionalShadowMaps_CSM_MediumTier.SampleCmpLevelZero(g_ShadowSampler, shadowSampleCoord, biasedDepth);
            else
                shadow = g_DirectionalShadowMaps_CSM_HighTier.SampleCmpLevelZero(g_ShadowSampler, shadowSampleCoord, biasedDepth);
        }
        else // PCF
        {
            uint sampleCount = (PCFSampleCount == 0) ? PCFCustomSampleCount : PCFSampleCount;
            [branch]
            if (tierInfo.TierIndex == 0)
                shadow = SamplePCF_2DArray(g_DirectionalShadowMaps_CSM_LowTier, shadowTexCoord, shadowMapIndex, biasedDepth, sampleCount, texelSize);
            else if (tierInfo.TierIndex == 1)
                shadow = SamplePCF_2DArray(g_DirectionalShadowMaps_CSM_MediumTier, shadowTexCoord, shadowMapIndex, biasedDepth, sampleCount, texelSize);
            else
                shadow = SamplePCF_2DArray(g_DirectionalShadowMaps_CSM_HighTier, shadowTexCoord, shadowMapIndex, biasedDepth, sampleCount, texelSize);
        }
    }
    else if (FilterType == 2) // VSM
    {
        float2 moments;
        [branch]
        if (tierInfo.TierIndex == 0)
            moments = SampleGaussian_2DArray(g_DirectionalShadowMaps_CSM_LowTier_Float, shadowTexCoord, shadowMapIndex, texelSize, 5.1f, 2.0f);
        else if (tierInfo.TierIndex == 1)
            moments = SampleGaussian_2DArray(g_DirectionalShadowMaps_CSM_MediumTier_Float, shadowTexCoord, shadowMapIndex, texelSize, 5.1f, 2.0f);
        else
            moments = SampleGaussian_2DArray(g_DirectionalShadowMaps_CSM_HighTier_Float, shadowTexCoord, shadowMapIndex, texelSize, 5.1f, 2.0f);
        
        shadow = ChebyshevUpperBound(moments, currentDepth, VSMMinVariance, VSMLightBleedingReduction);
    }
    else if (FilterType == 3) // ESM
    {
        float2 moments;
        [branch]
        if (tierInfo.TierIndex == 0)
            moments = SampleGaussian_2DArray(g_DirectionalShadowMaps_CSM_LowTier_Float, shadowTexCoord, shadowMapIndex, texelSize, 5.1f, 2.0f);
        else if (tierInfo.TierIndex == 1)
            moments = SampleGaussian_2DArray(g_DirectionalShadowMaps_CSM_MediumTier_Float, shadowTexCoord, shadowMapIndex, texelSize, 5.1f, 2.0f);
        else
            moments = SampleGaussian_2DArray(g_DirectionalShadowMaps_CSM_HighTier_Float, shadowTexCoord, shadowMapIndex, texelSize, 5.1f, 2.0f);

        shadow = SampleESM(moments.r, currentDepth, ESMExponent);
    }
    else if (FilterType == 4) // EVSM
    {
        // EVSM은 바이어스를 적용해야 합니다 (원본 SpotLight 코드 [cite: 112] 참고)
        float bias_evsm = lerp(0.0001f, 0.0005f, NdotL);
        float biasedDepthEVSM = currentDepth - bias_evsm;

        [branch]
        if (tierInfo.TierIndex == 0)
            shadow = SampleEVSM_PCF_2DArray(g_DirectionalShadowMaps_CSM_LowTier_Float, shadowTexCoord, shadowMapIndex, biasedDepthEVSM, EVSMPositiveExponent, EVSMNegativeExponent, EVSMLightBleedingReduction, texelSize, 2.5f);
        else if (tierInfo.TierIndex == 1)
            shadow = SampleEVSM_PCF_2DArray(g_DirectionalShadowMaps_CSM_MediumTier_Float, shadowTexCoord, shadowMapIndex, biasedDepthEVSM, EVSMPositiveExponent, EVSMNegativeExponent, EVSMLightBleedingReduction, texelSize, 2.5f);
        else
            shadow = SampleEVSM_PCF_2DArray(g_DirectionalShadowMaps_CSM_HighTier_Float, shadowTexCoord, shadowMapIndex, biasedDepthEVSM, EVSMPositiveExponent, EVSMNegativeExponent, EVSMLightBleedingReduction, texelSize, 2.5f);
    }

    return shadow;
}


// Directional Light 계산 (Diffuse + Specular + CSM Shadow)
float3 CalculateDirectionalLight(FDirectionalLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    // Light.Direction이 영벡터인 경우, 정규화 문제 방지
    if (all(light.Direction == float3(0.0f, 0.0f, 0.0f)))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 lightDir = normalize(-light.Direction);

    // Shadow mapping (if this light casts shadows)
    float shadow = 1.0f;
    if (light.bCastShadow && light.ShadowMapIndex != 0xFFFFFFFF)
    {
        if (light.bUseCSM)
        {
            // CSM: 뷰 공간 깊이 기준으로 캐스케이드 선택
            float3 viewSpacePos = mul(float4(worldPos, 1.0f), ViewMatrix).xyz;
            float viewDepth = abs(viewSpacePos.z); // 카메라로부터의 거리

            // 캐스케이드 선택 (NumCascades만큼 동적으로)
            int cascadeIndex = 0;
            for (int i = 0; i < (int)light.NumCascades - 1; i++)
            {
                if (viewDepth > light.CascadeSplitDistances[i])
                    cascadeIndex = i + 1;
            }

            // 선택된 캐스케이드의 VP 행렬로 라이트 공간 변환
            float4 lightSpacePos = mul(float4(worldPos, 1.0f), light.CascadeViewProjection[cascadeIndex]);

            // 1. light.CascadeTierInfos에서 올바른 티어 정보 가져오기 
            FCascadeTierInfo tierInfo = light.CascadeTierInfos[cascadeIndex];

            // 2. 1단계에서 추가한 CSM 전용 샘플링 함수 호출
            shadow = SampleDirectionalLightShadowMapCSM(
                tierInfo,
                lightSpacePos,
                normal,
                lightDir
            );
        }
        else
        {
            // Default: 단일 쉐도우 맵 사용 (카메라 프러스텀 전체)
            float4 lightSpacePos = mul(float4(worldPos, 1.0f), light.LightViewProjection);
            shadow = SampleDirectionalLightShadowMap(light.ShadowMapIndex, lightSpacePos, normal, lightDir, DirectionalLightResolution);
        }
    }

    // Diffuse (light.Color는 이미 Intensity 포함)
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, materialColor) * shadow;

    // Specular (선택사항)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, specularPower) * shadow;
    }

    return diffuse + specular;
}

// Point Light 계산 (Diffuse + Specular with Attenuation and Falloff + Shadow)
float3 CalculatePointLight(FPointLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // epsilon으로 0으로 나누기 방지
    distance = max(distance, 0.0001f);
    float3 lightDir = lightVec / distance;

    // Falloff 모드에 따라 감쇠 계산
    float attenuation;
    if (light.bUseInverseSquareFalloff)
    {
        // 물리 기반 역제곱 감쇠 (Unreal Engine 스타일)
        attenuation = CalculateInverseSquareFalloff(distance, light.AttenuationRadius);
    }
    else
    {
        // 더 큰 제어를 위한 예술적 지수 기반 감쇠
        attenuation = CalculateExponentFalloff(distance, light.AttenuationRadius, light.FalloffExponent);
    }

    // Shadow mapping (if this light casts shadows)
    float shadow = 1.0f;
    if (light.bCastShadow && light.ShadowMapIndex != 0xFFFFFFFF)
    {
        // LightViewProjection 기반 Shadow 샘플링
        shadow = SamplePointLightShadowMap(light, worldPos, normal, PointLightResolution);
    }

    // Diffuse (light.Color는 이미 Intensity 포함)
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, materialColor) * attenuation * shadow;

    // Specular (선택사항)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, specularPower) * attenuation * shadow;
    }

    return diffuse + specular;
}

// Spot Light 계산 (Diffuse + Specular with Attenuation and Cone)
float3 CalculateSpotLight(FSpotLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // epsilon으로 0으로 나누기 방지
    distance = max(distance, 0.0001f);
    float3 lightDir = lightVec / distance;
    float3 spotDir = normalize(light.Direction);

    // Spot 원뿔 감쇠 (각도 공간에서 보간하여 중간에서 급격하게 변함)
    float cosAngle = dot(-lightDir, spotDir);

    // cosine 공간 대신 각도 공간에서 보간 (균등한 감쇠를 위해)
    float angle = degrees(acos(saturate(cosAngle)));  // 현재 각도 (도 단위)

    // inner cone에서 1, outer cone에서 0, 중간에서 급격하게 변함
    float spotAttenuation = 1.0 - smoothstep(light.InnerConeAngle, light.OuterConeAngle, angle);

    // Falloff 모드에 따라 거리 감쇠 계산
    float distanceAttenuation;
    if (light.bUseInverseSquareFalloff)
    {
        // 물리 기반 역제곱 감쇠 (Unreal Engine 스타일)
        distanceAttenuation = CalculateInverseSquareFalloff(distance, light.AttenuationRadius);
    }
    else
    {
        // 더 큰 제어를 위한 예술적 지수 기반 감쇠
        distanceAttenuation = CalculateExponentFalloff(distance, light.AttenuationRadius, light.FalloffExponent);
    }

    // 두 감쇠를 결합
    float attenuation = distanceAttenuation * spotAttenuation;

    // Shadow mapping (if this light casts shadows)
    float shadow = 1.0f;
    if (light.bCastShadow && light.ShadowMapIndex != 0xFFFFFFFF)
    {
        // Transform world position to light space
        float4 lightSpacePos = mul(float4(worldPos, 1.0f), light.LightViewProjection);
        shadow = SampleSpotLightShadowMap(light.ShadowMapIndex, lightSpacePos, normal, lightDir, SpotLightResolution);
    }

    // Diffuse (light.Color는 이미 Intensity 포함)
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, materialColor) * attenuation * shadow;

    // Specular (선택사항)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, specularPower) * attenuation * shadow;
    }

    return diffuse + specular;
}

//================================================================================================
// 매크로 기반 조명 모델 헬퍼
//================================================================================================

// 조명 모델에 따라 적절한 파라미터 자동 설정
#ifdef LIGHTING_MODEL_GOURAUD
    #define LIGHTING_INCLUDE_SPECULAR true
    #define LIGHTING_NEED_VIEWDIR true
#elif defined(LIGHTING_MODEL_LAMBERT)
    #define LIGHTING_INCLUDE_SPECULAR false
    #define LIGHTING_NEED_VIEWDIR false
#elif defined(LIGHTING_MODEL_PHONG)
    #define LIGHTING_INCLUDE_SPECULAR true
    #define LIGHTING_NEED_VIEWDIR true
#else
    // 조명 모델 미정의 - 기본값 제공
    #define LIGHTING_INCLUDE_SPECULAR false
    #define LIGHTING_NEED_VIEWDIR false
#endif

//================================================================================================
// 타일 컬링 기반 전체 조명 계산 (매크로 자동 대응)
//================================================================================================

// 모든 라이트(Point + Spot) 계산 - 타일 컬링 지원
// 매크로에 따라 자동으로 specular on/off
// 주의: 이 헬퍼 함수는 ambient에 baseColor 사용 (Ka = Kd 가정)
//       OBJ/MTL 재질의 경우, Material.AmbientColor로 CalculateAmbientLight를 별도로 호출할 것
float3 CalculateAllLights(
    float3 worldPos,
    float3 normal,
    float3 viewDir,        // LAMBERT에서는 사용 안 함
    float4 baseColor,
    float specularPower,
    float4 screenPos)      // 타일 컬링용
{
    float3 litColor = float3(0, 0, 0);

    // Ambient (비재질 오브젝트는 Ka = Kd 가정)
    litColor += CalculateAmbientLight(AmbientLight, baseColor.rgb);

    // Directional (with shadow support)
    litColor += CalculateDirectionalLight(
        DirectionalLight,
        worldPos,  // worldPos 추가 (shadow 계산용)
        normal,
        viewDir,  // LAMBERT에서는 무시됨
        baseColor,
        LIGHTING_INCLUDE_SPECULAR,  // 매크로에 따라 자동 설정
        specularPower
    );

    // Point + Spot with 타일 컬링
    if (bUseTileCulling)
    {
        uint tileIndex = CalculateTileIndex(screenPos);
        uint tileDataOffset = GetTileDataOffset(tileIndex);
        uint lightCount = g_TileLightIndices[tileDataOffset];

        for (uint i = 0; i < lightCount; i++)
        {
            uint packedIndex = g_TileLightIndices[tileDataOffset + 1 + i];
            uint lightType = (packedIndex >> 16) & 0xFFFF;
            uint lightIdx = packedIndex & 0xFFFF;

            if (lightType == 0)  // Point Light
            {
                litColor += CalculatePointLight(
                    g_PointLightList[lightIdx],
                    worldPos,
                    normal,
                    viewDir,
                    baseColor,
                    LIGHTING_INCLUDE_SPECULAR,  // 매크로 자동 대응
                    specularPower
                );
            }
            else if (lightType == 1)  // Spot Light
            {
                litColor += CalculateSpotLight(
                    g_SpotLightList[lightIdx],
                    worldPos,
                    normal,
                    viewDir,
                    baseColor,
                    LIGHTING_INCLUDE_SPECULAR,  // 매크로 자동 대응
                    specularPower
                );
            }
        }
    }
    else
    {
        // 모든 라이트 순회 (타일 컬링 비활성화)
        for (int i = 0; i < PointLightCount; i++)
        {
            litColor += CalculatePointLight(
                g_PointLightList[i],
                worldPos,
                normal,
                viewDir,
                baseColor,
                LIGHTING_INCLUDE_SPECULAR,
                specularPower
            );
        }

        for (int j = 0; j < SpotLightCount; j++)
        {
            litColor += CalculateSpotLight(
                g_SpotLightList[j],
                worldPos,
                normal,
                viewDir,
                baseColor,
                LIGHTING_INCLUDE_SPECULAR,
                specularPower
            );
        }
    }

    return litColor;
}
