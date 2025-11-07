#include "pch.h"
#include "Line.h"

IMPLEMENT_CLASS(ULine)

FVector ULine::GetWorldStartPoint(const FMatrix& WorldMatrix) const
{
    FVector4 StartHomogeneous(StartPoint.X, StartPoint.Y, StartPoint.Z, 1.0f);
    FVector4 WorldStart4 = StartHomogeneous * WorldMatrix;
    return FVector(WorldStart4.X, WorldStart4.Y, WorldStart4.Z);
}

FVector ULine::GetWorldEndPoint(const FMatrix& WorldMatrix) const
{
    FVector4 EndHomogeneous(EndPoint.X, EndPoint.Y, EndPoint.Z, 1.0f);
    FVector4 WorldEnd4 = EndHomogeneous * WorldMatrix;
    return FVector(WorldEnd4.X, WorldEnd4.Y, WorldEnd4.Z);
}

void ULine::GetWorldPoints(const FMatrix& WorldMatrix, FVector& OutStart, FVector& OutEnd) const
{
    FVector4 StartHomogeneous(StartPoint.X, StartPoint.Y, StartPoint.Z, 1.0f);
    FVector4 EndHomogeneous(EndPoint.X, EndPoint.Y, EndPoint.Z, 1.0f);
    
    FVector4 WorldStart4 = StartHomogeneous * WorldMatrix;
    FVector4 WorldEnd4 = EndHomogeneous * WorldMatrix;
    
    OutStart = FVector(WorldStart4.X, WorldStart4.Y, WorldStart4.Z);
    OutEnd = FVector(WorldEnd4.X, WorldEnd4.Y, WorldEnd4.Z);
}

void ULine::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
