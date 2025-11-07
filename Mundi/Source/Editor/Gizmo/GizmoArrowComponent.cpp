#include "pch.h"
#include "GizmoArrowComponent.h"
#include "EditorEngine.h"
#include "MeshBatchElement.h"
#include "SceneView.h"

IMPLEMENT_CLASS(UGizmoArrowComponent)

UGizmoArrowComponent::UGizmoArrowComponent()
{
	SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");

	// 기즈모 셰이더로 설정
	SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

	// 기즈모는 기본적으로 게임(PIE)에서 숨김
	bHiddenInGame = true;
}

UGizmoArrowComponent::~UGizmoArrowComponent()
{

}

UMaterialInterface* UGizmoArrowComponent::GetMaterial(uint32 InSectionIndex) const
{
	return GizmoMaterial;
}

void UGizmoArrowComponent::SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
{
	GizmoMaterial = InNewMaterial;
}

float UGizmoArrowComponent::ComputeScreenConstantScale(float ViewWidth, float ViewHeight, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, float TargetPixels) const
{
	const bool bOrtho = std::fabs(ProjectionMatrix.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
	float WorldPerPixel = 0.0f;
	if (bOrtho)
	{
		const float HalfH = 1.0f / ProjectionMatrix.M[1][1];
		WorldPerPixel = (2.0f * HalfH) / ViewHeight;
	}
	else
	{
		const float YScale = ProjectionMatrix.M[1][1];
		const FVector GizPosWorld = GetWorldLocation();
		const FVector4 GizPos4(GizPosWorld.X, GizPosWorld.Y, GizPosWorld.Z, 1.0f);
		const FVector4 ViewPos4 = GizPos4 * ViewMatrix;
		float Z = ViewPos4.Z;
		if (Z < 1.0f) Z = 1.0f;
		WorldPerPixel = (2.0f * Z) / (ViewHeight * YScale);
	}

	float ScaleFactor = TargetPixels * WorldPerPixel;
	if (ScaleFactor < 0.001f) ScaleFactor = 0.001f;
	if (ScaleFactor > 10000.0f) ScaleFactor = 10000.0f;
	return ScaleFactor;
}

// View 화면에 그려지는 크기를 반환
void UGizmoArrowComponent::SetDrawScale(float ViewWidth, float ViewHeight, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
	FVector DrawScale = DefaultScale;

	// Screen에 따라 크기 자동 조절
	if (bUseScreenConstantScale)
	{
		const float ScaleFactor = ComputeScreenConstantScale(ViewWidth, ViewHeight, ViewMatrix, ProjectionMatrix, 30.0f);
		DrawScale *= ScaleFactor;
	}

	SetWorldScale(DrawScale);
}

void UGizmoArrowComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!IsVisible() || !StaticMesh)
	{
		return; // 그릴 메시 애셋이 없거나 보이지 않음
	}

	if (!View)
	{
		return; // 그릴 화면이 없음
	}

	// 화면에 그려지는 크기 설정
	SetDrawScale(View->ViewRect.Width(), View->ViewRect.Height(), View->ViewMatrix, View->ProjectionMatrix);

	// --- 사용할 머티리얼 결정 ---
	UMaterialInterface* MaterialToUse = GizmoMaterial;
	UShader* ShaderToUse = nullptr;

	if (MaterialToUse && MaterialToUse->GetShader())
	{
		ShaderToUse = MaterialToUse->GetShader();
	}
	else
	{
		// [Fallback 로직]
		UE_LOG("GizmoArrowComponent: GizmoMaterial is invalid. Falling back to default vertex color shader.");
		MaterialToUse = UResourceManager::GetInstance().Load<UMaterial>("Shaders/UI/Gizmo.hlsl");
		if (MaterialToUse)
		{
			ShaderToUse = MaterialToUse->GetShader();
		}

		if (!MaterialToUse || !ShaderToUse)
		{
			UE_LOG("GizmoArrowComponent: Default vertex color material/shader not found!");
			return;
		}
	}

	// --- FMeshBatchElement 수집 ---
	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

	// 처리할 섹션 수 결정
	const bool bHasSections = !MeshGroupInfos.IsEmpty();
	const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

	// 단일 루프로 통합
	for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
	{
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;

		// 1. 섹션 정보 유무에 따라 드로우 데이터 범위를 결정합니다.
		if (bHasSections)
		{
			// [서브 메시 처리]
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
			IndexCount = Group.IndexCount;
			StartIndex = Group.StartIndex;
		}
		else
		{
			// [단일 배치 처리]
			// bHasSections가 false이면 이 루프는 SectionIndex가 0일 때 한 번만 실행됩니다.
			IndexCount = StaticMesh->GetIndexCount();
			StartIndex = 0;
		}

		// 2. (공통) 인덱스 수가 0이면 스킵
		if (IndexCount == 0)
		{
			continue;
		}

		// 3. (공통) FMeshBatchElement 생성 및 설정
		// 머티리얼과 셰이더는 루프 밖에서 이미 결정되었습니다.
		FMeshBatchElement BatchElement;

		FShaderVariant* ShaderVariant = ShaderToUse->GetShaderVariant(MaterialToUse->GetShaderMacros());

		// --- 정렬 키 ---
		BatchElement.VertexShader = ShaderVariant->VertexShader;
		BatchElement.PixelShader = ShaderVariant->PixelShader;
		BatchElement.InputLayout = ShaderVariant->InputLayout;
		BatchElement.Material = MaterialToUse;
		BatchElement.VertexBuffer = StaticMesh->GetVertexBuffer();
		BatchElement.IndexBuffer = StaticMesh->GetIndexBuffer();
		BatchElement.VertexStride = StaticMesh->GetVertexStride();

		// --- 드로우 데이터 (1번에서 결정된 값 사용) ---
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;

		// --- 인스턴스 데이터 ---
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = 0; // 기즈모는 피킹 대상이 아니므로 0
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		if (bHighlighted)
		{
			BatchElement.InstanceColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			BatchElement.InstanceColor = FLinearColor(GetColor());
		}

		OutMeshBatchElements.Add(BatchElement);
	}
}

void UGizmoArrowComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
