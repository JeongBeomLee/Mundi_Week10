#include "pch.h"
#include <cmath>
#include "DecalComponent.h"
#include "OBB.h"
#include "StaticMeshComponent.h"
#include "JsonSerializer.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"

IMPLEMENT_CLASS(UDecalComponent)

BEGIN_PROPERTIES(UDecalComponent)
	MARK_AS_COMPONENT("데칼 컴포넌트", "표면에 투영되는 데칼 효과를 생성합니다.")
	ADD_PROPERTY_TEXTURE(UTexture*, DecalTexture, "Decal", true)
	ADD_PROPERTY_RANGE(float, DecalOpacity, "Decal", 0.0f, 1.0f, true, "데칼 불투명도입니다.")
	ADD_PROPERTY_RANGE(float, FadeSpeed, "Decal", 0.0f, 10.0f, true, "페이드 속도입니다 (초당 변화량).")
END_PROPERTIES()

UDecalComponent::UDecalComponent()
{
	UResourceManager::GetInstance().Load<UMaterial>("Shaders/Effects/Decal.hlsl");
	DecalTexture = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Textures/grass.jpg");
	SetTickEnabled(true);
	SetCanEverTick(true);
}

void UDecalComponent::OnSerialized()
{
	Super::OnSerialized();

}

void UDecalComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	DirectionGizmo = nullptr;
	SpriteComponent = nullptr;
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	// Opacity 업데이트
	DecalOpacity += static_cast<float>(FadeDirection) * FadeSpeed * DeltaTime;

	// 범위 체크 및 방향 반전
	if (DecalOpacity <= 0.0f)
	{
		DecalOpacity = 0.0f;
		FadeDirection = +1; // 다시 증가 시작
	}
	else if (DecalOpacity >= 1.0f)
	{
		DecalOpacity = 1.0f;
		FadeDirection = -1; // 다시 감소 시작
	}
}

void UDecalComponent::OnRegister(UWorld* InWorld)
{
	if (!SpriteComponent && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/S_DecalActorIcon.dds");

		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);
		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));
		DirectionGizmo->SetDirection(FVector(0.5f, 0.0f, 0.0f));
		DirectionGizmo->SetColor(FVector(0.62f, 0.125f, 0.94f));

	}
}

void UDecalComponent::RenderDebugVolume(URenderer* Renderer) const
{
	// 라인 색상
	const FVector4 BoxColor(0.5f, 0.8f, 0.9f, 1.0f); // 하늘색

	// AddLines 함수에 전달할 데이터 배열들을 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;
	
	TArray<FVector> Coners = GetWorldOBB().GetCorners();

	const int Edges[12][2] = {
		{6, 4}, {7, 5}, {6, 7}, {4, 5}, // 앞면
		{4, 0}, {5, 1}, {6, 2}, {7, 3}, // 옆면
		{0, 2}, {1, 3}, {0, 1}, {2, 3}  // 뒷면
	};

	// 12개의 선 데이터를 배열에 채워 넣습니다.
	for (int i = 0; i < 12; ++i)
	{
		// 월드 좌표로 변환
		const FVector WorldStart = Coners[Edges[i][0]];
		const FVector WorldEnd = Coners[Edges[i][1]];

		StartPoints.Add(WorldStart);
		EndPoints.Add(WorldEnd);
		Colors.Add(BoxColor);
	}

	// 모든 데이터를 준비한 뒤, 단 한 번의 호출로 렌더러에 전달합니다.
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void UDecalComponent::SetDecalTexture(UTexture* InTexture)
{
	DecalTexture = InTexture;
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
	DecalTexture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
}

FAABB UDecalComponent::GetWorldAABB() const
{
    // Step 1: Build the decal's oriented box so we can inspect its world-space corners.
    const FOBB DecalOBB = GetWorldOBB();

    // Step 2: Initialize min/max accumulators that will grow to the final axis-aligned bounds.
    FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // Step 3: Evaluate all 8 OBB corners in world-space.
    const FVector& Center = DecalOBB.Center;
    const FVector& HalfExtent = DecalOBB.HalfExtent;
    const FVector (&Axes)[3] = DecalOBB.Axes;

    for (int8 sx = -1; sx <= 1; sx += 2)
    {
        for (int8 sy = -1; sy <= 1; sy += 2)
        {
            for (int8 sz = -1; sz <= 1; sz += 2)
            {
                const FVector Corner = Center
                    + Axes[0] * (HalfExtent.X * static_cast<float>(sx))
                    + Axes[1] * (HalfExtent.Y * static_cast<float>(sy))
                    + Axes[2] * (HalfExtent.Z * static_cast<float>(sz));

                MinBounds.X = std::min(MinBounds.X, Corner.X);
                MinBounds.Y = std::min(MinBounds.Y, Corner.Y);
                MinBounds.Z = std::min(MinBounds.Z, Corner.Z);

                MaxBounds.X = std::max(MaxBounds.X, Corner.X);
                MaxBounds.Y = std::max(MaxBounds.Y, Corner.Y);
                MaxBounds.Z = std::max(MaxBounds.Z, Corner.Z);
            }
        }
    }

    // Step 4: Package the accumulated extremes into a world-space AABB.
    return FAABB(MinBounds, MaxBounds);
}

FOBB UDecalComponent::GetWorldOBB() const
{
    const FVector Center = GetWorldLocation();
    const FVector HalfExtent = GetWorldScale() / 2.0f;

    const FQuat Quat = GetWorldRotation();

    FVector Axes[3];
    Axes[0] = Quat.GetForwardVector();
    Axes[1] = Quat.GetRightVector();
    Axes[2] = Quat.GetUpVector();

    FOBB Obb(Center, HalfExtent, Axes);

    return Obb;
}

FMatrix UDecalComponent::GetDecalProjectionMatrix() const
{
    const FOBB Obb = GetWorldOBB();

	// yup to zup 행렬이 적용이 안되게 함: x방향 projection 행렬을 적용하기 위해.
	const FMatrix DecalWorld = FMatrix::FromTRS(GetWorldLocation(), GetWorldRotation(), {1.0f, 1.0f, 1.0f});
	const FMatrix DecalView = DecalWorld.InverseAffine();

	const FVector Scale = GetWorldScale();
	const FMatrix DecalProj = FMatrix::OrthoLH_XForward(Scale.Y, Scale.Z, -Obb.HalfExtent.X, Obb.HalfExtent.X);

	FMatrix DecalViewProj = DecalView * DecalProj;

    return DecalViewProj;
}




