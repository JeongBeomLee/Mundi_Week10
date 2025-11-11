#include "pch.h"
#include "ComponentDetailRenderer.h"
#include "ImGui/imgui.h"
#include "SpotLightComponent.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"

void UComponentDetailRenderer::RenderCustomUI(UActorComponent* Component)
{
	if (!Component)
		return;

	// 런타임 타입 체크 → 컴파일 타입 오버로딩 디스패치
	if (auto* SpotLight = Cast<USpotLightComponent>(Component))
		RenderCustomUIImpl(SpotLight);
	else if (auto* DirLight = Cast<UDirectionalLightComponent>(Component))
		RenderCustomUIImpl(DirLight);
	else if (auto* PointLight = Cast<UPointLightComponent>(Component))
		RenderCustomUIImpl(PointLight);
	else
		RenderCustomUIImpl(Component);  // Fallback
}

// ===== SpotLight UI =====
// TODO: 기존 TargetActorTransformWidget.cpp의 코드를 여기로 이동 (리팩토링)
void UComponentDetailRenderer::RenderCustomUIImpl(USpotLightComponent* Component)
{
	// 현재는 비워둠 (나중에 리팩토링 시 구현)
}

// ===== DirectionalLight UI =====
// TODO: 기존 TargetActorTransformWidget.cpp의 코드를 여기로 이동 (리팩토링)
void UComponentDetailRenderer::RenderCustomUIImpl(UDirectionalLightComponent* Component)
{
	// 현재는 비워둠 (나중에 리팩토링 시 구현)
}

// ===== PointLight UI =====
// TODO: 기존 TargetActorTransformWidget.cpp의 코드를 여기로 이동 (리팩토링)
void UComponentDetailRenderer::RenderCustomUIImpl(UPointLightComponent* Component)
{
	// 현재는 비워둠 (나중에 리팩토링 시 구현)
}

// ===== SkeletalMesh UI =====
// NOTE: Skeletal Mesh Editor는 Property Reflection을 통해 자동으로 노출됨
// (PropertyRenderer::RenderSkeletalMeshProperty에서 처리)