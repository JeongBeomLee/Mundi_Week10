#include "pch.h"
#include "SDetailsWindow.h"
#include "Widgets/TargetActorTransformWidget.h"
#include "Widgets/WorldDetailsWidget.h"
#include "UIManager.h"

SDetailsWindow::SDetailsWindow()
    : SWindow()
    , ActorDetailsWidget(nullptr)
    , WorldDetailsWidget(nullptr)
{
    Initialize();
}

SDetailsWindow::~SDetailsWindow()
{
    if (ActorDetailsWidget)
    {
        DeleteObject(ActorDetailsWidget);
        ActorDetailsWidget = nullptr;
    }

    if (WorldDetailsWidget)
    {
        DeleteObject(WorldDetailsWidget);
        WorldDetailsWidget = nullptr;
    }
}

void SDetailsWindow::Initialize()
{
    // Actor Transform Widget 생성
    ActorDetailsWidget = NewObject<UTargetActorTransformWidget>();
    ActorDetailsWidget->Initialize();

    // World Details Widget 생성
    WorldDetailsWidget = NewObject<UWorldDetailsWidget>();
    WorldDetailsWidget->Initialize();

    // UIManager의 World를 WorldDetailsWidget에 전달
    UWorld* World = GWorld;
    if (GWorld)
    {
        WorldDetailsWidget->SetWorld(GWorld);
    }
}

void SDetailsWindow::OnRender()
{
    // === 패널 위치/크기 고정 (SceneIOWindow와 동일) ===
    ImGui::SetNextWindowPos(ImVec2(Rect.Min.X, Rect.Min.Y));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("디테일", nullptr, flags))
    {
        // World Settings (상단)
        if (WorldDetailsWidget)
        {
            WorldDetailsWidget->RenderWidget();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Actor Details (하단)
        if (ActorDetailsWidget)
        {
            ActorDetailsWidget->RenderWidget();
        }
    }
    ImGui::End();
}

void SDetailsWindow::OnUpdate(float deltaSecond)
{
    if (ActorDetailsWidget)
    {
        ActorDetailsWidget->Update();
    }

    if (WorldDetailsWidget)
    {
        WorldDetailsWidget->Update();
    }
}