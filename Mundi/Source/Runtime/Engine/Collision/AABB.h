#pragma once
#include "Vector.h"
#include "Picking.h"

enum class EAxis : uint8
{
	X = 0,
	Y = 1,
	Z = 2
};

// NOTE:
// Originally named FBound.
// Renamed to FAABB as part of introducing OBB support (2025-10).
// This struct now explicitly represents an Axis-Aligned Bounding Box.
struct FAABB
{
	FVector Min;
	FVector Max;

	FAABB();
	FAABB(const FVector& InMin, const FVector& InMax);

	// 중심점
	FVector GetCenter() const;

	// 반쪽 크기 (Extent)
	FVector GetHalfExtent() const;

	// 다른 박스를 완전히 포함하는지 확인
	bool Contains(const FAABB& Other) const;

	// 교차 여부만 확인
	bool Intersects(const FAABB& Other) const;

	// i번째 옥탄트 Bounds 반환
	FAABB CreateOctant(int i) const;
	
	bool IntersectsRay(const FRay& InRay, float& OutEnterDistance, float& OutExitDistance);

	static FAABB Union(const FAABB& A, const FAABB& B);
};