#include "pch.h"
#include "AABB.h"

FAABB::FAABB() : Min(FVector()), Max(FVector()) {}

FAABB::FAABB(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}

// 중심점
FVector FAABB::GetCenter() const
{
	return (Min + Max) * 0.5f;
}

// 반쪽 크기 (Extent)
FVector FAABB::GetHalfExtent() const
{
	return (Max - Min) * 0.5f;
}

// 다른 박스를 완전히 포함하는지 확인
bool FAABB::Contains(const FAABB& Other) const
{
	return (Min.X <= Other.Min.X && Max.X >= Other.Max.X) &&
		(Min.Y <= Other.Min.Y && Max.Y >= Other.Max.Y) &&
		(Min.Z <= Other.Min.Z && Max.Z >= Other.Max.Z);
}

// 교차 여부만 확인
bool FAABB::Intersects(const FAABB& Other) const
{
	return (Min.X <= Other.Max.X && Max.X >= Other.Min.X) &&
		(Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y) &&
		(Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z);
}

// i번째 옥탄트 Bounds 반환
FAABB FAABB::CreateOctant(int i) const
{
	const FVector Center = GetCenter();

	FVector NewMin, NewMax;

	// X축 (i의 1비트)
	// 0 왼쪽 1 오른쪽 
	if (i & 1)
	{
		NewMin.X = Center.X;
		NewMax.X = Max.X;
	}
	else
	{
		NewMin.X = Min.X;
		NewMax.X = Center.X;
	}

	// Y축 (i의 2비트)
	// 0 앞 2 뒤 
	if (i & 2)
	{
		NewMin.Y = Center.Y;
		NewMax.Y = Max.Y;
	}
	else
	{
		NewMin.Y = Min.Y;
		NewMax.Y = Center.Y;
	}

	// Z축 (i의 4비트)
	// 0 아래 4 위 
	if (i & 4)
	{
		NewMin.Z = Center.Z;
		NewMax.Z = Max.Z;
	}
	else
	{
		NewMin.Z = Min.Z;
		NewMax.Z = Center.Z;
	}

	return FAABB(NewMin, NewMax);
}

bool FAABB::IntersectsRay(const FRay& InRay, float& OutEnterDistance, float& OutExitDistance)
{
	// 레이가 박스를 통과할 수 있는 [Enter, Exit] 구간
	float ClosestEnter = -FLT_MAX;
	float FarthestExit = FLT_MAX;

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		const float RayOriginAxis = InRay.Origin[AxisIndex];
		const float RayDirectionAxis = InRay.Direction[AxisIndex];
		const float BoxMinAxis = Min[AxisIndex];
		const float BoxMaxAxis = Max[AxisIndex];

		// 레이가 축에 평행한데, 박스 범위를 벗어나면 교차 불가
		if (std::abs(RayDirectionAxis) < 1e-6f)
		{
			if (RayOriginAxis < BoxMinAxis || RayOriginAxis > BoxMaxAxis)
			{
				return false;
			}
		}
		else
		{
			const float InvDirection = 1.0f / RayDirectionAxis;

			// 평면과의 교차 거리 
			// 레이가 AABB의 min 평면과 max 평면을 만나는 t 값 (거리)
			float DistanceToMinPlane = (BoxMinAxis - RayOriginAxis) * InvDirection;
			float DistanceToMaxPlane = (BoxMaxAxis - RayOriginAxis) * InvDirection;

			if (DistanceToMinPlane > DistanceToMaxPlane)
			{
				std::swap(DistanceToMinPlane, DistanceToMaxPlane);
			}
			// ClosestEnter : AABB 안에 들어가는 시점
			// 더 늦게 들어오는 값으로 갱신
			if (DistanceToMinPlane > ClosestEnter)  ClosestEnter = DistanceToMinPlane;

			// FarthestExit : AABB에서 나가는 시점
			// 더 빨리 나가는 값으로 갱신 
			if (DistanceToMaxPlane < FarthestExit) FarthestExit = DistanceToMaxPlane;

			// 가장 늦게 들어오는 시점이 빠르게 나가는 시점보다 늦다는 것은 교차하지 않음을 의미한다. 
			if (ClosestEnter > FarthestExit)
			{
				return false; // 레이가 박스를 관통하지 않음
			}
		}
	}
	// 레이가 박스와 실제로 만나는 구간이다. 
	OutEnterDistance = (ClosestEnter < 0.0f) ? 0.0f : ClosestEnter;
	OutExitDistance = FarthestExit;
	return true;
}

FAABB FAABB::Union(const FAABB& A, const FAABB& B)
{
	FAABB out;
	out.Min = FVector(
		std::min(A.Min.X, B.Min.X),
		std::min(A.Min.Y, B.Min.Y),
		std::min(A.Min.Z, B.Min.Z));
	out.Max = FVector(
		std::max(A.Max.X, B.Max.X),
		std::max(A.Max.Y, B.Max.Y),
		std::max(A.Max.Z, B.Max.Z));
	return out;
}