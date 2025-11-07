#pragma once
#include "Vector.h"
#include "AABB.h"
#include "Sphere.h"

struct FBoxSphereBounds
{
    // ────────────────────────────────────────────────
    // ✅ 핵심 데이터
    // ────────────────────────────────────────────────
    FVector Origin;       // 중심점 (Box와 Sphere의 공통 중심)
    FVector BoxExtent;    // Box의 반크기 (Half Extent)
    float SphereRadius;   // Box를 감싸는 구의 반경

    // ────────────────────────────────────────────────
    // ✅ 생성자
    // ────────────────────────────────────────────────
    FBoxSphereBounds() = default;

    FBoxSphereBounds(const FVector& InOrigin, const FVector& InBoxExtent)
        : Origin(InOrigin)
        , BoxExtent(InBoxExtent)
        , SphereRadius(InBoxExtent.Size()) // 대각선 길이 절반 = Bounding Sphere 반지름
    {
    }

    // ────────────────────────────────────────────────
    // ✅ Box (AABB) 반환
    // ────────────────────────────────────────────────
    FAABB GetBox() const
    {
        return FAABB(Origin - BoxExtent, Origin + BoxExtent);
    }

    // ────────────────────────────────────────────────
    // ✅ Sphere 반환
    // ────────────────────────────────────────────────
    FSphere GetSphere() const
    {
        return FSphere(Origin, SphereRadius);
    }

    // ────────────────────────────────────────────────
    // ✅ AABB 교차 여부 (빠른 Broad Phase 용)
    // ────────────────────────────────────────────────
    bool Intersects(const FBoxSphereBounds& Other) const
    {
        return GetBox().Intersects(Other.GetBox());
    }

    // ────────────────────────────────────────────────
    // ✅ Bounds 확장 (예: 여러 Mesh 묶기용)
    // ────────────────────────────────────────────────
    static FBoxSphereBounds Union(const FBoxSphereBounds& A, const FBoxSphereBounds& B)
    {
        FAABB CombinedBox = FAABB::Union(A.GetBox(), B.GetBox());
        FVector Center = CombinedBox.GetCenter();
        FVector Extent = CombinedBox.GetHalfExtent();
        return FBoxSphereBounds(Center, Extent);
    }
};