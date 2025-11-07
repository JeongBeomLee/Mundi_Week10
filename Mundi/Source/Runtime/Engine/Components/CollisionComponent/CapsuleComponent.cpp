// ────────────────────────────────────────────────────────────────────────────
// CapsuleComponent.cpp
// Capsule 형태의 충돌 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CapsuleComponent.h"
#include "SphereComponent.h"
#include "BoxComponent.h"
#include "Actor.h"

IMPLEMENT_CLASS(UCapsuleComponent)

BEGIN_PROPERTIES(UCapsuleComponent)
	MARK_AS_COMPONENT("캡슐 컴포넌트", "Capsule(캡슐) 형태의 충돌 컴포넌트입니다. 캐릭터 충돌에 주로 사용됩니다.")
	ADD_PROPERTY_RANGE(float, CapsuleRadius, "Shape", 0.0f, 10000.0f, true, "Capsule의 반지름 (로컬 스페이스)")
	ADD_PROPERTY_RANGE(float, CapsuleHalfHeight, "Shape", 0.0f, 10000.0f, true, "Capsule의 반 높이 (로컬 스페이스, 중심에서 끝까지, 반지름 제외)")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCapsuleComponent::UCapsuleComponent()
{
	// 기본 Capsule 크기 (반지름 50, 반 높이 100)
	CapsuleRadius = 50.0f;
	CapsuleHalfHeight = 100.0f;

	// 초기 Bounds 계산
	UpdateBounds();
}

UCapsuleComponent::~UCapsuleComponent()
{
}

void UCapsuleComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

// ────────────────────────────────────────────────────────────────────────────
// Capsule 속성 함수
// ────────────────────────────────────────────────────────────────────────────

/**
 * Capsule 크기를 설정합니다.
 *
 * @param InRadius - 새로운 반지름
 * @param InHalfHeight - 새로운 반 높이
 * @param bUpdateBounds - Bounds를 즉시 업데이트할지 여부
 */
void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateBoundsNow)
{
	CapsuleRadius = InRadius;
	CapsuleHalfHeight = InHalfHeight;

	if (bUpdateBoundsNow)
	{
		UpdateBounds();
	}
}

/**
 * 스케일이 적용된 Capsule 반지름을 반환합니다 (월드 스페이스).
 * XY 평면 스케일의 최대값을 사용합니다 (Z축은 높이 방향).
 *
 * @return 스케일 적용된 Capsule 반지름
 */
float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	FVector Scale = GetWorldScale();

	// XY 평면 스케일 사용 (Capsule은 Z축 방향)
	float MaxRadialScale = FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y));

	return CapsuleRadius * MaxRadialScale;
}

/**
 * 스케일이 적용된 Capsule 반 높이를 반환합니다 (월드 스페이스).
 *
 * @return 스케일 적용된 Capsule 반 높이
 */
float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	FVector Scale = GetWorldScale();

	// Z축 스케일 사용
	return CapsuleHalfHeight * FMath::Abs(Scale.Z);
}

/**
 * 월드 스페이스의 Capsule 중심을 반환합니다.
 *
 * @return Capsule 중심 위치 (월드 스페이스)
 */
FVector UCapsuleComponent::GetCapsuleCenter() const
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
void UCapsuleComponent::UpdateBounds()
{
	float ScaledRadius = GetScaledCapsuleRadius();
	float ScaledHalfHeight = GetScaledCapsuleHalfHeight();
	FVector Center = GetCapsuleCenter();

	// 회전 정보 가져오기
	FQuat Rotation = GetWorldRotation();

	// 회전이 없으면 기존 방식 사용 (Z축 정렬 가정)
	if (Rotation.IsIdentity())
	{
		FVector Extent(ScaledRadius, ScaledRadius, ScaledHalfHeight + ScaledRadius);
		CachedBounds = FBoxSphereBounds(Center, Extent);
		return;
	}

	// 회전된 Capsule의 끝점 계산
	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);

	// Capsule을 감싸는 AABB 계산
	// 선분의 끝점 + 반지름을 고려
	FVector Min = FVector(
		FMath::Min(SegmentStart.X, SegmentEnd.X) - ScaledRadius,
		FMath::Min(SegmentStart.Y, SegmentEnd.Y) - ScaledRadius,
		FMath::Min(SegmentStart.Z, SegmentEnd.Z) - ScaledRadius
	);

	FVector Max = FVector(
		FMath::Max(SegmentStart.X, SegmentEnd.X) + ScaledRadius,
		FMath::Max(SegmentStart.Y, SegmentEnd.Y) + ScaledRadius,
		FMath::Max(SegmentStart.Z, SegmentEnd.Z) + ScaledRadius
	);

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
FBoxSphereBounds UCapsuleComponent::GetScaledBounds() const
{
	return CachedBounds;
}

/**
 * Capsule을 디버그 렌더링합니다.
 */
void UCapsuleComponent::DrawDebugShape() const
{
	// TODO: 디버그 렌더링 시스템과 연동
	// FVector Center = GetCapsuleCenter();
	// float Radius = GetScaledCapsuleRadius();
	// float HalfHeight = GetScaledCapsuleHalfHeight();
	// DrawDebugCapsule(Center, HalfHeight, Radius, ShapeColor);
}

/**
 * Renderer를 통해 Capsule을 디버그 렌더링합니다.
 * 중심 선분, 상하단 반구, 수직 라인으로 구성된 Capsule 와이어프레임을 그립니다.
 *
 * @param Renderer - 렌더러 객체
 */
void UCapsuleComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (!Renderer)
	{
		return;
	}

	FVector Center = GetCapsuleCenter();
	float Radius = GetScaledCapsuleRadius();
	float HalfHeight = GetScaledCapsuleHalfHeight();

	// Capsule의 선분 끝점 계산
	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);

	// 회전 정보 가져오기
	FQuat Rotation = GetWorldRotation();
	FVector UpVector = Rotation.GetUpVector();
	FVector RightVector = Rotation.GetRightVector();
	FVector ForwardVector = Rotation.GetForwardVector();

	// 라인 데이터 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 충돌 중이면 빨간색, 아니면 원래 색상
	const FVector4 LineColor = bIsOverlapping ?
		FVector4(1.0f, 0.0f, 0.0f, 1.0f) :
		FVector4(ShapeColor.X, ShapeColor.Y, ShapeColor.Z, 1.0f);
	const int32 NumSegments = 16; // 원의 세그먼트 수

	// 상단 반구 (회전이 적용된 원 - Z축에 수직)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + ForwardVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + ForwardVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentEnd + Offset1);
		EndPoints.push_back(SegmentEnd + Offset2);
		Colors.push_back(LineColor);
	}

	// 하단 반구 (회전이 적용된 원 - Z축에 수직)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + ForwardVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + ForwardVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentStart + Offset1);
		EndPoints.push_back(SegmentStart + Offset2);
		Colors.push_back(LineColor);
	}

	// 중앙 원 (회전이 적용된 원 - Z축에 수직)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + ForwardVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + ForwardVector * sin(Angle2)) * Radius;

		StartPoints.push_back(Center + Offset1);
		EndPoints.push_back(Center + Offset2);
		Colors.push_back(LineColor);
	}

	// 상단 반원 호 (Right 방향)
	for (int32 i = 0; i < NumSegments / 2; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentEnd + Offset1);
		EndPoints.push_back(SegmentEnd + Offset2);
		Colors.push_back(LineColor);
	}

	// 하단 반원 호 (Right 방향)
	for (int32 i = NumSegments / 2; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (RightVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (RightVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentStart + Offset1);
		EndPoints.push_back(SegmentStart + Offset2);
		Colors.push_back(LineColor);
	}

	// 상단 반원 호 (Forward 방향)
	for (int32 i = 0; i < NumSegments / 2; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (ForwardVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (ForwardVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentEnd + Offset1);
		EndPoints.push_back(SegmentEnd + Offset2);
		Colors.push_back(LineColor);
	}

	// 하단 반원 호 (Forward 방향)
	for (int32 i = NumSegments / 2; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>(i + 1) / NumSegments) * 2.0f * PI;

		FVector Offset1 = (ForwardVector * cos(Angle1) + UpVector * sin(Angle1)) * Radius;
		FVector Offset2 = (ForwardVector * cos(Angle2) + UpVector * sin(Angle2)) * Radius;

		StartPoints.push_back(SegmentStart + Offset1);
		EndPoints.push_back(SegmentStart + Offset2);
		Colors.push_back(LineColor);
	}

	// 수직 연결 라인 (4개 방향 - 회전 적용)
	FVector Offsets[4] = {
		RightVector * Radius,
		RightVector * -Radius,
		ForwardVector * Radius,
		ForwardVector * -Radius
	};

	for (int32 i = 0; i < 4; ++i)
	{
		StartPoints.push_back(SegmentStart + Offsets[i]);
		EndPoints.push_back(SegmentEnd + Offsets[i]);
		Colors.push_back(LineColor);
	}

	// 렌더러에 라인 추가
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

/**
 * 다른 Shape 컴포넌트와 겹쳐있는지 확인합니다.
 * Capsule vs Capsule, Capsule vs Sphere, Capsule vs Box 충돌을 정밀하게 감지합니다.
 *
 * @param Other - 확인할 상대방 Shape 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UCapsuleComponent::IsOverlappingComponent(const UShapeComponent* Other) const
{
	if (!Other)
	{
		return false;
	}

	// Capsule vs Capsule 충돌
	if (const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(Other))
	{
		return IsOverlappingCapsule(OtherCapsule);
	}

	// Capsule vs Sphere 충돌
	if (const USphereComponent* OtherSphere = Cast<USphereComponent>(Other))
	{
		return IsOverlappingSphere(OtherSphere);
	}

	// Capsule vs Box 충돌
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other))
	{
		return IsOverlappingBox(OtherBox);
	}

	// 기타: Bounds 기반 체크 (부모 클래스 구현)
	return Super::IsOverlappingComponent(Other);
}

// ────────────────────────────────────────────────────────────────────────────
// Capsule 전용 충돌 감지 함수
// ────────────────────────────────────────────────────────────────────────────

/**
 * Capsule의 선분 끝점을 계산합니다 (Z축 방향 기준).
 *
 * @param OutStart - 시작점 (하단)
 * @param OutEnd - 끝점 (상단)
 */
void UCapsuleComponent::GetCapsuleSegment(FVector& OutStart, FVector& OutEnd) const
{
	FVector Center = GetCapsuleCenter();
	float HalfHeight = GetScaledCapsuleHalfHeight();

	// Z축 방향 (로컬)
	FVector UpDirection = GetWorldRotation().GetUpVector();

	OutStart = Center - UpDirection * HalfHeight;
	OutEnd = Center + UpDirection * HalfHeight;
}

/**
 * 점과 선분 사이의 최단 거리 제곱을 계산합니다.
 * 제곱근 연산을 회피하여 성능을 향상시킵니다.
 *
 * @param Point - 확인할 점
 * @param SegmentStart - 선분 시작점
 * @param SegmentEnd - 선분 끝점
 * @return 최단 거리의 제곱
 */
float UCapsuleComponent::PointToSegmentDistanceSquared(
	const FVector& Point,
	const FVector& SegmentStart,
	const FVector& SegmentEnd)
{
	FVector Segment = SegmentEnd - SegmentStart;
	FVector PointToStart = Point - SegmentStart;

	float SegmentLengthSquared = Segment.SizeSquared();

	// 선분이 점인 경우 (길이가 0)
	if (SegmentLengthSquared < KINDA_SMALL_NUMBER)
	{
		return PointToStart.SizeSquared();
	}

	// 투영 비율 계산 (0~1 사이로 클램프)
	// t = 0이면 SegmentStart, t = 1이면 SegmentEnd
	float t = FVector::Dot(PointToStart, Segment) / SegmentLengthSquared;
	t = FMath::Clamp(t, 0.0f, 1.0f);

	// 선분 위의 가장 가까운 점
	FVector ClosestPoint = SegmentStart + Segment * t;

	return (Point - ClosestPoint).SizeSquared();
}

/**
 * 두 선분 사이의 최단 거리 제곱을 계산합니다.
 * 간략 구현: 각 선분의 끝점 4개와 중점 2개를 샘플링하여 근사 계산
 *
 * @param Seg1Start - 첫 번째 선분 시작점
 * @param Seg1End - 첫 번째 선분 끝점
 * @param Seg2Start - 두 번째 선분 시작점
 * @param Seg2End - 두 번째 선분 끝점
 * @return 최단 거리의 제곱
 */
float UCapsuleComponent::SegmentToSegmentDistanceSquared(
	const FVector& Seg1Start,
	const FVector& Seg1End,
	const FVector& Seg2Start,
	const FVector& Seg2End)
{
	// 간략 구현: 각 선분의 끝점과 중점을 샘플링하여 최단 거리 근사
	float MinDistSq = FLT_MAX;

	// Seg1의 샘플 점들과 Seg2 사이의 거리
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg1Start, Seg2Start, Seg2End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg1End, Seg2Start, Seg2End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared((Seg1Start + Seg1End) * 0.5f, Seg2Start, Seg2End));

	// Seg2의 샘플 점들과 Seg1 사이의 거리
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg2Start, Seg1Start, Seg1End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared(Seg2End, Seg1Start, Seg1End));
	MinDistSq = FMath::Min(MinDistSq, PointToSegmentDistanceSquared((Seg2Start + Seg2End) * 0.5f, Seg1Start, Seg1End));

	return MinDistSq;
}

/**
 * 다른 Capsule 컴포넌트와 겹쳐있는지 확인합니다.
 * Capsule vs Capsule 충돌은 두 선분 사이의 거리가 반지름의 합보다 작으면 충돌입니다.
 *
 * @param OtherCapsule - 확인할 상대방 Capsule 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UCapsuleComponent::IsOverlappingCapsule(const UCapsuleComponent* OtherCapsule) const
{
	if (!OtherCapsule)
	{
		return false;
	}

	// 두 Capsule의 선분 끝점 계산
	FVector MyStart, MyEnd;
	GetCapsuleSegment(MyStart, MyEnd);
	float MyRadius = GetScaledCapsuleRadius();

	FVector OtherStart, OtherEnd;
	OtherCapsule->GetCapsuleSegment(OtherStart, OtherEnd);
	float OtherRadius = OtherCapsule->GetScaledCapsuleRadius();

	// 선분 vs 선분 최단 거리 계산
	float DistanceSquared = SegmentToSegmentDistanceSquared(MyStart, MyEnd, OtherStart, OtherEnd);
	float RadiusSum = MyRadius + OtherRadius;

	// 거리가 반지름의 합보다 작거나 같으면 충돌
	return DistanceSquared <= (RadiusSum * RadiusSum);
}

/**
 * Sphere 컴포넌트와 겹쳐있는지 확인합니다.
 * Capsule vs Sphere 충돌은 Sphere 중심과 Capsule 선분 사이의 거리가 반지름의 합보다 작으면 충돌입니다.
 *
 * @param OtherSphere - 확인할 상대방 Sphere 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UCapsuleComponent::IsOverlappingSphere(const USphereComponent* OtherSphere) const
{
	if (!OtherSphere)
	{
		return false;
	}

	FVector SphereCenter = OtherSphere->GetSphereCenter();
	float SphereRadius = OtherSphere->GetScaledSphereRadius();

	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);
	float CapsuleRadius = GetScaledCapsuleRadius();

	// 점과 선분 사이의 거리 계산
	float DistanceSquared = PointToSegmentDistanceSquared(SphereCenter, SegmentStart, SegmentEnd);
	float RadiusSum = CapsuleRadius + SphereRadius;

	// 거리가 반지름의 합보다 작거나 같으면 충돌
	return DistanceSquared <= (RadiusSum * RadiusSum);
}

/**
 * Box 컴포넌트와 겹쳐있는지 확인합니다.
 * Capsule vs Box 충돌은 간략 구현으로 Bounds 기반 체크를 수행합니다.
 * (정밀한 구현은 SAT나 GJK 알고리즘 필요)
 *
 * @param OtherBox - 확인할 상대방 Box 컴포넌트
 * @return 겹쳐있으면 true
 */
bool UCapsuleComponent::IsOverlappingBox(const UBoxComponent* OtherBox) const
{
	if (!OtherBox)
	{
		return false;
	}

	// 간략 구현: Bounds 기반 체크
	// 정밀한 Capsule vs Box 충돌은 복잡하므로 Bounds로 대체
	return Super::IsOverlappingComponent(OtherBox);
}

/**
 * 특정 점이 Capsule 내부에 있는지 확인합니다.
 * 점과 Capsule 선분 사이의 거리가 반지름보다 작으면 포함됩니다.
 *
 * @param Point - 확인할 점 (월드 스페이스)
 * @return 포함되어 있으면 true
 */
bool UCapsuleComponent::ContainsPoint(const FVector& Point) const
{
	FVector SegmentStart, SegmentEnd;
	GetCapsuleSegment(SegmentStart, SegmentEnd);
	float Radius = GetScaledCapsuleRadius();

	// 점과 선분 사이의 거리 계산
	float DistanceSquared = PointToSegmentDistanceSquared(Point, SegmentStart, SegmentEnd);

	// 거리가 반지름보다 작거나 같으면 포함
	return DistanceSquared <= (Radius * Radius);
}
