#pragma once
#include "SWindow.h"

class UPropertyWindow;
class UTargetActorTransformWidget;
class UWorldDetailsWidget;

class SDetailsWindow :
    public SWindow
{
public:
    SDetailsWindow();
    virtual ~SDetailsWindow();

    void Initialize();
    virtual void OnRender() override;
    virtual void OnUpdate(float deltaSecond) override;

private:
    // Actor 디테일 위젯
    UTargetActorTransformWidget* ActorDetailsWidget;

    // World 설정 위젯
    UWorldDetailsWidget* WorldDetailsWidget;
};

