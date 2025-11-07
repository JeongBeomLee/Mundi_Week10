#pragma once
#include"Vector.h"
#include "SWindow.h"
#include <fstream>
#include <string>
#include <vector>
class SSplitter : public SWindow
{
public:
    SSplitter();
    virtual ~SSplitter();

    // 자식 윈도우 설정
    void SetLeftOrTop(SWindow* Window) { SideLT = Window; }
    void SetRightOrBottom(SWindow* Window) { SideRB = Window; }

    SWindow* GetLeftOrTop() const { return SideLT; }
    SWindow* GetRightOrBottom() const { return SideRB; }

    // 분할 비율 (0.0 ~ 1.0)
    void SetSplitRatio(float Ratio) { SplitRatio = FMath::Clamp(Ratio, 0.1f, 0.9f); }
    float GetSplitRatio() const { return SplitRatio; }

    // 드래그 관련
    bool IsMouseOnSplitter(FVector2D MousePos) const;
    void StartDrag(FVector2D MousePos);
    virtual void UpdateDrag(FVector2D MousePos);
    void EndDrag();

    // 가상 함수들
    void OnRender() override;
    void OnUpdate(float DeltaSeconds) override;
    void OnMouseMove(FVector2D MousePos) override;
    void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // 설정 저장/로드
    virtual void SaveToConfig(const FString& SectionName) const;
    virtual void LoadFromConfig(const FString& SectionName);

    SWindow* SideLT = nullptr;  // Left or Top
    SWindow* SideRB = nullptr;  // Right or Bottom
    float SplitRatio = 0.5f;    // 분할 비율
protected:


   
    int SplitterThickness = 8;  // 스플리터 두께

    bool bIsDragging = false;
    FVector2D DragStartPos;
    float DragStartRatio;

    // 추상 함수들 - 하위 클래스에서 구현
    virtual void UpdateChildRects() = 0;
    virtual FRect GetSplitterRect() const = 0;
    virtual ImGuiMouseCursor GetMouseCursor() const = 0;
};



