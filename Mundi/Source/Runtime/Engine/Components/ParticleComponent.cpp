#include "pch.h"
#include "ParticleComponent.h"

#include "Quad.h"
#include "Material.h"
#include "Shader.h"
#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "MeshBatchElement.h"
#include "LightComponent.h"

IMPLEMENT_CLASS(UParticleComponent)

BEGIN_PROPERTIES(UParticleComponent)
    MARK_AS_COMPONENT("스프라이트 애니메이션 컴포넌트", "스프라이트 시트 애니메이션을 재생하는 빌보드")
    ADD_PROPERTY_RANGE(int32, SpriteRows, "SpriteAnimation", 1, 32, true, "스프라이트 시트의 행(세로) 개수")
    ADD_PROPERTY_RANGE(int32, SpriteColumns, "SpriteAnimation", 1, 32, true, "스프라이트 시트의 열(가로) 개수")
    ADD_PROPERTY_RANGE(float, FrameRate, "SpriteAnimation", 1.0f, 120.0f, true, "초당 재생할 프레임 수 (FPS)")
    ADD_PROPERTY(bool, bLooping, "SpriteAnimation", true, "애니메이션을 반복 재생할지 여부")
END_PROPERTIES()

UParticleComponent::UParticleComponent()
{
    // 파티클 전용 셰이더 사용 (스프라이트 애니메이션 지원)
    SetMaterialByName(0, "Shaders/Effects/Particle.hlsl");

    // 일단 디폴트 텍스쳐로 설정하기 .
    SetTextureName(GDataDir + "/UI/Sprite/FireExplosion_6x6.dds");
    // 기본적으로 게임에서 보임
    bHiddenInGame = false;

    // Tick 활성화 (애니메이션 업데이트를 위해 필요)
    bCanEverTick = true;
}

void UParticleComponent::TickComponent(float DeltaSeconds)
{
    if (!bIsPlaying)
        return;

    // 프레임 업데이트
    CurrentFrame += FrameRate * DeltaSeconds;

    if (CurrentFrame >= TotalFrames)
    {
        if (bLooping)
        {
            CurrentFrame = 0.0f; // 루프
        }
        else
        {
            CurrentFrame = TotalFrames - 1.0f; // 마지막 프레임에서 정지
            bIsPlaying = false;
        }
    }
}

void UParticleComponent::SetSpriteSheet(const FString& InTexturePath, int32 InRows, int32 InColumns)
{
    SetTextureName(InTexturePath);
    SpriteRows = InRows;
    SpriteColumns = InColumns;
    TotalFrames = SpriteRows * SpriteColumns;
}

void UParticleComponent::Play()
{
    bIsPlaying = true;
}

void UParticleComponent::Stop()
{
    bIsPlaying = false;
}

void UParticleComponent::Reset()
{
    CurrentFrame = 0.0f;
    bIsPlaying = false;
}

void UParticleComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	// 1. 렌더링할 애셋이 유효한지 검사
	  // (IsVisible()는 UPrimitiveComponent 또는 그 부모에 있다고 가정)
	if (!IsVisible() || !Quad || Quad->GetIndexCount() == 0 || !Texture || !Texture->GetShaderResourceView())
	{
		return; // 그릴 메시 데이터 없음
	}

	// 2. 사용할 머티리얼과 셰이더 결정
	UMaterialInterface* MaterialToUse = GetMaterial(0); // this->Material 반환
	UShader* ShaderToUse = nullptr;

	if (MaterialToUse && MaterialToUse->GetShader())
	{
		ShaderToUse = MaterialToUse->GetShader();
	}
	else
	{
		// [Fallback 로직]
		UE_LOG("UBillboardComponent: Material이 없거나 셰이더가 없어서 기본 빌보드 셰이더 사용");

		// 생성자에서 사용한 경로와 동일하게 Fallback
		MaterialToUse = UResourceManager::GetInstance().Load<UMaterial>("Shaders/UI/Billboard.hlsl");
		if (MaterialToUse)
		{
			ShaderToUse = MaterialToUse->GetShader();
		}

		// 기본 셰이더조차 없으면 렌더링 불가
		if (!MaterialToUse || !ShaderToUse)
		{
			UE_LOG("UBillboardComponent: 기본 빌보드 머티리얼/셰이더를 찾을 수 없습니다!");
			return;
		}
	}

	// 3. FMeshBatchElement 생성
	// UQuad는 GroupInfo가 없는 단일 메시로 처리합니다.
	FMeshBatchElement BatchElement;

	FShaderVariant* ShaderVariant = ShaderToUse->GetShaderVariant(MaterialToUse->GetShaderMacros());

	// --- 정렬 키 ---
	BatchElement.VertexShader = ShaderVariant->VertexShader;
	BatchElement.PixelShader = ShaderVariant->PixelShader;
	BatchElement.InputLayout = ShaderVariant->InputLayout;
	BatchElement.Material = MaterialToUse;
	BatchElement.VertexBuffer = Quad->GetVertexBuffer();
	BatchElement.IndexBuffer = Quad->GetIndexBuffer();

	// 참고: UQuad 클래스에 GetVertexStride() 함수가 필요합니다.
	BatchElement.VertexStride = Quad->GetVertexStride();

	// --- 드로우 데이터 (메시 전체 범위 사용) ---
	BatchElement.IndexCount = Quad->GetIndexCount(); // 전체 인덱스 수
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;

	// --- 인스턴스 데이터 ---
	// 빌보드는 3개의 스케일 펙터중에서 가장 큰 값으로 유니폼스케일, 회전 미적용, Tarnslation 적용
	float Scale = GetRelativeScale().GetMaxValue();
	BatchElement.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeTranslation(GetWorldLocation());

	BatchElement.ObjectID = InternalIndex;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	BatchElement.InstanceShaderResourceView = Texture->GetShaderResourceView();

	FLinearColor Color{ 1,1,1,1 };
	if (ULightComponentBase* LightBase = Cast<ULightComponentBase>(this->GetAttachParent()))
	{
		Color = LightBase->GetLightColor();
	}
	BatchElement.InstanceColor = Color;

	// CustomData에 스프라이트 애니메이션 정보 추가
	BatchElement.CustomData.clear();
	BatchElement.CustomData.push_back(static_cast<float>(SpriteRows));
	BatchElement.CustomData.push_back(static_cast<float>(SpriteColumns));
	BatchElement.CustomData.push_back(CurrentFrame);

	OutMeshBatchElements.Add(BatchElement);
}
