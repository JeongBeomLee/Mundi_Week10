#include "pch.h"
#include "DirectionalLightComponent.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "ShadowManager.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
	// DirectionalLight는 넓은 영역을 커버하므로 높은 bias와 해상도 필요
	ShadowBias = 0.005f;          // 높은 bias (shadow acne 방지)
	ShadowSlopeBias = 2.0f;        // 높은 slope bias (경사면 아크네 방지)
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

IMPLEMENT_CLASS(UDirectionalLightComponent)

BEGIN_PROPERTIES(UDirectionalLightComponent)
	MARK_AS_COMPONENT("디렉셔널 라이트", "방향성 라이트 (태양광 같은 평행광) 컴포넌트입니다.")

	// Shadow Map Type Enum
	{
		static const char* ShadowMapTypeNames[] = { "Default", "CSM" };
		FProperty EnumProp;
		EnumProp.Name = "ShadowMapType";
		EnumProp.Type = EPropertyType::Enum;
		EnumProp.Offset = offsetof(ThisClass_t, ShadowMapType);
		EnumProp.Category = "Shadow";
		EnumProp.bIsEditAnywhere = true;
		EnumProp.Tooltip = "쉐도우맵 타입을 선택합니다.";
		EnumProp.EnumNames = ShadowMapTypeNames;
		EnumProp.EnumCount = 2;
		Class->AddProperty(EnumProp);
	}

	// Shadow Projection Type Enum
	{
		static const char* ShadowProjectionTypeNames[] = { "LVP", "LiSPSM" };
		FProperty EnumProp;
		EnumProp.Name = "ShadowProjectionType";
		EnumProp.Type = EPropertyType::Enum;
		EnumProp.Offset = offsetof(ThisClass_t, ShadowProjectionType);
		EnumProp.Category = "Shadow";
		EnumProp.bIsEditAnywhere = true;
		EnumProp.Tooltip = "쉐도우 프로젝션 타입 (LVP: 표준 직교 투영, LiSPSM: Light-space Perspective Shadow Map)을 선택합니다.";
		EnumProp.EnumNames = ShadowProjectionTypeNames;
		EnumProp.EnumCount = 2;
		Class->AddProperty(EnumProp);
	}

	// CSM Configuration
	ADD_PROPERTY_RANGE(int32, NumCascades, "Shadow", 2, 5, true, "캐스케이드 수 (2~5)")
	ADD_PROPERTY_RANGE(float, CSMLambda, "Shadow", 0.0f, 1.0f, true, "PSSM 혼합 비율 (0=선형, 1=로그)")
END_PROPERTIES()

FVector UDirectionalLightComponent::GetLightDirection() const
{
	// Z-Up Left-handed 좌표계에서 Forward는 X축
	FQuat Rotation = GetWorldRotation();
	return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FDirectionalLightInfo UDirectionalLightComponent::GetLightInfo() const
{
	FDirectionalLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Direction = GetLightDirection();

	// Shadow casting information
	Info.bCastShadow = GetIsCastShadows() ? 1u : 0u;
	Info.ShadowMapIndex = static_cast<uint32>(GetShadowMapIndex()); // Cast int32 to uint32 (converts -1 to 0xFFFFFFFF)

	// CSM mode from this light's ShadowMapType
	Info.bUseCSM = (GetShadowMapType() == EShadowMapType::CSM) ? 1u : 0u;
	Info.NumCascades = static_cast<uint32>(GetNumCascades());

	// Use cached LightViewProjection (updated by ShadowManager each frame during shadow pass)
	// NOTE: Unlike SpotLight which calculates VP matrix here, DirectionalLight needs camera frustum info,
	// so the matrix is calculated and cached by ShadowManager during RenderShadowPass()
	Info.LightViewProjection = CachedLightViewProjection;

	// CSM (Cascaded Shadow Maps) Data
	// Cascade ViewProjection matrices (updated by ShadowManager during shadow pass)
	for (int i = 0; i < 4; ++i)
	{
		Info.CascadeViewProjection[i] = CascadeViewProjections[i];
		Info.CascadeSplitDistances[i] = CascadeSplitDistances[i];

		// CSM Tier Info (updated by ShadowManager)
		Info.CascadeTierInfos[i].TierIndex = CascadeTierInfos[i].TierIndex;
		Info.CascadeTierInfos[i].SliceIndex = CascadeTierInfos[i].SliceIndex;
		Info.CascadeTierInfos[i].Resolution = CascadeTierInfos[i].Resolution;
		Info.CascadeTierInfos[i].Padding = 0.0f;
	}

	return Info;
}

void UDirectionalLightComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);

	UE_LOG("DirectionalLightComponent::OnRegister called");

	if (SpriteComponent)
	{
		SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/S_LightDirectional.dds");
	}

	// Create Direction Gizmo if not already created
	if (!DirectionGizmo && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));

		// Update gizmo properties to match light
		UpdateDirectionGizmo();
		UE_LOG("DirectionGizmo created successfully");
	}
	else
	{
		UE_LOG("DirectionGizmo already exists");
	}
	InWorld->GetLightManager()->RegisterLight(this);
}

void UDirectionalLightComponent::OnUnregister()
{
	// Serialize 시 Crash 방지를 위한 주석 처리
	//if (bPendingDestroy == false)
	//{
		if (UWorld* World = GetWorld())
		{
			World->GetLightManager()->DeRegisterLight(this);
		}
	//}
}

void UDirectionalLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 방향성 라이트 특화 업데이트 로직
	if (UWorld* World = GetWorld())
	{
		World->GetLightManager()->UpdateLight(this);
	}

	// Update direction gizmo to reflect any changes
	UpdateDirectionGizmo();
}

void UDirectionalLightComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	if (UWorld* World = GetWorld())
	{
		World->GetLightManager()->UpdateLight(this);
	}
}

void UDirectionalLightComponent::OnSerialized()
{
	Super::OnSerialized();

}

void UDirectionalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	DirectionGizmo = nullptr;
}

void UDirectionalLightComponent::UpdateDirectionGizmo()
{
	if (!DirectionGizmo)
		return;

	// Set direction to match light direction (used for hovering/picking, not for rendering)
	FVector LightDir = GetLightDirection();
	DirectionGizmo->SetDirection(LightDir);

	// Set color to match base light color (without intensity or temperature multipliers)
	const FLinearColor& BaseColor = GetLightColor();
	DirectionGizmo->SetColor(FVector(BaseColor.R, BaseColor.G, BaseColor.B));

	// DirectionGizmo is a child of DirectionalLightComponent, so it inherits the parent's rotation automatically.
	// Arrow mesh points along +X axis by default, which matches the DirectionalLight's forward direction.
	// No need to set RelativeRotation - it should remain identity (0, 0, 0)
}
