#include "pch.h"
#include "BillboardComponent.h"

#include "Quad.h"
#include "Material.h"
#include "Shader.h"
#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "CameraActor.h"
#include "JsonSerializer.h"
#include "LightComponentBase.h"
#include "MeshBatchElement.h"

IMPLEMENT_CLASS(UBillboardComponent)

BEGIN_PROPERTIES(UBillboardComponent)
	MARK_AS_COMPONENT("빌보드 컴포넌트", "항상 카메라를 향하는 2D 아이콘을 표시합니다.")
	ADD_PROPERTY_TEXTURE(UTexture*, Texture, "Billboard", true)
END_PROPERTIES()

UBillboardComponent::UBillboardComponent()
{
	auto& RM = UResourceManager::GetInstance();
	Quad = RM.Get<UQuad>("BillboardQuad");
	if (Quad == nullptr)
	{
		UE_LOG("Quad is nullptr");
		return;
	}

	// HSLS 설정 
	SetMaterialByName(0, "Shaders/UI/Billboard.hlsl");

	// 일단 디폴트 텍스쳐로 설정하기 .
	SetTextureName(GDataDir + "/UI/Icons/Pawn_64x.png");
	//빌보드는 기본적으로 게임에서 숨김
	bHiddenInGame = true;
}

void UBillboardComponent::SetTextureName(FString TexturePath)
{
	TexturePath = TexturePath;
	Texture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
}

UMaterialInterface* UBillboardComponent::GetMaterial(uint32 InSectionIndex) const
{
	return Material;
}

void UBillboardComponent::SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
{
	Material = InNewMaterial;
}

void UBillboardComponent::OnSerialized()
{
	Super::OnSerialized();

	TexturePath = Texture->GetFilePath();

}

void UBillboardComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// Quad, Material은 공유 리소스이므로 복제하지 않음
	// Texture는 TextureName을 통해 리소스 매니저에서 가져오므로 복제하지 않음
}

void UBillboardComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
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

	OutMeshBatchElements.Add(BatchElement);
}