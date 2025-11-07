#include "pch.h"
#include "BoundingSphere.h"

FBoundingSphere::FBoundingSphere(): Center(FVector(0.f, 0.f, 0.f)), Radius(0.0f) {}

FBoundingSphere::FBoundingSphere(const FVector& InCenter, float InRadius)
	: Center(InCenter), Radius(InRadius)
{
}

bool FBoundingSphere::Contains(const FVector& Point) const
{
	const float DistSquared = (Point - Center).SizeSquared();
	return DistSquared <= (Radius * Radius);
}

bool FBoundingSphere::Contains(const FBoundingSphere& Other) const
{
	const float DistSquared = (Other.Center - Center).SizeSquared();
	const float RadiusDiff = Radius - Other.Radius;
	return DistSquared <= (RadiusDiff * RadiusDiff);
}

bool FBoundingSphere::Intersects(const FBoundingSphere& Other) const
{
	const float DistSquared = (Other.Center - Center).SizeSquared();
	const float RadiusSum = Radius + Other.Radius;
	return DistSquared <= (RadiusSum * RadiusSum);
}

bool FBoundingSphere::IntersectsRay(const FRay& Ray, float& TEnter, float& TExit) const
{
    // 직선 & 구 연립방정식 해 구하기
    const FVector OC = Ray.Origin - Center;
    const float A = FVector::Dot(Ray.Direction, Ray.Direction);
    const float B = 2.0f * FVector::Dot(OC, Ray.Direction);
    const float C = FVector::Dot(OC, OC) - Radius * Radius;

    const float Discriminant = B * B - 4 * A * C;
    if (Discriminant < 0.0f)
        return false;

    const float SqrtDisc = std::sqrt(Discriminant);

    float Tmin = (-B - SqrtDisc) / (2.0f * A);
    float Tmax = (-B + SqrtDisc) / (2.0f * A);
    if (Tmin > Tmax) std::swap(Tmin, Tmax);

    if (Tmax < 0.0f) return false; // 둘 다 뒤쪽

    TEnter = (Tmin < 0.0f) ? 0.0f : Tmin;
    TExit = Tmax;
    return true;
}