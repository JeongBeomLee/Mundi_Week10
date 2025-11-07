#include "pch.h"
#include "SActorBlueprintEditor.h"
#include "ImGui/imgui.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "Widgets/PropertyRenderer.h"
#include "World.h"

IMPLEMENT_CLASS(SActorBlueprintEditor)

SActorBlueprintEditor::SActorBlueprintEditor()
    : TargetActor(nullptr)
    , bIsOpen(false)
    , WindowWidth(1000.0f)
    , WindowHeight(800.0f)
    , SelectedComponent(nullptr)
{
}

SActorBlueprintEditor::~SActorBlueprintEditor()
{
}

void SActorBlueprintEditor::OpenWindow(AActor* InActor)
{
    if (!InActor)
    {
        return;
    }

    TargetActor = InActor;
    bIsOpen = true;
    SelectedComponent = TargetActor->GetRootComponent();
}

void SActorBlueprintEditor::CloseWindow()
{
    bIsOpen = false;
    TargetActor = nullptr;
    SelectedComponent = nullptr;
}

void SActorBlueprintEditor::Render()
{
    if (!bIsOpen || !TargetActor)
    {
        return;
    }

    // 윈도우 플래그 설정
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

    // 윈도우 크기 설정 (첫 프레임만)
    ImGui::SetNextWindowSize(ImVec2(WindowWidth, WindowHeight), ImGuiCond_FirstUseEver);

    // 윈도우 시작
    FString WindowTitle = FString("Blueprint Editor - ") + TargetActor->GetName().ToString();
    bool bOpen = bIsOpen;

    if (ImGui::Begin(WindowTitle.c_str(), &bOpen, flags))
    {
        // 윈도우가 닫혔는지 확인
        if (!bOpen)
        {
            CloseWindow();
            ImGui::End();
            return;
        }

        // 메뉴바
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Close"))
                {
                    CloseWindow();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Delete Actor"))
                {
                    // TODO: 액터 삭제 구현
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // 헤더 렌더링
        RenderHeader();

        ImGui::Separator();

        // 레이아웃: 왼쪽(컴포넌트 트리) | 오른쪽(디테일)
        ImGui::BeginChild("LeftPane", ImVec2(300, 0), true);
        RenderComponentTree();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
        RenderComponentDetails();
        ImGui::EndChild();
    }
    ImGui::End();
}

void SActorBlueprintEditor::Update(float DeltaSeconds)
{
    // 편집 중인 액터가 삭제되었는지 확인
    if (bIsOpen && !TargetActor)
    {
        CloseWindow();
    }
}

void SActorBlueprintEditor::RenderHeader()
{
    if (!TargetActor)
    {
        return;
    }

    ImGui::Text("Actor: %s", TargetActor->GetName().ToString().c_str());
    ImGui::Text("Class: %s", TargetActor->GetClass()->Name);

    ImGui::Spacing();

    // 액터 기본 속성 표시
    if (ImGui::CollapsingHeader("Actor Properties", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 액터의 모든 속성 렌더링
        UPropertyRenderer::RenderAllPropertiesWithInheritance(TargetActor);
    }
}

void SActorBlueprintEditor::RenderComponentTree()
{
    ImGui::Text("Components");
    ImGui::Separator();

    if (!TargetActor)
    {
        return;
    }

    // 루트 컴포넌트부터 재귀적으로 렌더링
    USceneComponent* RootComponent = TargetActor->GetRootComponent();
    if (RootComponent)
    {
        bool bComponentSelected = false;
        RenderComponentNode(RootComponent, bComponentSelected);
    }

    // 루트가 아닌 다른 컴포넌트들도 표시
    for (UActorComponent* Component : TargetActor->GetOwnedComponents())
    {
        if (Component && !Cast<USceneComponent>(Component))
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (Component == SelectedComponent)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(Component->GetName().c_str(), flags);

            if (ImGui::IsItemClicked())
            {
                SelectedComponent = Component;
            }
        }
    }

    ImGui::Spacing();

    // 컴포넌트 추가 버튼
    if (ImGui::Button("Add Component", ImVec2(-1, 0)))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        ImGui::Text("Select Component Type");
        ImGui::Separator();

        // 여기에 컴포넌트 목록 추가
        // TODO: 리플렉션 시스템을 통해 자동으로 컴포넌트 목록 표시

        ImGui::EndPopup();
    }
}

void SActorBlueprintEditor::RenderComponentNode(USceneComponent* Component, bool& bComponentSelected)
{
    if (!Component)
    {
        return;
    }

    // 트리 노드 플래그 설정
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (Component == SelectedComponent)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // 자식이 없으면 Leaf 플래그 추가
    if (Component->GetAttachChildren().size() == 0)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    // 트리 노드 렌더링
    bool bNodeOpen = ImGui::TreeNodeEx(Component->GetName().c_str(), flags);

    // 클릭 감지
    if (ImGui::IsItemClicked())
    {
        SelectedComponent = Component;
        bComponentSelected = true;
    }

    // 노드가 열려있으면 자식들 렌더링
    if (bNodeOpen)
    {
        for (USceneComponent* Child : Component->GetAttachChildren())
        {
            RenderComponentNode(Child, bComponentSelected);
        }
        ImGui::TreePop();
    }
}

void SActorBlueprintEditor::RenderComponentDetails()
{
    ImGui::Text("Details");
    ImGui::Separator();

    if (!SelectedComponent)
    {
        ImGui::TextDisabled("No component selected");
        return;
    }

    // 선택된 컴포넌트의 이름과 타입 표시
    ImGui::Text("Component: %s", SelectedComponent->GetName().c_str());
    ImGui::Text("Type: %s", SelectedComponent->GetClass()->Name);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 컴포넌트의 모든 속성 렌더링
    UPropertyRenderer::RenderAllPropertiesWithInheritance(SelectedComponent);
}
