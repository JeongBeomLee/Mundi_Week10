#include "pch.h"
#include "OBB.h"
#include "PerspectiveDecalComponent.h"
#include "JsonSerializer.h"

IMPLEMENT_CLASS(UPerspectiveDecalComponent)

BEGIN_PROPERTIES(UPerspectiveDecalComponent)
	MARK_AS_COMPONENT("원근 데칼 컴포넌트", "원근 투영을 사용하는 데칼 효과를 생성합니다.")
	ADD_PROPERTY_RANGE(float, FovY, "Decal", 1.0f, 179.0f, true, "수직 시야각 (FOV, Degrees)입니다.")
END_PROPERTIES()

UPerspectiveDecalComponent::UPerspectiveDecalComponent()
{
	// PIE에서 fade in, fade out 하지 않도록 설정
	SetTickEnabled(false);
	SetCanEverTick(false);
}

// 이 함수 전체를 복사하여 기존 함수를 대체하십시오.
void UPerspectiveDecalComponent::RenderDebugVolume(URenderer* Renderer) const
{
	// --- OBB(Oriented Bounding Box) 그리기 ---
	const FVector4 LineColor(0.5f, 0.8f, 0.9f, 1.0f); // 하늘색

	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// --- 1. 원뿔을 '정규화된 로컬 공간'에서 +X축 방향으로 먼저 정의 ---
	// 이렇게 하면 컴포넌트의 스케일이 적용되었을 때 OBB와 크기가 정확히 일치합니다.
	const int NumBaseSegments = 32;
	const float HalfLength = 0.5f;
	const float BaseRadius = 0.5f;

	// 원뿔의 정점(apex)은 로컬 공간의 -X 끝에 위치합니다.
	const FVector ApexLocal_Initial(-HalfLength, 0.0f, 0.0f);

	// 원뿔 밑면의 점들은 로컬 공간의 -X 면에 내접하는 원을 형성합니다.
	TArray<FVector> BasePointsLocal_Initial;
	BasePointsLocal_Initial.Reserve(NumBaseSegments);
	for (int i = 0; i < NumBaseSegments; ++i)
	{
		const float Angle = (static_cast<float>(i) / NumBaseSegments) * TWO_PI;
		BasePointsLocal_Initial.Add(FVector(
			HalfLength,
			BaseRadius * std::cos(Angle),
			BaseRadius * std::sin(Angle)
		));
	}

	// --- 3. '월드 변환 행렬'을 이용해 모든 로컬 점들을 월드 좌표로 변환 ---
	const FMatrix WorldMatrix = GetWorldMatrix();
	const FVector ApexWorld = ApexLocal_Initial * WorldMatrix;
	TArray<FVector> BasePointsWorld;
	BasePointsWorld.Reserve(NumBaseSegments);
	for (const FVector& LocalPoint : BasePointsLocal_Initial)
	{
		BasePointsWorld.Add(LocalPoint * WorldMatrix);
	}

	// --- 4. 변환된 월드 좌표를 사용하여 렌더링할 라인 데이터를 생성 ---
	for (int i = 0; i < NumBaseSegments; ++i)
	{
		StartPoints.Add(ApexWorld);
		EndPoints.Add(BasePointsWorld[i]);
		Colors.Add(LineColor);
	}
	for (int i = 0; i < NumBaseSegments; ++i)
	{
		StartPoints.Add(BasePointsWorld[i]);
		EndPoints.Add(BasePointsWorld[(i + 1) % NumBaseSegments]);
		Colors.Add(LineColor);
	}

	// --- 5. 렌더러에 모든 라인 데이터를 한 번에 전달 ---
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

FMatrix UPerspectiveDecalComponent::GetDecalProjectionMatrix() const
{
	const FOBB Obb = GetWorldOBB();

	FVector WorldScale = GetWorldScale();
	FVector WorldLocation = GetWorldLocation();
	FQuat WorldRotation = GetWorldRotation();

	// 1. 로컬 공간에서의 이동(-X * 0.5)을 나타내는 행렬을 만듭니다.
	const FMatrix OffsetMatrix = FMatrix::MakeTranslation({ -WorldScale.X * 0.5f, 0.0f, 0.0f });

	// 2. 데칼의 원래 월드 변환 행렬을 만듭니다.
	const FMatrix OriginalDecalWorld = FMatrix::FromTRS(WorldLocation, WorldRotation, { 1.0f, 1.0f, 1.0f });

	// 3. 로컬 오프셋을 적용하여 최종 월드 행렬을 계산합니다.
	//    [중요] 로컬 변환을 적용하려면 '오프셋 * 월드' 순서로 곱해야 합니다.
	const FMatrix DecalWorld = OffsetMatrix * OriginalDecalWorld;

	// 4. 최종 월드 행렬의 역행렬을 구합니다.
	const FMatrix DecalView = DecalWorld.InverseAffine();

	// 시야각을 라디안으로 변환
	const float FovAngleRadian = DegreesToRadians(FovY);

	// tan(A/2) 값을 미리 계산
	const float TanHalfFov = std::tan(FovAngleRadian / 2.0f);

	const float AspectRatio = 1;

	const float F = WorldScale.X;
	const float N = 0.01f;

	FMatrix DecalProj = FMatrix::Identity();

	// 1열 (Column 0)
	DecalProj.M[0][0] = F / (F - N);
	DecalProj.M[3][0] = -(F * N) / (F - N);

	// 2열 (Column 1)
	DecalProj.M[1][1] = 1.0f / TanHalfFov;

	// 3열 (Column 2)
	DecalProj.M[2][2] = AspectRatio / TanHalfFov;

	// 4열 (Column 3)
	DecalProj.M[0][3] = 1.0f;
	DecalProj.M[3][3] = 0.0f;

	FMatrix DecalViewProj = DecalView * DecalProj;

	return DecalViewProj;
}

void UPerspectiveDecalComponent::SetFovY(float InFov)
{
	// 1. 내부 FovY 멤버 변수를 업데이트합니다.
	this->FovY = InFov;

	// 2. 현재 컴포넌트의 상대 스케일을 가져옵니다.
	// GetRelativeScale() 함수가 있다고 가정합니다. GetWorldScale()도 사용 가능하지만,
	// 상대 스케일을 직접 조절하는 것이 더 명확합니다.
	FVector CurrentScale = GetRelativeScale();

	// 3. FovY와 X 스케일을 사용하여 새로운 Y/Z 스케일(원뿔 밑면의 지름)을 계산합니다.

	// a) 시야각의 절반을 라디안으로 변환합니다.
	const float HalfFovRad = DegreesToRadians(InFov * 0.5f);

	// b) 원뿔의 길이(밑변)는 현재 X 스케일입니다.
	const float ConeLength = CurrentScale.X;

	// c) 밑면의 반지름을 계산합니다. (높이 = 밑변 * tan(세타))
	const float BaseRadius = ConeLength * std::tan(HalfFovRad);

	// d) 최종 Y/Z 스케일은 밑면의 지름(반지름 * 2)과 같습니다.
	const float NewYZScale = BaseRadius * 2.0f;

	// 4. 계산된 새 스케일로 컴포넌트의 스케일을 설정합니다.
	// X 스케일은 유지하고, Y와 Z 스케일만 업데이트합니다.
	SetRelativeScale(FVector(CurrentScale.X, NewYZScale, NewYZScale));
}

void UPerspectiveDecalComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
void UPerspectiveDecalComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		float FovYTemp;
		FJsonSerializer::ReadFloat(InOutHandle, "FovY", FovYTemp);
		SetFovY(FovYTemp);
	}
	else
	{
		InOutHandle["FovY"] = GetFovY();
	}
}

void UPerspectiveDecalComponent::OnSerialized()
{
	Super::OnSerialized();

	

}
