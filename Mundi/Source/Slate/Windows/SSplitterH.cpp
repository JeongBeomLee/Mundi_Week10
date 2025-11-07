#include "pch.h"
#include "SSplitterH.h"

SSplitterH::~SSplitterH()
{
    //// 자식 윈도우 재귀 해제
    //delete SideLT;
    //delete SideRB;
    //SideLT = nullptr;
    //SideRB = nullptr;
}
void SSplitterH::UpdateDrag(FVector2D MousePos)
{
    if (!bIsDragging) return;

    // 마우스 X 좌표를 비율로 변환
    float NewSplitX = MousePos.X;
    float MinX = Rect.Min.X + SplitterThickness;
    float MaxX = Rect.Max.X - SplitterThickness;

    // 경계 제한
    NewSplitX = FMath::Clamp(NewSplitX, MinX, MaxX);

    // 새 비율 계산
    float NewRatio = (NewSplitX - Rect.Min.X) / GetWidth();
    SetSplitRatio(NewRatio);
}

void SSplitterH::UpdateChildRects()
{
    if (!SideLT || !SideRB) return;

    float SplitX = Rect.Min.X + (GetWidth() * SplitRatio);

    // Left 영역
    SideLT->SetRect(
        Rect.Min.X, Rect.Min.Y,
        SplitX - SplitterThickness / 2, Rect.Max.Y
    );

    // Right 영역
    SideRB->SetRect(
        SplitX + SplitterThickness / 2, Rect.Min.Y,
        Rect.Max.X, Rect.Max.Y
    );
}

FRect SSplitterH::GetSplitterRect() const
{
    float SplitX = Rect.Min.X + (GetWidth() * SplitRatio);
    return FRect(
        SplitX - SplitterThickness / 2, Rect.Min.Y,
        SplitX + SplitterThickness / 2, Rect.Max.Y
    );
}