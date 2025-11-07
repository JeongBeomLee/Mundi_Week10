#pragma once
#include "Vector.h"

struct FRect
{
    float Left;
    float Top;
    float Right;
    float Bottom;

    FVector2D Min; // 좌상단 (Left, Top)
    FVector2D Max; // 우하단 (Right, Bottom)

    FRect() : Left(0), Top(0), Right(0), Bottom(0), Min(0, 0), Max(0, 0) {}
    FRect(float InLeft, float InTop, float InRight, float InBottom)
        : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom)
        , Min(InLeft, InTop), Max(InRight, InBottom) {}

    float GetWidth() const { return Right - Left; }
    float GetHeight() const { return Bottom - Top; }

    bool Contains(FVector2D P) const
    {
        // swindow 내부 영역은 보수적으로 검사해서 스플리터와 겹치지 않게 처리
        return (P.X > Left && P.X < Right &&
            P.Y > Top && P.Y < Bottom);
    }

    void UpdateMinMax()
    {
        Min.X = Left; Min.Y = Top;
        Max.X = Right; Max.Y = Bottom;
    }
};

class SWindow
{
public:
    SWindow() = default;
    virtual ~SWindow() = default;

    // 기본 속성
    FRect Rect;


    // 마우스 호버 검사
    bool IsHover(FVector2D coord) const
    {
        return coord.X >= Rect.Min.X && coord.X <= Rect.Max.X &&
            coord.Y >= Rect.Min.Y && coord.Y <= Rect.Max.Y;
    }

    // 가상 함수들
    virtual void OnRender() {}
    virtual void OnUpdate(float DeltaSeconds) {}
    virtual void OnMouseMove(FVector2D MousePos) {}
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) {}
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) {}

    // 영역 설정
    void SetRect(const FRect& NewRect) { Rect = NewRect; }
    void SetRect(float MinX, float MinY, float MaxX, float MaxY)
    {
        Rect.Left = MinX; Rect.Top = MinY;
        Rect.Right = MaxX; Rect.Bottom = MaxY;
        Rect.UpdateMinMax();
    }

    FRect GetRect() const { return Rect; }
    float GetWidth() const { return Rect.Max.X - Rect.Min.X; }
    float GetHeight() const { return Rect.Max.Y - Rect.Min.Y; }
};