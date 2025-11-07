#include "pch.h"
#include "LightManager.h"
#include "AmbientLightComponent.h"
#include "DirectionalLightComponent.h"
#include "SpotLightComponent.h"
#include "PointLightComponent.h"
#include "D3D11RHI.h"
#include "SceneComponent.h"

#define NUM_POINT_LIGHT_MAX 256
#define NUM_SPOT_LIGHT_MAX 256

FLightManager::~FLightManager()
{
	Release();
}
void FLightManager::Initialize(D3D11RHI* RHIDevice)
{
	if (!PointLightBuffer)
	{
		RHIDevice->CreateStructuredBuffer(sizeof(FPointLightInfo), NUM_POINT_LIGHT_MAX, nullptr, &PointLightBuffer);
		RHIDevice->CreateStructuredBufferSRV(PointLightBuffer, &PointLightBufferSRV);
	}
	if (!SpotLightBuffer)
	{
		RHIDevice->CreateStructuredBuffer(sizeof(FSpotLightInfo), NUM_SPOT_LIGHT_MAX, nullptr, &SpotLightBuffer);
		RHIDevice->CreateStructuredBufferSRV(SpotLightBuffer, &SpotLightBufferSRV);
	}
}
void FLightManager::Release()
{
	if (PointLightBuffer)
	{
		PointLightBuffer->Release();
		PointLightBuffer = nullptr;
	}
	if (PointLightBufferSRV)
	{
		PointLightBufferSRV->Release();
		PointLightBufferSRV = nullptr;
	}

	if (SpotLightBuffer)
	{
		SpotLightBuffer->Release();
		SpotLightBuffer = nullptr;
	}
	if (SpotLightBufferSRV)
	{
		SpotLightBufferSRV->Release();
		SpotLightBufferSRV = nullptr;
	}
}

void FLightManager::UpdateLightBuffer(D3D11RHI* RHIDevice)
{
	if (bHaveToUpdate)
	{
		if (!PointLightBuffer)
		{
			// Lazy initialization
			Initialize(RHIDevice);
		}

		FLightBufferType LightBuffer{};

		if (AmbientLightList.Num() > 0)
		{
			if (AmbientLightList[0]->IsVisible()&&
				AmbientLightList[0]->GetOwner()->IsActorVisible())
			{
				LightBuffer.AmbientLight = AmbientLightList[0]->GetLightInfo();
			}
		}

		if (DIrectionalLightList.Num() > 0)
		{
			if (DIrectionalLightList[0]->IsVisible() &&
				DIrectionalLightList[0]->GetOwner()->IsActorVisible())
			{
				LightBuffer.DirectionalLight = DIrectionalLightList[0]->GetLightInfo();
			}
		}


		if (bPointLightDirty)
		{
			PointLightInfoList.clear();
			for (int Index = 0; Index < PointLightList.Num(); Index++)
			{
				if (PointLightList[Index]->IsVisible()&&
					PointLightList[Index]->GetOwner()->IsActorVisible())
				{
					PointLightInfoList.Add(PointLightList[Index]->GetLightInfo());
				}
			}
			RHIDevice->UpdateStructuredBuffer(PointLightBuffer, PointLightInfoList.data(), PointLightNum * sizeof(FPointLightInfo));
			bPointLightDirty = false;
		}

		if (bSpotLightDirty)
		{
			SpotLightInfoList.clear();
			for (int Index = 0; Index < SpotLightList.Num(); Index++)
			{
				if (SpotLightList[Index]->IsVisible()&&
					SpotLightList[Index]->GetOwner()->IsActorVisible())
				{
					SpotLightInfoList.Add(SpotLightList[Index]->GetLightInfo());
				}
			}

			RHIDevice->UpdateStructuredBuffer(SpotLightBuffer, SpotLightInfoList.data(), 
				SpotLightNum * sizeof(FSpotLightInfo));

			bSpotLightDirty = false;
		}

		LightBuffer.PointLightCount = PointLightNum;
		LightBuffer.SpotLightCount = SpotLightNum;

		//슬롯 재활용 하고싶을 시 if문 밖으로 빼서 매 프레임 세팅 해줘야함.
		RHIDevice->SetAndUpdateConstantBuffer(LightBuffer);
	
		ID3D11ShaderResourceView* SRVList[2]{ PointLightBufferSRV, SpotLightBufferSRV };

		//Gouraud shader 사용 여부로 아래 세팅 분기도 가능, 일단 둘다 바인딩함
		RHIDevice->GetDeviceContext()->PSSetShaderResources(3, 2, SRVList);
		RHIDevice->GetDeviceContext()->VSSetShaderResources(3, 2, SRVList);

		bHaveToUpdate = false;
	}
	
}

void FLightManager::SetDirtyFlag()
{
	bHaveToUpdate = true;
	bPointLightDirty = true;
	bSpotLightDirty = true;
}

void FLightManager::ClearAllLightList()
{
	AmbientLightList.clear();
	DIrectionalLightList.clear();
	PointLightList.clear();
	SpotLightList.clear();

	//업데이트 시에만 clear하고 다시 수집할 실제 데이터 리스트
	PointLightInfoList.clear();
	SpotLightInfoList.clear();

	//이미 레지스터된 라이트인지 확인하는 용도
	LightComponentList.clear();
	PointLightNum = 0;
	SpotLightNum = 0;
}

template<>
void FLightManager::RegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	AmbientLightList.Add(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	DIrectionalLightList.Add(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent)||
		Cast<USpotLightComponent>(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	PointLightList.Add(LightComponent);
	PointLightInfoList.Add(LightComponent->GetLightInfo());
	PointLightNum++;
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	SpotLightList.Add(LightComponent);
	SpotLightInfoList.Add(LightComponent->GetLightInfo());
	SpotLightNum++;
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}


template<>
void FLightManager::DeRegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	AmbientLightList.Remove(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	DIrectionalLightList.Remove(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent) ||
		Cast<USpotLightComponent>(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	PointLightList.Remove(LightComponent);
	PointLightNum--;
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	SpotLightList.Remove(LightComponent);
	SpotLightNum--;
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}


template<> void FLightManager::UpdateLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent) ||
		Cast<USpotLightComponent>(LightComponent))
	{
		return;
	}
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}