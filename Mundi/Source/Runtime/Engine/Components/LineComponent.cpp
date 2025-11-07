#include "pch.h"
#include "LineComponent.h"
#include "Renderer.h"

IMPLEMENT_CLASS(ULineComponent)

void ULineComponent::GetWorldLineData(TArray<FVector>& OutStartPoints, TArray<FVector>& OutEndPoints, TArray<FVector4>& OutColors) const
{
    if (!bLinesVisible || Lines.empty())
    {
        OutStartPoints.clear();
        OutEndPoints.clear();
        OutColors.clear();
        return;
    }
    
    FMatrix worldMatrix = GetWorldMatrix();
    size_t lineCount = Lines.size();
    
    for (const ULine* Line : Lines)
    {
        if (Line)
        {
            FVector worldStart, worldEnd;
            Line->GetWorldPoints(worldMatrix, worldStart, worldEnd);
            
            OutStartPoints.push_back(worldStart);
            OutEndPoints.push_back(worldEnd);
            OutColors.push_back(Line->GetColor());
        }
    }
    return;
}

void ULineComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    bLinesVisible = true;

    const uint64 NumLines = Lines.size();
    for (uint64 idx = 0; idx < NumLines; ++idx)
    {
        if (Lines[idx])
        {
            Lines[idx] = Lines[idx]->Duplicate();
        }
    }
}

ULineComponent::ULineComponent()
{
    bLinesVisible = true;
}

ULineComponent::~ULineComponent()
{
    ClearLines();
}

ULine* ULineComponent::AddLine(const FVector& StartPoint, const FVector& EndPoint, const FVector4& Color)
{
    ULine* NewLine = NewObject<ULine>();
    NewLine->SetLine(StartPoint, EndPoint);
    NewLine->SetColor(Color);
    
    Lines.push_back(NewLine);
    
    return NewLine;
}

void ULineComponent::RemoveLine(ULine* Line)
{
    if (!Line) return;
    
    auto it = std::find(Lines.begin(), Lines.end(), Line);
    if (it != Lines.end())
    {
        DeleteObject(*it);
        Lines.erase(it);
    }
}

void ULineComponent::ClearLines()
{
    for (ULine* Line : Lines)
    {
        if (Line)
        {
            DeleteObject(Line);
        }
    }
    Lines.Empty();
}

void ULineComponent::CollectLineBatches(URenderer* Renderer)
{
    if (!HasVisibleLines() || !Renderer)
        return;

    TArray<FVector> startPoints, endPoints;
    TArray<FVector4> colors;
    
    // Extract world coordinate line data efficiently
    GetWorldLineData(startPoints, endPoints, colors);
    
    // Add all lines to renderer batch at once
    if (!startPoints.empty())
    {
        Renderer->AddLines(startPoints, endPoints, colors);
    }
    startPoints.clear();
    startPoints.Shrink();
    endPoints.clear();
    endPoints.Shrink();
    colors.clear();
    colors.Shrink();
}

