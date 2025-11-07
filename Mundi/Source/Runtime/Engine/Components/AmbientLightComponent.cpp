#include "pch.h"
#include "AmbientLightComponent.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(UAmbientLightComponent)

BEGIN_PROPERTIES(UAmbientLightComponent)
	MARK_AS_COMPONENT("앰비언트 라이트", "환경광 (전역 조명) 컴포넌트입니다.")
END_PROPERTIES()

UAmbientLightComponent::UAmbientLightComponent()
{
}

UAmbientLightComponent::~UAmbientLightComponent()
{
}

FAmbientLightInfo UAmbientLightComponent::GetLightInfo() const
{
	FAmbientLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	// Optimized: No padding needed (16 bytes perfect alignment)
	return Info;
}

void UAmbientLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 환경광 특화 업데이트 로직
	GWorld->GetLightManager()->UpdateLight(this);
}

void UAmbientLightComponent::OnTransformUpdated()
{
	//Ambient는 방향이나 위치가 바꾸지 않으므로 처리 X
}

void UAmbientLightComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);
	if (SpriteComponent)
	{
		SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/SkyLight.dds");
	}
	InWorld->GetLightManager()->RegisterLight(this);
}

void UAmbientLightComponent::OnUnregister()
{
	// Serialize 시 Crash 방지를 위한 주석 처리
	//if (bPendingDestroy == false)
	//{
		GWorld->GetLightManager()->DeRegisterLight(this);
	//}
}

void UAmbientLightComponent::OnSerialized()
{
	Super::OnSerialized();

	//// 리플렉션 기반 자동 직렬화 (추가 프로퍼티 없음)
	//Serialize(bInIsLoading, InOutHandle, UAmbientLightComponent::StaticClass());
}

void UAmbientLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
