#pragma once
#include "Object.h"

class AActor;
class UPropertyRenderer;

// 액터 블루프린트 에디터 윈도우
// 언리얼의 블루프린트 에디터처럼 별도 팝업 창으로 액터 속성을 편집
class SActorBlueprintEditor : public UObject
{
public:
    DECLARE_CLASS(SActorBlueprintEditor, UObject)

    SActorBlueprintEditor();
    virtual ~SActorBlueprintEditor() override;

    // 윈도우 열기
    void OpenWindow(AActor* InActor);

    // 윈도우 닫기
    void CloseWindow();

    // 윈도우가 열려있는지 확인
    bool IsOpen() const { return bIsOpen; }

    // 렌더링 (매 프레임 호출)
    void Render();

    // 업데이트 (매 프레임 호출)
    void Update(float DeltaSeconds);

private:
    // 편집 중인 액터
    AActor* TargetActor;

    // 윈도우 열림 상태
    bool bIsOpen;

    // 윈도우 크기
    float WindowWidth;
    float WindowHeight;

    // 선택된 컴포넌트
    UActorComponent* SelectedComponent;

    // UI 렌더링 함수들
    void RenderHeader();
    void RenderComponentTree();
    void RenderComponentDetails();
    void RenderComponentNode(USceneComponent* Component, bool& bComponentSelected);
};
