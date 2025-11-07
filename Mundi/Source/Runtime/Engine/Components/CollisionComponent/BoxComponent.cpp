// ────────────────────────────────────────────────────────────────────────────
// BoxComponent.cpp
// Box 형태의 충돌 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "BoxComponent.h"
#include "SphereComponent.h"
#include "Actor.h"
#include "Sphere.h"

IMPLEMENT_CLASS(UBoxComponent)

BEGIN_PROPERTIES(UBoxComponent)
	MARK_AS_COMPONENT("박스 컴포넌트", "Box(직육면체) 형태의 충돌 컴포넌트입니다. AABB 기반 충돌 감지를 수행합니다.")
	ADD_PROPERTY(FVector, BoxExtent, "Shape", true, "Box의 반 크기 (로컬 스페이스, 중심에서 각 축으로의 거리)")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UBoxComponent::UBoxComponent()
{
	// 초기 Bounds 계산
	UpdateBounds();
}

UBoxComponent::~UBoxComponent()
{
}

void UBoxComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	
}

// ────────────────────────────────────────────────────────────────────────────
// Box 속성 함수
// ────────────────────────────────────────────────────────────────────────────

/**
 * Box 크기를 설정합니다 (로컬 스페이스).
 *
 * @param InExtent - 새로운 Box 반 크기
 * @param bUpdateBounds - Bounds를 즉시 업데이트할지 여부
 */
void UBoxComponent::SetBoxExtent(const FVector& InExtent, bool bUpdateBoundsNow)
{
	BoxExtent = InExtent;

	if (bUpdateBoundsNow)
	{
		UpdateBounds();
	}
}

/**
 * 스케일이 적용된 Box 크기를 반환합니다 (월드 스페이스).
 *
 * @return 스케일 적용된 Box 반 크기
 */
FVector UBoxComponent::GetScaledBoxExtent() const
{
	FVector Scale = GetWorldScale();
	return FVector(
		BoxExtent.X * FMath::Abs(Scale.X),
		BoxExtent.Y * FMath::Abs(Scale.Y),
		BoxExtent.Z * FMath::Abs(Scale.Z)
	);
}

/**
 * 월드 스페이스의 Box 중심을 반환합니다.
 *
 * @return Box 중심 위치 (월드 스페이스)
 */
FVector UBoxComponent::GetBoxCenter() const
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
void UBoxComponent::UpdateBounds()
{
	FVector ScaledExtent = GetScaledBoxExtent();
	FVector Center = GetBoxCenter();

	// 회전을 고려한 AABB 계산
	FQuat Rotation = GetWorldRotation();

	// 회전이 없으면 기존 방식 사용
	if (Rotation.IsIdentity())
	{
		CachedBounds = FBoxSphereBounds(Center, ScaledExtent);
		return;
	}

	// 회전된 Box의 8개 꼭짓점을 계산하여 AABB 구하기
	FVector Corners[8];
	Corners[0] = FVector(-ScaledExtent.X, -ScaledExtent.Y, -ScaledExtent.Z);
	Corners[1] = FVector(+ScaledExtent.X, -ScaledExtent.Y, -ScaledExtent.Z);
	Corners[2] = FVector(+ScaledExtent.X, +ScaledExtent.Y, -ScaledExtent.Z);
	Corners[3] = FVector(-ScaledExtent.X, +ScaledExtent.Y, -ScaledExtent.Z);
	Corners[4] = FVector(-ScaledExtent.X, -ScaledExtent.Y, +ScaledExtent.Z);
	Corners[5] = FVector(+ScaledExtent.X, -ScaledExtent.Y, +ScaledExtent.Z);
	Corners[6] = FVector(+ScaledExtent.X, +ScaledExtent.Y, +ScaledExtent.Z);
	Corners[7] = FVector(-ScaledExtent.X, +ScaledExtent.Y, +ScaledExtent.Z);

	// 회전 적용
	FVector Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 i = 0; i < 8; ++i)
	{
		FVector RotatedCorner = Rotation.RotateVector(Corners[i]) + Center;
		Min.X = FMath::Min(Min.X, RotatedCorner.X);
		Min.Y = FMath::Min(Min.Y, RotatedCorner.Y);
		Min.Z = FMath::Min(Min.Z, RotatedCorner.Z);
		Max.X = FMath::Max(Max.X, RotatedCorner.X);
		Max.Y = FMath::Max(Max.Y, RotatedCorner.Y);
		Max.Z = FMath::Max(Max.Z, RotatedCorner.Z);
	}

	// AABB의 중심과 Extent 계산
	FVector AABBCenter = (Min + Max) * 0.5f;
	FVector AABBExtent = (Max - Min) * 0.5f;

	CachedBounds = FBoxSphereBounds(AABBCenter, AABBExtent);
}

/**
 * 스케일이 적용된 Bounds를 반환합니다.
 *
 * @return FBoxSphereBounds 구조체
 */
FBoxSphereBounds UBoxComponent::GetScaledBounds() const
{
	return CachedBounds;
}

/**
 * Box를 디버그 렌더링합니다.
 */
void UBoxComponent::DrawDebugShape() const
{
	// TODO: 디버그 렌더링 시스템과 연동
	// FVector Center = GetBoxCenter();
	// FVector Extent = GetScaledBoxExtent();
	// DrawDebugBox(Center, Extent, ShapeColor);
}

/**
 * Renderer를 통해 Box를 디버그 렌더링합니다.
 * 12개의 선분으로 구성된 Box 와이어프레임을 그립니다.
 *
 * @param Renderer - 렌더러 객체
 */
void UBoxComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (!Renderer)
	{
		return;
	}

	FVector Center = GetBoxCenter();
	FVector Extent = GetScaledBoxExtent();

	// Box의 8개 꼭짓점 계산
	FVector Corners[8];
	Corners[0] = Center + FVector(-Extent.X, -Extent.Y, -Extent.Z); // 왼쪽 아래 뒤
	Corners[1] = Center + FVector(+Extent.X, -Extent.Y, -Extent.Z); // 오른쪽 아래 뒤
	Corners[2] = Center + FVector(+Extent.X, +Extent.Y, -Extent.Z); // 오른쪽 위 뒤
	Corners[3] = Center + FVector(-Extent.X, +Extent.Y, -Extent.Z); // 왼쪽 위 뒤
	Corners[4] = Center + FVector(-Extent.X, -Extent.Y, +Extent.Z); // 왼쪽 아래 앞
	Corners[5] = Center + FVector(+Extent.X, -Extent.Y, +Extent.Z); // 오른쪽 아래 앞
	Corners[6] = Center + FVector(+Extent.X, +Extent.Y, +Extent.Z); // 오른쪽 위 앞
	Corners[7] = Center + FVector(-Extent.X, +Extent.Y, +Extent.Z); // 왼쪽 위 앞

	// 라인 데이터 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 충돌 중이면 빨간색, 아니면 원래 색상
	const FVector4 LineColor = bIsOverlapping ?
		FVector4(1.0f, 0.0f, 0.0f, 1.0f) :
		FVector4(ShapeColor.X, ShapeColor.Y, ShapeColor.Z, 1.0f);

	// 뒤쪽 면 (4개 선분)
	StartPoints.push_back(Corners[0]); EndPoints.push_back(Corners[1]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[1]); EndPoints.push_back(Corners[2]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[2]); EndPoints.push_back(Corners[3]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[3]); EndPoints.push_back(Corners[0]); Colors.push_back(LineColor);

	// 앞쪽 면 (4개 선분)
	StartPoints.push_back(Corners[4]); EndPoints.push_back(Corners[5]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[5]); EndPoints.push_back(Corners[6]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[6]); EndPoints.push_back(Corners[7]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[7]); EndPoints.push_back(Corners[4]); Colors.push_back(LineColor);

	// 앞뒤 연결 (4개 선분)
	StartPoints.push_back(Corners[0]); EndPoints.push_back(Corners[4]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[1]); EndPoints.push_back(Corners[5]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[2]); EndPoints.push_back(Corners[6]); Colors.push_back(LineColor);
	StartPoints.push_back(Corners[3]); EndPoints.push_back(Corners[7]); Colors.push_back(LineColor);

	// 렌더러에 라인 추가
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

/**
 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
 * Box vs Box, Box vs Sphere 충돌을 정밀하게 감지합니다.
 *
 * @param Other - 확인할 상대방 Shape 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UBoxComponent::IsOverlappingComponent(const UShapeComponent* Other) const
{
	if (!Other)
	{
		return false;
	}

	// Box vs Box 충돌
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other))
	{
		return IsOverlappingBox(OtherBox);
	}

	// Box vs Sphere 충돌
	if (const USphereComponent* OtherSphere = Cast<USphereComponent>(Other))
	{
		return IsOverlappingSphere(OtherSphere);
	}

	// 기타: Bounds 기반 체크 (부모 클래스 구현)
	return Super::IsOverlappingComponent(Other);
}

// ────────────────────────────────────────────────────────────────────────────
// Box 전용 충돌 감지 함수
// ────────────────────────────────────────────────────────────────────────────

/**
 * 다른 Box 컴포넌트와 겹쳐있는지 확인합니다.
 * AABB(Axis-Aligned Bounding Box) 교차 테스트를 수행합니다.
 *
 * @param OtherBox - 확인할 상대방 Box 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UBoxComponent::IsOverlappingBox(const UBoxComponent* OtherBox) const
{
	if (!OtherBox)
	{
		return false;
	}

	// 두 Box의 중심과 크기를 가져옵니다
	FVector MyCenter = GetBoxCenter();
	FVector MyExtent = GetScaledBoxExtent();

	FVector OtherCenter = OtherBox->GetBoxCenter();
	FVector OtherExtent = OtherBox->GetScaledBoxExtent();

	// AABB 교차 테스트 (SAT - Separating Axis Theorem)
	// 각 축에서 두 Box의 투영 구간이 겹치는지 확인합니다
	bool bOverlapX = FMath::Abs(MyCenter.X - OtherCenter.X) <= (MyExtent.X + OtherExtent.X);
	bool bOverlapY = FMath::Abs(MyCenter.Y - OtherCenter.Y) <= (MyExtent.Y + OtherExtent.Y);
	bool bOverlapZ = FMath::Abs(MyCenter.Z - OtherCenter.Z) <= (MyExtent.Z + OtherExtent.Z);

	// 모든 축에서 겹쳐야 충돌
	return bOverlapX && bOverlapY && bOverlapZ;
}

/**
 * Sphere 컴포넌트와 겹쳐있는지 확인합니다.
 * Box와 Sphere의 교차 테스트를 수행합니다.
 *
 * @param OtherSphere - 확인할 상대방 Sphere 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UBoxComponent::IsOverlappingSphere(const USphereComponent* OtherSphere) const
{
	if (!OtherSphere)
	{
		return false;
	}

	// Box의 중심과 크기
	FVector BoxCenter = GetBoxCenter();
	FVector BoxExtent = GetScaledBoxExtent();

	// Sphere의 중심과 반지름
	FVector SphereCenter = OtherSphere->GetSphereCenter();
	float SphereRadius = OtherSphere->GetScaledSphereRadius();

	// AABB를 생성합니다
	FAABB Box(BoxCenter - BoxExtent, BoxCenter + BoxExtent);

	// FSphere를 생성합니다
	FSphere Sphere(SphereCenter, SphereRadius);

	// Box와 Sphere 교차 테스트
	return Sphere.IntersectsAABB(Box);
}

/**
 * 특정 점이 Box 내부에 있는지 확인합니다.
 *
 * @param Point - 확인할 점 (월드 스페이스)
 * @return 포함되어 있으면 true
 */
bool UBoxComponent::ContainsPoint(const FVector& Point) const
{
	FVector BoxCenter = GetBoxCenter();
	FVector BoxExtent = GetScaledBoxExtent();

	// 각 축에서 점이 Box 범위 안에 있는지 확인
	bool bInsideX = FMath::Abs(Point.X - BoxCenter.X) <= BoxExtent.X;
	bool bInsideY = FMath::Abs(Point.Y - BoxCenter.Y) <= BoxExtent.Y;
	bool bInsideZ = FMath::Abs(Point.Z - BoxCenter.Z) <= BoxExtent.Z;

	return bInsideX && bInsideY && bInsideZ;
}
