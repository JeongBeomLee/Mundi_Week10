
#pragma once
#include "Vector.h"
#include "AABB.h"
#include "math.h"
#include <algorithm>

struct FSphere
{
    FVector Center;   // 구의 중심
    float Radius;     // 반지름

    // ─────────────────────────────────────
    // 기본 생성자
    // ─────────────────────────────────────
    FSphere()
        : Center(FVector()), Radius(0.0f)
    {
    }

    FSphere(const FVector& InCenter, float InRadius)
        : Center(InCenter), Radius(InRadius)
    {
    }

    // ─────────────────────────────────────
    // 중심 이동 (Translate)
    // ─────────────────────────────────────
    inline void Translate(const FVector& Offset)
    {
        Center += Offset;
    }

    // ─────────────────────────────────────
    // 점이 구 내부에 포함되는가?
    // ─────────────────────────────────────
    inline bool Contains(const FVector& Point) const
    {
        return (Point - Center).SizeSquared() <= Radius * Radius;
    }

    // ─────────────────────────────────────
    // 다른 구체와 교차하는가?
    // ─────────────────────────────────────
    inline bool Intersects(const FSphere& Other) const
    {
        const float DistSq = (Center - Other.Center).SizeSquared();
        const float RadiusSum = Radius + Other.Radius;
        return DistSq <= RadiusSum * RadiusSum;
    }

    // ─────────────────────────────────────
    // 구 vs AABB 교차 검사
    // ─────────────────────────────────────
    inline bool IntersectsAABB(const FAABB& Box) const
    {
        FVector ClosestPoint(
            std::clamp(Center.X, Box.Min.X, Box.Max.X),
            std::clamp(Center.Y, Box.Min.Y, Box.Max.Y),
            std::clamp(Center.Z, Box.Min.Z, Box.Max.Z)
        );

        const float DistSq = (Center - ClosestPoint).SizeSquared();
        return DistSq <= Radius * Radius;
    }

    // ─────────────────────────────────────
    // 구체 병합 (두 구를 포함하는 최소 구체)
    // ─────────────────────────────────────
    static FSphere Union(const FSphere& A, const FSphere& B)
    {
        const FVector Delta = B.Center - A.Center;
        const float Dist = Delta.Size();

        // 한쪽 구가 다른 구를 완전히 포함하는 경우
        if (A.Radius >= Dist + B.Radius)
            return A;
        if (B.Radius >= Dist + A.Radius)
            return B;

        // 그렇지 않다면 두 구를 감싸는 새로운 구 계산
        const float NewRadius = (Dist + A.Radius + B.Radius) * 0.5f;
        const FVector Direction = (Dist > 1e-6f) ? Delta / Dist : FVector();
        const FVector NewCenter = A.Center + Direction * (NewRadius - A.Radius);

        return FSphere(NewCenter, NewRadius);
    }

    // ─────────────────────────────────────
    // 디버그용 (크기 문자열)
    // ─────────────────────────────────────
    /*FName ToString() const
    {
        return TEXT("Center(%.2f, %.2f, %.2f), Radius=%.2f"),
            Center.X, Center.Y, Center.Z, Radius);
    }*/
};