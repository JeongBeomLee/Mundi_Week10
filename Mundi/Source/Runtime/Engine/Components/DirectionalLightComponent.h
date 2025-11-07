#pragma once
#include "LightComponent.h"
#include "LightManager.h"

// 방향광 쉐도우맵 타입
enum class EShadowMapType : uint8
{
	Default = 0,  // 일반 쉐도우맵
	CSM = 1       // Cascaded Shadow Maps
};

// 쉐도우 프로젝션 타입 (LVP, LiSPSM)
enum class EShadowProjectionType : uint8
{
	LVP = 0,      // Light View Projection (기본 Orthographic)
	LiSPSM = 1    // Light-space Perspective Shadow Map (Hybrid: TSM + OpenGL LiSPSM)
};

// 방향성 라이트 (태양광 같은 평행광)
class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	GENERATED_REFLECTION_BODY()

	UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

public:
	// 월드 회전을 반영한 라이트 방향 반환 (Transform의 Forward 벡터)
	FVector GetLightDirection() const;

	// Light Info
	FDirectionalLightInfo GetLightInfo() const;

	// Shadow ViewProjection (updated by ShadowManager each frame)
	void SetLightViewProjection(const FMatrix& InViewProjection) { CachedLightViewProjection = InViewProjection; }
	const FMatrix& GetLightViewProjection() const { return CachedLightViewProjection; }

	// CSM (Cascaded Shadow Maps) Data
	void SetCascadeViewProjection(int CascadeIndex, const FMatrix& VP)
	{
		if (CascadeIndex >= 0 && CascadeIndex < 4)
		{
			CascadeViewProjections[CascadeIndex] = VP;
		}
	}

	const FMatrix& GetCascadeViewProjection(int CascadeIndex) const
	{
		static FMatrix Identity = FMatrix::Identity();
		if (CascadeIndex >= 0 && CascadeIndex < 4)
		{
			return CascadeViewProjections[CascadeIndex];
		}
		return Identity;
	}

	void SetCascadeSplitDistance(int CascadeIndex, float Distance)
	{
		if (CascadeIndex >= 0 && CascadeIndex < 4)
		{
			CascadeSplitDistances[CascadeIndex] = Distance;
		}
	}

	float GetCascadeSplitDistance(int CascadeIndex) const
	{
		if (CascadeIndex >= 0 && CascadeIndex < 4)
		{
			return CascadeSplitDistances[CascadeIndex];
		}
		return 0.0f;
	}

	// Virtual Interface
	void OnRegister(UWorld* InWorld) override;
	void OnUnregister() override;
	virtual void UpdateLightData() override;
	void OnTransformUpdated() override;

	// Serialization & Duplication
	virtual void OnSerialized() override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UDirectionalLightComponent)

	// Update Gizmo to match light properties
	void UpdateDirectionGizmo();

	// Shadow Map Type Getter/Setter
	EShadowMapType GetShadowMapType() const { return ShadowMapType; }
	void SetShadowMapType(EShadowMapType Type) { ShadowMapType = Type; }

	// Shadow Projection Type Getter/Setter
	EShadowProjectionType GetShadowProjectionType() const { return ShadowProjectionType; }
	void SetShadowProjectionType(EShadowProjectionType Type) { ShadowProjectionType = Type; }

	// Actual algorithm used (for LiSPSM hybrid: TSM or OpenGL LiSPSM)
	// Updated by ShadowManager each frame
	void SetActualUsedTSM(bool bUsedTSM) { bActuallyUsedTSM = bUsedTSM; }
	bool IsActuallyUsingTSM() const { return bActuallyUsedTSM; }

	// CSM Configuration Getter/Setter
	int32 GetNumCascades() const { return NumCascades; }
	void SetNumCascades(int32 Num) { NumCascades = Num; }

	float GetCSMLambda() const { return CSMLambda; }
	void SetCSMLambda(float Lambda) { CSMLambda = Lambda; }

	// CSM Tier Info Cache 구조체 (Public access for Shadow Map View)
	struct FCascadeTierInfoCache
	{
		uint32 TierIndex = 0;
		uint32 SliceIndex = 0;
		float Resolution = 0.0f;
	};

	// CSM Tier Info Setter (ShadowManager가 업데이트)
	void SetCascadeTierInfo(int CascadeIndex, uint32 TierIndex, uint32 SliceIndex, float Resolution)
	{
		if (CascadeIndex >= 0 && CascadeIndex < 4)
		{
			CascadeTierInfos[CascadeIndex].TierIndex = TierIndex;
			CascadeTierInfos[CascadeIndex].SliceIndex = SliceIndex;
			CascadeTierInfos[CascadeIndex].Resolution = Resolution;
		}
	}

	// CSM Tier Info Getter (Shadow Map View에서 사용)
	const FCascadeTierInfoCache& GetCascadeTierInfo(int CascadeIndex) const
	{
		static FCascadeTierInfoCache EmptyCache;
		if (CascadeIndex >= 0 && CascadeIndex < 4)
		{
			return CascadeTierInfos[CascadeIndex];
		}
		return EmptyCache;
	}

protected:
	// Direction Gizmo (shows light direction)
	class UGizmoArrowComponent* DirectionGizmo = nullptr;

	// Cached LightViewProjection matrix (updated by ShadowManager)
	FMatrix CachedLightViewProjection = FMatrix::Identity();

	// CSM Data (4 cascades)
	FMatrix CascadeViewProjections[4] = {
		FMatrix::Identity(),
		FMatrix::Identity(),
		FMatrix::Identity(),
		FMatrix::Identity()
	};
	float CascadeSplitDistances[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// CSM Tier Info (updated by ShadowManager)
	FCascadeTierInfoCache CascadeTierInfos[4];

	// Shadow Map Type (Default or CSM)
	EShadowMapType ShadowMapType = EShadowMapType::CSM;

	// Shadow Projection Type (LVP or LiSPSM)
	EShadowProjectionType ShadowProjectionType = EShadowProjectionType::LVP;

	// Actual algorithm used in LiSPSM hybrid (updated by ShadowManager)
	// true = TSM, false = OpenGL LiSPSM
	bool bActuallyUsedTSM = false;

	// CSM Configuration (only visible when ShadowMapType == CSM)
	int32 NumCascades = 3;
	float CSMLambda = 0.5f;
};
