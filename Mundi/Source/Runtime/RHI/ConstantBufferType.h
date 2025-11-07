#pragma once
// b0 in VS    
#include "Color.h"
#include "LightManager.h"

struct ModelBufferType // b0
{
    FMatrix Model;
    FMatrix ModelInverseTranspose;  // For correct normal transformation with non-uniform scale
};

struct ViewProjBufferType // b1 고유번호 고정
{
    FMatrix View;
    FMatrix Proj;
    FMatrix InvView;
    FMatrix InvProj;
};

struct DecalBufferType
{
    FMatrix DecalMatrix;
    float Opacity;
};

struct PostProcessBufferType // b0
{
    float Near;
    float Far;
    int IsOrthographic; // 0 = Perspective, 1 = Orthographic
    float Padding; // 16바이트 정렬을 위한 패딩
};

struct FogBufferType // b2
{
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;

    FVector4 FogInscatteringColor; // 16 bytes alignment 위해 중간에 넣음

    float FogMaxOpacity;
    float FogHeight; // fog base height
    float Padding[2]; // 16바이트 정렬을 위한 패딩
};

struct LetterBoxBufferType // b2 (PostProcess와 동일한 슬롯 사용)
{
    float LetterBoxSize;     // 레터박스 크기 (0.0 ~ 1.0)
    float LetterBoxOpacity;  // 레터박스 불투명도 (0.0 ~ 1.0)
    FVector2D Padding;       // 16바이트 정렬
};

struct FXAABufferType // b2
{
    FVector2D ScreenSize; // 화면 해상도 (e.g., float2(1920.0f, 1080.0f))
    FVector2D InvScreenSize; // 1.0f / ScreenSize (픽셀 하나의 크기)

    float EdgeThresholdMin; // 엣지 감지 최소 휘도 차이 (0.0833f 권장)
    float EdgeThresholdMax; // 엣지 감지 최대 휘도 차이 (0.166f 권장)
    float QualitySubPix; // 서브픽셀 품질 (낮을수록 부드러움, 0.75 권장)
    int32_t QualityIterations; // 엣지 탐색 반복 횟수 (12 권장)
};

struct GammaCorrectionBufferType // b2
{
    float Gamma;           // 감마 값 (일반적으로 2.2가 표준)
    float InvGamma;        // 1.0 / Gamma (미리 계산해서 넘겨주면 성능 향상)
    float Brightness;      // 추가 밝기 조정 (1.0 = 변화 없음)
    float Saturation;      // 채도 조정 (1.0 = 변화 없음)
};

struct FFadeBufferType // b2
{
    float FadeAlpha; // 0 ~ 1 범위, 0 = 완전 Fade 색상, 1 = 원본 씬 색상
    FVector FadeColor; // Fade 목표 색상 (RGB)
};

struct FVignettingBufferType    // b2
{
    FVector4 VignettingColor;

    float Radius;
    float Softness;
    float AspectRatio;
    float Padding;
};

// b0 in PS
struct FMaterialInPs
{
    FVector DiffuseColor; // Kd
    float OpticalDensity; // Ni

    FVector AmbientColor; // Ka
    float Transparency; // Tr Or d

    FVector SpecularColor; // Ks
    float SpecularExponent; // Ns

    FVector EmissiveColor; // Ke
    uint32 IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials

    FVector TransmissionFilter; // Tf
    float dummy; // 4 bytes padding
    FMaterialInPs() = default;
    FMaterialInPs(const FMaterialInfo& MaterialInfo)
        :DiffuseColor(MaterialInfo.DiffuseColor),
        OpticalDensity(MaterialInfo.OpticalDensity),
        AmbientColor(MaterialInfo.AmbientColor),
        Transparency(MaterialInfo.Transparency),
        SpecularColor(MaterialInfo.SpecularColor),
        SpecularExponent(MaterialInfo.SpecularExponent),
        EmissiveColor(MaterialInfo.EmissiveColor),
        IlluminationModel(MaterialInfo.IlluminationModel),
        TransmissionFilter(MaterialInfo.TransmissionFilter),
        dummy(0)
    { 

    }
};


struct FPixelConstBufferType
{
    FMaterialInPs Material;
    uint32 bHasMaterial;
    uint32 bHasDiffuseTexture;
    uint32 bHasNormalTexture;
	float Padding; // 16바이트 정렬을 위한 패딩
};

struct ColorBufferType // b3
{
    FLinearColor Color;      // 16 bytes (4 floats)
    uint32 UUID;             // 4 bytes
    float SpriteRows;        // 4 bytes - 스프라이트 시트 행 수 (0이면 일반 빌보드)
    float SpriteColumns;     // 4 bytes - 스프라이트 시트 열 수
    float CurrentFrame;      // 4 bytes - 현재 프레임
    // 총 32 bytes = 16바이트 정렬됨
};

struct FLightBufferType
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;

    uint32 PointLightCount;
    uint32 SpotLightCount;
    FVector2D Padding;
};

// b10 고유번호 고정
struct FViewportConstants
{
    // x = Viewport TopLeftX
    // y = Viewport TopLeftY
    // z = Viewport Width
    // w = Viewport Height
    FVector4 ViewportRect;

    // x = Screen Width (전체 렌더 타겟 너비)
    // y = Screen Height (전체 렌더 타겟 높이)
    // z = 1.0f / Screen Width
    // w = 1.0f / Screen Height
    FVector4 ScreenSize;
};

struct CameraBufferType // b7
{
    FVector CameraPosition;
    float Padding;
};

// b11: 타일 기반 라이트 컬링 상수 버퍼
struct FTileCullingBufferType
{
    uint32 TileSize;          // 타일 크기 (픽셀, 기본 16)
    uint32 TileCountX;        // 가로 타일 개수
    uint32 TileCountY;        // 세로 타일 개수
    uint32 bUseTileCulling;   // 타일 컬링 활성화 여부 (0=비활성화, 1=활성화)
};

// b12: 섀도우 필터링 상수 버퍼
struct FShadowFilterBufferType
{
    uint32 FilterType;               // 필터링 타입 (0=NONE, 1=PCF, 2=VSM, 3=ESM, 4=EVSM)
    uint32 PCFSampleCount;           // PCF 샘플 수 (3, 4, 5)
    uint32 PCFCustomSampleCount;     // PCF Custom 샘플 수
    float DirectionalLightResolution;// Directional Light 섀도우 맵 해상도

    float SpotLightResolution;       // Spot Light 섀도우 맵 해상도
    float PointLightResolution;      // Point Light 섀도우 맵 해상도
    float VSMLightBleedingReduction; // VSM Light bleeding 감소 (0.0 ~ 1.0)
    float VSMMinVariance;            // VSM 최소 분산 값

    float ESMExponent;               // ESM Exponential 계수
    float EVSMPositiveExponent;      // EVSM Positive exponent
    float EVSMNegativeExponent;      // EVSM Negative exponent
    float EVSMLightBleedingReduction;// EVSM Light bleeding 감소
};

#define CONSTANT_BUFFER_INFO(TYPE, SLOT, VS, PS) \
constexpr uint32 TYPE##Slot = SLOT;\
constexpr bool TYPE##IsVS = VS;\
constexpr bool TYPE##IsPS = PS;

//매크로를 인자로 받고 그 매크로 함수에 버퍼 전달
#define CONSTANT_BUFFER_LIST(MACRO) \
MACRO(ModelBufferType)              \
MACRO(DecalBufferType)              \
MACRO(PostProcessBufferType)        \
MACRO(FogBufferType)                \
MACRO(LetterBoxBufferType)          \
MACRO(FXAABufferType)               \
MACRO(GammaCorrectionBufferType)    \
MACRO(FFadeBufferType)              \
MACRO(FVignettingBufferType)        \
MACRO(FPixelConstBufferType)        \
MACRO(ViewProjBufferType)           \
MACRO(ColorBufferType)              \
MACRO(CameraBufferType)             \
MACRO(FLightBufferType)             \
MACRO(FViewportConstants)           \
MACRO(FTileCullingBufferType)       \
MACRO(FShadowFilterBufferType)      \

// 16 바이트 패딩 어썰트
#define STATIC_ASSERT_CBUFFER_ALIGNMENT(Type) \
    static_assert(sizeof(Type) % 16 == 0, "[ " #Type " ] Bad Size. Needs 16-Byte Padding.");
CONSTANT_BUFFER_LIST(STATIC_ASSERT_CBUFFER_ALIGNMENT)

//VS, PS 세팅은 함수 파라미터로 결정하게 하는게 훨씬 나을듯 나중에 수정 필요
//그리고 UV Scroll 상수버퍼도 처리해줘야함
CONSTANT_BUFFER_INFO(ModelBufferType, 0, true, false)
CONSTANT_BUFFER_INFO(PostProcessBufferType, 0, false, true)
CONSTANT_BUFFER_INFO(ViewProjBufferType, 1, true, true) // b1 카메라 행렬 고정
CONSTANT_BUFFER_INFO(FogBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(LetterBoxBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FXAABufferType, 2, false, true)
CONSTANT_BUFFER_INFO(GammaCorrectionBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FFadeBufferType, 2, false, true)   // b2
CONSTANT_BUFFER_INFO(FVignettingBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(ColorBufferType, 3, true, true)   // b3 color
CONSTANT_BUFFER_INFO(FPixelConstBufferType, 4, true, true) // GOURAUD에도 사용되므로 VS도 true
CONSTANT_BUFFER_INFO(DecalBufferType, 6, true, true)
CONSTANT_BUFFER_INFO(CameraBufferType, 7, true, true)  // b7, VS+PS (UberLit.hlsl과 일치)
CONSTANT_BUFFER_INFO(FLightBufferType, 8, true, true)
CONSTANT_BUFFER_INFO(FViewportConstants, 10, true, false)   // 뷰 포트 크기에 따라 전체 화면 복사를 보정하기 위해 설정 (10번 고유번호로 사용)
CONSTANT_BUFFER_INFO(FTileCullingBufferType, 11, false, true)  // b11, PS only (UberLit.hlsl과 일치)
CONSTANT_BUFFER_INFO(FShadowFilterBufferType, 12, false, true) // b12, PS only (Shadow filtering)
