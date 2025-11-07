#include "pch.h"
#include "Collision.h"
#include "AABB.h"
#include "OBB.h"
#include "BoundingSphere.h"

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb)
    {
        const FOBB ConvertedOBB(Aabb, FMatrix::Identity());
        return ConvertedOBB.Intersects(Obb);
    }

    bool Intersects(const FAABB& Aabb, const FBoundingSphere& Sphere)
    {
		// Real Time Rendering 4th, 22.13.2 Sphere/Box Intersection
        float Dist2 = 0.0f;

        for (int32 i = 0; i < 3; ++i)
        {
            if (Sphere.Center[i] < Aabb.Min[i])
            {
                float Delta = Sphere.Center[i] - Aabb.Min[i];
                if (Delta < -Sphere.GetRadius())
					return false;
				Dist2 += Delta * Delta;
            }
            else if (Sphere.Center[i] > Aabb.Max[i])
            {
                float Delta = Sphere.Center[i] - Aabb.Max[i];
                if (Delta > Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
        }
        return Dist2 <= (Sphere.Radius * Sphere.Radius);
	}

    bool Intersects(const FOBB& Obb, const FBoundingSphere& Sphere)
    {
        // Real Time Rendering 4th, 22.13.2 Sphere/Box Intersection
        float Dist2 = 0.0f;
        // OBB의 로컬 좌표계로 구 중심점 변환
        FVector LocalCenter = Obb.Center;
        for (int32 i = 0; i < 3; ++i)
        {
            FVector Axis = Obb.Axes[i];
            float Offset = FVector::Dot(Sphere.Center - Obb.Center, Axis);
            if (Offset > Obb.HalfExtent[i])
                Offset = Obb.HalfExtent[i];
            else if (Offset < -Obb.HalfExtent[i])
                Offset = -Obb.HalfExtent[i];
            LocalCenter += Axis * Offset;
        }
        for (int i = 0; i < 3; ++i)
        {
            if (LocalCenter[i] < -Obb.HalfExtent[i])
            {
                float Delta = LocalCenter[i] + Obb.HalfExtent[i];
                if (Delta < -Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
            else if (LocalCenter[i] > Obb.HalfExtent[i])
            {
                float Delta = LocalCenter[i] - Obb.HalfExtent[i];
                if (Delta > Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
        }
        return Dist2 <= (Sphere.Radius * Sphere.Radius);
	}
}
