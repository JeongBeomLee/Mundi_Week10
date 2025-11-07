// ────────────────────────────────────────────────────────────────────────────
// SphereComponent.cpp
// Sphere 형태의 충돌 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "SphereComponent.h"
#include "BoxComponent.h"
#include "Actor.h"
#include "AABB.h"

IMPLEMENT_CLASS(USphereComponent)

BEGIN_PROPERTIES(USphereComponent)
	MARK_AS_COMPONENT("스피어 컴포넌트", "Sphere(구) 형태의 충돌 컴포넌트입니다. 거리 기반 충돌 감지를 수행합니다.")
	ADD_PROPERTY_RANGE(float, SphereRadius, "Shape", 0.0f, 10000.0f, true, "Sphere의 반지름 (로컬 스페이스)")
END_PROPERTIES()
// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

USphereComponent::USphereComponent()
{
	// 기본 Sphere 반지름 (50 단위)
	SphereRadius = 50.0f;

	// 초기 Bounds 계산
	UpdateBounds();
}

USphereComponent::~USphereComponent()
{
}

void USphereComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

// ────────────────────────────────────────────────────────────────────────────
// Sphere 속성 함수
// ────────────────────────────────────────────────────────────────────────────

/**
 * Sphere 반지름을 설정합니다.
 *
 * @param InRadius - 새로운 반지름
 * @param bUpdateBounds - Bounds를 즉시 업데이트할지 여부
 */
void USphereComponent::SetSphereRadius(float InRadius, bool bUpdateBoundsNow)
{
	SphereRadius = InRadius;

	if (bUpdateBoundsNow)
	{
		UpdateBounds();
	}
}

/**
 * 스케일이 적용된 Sphere 반지름을 반환합니다 (월드 스페이스).
 * Sphere는 균등 스케일을 가정하므로 최대 스케일 값을 사용합니다.
 *
 * @return 스케일 적용된 Sphere 반지름
 */
float USphereComponent::GetScaledSphereRadius() const
{
	FVector Scale = GetWorldScale();

	// 최대 스케일 값을 사용 (구는 균등 스케일 가정)
	float MaxScale = FMath::Max(FMath::Abs(Scale.X), FMath::Max(FMath::Abs(Scale.Y), FMath::Abs(Scale.Z)));

	return SphereRadius * MaxScale;
}

/**
 * 월드 스페이스의 Sphere 중심을 반환합니다.
 *
 * @return Sphere 중심 위치 (월드 스페이스)
 */
FVector USphereComponent::GetSphereCenter() const
{
	return GetWorldLocation();
}

// ────────────────────────────────────────────────────────────────────────────
// UShapeComponent 인터페이스 구현
// ────────────────────────────────────────────────────────────────────────────

/**
 * Bounds를 업데이트합니다.
 * Transform 변경 시 자동으로 호출됩니다.
 */
void USphereComponent::UpdateBounds()
{
	float ScaledRadius = GetScaledSphereRadius();
	FVector Center = GetSphereCenter();

	// Sphere의 Bounds는 반지름을 Extent로 하는 Box
	FVector Extent(ScaledRadius, ScaledRadius, ScaledRadius);

	CachedBounds = FBoxSphereBounds(Center, Extent);
}

/**
 * 스케일이 적용된 Bounds를 반환합니다.
 *
 * @return FBoxSphereBounds 구조체
 */
FBoxSphereBounds USphereComponent::GetScaledBounds() const
{
	return CachedBounds;
}

/**
 * Sphere를 디버그 렌더링합니다.
 */
void USphereComponent::DrawDebugShape() const
{
	// TODO: 디버그 렌더링 시스템과 연동
	// FVector Center = GetSphereCenter();
	// float Radius = GetScaledSphereRadius();
	// DrawDebugSphere(Center, Radius, ShapeColor);
}

/**
 * Renderer를 통해 Sphere를 디버그 렌더링합니다.
 * 3개의 직교하는 원으로 구성된 Sphere 와이어프레임을 그립니다.
 *
 * @param Renderer - 렌더러 객체
 */
void USphereComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (!Renderer)
	{
		return;
	}

	FVector Center = GetSphereCenter();
	float Radius = GetScaledSphereRadius();

	// 라인 데이터 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 충돌 중이면 빨간색, 아니면 원래 색상
	const FVector4 LineColor = bIsOverlapping ?
		FVector4(1.0f, 0.0f, 0.0f, 1.0f) :
		FVector4(ShapeColor.X, ShapeColor.Y, ShapeColor.Z, 1.0f);
	const int32 NumSegments = 32; // 원의 세그먼트 수

	// XY 평면 원 (Z축 중심)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Center + FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.0f);
		FVector Point2 = Center + FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.0f);

		StartPoints.push_back(Point1);
		EndPoints.push_back(Point2);
		Colors.push_back(LineColor);
	}

	// XZ 평면 원 (Y축 중심)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Center + FVector(cos(Angle1) * Radius, 0.0f, sin(Angle1) * Radius);
		FVector Point2 = Center + FVector(cos(Angle2) * Radius, 0.0f, sin(Angle2) * Radius);

		StartPoints.push_back(Point1);
		EndPoints.push_back(Point2);
		Colors.push_back(LineColor);
	}

	// YZ 평면 원 (X축 중심)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Center + FVector(0.0f, cos(Angle1) * Radius, sin(Angle1) * Radius);
		FVector Point2 = Center + FVector(0.0f, cos(Angle2) * Radius, sin(Angle2) * Radius);

		StartPoints.push_back(Point1);
		EndPoints.push_back(Point2);
		Colors.push_back(LineColor);
	}

	// 렌더러에 라인 추가
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

/**
 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
 * Sphere vs Sphere, Sphere vs Box 충돌을 정밀하게 감지합니다.
 *
 * @param Other - 확인할 상대방 Shape 컴포넌트
 * @return 겹쳐있으면 true
 */
bool USphereComponent::IsOverlappingComponent(const UShapeComponent* Other) const
{
	if (!Other)
	{
		return false;
	}

	// Sphere vs Sphere 충돌
	if (const USphereComponent* OtherSphere = Cast<USphereComponent>(Other))
	{
		return IsOverlappingSphere(OtherSphere);
	}

	// Sphere vs Box 충돌
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other))
	{
		return IsOverlappingBox(OtherBox);
	}

	// 기타: Bounds 기반 체크 (부모 클래스 구현)
	return Super::IsOverlappingComponent(Other);
}

// ────────────────────────────────────────────────────────────────────────────
// Sphere 전용 충돌 감지 함수
// ────────────────────────────────────────────────────────────────────────────

/**
 * 다른 Sphere 컴포넌트와 겹쳐있는지 확인합니다.
 * Sphere vs Sphere 충돌은 두 중심 간의 거리가 반지름의 합보다 작으면 충돌입니다.
 *
 * @param OtherSphere - 확인할 상대방 Sphere 컴포넌트
 * @return 겹쳐있으면 true
 */
bool USphereComponent::IsOverlappingSphere(const USphereComponent* OtherSphere) const
{
	if (!OtherSphere)
	{
		return false;
	}

	// 두 Sphere의 중심과 반지름을 가져옵니다
	FVector MyCenter = GetSphereCenter();
	float MyRadius = GetScaledSphereRadius();

	FVector OtherCenter = OtherSphere->GetSphereCenter();
	float OtherRadius = OtherSphere->GetScaledSphereRadius();

	// 중심 간 거리 제곱 계산 (제곱근 연산 회피)
	float DistanceSquared = (MyCenter - OtherCenter).SizeSquared();
	float RadiusSum = MyRadius + OtherRadius;

	// 거리가 반지름의 합보다 작거나 같으면 충돌
	return DistanceSquared <= (RadiusSum * RadiusSum);
}

/**
 * Box 컴포넌트와 겹쳐있는지 확인합니다.
 * Box 측에서 구현된 충돌 감지를 사용합니다.
 *
 * @param OtherBox - 확인할 상대방 Box 컴포넌트
 * @return 겹쳐있으면 true
 */
bool USphereComponent::IsOverlappingBox(const UBoxComponent* OtherBox) const
{
	if (!OtherBox)
	{
		return false;
	}

	// Box 측에서 구현된 충돌 감지 사용 (중복 구현 방지)
	return OtherBox->IsOverlappingSphere(this);
}

/**
 * 특정 점이 Sphere 내부에 있는지 확인합니다.
 *
 * @param Point - 확인할 점 (월드 스페이스)
 * @return 포함되어 있으면 true
 */
bool USphereComponent::ContainsPoint(const FVector& Point) const
{
	FVector Center = GetSphereCenter();
	float Radius = GetScaledSphereRadius();

	// 점과 중심 사이의 거리 제곱 계산
	float DistanceSquared = (Point - Center).SizeSquared();

	// 거리가 반지름보다 작거나 같으면 포함
	return DistanceSquared <= (Radius * Radius);
}
