#include "pch.h"
#include "SSplitterV.h"

SSplitterV::~SSplitterV()
{
  /*  delete SideLT;
    delete SideRB;
    SideLT = nullptr;
    SideRB = nullptr;*/
}
void SSplitterV::UpdateDrag(FVector2D MousePos)
{
    if (!bIsDragging) return;

    // 마우스 Y 좌표를 비율로 변환
    float NewSplitY = MousePos.Y;
    float MinY = Rect.Min.Y + SplitterThickness;
    float MaxY = Rect.Max.Y - SplitterThickness;

    // 경계 제한
    NewSplitY = FMath::Clamp(NewSplitY, MinY, MaxY);

    // 새 비율 계산
    float NewRatio = (NewSplitY - Rect.Min.Y) / GetHeight();
    SetSplitRatio(NewRatio);
}

void SSplitterV::UpdateChildRects()
{
    if (!SideLT || !SideRB) return;

    float SplitY = Rect.Min.Y + (GetHeight() * SplitRatio);

    // Top 영역
    SideLT->SetRect(
        Rect.Min.X, Rect.Min.Y,
        Rect.Max.X, SplitY - SplitterThickness / 2
    );

    // Bottom 영역
    SideRB->SetRect(
        Rect.Min.X, SplitY + SplitterThickness / 2,
        Rect.Max.X, Rect.Max.Y
    );
}

FRect SSplitterV::GetSplitterRect() const
{
    float SplitY = Rect.Min.Y + (GetHeight() * SplitRatio);
    return FRect(
        Rect.Min.X, SplitY - SplitterThickness / 2,
        Rect.Max.X, SplitY + SplitterThickness / 2
    );
}
