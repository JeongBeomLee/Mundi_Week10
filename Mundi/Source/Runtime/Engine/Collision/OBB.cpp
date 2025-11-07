#include "pch.h"
#include <cassert>
#include "OBB.h"
        
FOBB::FOBB() : Center(0.f, 0.f, 0.f)
    , HalfExtent(0.f, 0.f, 0.f)
    , Axes{ {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f} } {}

FOBB::FOBB(const FAABB& LocalAABB, const FMatrix& WorldMatrix)
{
    const FVector LocalCenter = LocalAABB.GetCenter();
    const FVector LocalHalfExtent = LocalAABB.GetHalfExtent();

    const FVector4 Center4(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, 1.0f);
    const FVector4 WorldCenter4 = Center4 * WorldMatrix;
    Center = FVector(WorldCenter4.X, WorldCenter4.Y, WorldCenter4.Z);

    const FVector4 WorldAxisX4 = FVector4(1.0f, 0.0f, 0.0f, 0.0f) * WorldMatrix;
    const FVector4 WorldAxisY4 = FVector4(0.0f, 1.0f, 0.0f, 0.0f) * WorldMatrix;
    const FVector4 WorldAxisZ4 = FVector4(0.0f, 0.0f, 1.0f, 0.0f) * WorldMatrix;

    const FVector WorldAxisX(WorldAxisX4.X, WorldAxisX4.Y, WorldAxisX4.Z);
    const FVector WorldAxisY(WorldAxisY4.X, WorldAxisY4.Y, WorldAxisY4.Z);
    const FVector WorldAxisZ(WorldAxisZ4.X, WorldAxisZ4.Y, WorldAxisZ4.Z);

    const float AxisXLength = WorldAxisX.Size();
    const float AxisYLength = WorldAxisY.Size();
    const float AxisZLength = WorldAxisZ.Size();

    Axes[0] = (AxisXLength > KINDA_SMALL_NUMBER) ? WorldAxisX * (1.0f / AxisXLength) : FVector{0.f, 0.f, 0.f};
    Axes[1] = (AxisYLength > KINDA_SMALL_NUMBER) ? WorldAxisY * (1.0f / AxisYLength) : FVector{0.f, 0.f, 0.f};
    Axes[2] = (AxisZLength > KINDA_SMALL_NUMBER) ? WorldAxisZ * (1.0f / AxisZLength) : FVector{0.f, 0.f, 0.f};
    
    HalfExtent = FVector(LocalHalfExtent.X * AxisXLength,
                         LocalHalfExtent.Y * AxisYLength,
                         LocalHalfExtent.Z * AxisZLength);
}

FOBB::FOBB(const FVector& InCenter, const FVector& InHalfExtent, const FVector (&InAxes)[3])
    : Center(InCenter), HalfExtent(InHalfExtent)
{
    for (int i = 0; i < 3; ++i)
        Axes[i] = InAxes[i].GetNormalized();
}

FVector FOBB::GetCenter() const
{
    return Center;
}

FVector FOBB::GetHalfExtent() const
{
    return HalfExtent;
}

bool FOBB::Contains(const FVector& Point) const
{
    const FVector VectorToPoint = Point - Center;
    const float Limits[3] = { HalfExtent.X, HalfExtent.Y, HalfExtent.Z };

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const FVector& Axis = Axes[AxisIndex];
        const float Limit = Limits[AxisIndex];

        if (Axis.SizeSquared() <= KINDA_SMALL_NUMBER)
        {
            // Axis is zero vector -> Invalid OBB
            return false;
        }

        const float Projection = FVector::Dot(VectorToPoint, Axis);
        if (Projection > Limit || Projection < -Limit)
        {
            return false;
        }
    }
    return true;
}


bool FOBB::Contains(const FOBB& Other) const
{
    const TArray<FVector> Corners = Other.GetCorners();
    for (const FVector& Corner : Corners)
    {
        if (!Contains(Corner))
        {
            return false;
        }
    }
    return true;
}

TArray<FVector> FOBB::GetCorners() const
{
    TArray<FVector> Corners;
    Corners.reserve(8);

    const FVector LocalOffsets[8] =
    {
        { +HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z },
        { +HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z },
        { +HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z },
        { +HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z },
        { -HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z },
        { -HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z },
        { -HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z },
        { -HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z }
    };

    for (const FVector& Offset : LocalOffsets)
    {
        Corners.emplace_back(
            Center
            + Axes[0] * Offset.X
            + Axes[1] * Offset.Y
            + Axes[2] * Offset.Z);
    }

    return Corners;
}

// SAT
bool FOBB::Intersects(const FOBB& Other) const
{
    // 표기: this = A, Other = B
    const FVector CenA = Center;
    const FVector ExtA = HalfExtent;
    const FVector (&UA)[3] = Axes;

    const FVector CenB = Other.Center;
    const FVector ExtB = Other.HalfExtent;
    const FVector (&UB)[3] = Other.Axes;

    // 중심차 t 를 A의 좌표계 성분으로 표현
    const FVector T = CenB - CenA;

    // Dot Product Table
    float R[3][3];
    float AbsR[3][3];
    for (int32 i = 0; i < 3 ; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            R[i][j] = FVector::Dot(UA[i], UB[j]);
            AbsR[i][j] = std::fabsf(R[i][j]) + KINDA_SMALL_NUMBER;
        }
    }

    // 1) A의 3축
    for (int32 i = 0; i < 3; ++i)
    {
        const float RA = (&ExtA.X)[i];
        const float RB = ExtB.X * AbsR[i][0] + ExtB.Y * AbsR[i][1] + ExtB.Z * AbsR[i][2];
        const float ProjT = FVector::Dot(T, UA[i]);
        if (std::fabsf(ProjT) > RA + RB)
            return false;
    }

    // 2) B의 3축
    for (int32 j = 0; j < 3; ++j)
    {
        const float RA = ExtA.X * AbsR[0][j] + ExtA.Y * AbsR[1][j] + ExtA.Z * AbsR[2][j];
        const float RB = (&ExtB.X)[j];
        const float ProjT = FVector::Dot(T, UB[j]);
        if (std::fabsf(ProjT) > RA + RB)
            return false;
    }

    // 3) 9개 교차축
    for (int32 i = 0; i < 3; ++i)
    {
        const int32 i0 = i;
        const int32 i1 = (i + 1) % 3;
        const int32 i2 = (i + 2) % 3;
        for (int32 j = 0; j < 3; ++j)
        {
            const int32 j0 = j;
            const int32 j1 = (j + 1) % 3;
            const int32 j2 = (j + 2) % 3;

            const float RA = (&ExtA.X)[i1]*AbsR[i2][j0] + (&ExtA.X)[i2]*AbsR[i1][j0];
            const float RB = (&ExtB.X)[j1]*AbsR[i0][j2] + (&ExtB.X)[j2]*AbsR[i0][j1];
            
            const float TL = std::fabsf(FVector::Dot(T, UA[i2])*R[i1][j0] - FVector::Dot(T, UA[i1])*R[i2][j0]);
            if (TL > RA + RB)
                return false;
        }
    }
    return true;
}

bool FOBB::IntersectsRay(const FRay& InRay, float& OutEnterDistance, float& OutExitDistance) const
{
    UE_LOG("WARNING: Method 'FOBB::IntersectsRay' is not Implemented.");
    return false;
}