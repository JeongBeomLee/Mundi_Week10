#include "pch.h"
#include "USlateManager.h"
#include "Windows/SWindow.h"
#include "Windows/SSplitterV.h"
#include "Windows/SDetailsWindow.h"
#include "Windows/SControlPanel.h"
#include "Windows/ControlPanelWindow.h"
#include "Windows/SViewportWindow.h"
#include "Windows/ConsoleWindow.h"
#include "Windows/SActorBlueprintEditor.h"
#include "Widgets/MainToolbarWidget.h"
#include "Widgets/GameHUDWidget.h"
#include "FViewportClient.h"
#include "UIManager.h"
#include "GlobalConsole.h"
#include "GameModeBase.h"
#include "RunnerGameMode.h"
#include "Actor.h"

IMPLEMENT_CLASS(USlateManager)

USlateManager& USlateManager::GetInstance()
{
    static USlateManager* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<USlateManager>();
    }
    return *Instance;
}
#include "FViewportClient.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow* USlateManager::ActiveViewport;

void USlateManager::SaveSplitterConfig()
{
    if (!TopPanel) return;

    EditorINI["TopPanel"] = std::to_string(TopPanel->SplitRatio);
    EditorINI["LeftTop"] = std::to_string(LeftTop->SplitRatio);
    EditorINI["LeftBottom"] = std::to_string(LeftBottom->SplitRatio);
    EditorINI["LeftPanel"] = std::to_string(LeftPanel->SplitRatio);
    EditorINI["RightPanel"] = std::to_string(RightPanel->SplitRatio);
}

void USlateManager::LoadSplitterConfig()
{
    if (!TopPanel) return;

    if (EditorINI.Contains("TopPanel"))
        TopPanel->SplitRatio = std::stof(EditorINI["TopPanel"]);
    if (EditorINI.Contains("LeftTop"))
        LeftTop->SplitRatio = std::stof(EditorINI["LeftTop"]);
    if (EditorINI.Contains("LeftBottom"))
        LeftBottom->SplitRatio = std::stof(EditorINI["LeftBottom"]);
    if (EditorINI.Contains("LeftPanel"))
        LeftPanel->SplitRatio = std::stof(EditorINI["LeftPanel"]);
    if (EditorINI.Contains("RightPanel"))
        RightPanel->SplitRatio = std::stof(EditorINI["RightPanel"]);
}

USlateManager::USlateManager()
{
    for (int i = 0; i < 4; i++)
        Viewports[i] = nullptr;
}

USlateManager::~USlateManager()
{
    Shutdown();
}

void USlateManager::Initialize(ID3D11Device* InDevice, UWorld* InWorld, const FRect& InRect)
{
#ifndef STANDALONE_BUILD
    // MainToolbar 생성 (Editor 전용)
    MainToolbar = NewObject<UMainToolbarWidget>();
    MainToolbar->Initialize();
#endif

    // GameHUD 생성
    //GameHUD = NewObject<UGameHUDWidget>();
    //GameHUD->Initialize();

    Device = InDevice;
    World = InWorld;
    Rect = InRect;

#ifdef STANDALONE_BUILD
    // Standalone 모드: 메인 뷰포트만 생성
    MainViewport = new SViewportWindow();
    MainViewport->Initialize(0, 0, Rect.GetWidth(), Rect.GetHeight(),
        World, Device, EViewportType::Perspective);
    World->SetCameraActor(MainViewport->GetViewportClient()->GetCamera());

    // 배열 초기화
    for (int i = 0; i < 4; i++)
        Viewports[i] = nullptr;
#else
    // === 전체 화면: 좌(4뷰포트) + 우(Control + Details) ===
    TopPanel = new SSplitterH();  // 수평 분할 (좌우)
    TopPanel->SetSplitRatio(0.7f);  // 70% 뷰포트, 30% UI
    TopPanel->SetRect(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);

    // 왼쪽: 4분할 뷰포트 영역
    LeftPanel = new SSplitterH();  // 수평 분할 (좌우)
    LeftTop = new SSplitterV();    // 수직 분할 (상하)
    LeftBottom = new SSplitterV(); // 수직 분할 (상하)
    LeftPanel->SideLT = LeftTop;
    LeftPanel->SideRB = LeftBottom;

    // 오른쪽: Control + Details (상하 분할)
    RightPanel = new SSplitterV();  // 수직 분할 (상하)
    RightPanel->SetSplitRatio(0.5f);  // 50-50 분할

    ControlPanel = new SControlPanel();
    DetailPanel = new SDetailsWindow();

    RightPanel->SideLT = ControlPanel;   // 위쪽: ControlPanel
    RightPanel->SideRB = DetailPanel;    // 아래쪽: DetailsWindow

    // TopPanel 좌우 배치
    TopPanel->SideLT = LeftPanel;
    TopPanel->SideRB = RightPanel;

    // === 뷰포트 생성 ===
    Viewports[0] = new SViewportWindow();
    Viewports[1] = new SViewportWindow();
    Viewports[2] = new SViewportWindow();
    Viewports[3] = new SViewportWindow();
    MainViewport = Viewports[0];

    Viewports[0]->Initialize(0, 0,
        Rect.GetWidth() / 2, Rect.GetHeight() / 2,
        World, Device, EViewportType::Perspective);

    Viewports[1]->Initialize(Rect.GetWidth() / 2, 0,
        Rect.GetWidth(), Rect.GetHeight() / 2,
        World, Device, EViewportType::Orthographic_Front);

    Viewports[2]->Initialize(0, Rect.GetHeight() / 2,
        Rect.GetWidth() / 2, Rect.GetHeight(),
        World, Device, EViewportType::Orthographic_Left);

    Viewports[3]->Initialize(Rect.GetWidth() / 2, Rect.GetHeight() / 2,
        Rect.GetWidth(), Rect.GetHeight(),
        World, Device, EViewportType::Orthographic_Top);

    World->SetCameraActor(MainViewport->GetViewportClient()->GetCamera());

    // 뷰포트들을 2x2로 연결
    LeftTop->SideLT = Viewports[0];
    LeftTop->SideRB = Viewports[1];
    LeftBottom->SideLT = Viewports[2];
    LeftBottom->SideRB = Viewports[3];

    SwitchLayout(EViewportLayoutMode::SingleMain);

    LoadSplitterConfig();

    // === Console Overlay 생성 ===
    ConsoleWindow = new UConsoleWindow();
    if (ConsoleWindow)
    {
        UE_LOG("USlateManager: ConsoleWindow created successfully");
        UGlobalConsole::SetConsoleWidget(ConsoleWindow->GetConsoleWidget());
        UE_LOG("USlateManager: GlobalConsole connected to ConsoleWidget");
    }
    else
    {
        UE_LOG("ERROR: Failed to create ConsoleWindow");
    }

    // === Blueprint Editor 생성 ===
    ActorBlueprintEditor = NewObject<SActorBlueprintEditor>();
#endif
}

void USlateManager::SwitchLayout(EViewportLayoutMode NewMode)
{
    if (NewMode == CurrentMode) return;

    if (NewMode == EViewportLayoutMode::FourSplit)
    {
        TopPanel->SideLT = LeftPanel;
    }
    else if (NewMode == EViewportLayoutMode::SingleMain)
    {
        TopPanel->SideLT = MainViewport;
    }

    CurrentMode = NewMode;
}

void USlateManager::SwitchPanel(SWindow* SwitchPanel)
{
    if (TopPanel->SideLT != SwitchPanel) {
        TopPanel->SideLT = SwitchPanel;
        CurrentMode = EViewportLayoutMode::SingleMain;
    }
    else {
        TopPanel->SideLT = LeftPanel;
        CurrentMode = EViewportLayoutMode::FourSplit;
    }
}

void USlateManager::Render()
{
#ifndef STANDALONE_BUILD
    // 메인 툴바 렌더링 (Editor 전용)
    if (MainToolbar)
        MainToolbar->RenderWidget();
#endif

#ifdef STANDALONE_BUILD
    // Standalone 모드: 메인 뷰포트만 렌더
    if (MainViewport)
    {
        MainViewport->OnRender();
    }
#else
    // Editor 모드: TopPanel 렌더
    if (TopPanel)
    {
        TopPanel->OnRender();
    }
#endif

    // 게임 HUD 오버레이 렌더링 (PIE 모드에서만)
    if (GameHUD)
    {
        // MainViewport의 Rect를 GameHUD에 전달 (오버레이 영역 제한용)
        if (MainViewport)
        {
            FRect viewportRect = MainViewport->GetRect();
            float vpX = viewportRect.Left;
            float vpY = viewportRect.Top;
            float vpWidth = viewportRect.Right - viewportRect.Left;
            float vpHeight = viewportRect.Bottom - viewportRect.Top;
            GameHUD->SetViewportBounds(vpX, vpY, vpWidth, vpHeight);
        }
        GameHUD->Update();
        GameHUD->RenderWidget();
    }

    // 콘솔 오버레이 렌더링 (모든 것 위에 표시)
    if (ConsoleWindow && ConsoleAnimationProgress > 0.0f)
    {
        extern float CLIENTWIDTH;
        extern float CLIENTHEIGHT;

        // 부드러운 감속을 위한 ease-out 곡선 적용
        float EasedProgress = 1.0f - (1.0f - ConsoleAnimationProgress) * (1.0f - ConsoleAnimationProgress);

        // 콘솔 높이 초기화 (첫 실행 시)
        if (ConsoleHeight == 0.0f)
        {
            ConsoleHeight = CLIENTHEIGHT * ConsoleHeightRatio;
        }

        // 좌우 여백을 포함한 콘솔 크기 계산
        float ConsoleWidth = CLIENTWIDTH - (ConsoleHorizontalMargin * 2.0f);
        float ConsoleXPos = ConsoleHorizontalMargin;

        // Y 위치 계산 (하단에서 슬라이드 업)
        float YPosWhenHidden = CLIENTHEIGHT; // 화면 밖 (하단)
        float YPosWhenVisible = CLIENTHEIGHT - ConsoleHeight; // 화면 내 (하단)
        float CurrentYPos = YPosWhenHidden + (YPosWhenVisible - YPosWhenHidden) * EasedProgress;

        // 둥근 모서리 스타일 적용
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));

        // 윈도우 위치 및 크기 설정
        ImGui::SetNextWindowPos(ImVec2(ConsoleXPos, CurrentYPos));
        ImGui::SetNextWindowSize(ImVec2(ConsoleWidth, ConsoleHeight));

        // 윈도우 플래그 (NoResize 유지 - 커스텀 리사이징 사용)
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;

        // 처음 열렸을 때 콘솔에 포커스
        if (bConsoleShouldFocus)
        {
            ImGui::SetNextWindowFocus();
            bConsoleShouldFocus = false;
        }

        // 콘솔 렌더링
        bool isWindowOpen = true;
        if (ImGui::Begin("ConsoleOverlay", &isWindowOpen, flags))
        {
            // 콘솔이 포커스를 잃으면 닫기
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                bIsConsoleVisible &&
                !bIsConsoleAnimating)
            {
                ToggleConsole(); // 콘솔 닫기
            }

            // 둥근 모서리가 있는 반투명 배경 추가
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 WindowPos = ImGui::GetWindowPos();
            ImVec2 WindowSize = ImGui::GetWindowSize();
            DrawList->AddRectFilled(
                WindowPos,
                ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y),
                IM_COL32(20, 20, 20, 240), // 높은 불투명도의 어두운 배경
                12.0f // 둥근 정도
            );

            // 콘솔 위젯 렌더링
            ConsoleWindow->RenderWidget();

            // 윗 테두리 리사이징 처리 (애니메이션이 완료된 경우에만)
            if (!bIsConsoleAnimating && EasedProgress >= 1.0f)
            {
                ImVec2 MousePos = ImGui::GetMousePos();

                // 윗 테두리 영역 정의
                ImVec2 ResizeBorderMin = ImVec2(WindowPos.x, WindowPos.y);
                ImVec2 ResizeBorderMax = ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + ConsoleResizeBorderThickness);

                bool bIsHoveringResizeBorder = ImGui::IsMouseHoveringRect(ResizeBorderMin, ResizeBorderMax);

                // 마우스 커서 변경
                if (bIsHoveringResizeBorder || bIsResizingConsole)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                }

                // 드래그 시작
                if (bIsHoveringResizeBorder && ImGui::IsMouseClicked(0))
                {
                    bIsResizingConsole = true;
                    ResizeDragStartY = MousePos.y;
                    ResizeDragStartHeight = ConsoleHeight;
                }

                // 드래그 중
                if (bIsResizingConsole)
                {
                    if (ImGui::IsMouseDown(0))
                    {
                        // 드래그 거리 계산 (위로 드래그하면 음수, 아래로 드래그하면 양수)
                        float DragDelta = MousePos.y - ResizeDragStartY;

                        // 새로운 높이 계산 (위로 드래그하면 높이 증가)
                        float NewHeight = ResizeDragStartHeight - DragDelta;

                        // 최소/최대 높이 제한
                        float MaxHeight = CLIENTHEIGHT * ConsoleMaxHeightRatio;
                        NewHeight = std::max(ConsoleMinHeight, std::min(NewHeight, MaxHeight));

                        ConsoleHeight = NewHeight;
                    }
                    else
                    {
                        // 마우스 버튼을 놓으면 드래그 종료
                        bIsResizingConsole = false;
                    }
                }

                // 리사이징 핸들 시각화 (선택사항)
                if (bIsHoveringResizeBorder || bIsResizingConsole)
                {
                    DrawList->AddRectFilled(
                        ResizeBorderMin,
                        ResizeBorderMax,
                        IM_COL32(100, 150, 255, 100) // 밝은 파란색 반투명
                    );
                }
            }
        }
        ImGui::End();

        // 스타일 변수 및 색상 복원
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
    }

    // 블루프린트 에디터 렌더링 (팝업 윈도우)
    if (ActorBlueprintEditor && ActorBlueprintEditor->IsOpen())
    {
        ActorBlueprintEditor->Render();
    }
}

void USlateManager::Update(float DeltaSeconds)
{
    ProcessInput();

#ifndef STANDALONE_BUILD
    // MainToolbar 업데이트 (Editor 전용)
    if (MainToolbar)
        MainToolbar->Update();
#endif

#ifdef STANDALONE_BUILD
    // Standalone 모드: 메인 뷰포트만 업데이트
    if (MainViewport)
    {
        MainViewport->Rect = FRect(0, 0, CLIENTWIDTH, CLIENTHEIGHT);
        MainViewport->OnUpdate(DeltaSeconds);
    }
#else
    // Editor 모드: TopPanel 업데이트
    if (TopPanel)
    {
        // 툴바 높이만큼 아래로 이동 (50px)
        const float toolbarHeight = 50.0f;
        TopPanel->Rect = FRect(0, toolbarHeight, CLIENTWIDTH, CLIENTHEIGHT);
        TopPanel->OnUpdate(DeltaSeconds);
    }
#endif

    // 콘솔 애니메이션 업데이트
    if (bIsConsoleAnimating)
    {
        if (bIsConsoleVisible)
        {
            // 애니메이션 인 (나타남)
            ConsoleAnimationProgress += DeltaSeconds / ConsoleAnimationDuration;
            if (ConsoleAnimationProgress >= 1.0f)
            {
                ConsoleAnimationProgress = 1.0f;
                bIsConsoleAnimating = false;
            }
        }
        else
        {
            // 애니메이션 아웃 (사라짐)
            ConsoleAnimationProgress -= DeltaSeconds / ConsoleAnimationDuration;
            if (ConsoleAnimationProgress <= 0.0f)
            {
                ConsoleAnimationProgress = 0.0f;
                bIsConsoleAnimating = false;
            }
        }
    }

    // ConsoleWindow 업데이트
    if (ConsoleWindow && ConsoleAnimationProgress > 0.0f)
    {
        ConsoleWindow->Update();
    }

    // 블루프린트 에디터 업데이트
    if (ActorBlueprintEditor && ActorBlueprintEditor->IsOpen())
    {
        ActorBlueprintEditor->Update(DeltaSeconds);
    }
}

void USlateManager::ProcessInput()
{
    const FVector2D MousePosition = INPUT.GetMousePosition();

    if (INPUT.IsMouseButtonPressed(LeftButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseDown(MousePosition, 0);
        }
    }
    if (INPUT.IsMouseButtonPressed(RightButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseDown(MousePosition, 1);
        }
    }
    if (INPUT.IsMouseButtonReleased(LeftButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseUp(MousePosition, 0);
        }
    }
    if (INPUT.IsMouseButtonReleased(RightButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseUp(MousePosition, 1);
        }
    }
    OnMouseMove(MousePosition);

    // Alt + ` (억음 부호 키)로 콘솔 토글
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent) && ImGui::GetIO().KeyAlt)
    {
        ToggleConsole();
    }

    // 단축키로 기즈모 모드 변경
    if (World->GetGizmoActor())
        World->GetGizmoActor()->ProcessGizmoModeSwitch();
}

void USlateManager::OnMouseMove(FVector2D MousePos)
{
    if (ActiveViewport)
    {
        ActiveViewport->OnMouseMove(MousePos);
    }
    else if (TopPanel)
    {
        TopPanel->OnMouseMove(MousePos);
    }
}

void USlateManager::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (ActiveViewport)
    {
    }
    else if (TopPanel)
    {
        TopPanel->OnMouseDown(MousePos, Button);

        // 어떤 뷰포트 안에서 눌렸는지 확인
        for (auto* VP : Viewports)
        {
            if (VP && VP->Rect.Contains(MousePos))
            {
                ActiveViewport = VP; // 고정

                // 우클릭인 경우 커서 숨김 및 잠금
                if (Button == 1)
                {
                    INPUT.SetCursorVisible(false);
                    INPUT.LockCursor();
                }
                break;
            }
        }
    }
}

void USlateManager::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    // 우클릭 해제 시 커서 복원 (ActiveViewport와 무관하게 처리)
    if (Button == 1 && INPUT.IsCursorLocked())
    {
        INPUT.SetCursorVisible(true);
        INPUT.ReleaseCursor();
    }

    if (ActiveViewport)
    {
        ActiveViewport->OnMouseUp(MousePos, Button);
        ActiveViewport = nullptr; // 드래그 끝나면 해제
    }
    // NOTE: ActiveViewport가 있더라도 Up 이벤트는 항상 보내주어 드래그 관련 버그를 제거
    if (TopPanel)
    {
        TopPanel->OnMouseUp(MousePos, Button);
    }
}

void USlateManager::OnShutdown()
{
    SaveSplitterConfig();
}

void USlateManager::Shutdown()
{
    // 레이아웃/설정 저장
    SaveSplitterConfig();

    // 콘솔 윈도우 삭제
    if (ConsoleWindow)
    {
        delete ConsoleWindow;
        ConsoleWindow = nullptr;
        UE_LOG("USlateManager: ConsoleWindow destroyed");
    }

    // D3D 컨텍스트를 해제하기 위해 UI 패널과 뷰포트를 명시적으로 삭제
    if (TopPanel) { delete TopPanel; TopPanel = nullptr; }
    if (LeftTop) { delete LeftTop; LeftTop = nullptr; }
    if (LeftBottom) { delete LeftBottom; LeftBottom = nullptr; }
    if (LeftPanel) { delete LeftPanel; LeftPanel = nullptr; }
    if (RightPanel) { delete RightPanel; RightPanel = nullptr; }

    if (ControlPanel) { delete ControlPanel; ControlPanel = nullptr; }
    if (DetailPanel) { delete DetailPanel; DetailPanel = nullptr; }

    for (int i = 0; i < 4; ++i)
    {
        if (Viewports[i]) { delete Viewports[i]; Viewports[i] = nullptr; }
    }
    MainViewport = nullptr;
    ActiveViewport = nullptr;
}

void USlateManager::SetPIEWorld(UWorld* InWorld)
{
    MainViewport->SetVClientWorld(InWorld);

    // PIE World의 GameState를 GameHUD에 설정
    if (GameHUD && InWorld)
    {
        AGameStateBase* GameState = InWorld->GetGameState();
        GameHUD->SetGameState(GameState);

        // PIE World의 GameMode를 GameHUD에 설정 (체력 델리게이트용)
        AGameModeBase* GameModeBase = InWorld->GetGameMode();
        if (GameModeBase)
        {
            // RunnerGameMode로 캐스팅 시도
            ARunnerGameMode* RunnerMode = Cast<ARunnerGameMode>(GameModeBase);
            if (RunnerMode)
            {
                GameHUD->SetGameMode(RunnerMode);
                UE_LOG("USlateManager: GameHUD connected to RunnerGameMode");
            }
        }
    }
}

void USlateManager::ToggleConsole()
{
    bIsConsoleVisible = !bIsConsoleVisible;
    bIsConsoleAnimating = true;

    // 콘솔을 열 때 포커스 플래그 설정
    if (bIsConsoleVisible)
    {
        bConsoleShouldFocus = true;
    }
}

void USlateManager::SetActiveViewport(SViewportWindow* ActiveViewport)
{
    // 모든 뷰포트 비활성화
    for (int i = 0; i < 4; i++)
    {
        if (Viewports[i])
        {
            Viewports[i]->SetActive(false);
            if (Viewports[i]->GetViewportClient())
            {
                Viewports[i]->GetViewportClient()->SetActive(false);
            }
        }
    }

    // 메인 뷰포트도 비활성화
    if (MainViewport)
    {
        MainViewport->SetActive(false);
        if (MainViewport->GetViewportClient())
        {
            MainViewport->GetViewportClient()->SetActive(false);
        }
    }

    // 선택된 뷰포트 활성화
    if (ActiveViewport)
    {
        ActiveViewport->SetActive(true);
        if (ActiveViewport->GetViewportClient())
        {
            ActiveViewport->GetViewportClient()->SetActive(true);
        }
    }
}

void USlateManager::StopPilotingActor(AActor* TargetActor)
{
    if (!TargetActor)
        return;

    // 모든 뷰포트에서 해당 액터를 piloting 중이면 해제
    for (int i = 0; i < 4; i++)
    {
        if (Viewports[i] && Viewports[i]->GetViewportClient())
        {
            FViewportClient* Client = Viewports[i]->GetViewportClient();
            if (Client->IsPiloting() && Client->GetPilotActor() == TargetActor)
            {
                Client->StopPiloting();
            }
        }
    }

    // 메인 뷰포트도 확인
    if (MainViewport && MainViewport->GetViewportClient())
    {
        FViewportClient* Client = MainViewport->GetViewportClient();
        if (Client->IsPiloting() && Client->GetPilotActor() == TargetActor)
        {
            Client->StopPiloting();
        }
    }
}

// ───────────────────────────────────────────
// 블루프린트 에디터 관리
// ───────────────────────────────────────────

void USlateManager::OpenActorBlueprintEditor(AActor* Actor)
{
    if (!Actor || !ActorBlueprintEditor)
    {
        return;
    }

    ActorBlueprintEditor->OpenWindow(Actor);
}

void USlateManager::CloseActorBlueprintEditor()
{
    if (ActorBlueprintEditor)
    {
        ActorBlueprintEditor->CloseWindow();
    }
}

bool USlateManager::IsActorBlueprintEditorOpen() const
{
    return ActorBlueprintEditor && ActorBlueprintEditor->IsOpen();
}
