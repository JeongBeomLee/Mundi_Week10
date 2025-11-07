#pragma once
#include "Vector.h"
#include "Picking.h"

struct FBoundingSphere
{
	FVector Center;
	float Radius;

	FBoundingSphere();
	FBoundingSphere(const FVector& InCenter, float InRadius);
	virtual ~FBoundingSphere() = default;

	// Public 멤버가 있지만 bounding volume 구조체간 일관성을 위해 구현
	FVector GetCenter() const { return Center; }
	float GetRadius() const { return Radius; }

	bool Contains(const FVector& Point) const;
	bool Contains(const FBoundingSphere& Other) const;
	bool Intersects(const FBoundingSphere& Other) const;
	bool IntersectsRay(const FRay& Ray, float& TEnter, float& TExit) const;
};