#pragma once
#include "Object.h"
#include "Vector.h"

/**
 * ULine - Individual line data structure
 * Represents a single line with start/end points and visual properties
 */
class ULine : public UObject
{
public:
    DECLARE_CLASS(ULine, UObject)
    
    ULine() = default;
    ULine(const FVector& InStart, const FVector& InEnd, const FVector4& InColor = FVector4(1,1,1,1))
        : StartPoint(InStart), EndPoint(InEnd), Color(InColor) {}
    
    virtual ~ULine() override = default;

public:
    // Getters
    const FVector& GetStartPoint() const { return StartPoint; }
    const FVector& GetEndPoint() const { return EndPoint; }
    FVector4 GetColor() const { return Color; }
    float GetThickness() const { return Thickness; }
    
    // Setters
    void SetStartPoint(const FVector& InStart) { StartPoint = InStart; }
    void SetEndPoint(const FVector& InEnd) { EndPoint = InEnd; }
    void SetLine(const FVector& InStart, const FVector& InEnd)
    {
        StartPoint = InStart;
        EndPoint = InEnd;
    }
    void SetColor(const FVector4& InColor) { Color = InColor; }
    void SetThickness(float InThickness) { Thickness = InThickness; }
    
    // Utility methods
    FVector GetDirection() const { return (EndPoint - StartPoint).GetNormalized(); }
    float GetLength() const { return (EndPoint - StartPoint).Size(); }
    FVector GetCenter() const { return (StartPoint + EndPoint) * 0.5f; }
    
    // World coordinate methods
    FVector GetWorldStartPoint(const FMatrix& WorldMatrix) const;
    FVector GetWorldEndPoint(const FMatrix& WorldMatrix) const;
    void GetWorldPoints(const FMatrix& WorldMatrix, FVector& OutStart, FVector& OutEnd) const;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(ULine)


private:
    // Line geometry
    FVector StartPoint = FVector();
    FVector EndPoint = FVector();
    
    // Visual properties
    FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Line properties
    float Thickness = 1.0f;  // For future use
};