#pragma once
#include "Vector.h"
#include "CameraComponent.h"


class UCameraComponent;
struct FAABB;

struct FPlane
{
    // unit vector
    FVector4 Normal = { 0.f, 1.f, 0.f , 0.f };

    // 평면이 원점에서 법선 N 방향으로 얼마만큼 떨어져 있는지
    float Distance = 0.f;
};

struct FFrustum
{
    FPlane TopFace;
    FPlane BottomFace;
    FPlane RightFace;
    FPlane LeftFace;
    FPlane NearFace;
    FPlane FarFace;
};

FFrustum CreateFrustumFromCamera(const UCameraComponent& Camera, float OverrideAspect = -1.0f);
bool IsAABBVisible(const FFrustum& Frustum, const FAABB& Bound);
bool IsAABBIntersects(const FFrustum& Frustum, const FAABB& Bound);

// AVX-optimized culling for 8 AABBs
// Processes 8 AABBs against the frustum.
// Returns an 8-bit mask: bit i is set if box i is visible.
uint8_t AreAABBsVisible_8_AVX(const FFrustum& Frustum, const FAABB Bounds[8]);

bool Intersects(const FPlane& P, const FVector4& Center, const FVector4& Extents);

// Frustum의 절두체 범위를 이루는 8개의 포인트의 월드 공간을 반환합니다.
// Near: 좌하단 -> 우하단 -> 우상단 -> 좌상단 -> Far: 좌하단 -> 우하단 -> 우상단 -> 좌상단 순서
void GetFrustumCornersWorldSpace(const FMatrix& InViewProjInverse, TArray<FVector>& OutCorners);

// CSM: Frustum의 절두체 범위를 이루는 8개의 포인트의 월드 공간을 반환합니다.
void GetFrustumCornersWorldSpace_Partial(const FMatrix& CameraView, const FMatrix& CameraProjection,
	float NearDistance, float FarDistance, TArray<FVector>& OutCorners);