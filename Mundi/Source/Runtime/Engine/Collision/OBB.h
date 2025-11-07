#pragma once
#include "Vector.h"
#include "Picking.h"

struct FOBB
{
    FVector Center;
    FVector HalfExtent;
    FVector Axes[3]; // 동적할당하지 않았으므로 기본소멸자로 자동 소멸

    FOBB();
    FOBB(const FAABB& LocalAABB, const FMatrix& WorldMatrix);
    FOBB(const FVector& InCenter, const FVector& InHalfExtent, const FVector (&InAxes)[3]);
    
    FVector GetCenter() const; // Public 멤버가 있지만 bounding volume 구조체간 일관성을 위해 구현
    FVector GetHalfExtent() const; // Public 멤버가 있지만 bounding volume 구조체간 일관성을 위해 구현

    bool Contains(const FVector& Point) const;
    bool Contains(const FOBB& Other) const;
    bool Intersects(const FOBB& Other) const;
    bool IntersectsRay(const FRay& InRay, float& OutEnterDistance, float& OutExitDistance) const;

    // Contain 연산을 위한 헬퍼 함수
    TArray<FVector> GetCorners() const;
};