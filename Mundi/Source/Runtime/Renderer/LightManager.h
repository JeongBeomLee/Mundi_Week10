#pragma once

class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class ULightComponent;
class D3D11RHI;
class FShadowMap;
class USceneComponent;

struct FVector2;

enum class ELightType
{
    AmbientLight,
    DirectionalLight,
    PointLight,
    SpotLight,
};
struct FAmbientLightInfo
{
    FLinearColor Color;     // 16 bytes - Color already includes Intensity and Temperature
    // Total: 16 bytes
};

// CSM Cascade Tier Info (C++ 측)
struct FCascadeTierInfo
{
    uint32 TierIndex;      // 4 bytes - 0=Low, 1=Medium, 2=High
    uint32 SliceIndex;     // 4 bytes - 해당 티어 내 슬라이스 인덱스
    float Resolution;      // 4 bytes - 실제 해상도 (PCF texelSize 계산용)
    float Padding;         // 4 bytes - 16-byte 정렬
    // Total: 16 bytes
};

struct FDirectionalLightInfo
{
    FLinearColor Color;     // 16 bytes - Color already includes Intensity and Temperature
    FVector Direction;      // 12 bytes
    uint32 bCastShadow;     // 4 bytes - true = this light casts shadow
    uint32 ShadowMapIndex;  // 4 bytes - index to shadow map array (base index for CSM)
    uint32 bUseCSM;         // 4 bytes - true = use CSM, false = use single shadow map
    uint32 NumCascades;     // 4 bytes - CSM 캐스케이드 수 (3~6)
    float Padding;          // 4 bytes - padding
    FMatrix LightViewProjection; // 64 bytes - Legacy: 단일 섀도우 맵용 (CSM 비활성화 시 사용)

    // CSM (Cascaded Shadow Maps) Data
    FMatrix CascadeViewProjection[4];  // 256 bytes (64 bytes × 4) - 각 캐스케이드의 VP 행렬
    float CascadeSplitDistances[4];    // 16 bytes - 각 캐스케이드의 Far 거리

    // CSM Tier Info (3-Tier System)
    FCascadeTierInfo CascadeTierInfos[4]; // 64 bytes (16 bytes × 4) - 각 캐스케이드의 티어 정보

    // Total: 116 + 256 + 16 + 64 = 452 bytes
};

struct FPointLightInfo
{
    FLinearColor Color;         // 16 bytes - Color already includes Intensity and Temperature
    FVector Position;           // 12 bytes
    float AttenuationRadius;    // 4 bytes (moved up to fill slot)
    float FalloffExponent;      // 4 bytes - Falloff exponent for artistic control
    uint32 bUseInverseSquareFalloff; // 4 bytes - true = physically accurate, false = exponent-based
	uint32 bCastShadow;         // 4 bytes - true = this light casts shadow
	uint32 ShadowMapIndex;      // 4 bytes - index to shadow map array (-1 if no shadow)
	float Padding[4];           // 16 bytes - padding for 16-byte alignment (required for FMatrix array)
	FMatrix LightViewProjection[6]; // 384 bytes (64 bytes × 6) - View-Projection matrices for cube map faces
	// Total: 448 bytes (16-byte aligned)
};

struct FSpotLightInfo
{
    FLinearColor Color;         // 16 bytes - Color already includes Intensity and Temperature
    FVector Position;           // 12 bytes
    float InnerConeAngle;       // 4 bytes
    FVector Direction;          // 12 bytes
    float OuterConeAngle;       // 4 bytes
    float AttenuationRadius;    // 4 bytes
    float FalloffExponent;      // 4 bytes - Falloff exponent for artistic control
    uint32 bUseInverseSquareFalloff; // 4 bytes - true = physically accurate, false = exponent-based
    uint32 bCastShadow;         // 4 bytes - true = this light casts shadow
    uint32 ShadowMapIndex;      // 4 bytes - index to shadow map array (-1 if no shadow)
    FVector Padding;            // 4 bytes - padding
    FMatrix LightViewProjection; // 64 bytes - Light space transformation matrix
	// Total: 144 bytes
};

class FLightManager
{

public:
    FLightManager() = default;
    ~FLightManager();

    void Initialize(D3D11RHI* RHIDevice);
    void Release();

    void UpdateLightBuffer(D3D11RHI* RHIDevice);
    void SetDirtyFlag();

    TArray<FPointLightInfo>& GetPointLightInfoList() { return PointLightInfoList; }
    TArray<FSpotLightInfo>& GetSpotLightInfoList() { return SpotLightInfoList; }

    template<typename T>
    void RegisterLight(T* LightComponent);
    template<typename T>
    void DeRegisterLight(T* LightComponent);
    template<typename T>
    void UpdateLight(T* LightComponent);

    void ClearAllLightList();
private:
    bool bHaveToUpdate = true;
    bool bPointLightDirty = true;
    bool bSpotLightDirty = true;

    //structured buffer
    ID3D11Buffer* PointLightBuffer = nullptr;
    ID3D11Buffer* SpotLightBuffer = nullptr;
    ID3D11ShaderResourceView* PointLightBufferSRV = nullptr;
    ID3D11ShaderResourceView* SpotLightBufferSRV = nullptr;

    TArray<UAmbientLightComponent*> AmbientLightList;
    TArray<UDirectionalLightComponent*> DIrectionalLightList;
    TArray<UPointLightComponent*> PointLightList;
    TArray<USpotLightComponent*> SpotLightList;

    //업데이트 시에만 clear하고 다시 수집할 실제 데이터 리스트
    TArray<FPointLightInfo> PointLightInfoList;
    TArray<FSpotLightInfo> SpotLightInfoList;

    //이미 레지스터된 라이트인지 확인하는 용도
    TSet<ULightComponent*> LightComponentList;
    uint32 PointLightNum = 0;
    uint32 SpotLightNum = 0;
};

template<> void FLightManager::RegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent);

template<> void FLightManager::DeRegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent);

template<> void FLightManager::UpdateLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<USpotLightComponent>(USpotLightComponent* LightComponent);