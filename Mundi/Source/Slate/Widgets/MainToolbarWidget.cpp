#include "pch.h"
#include "MainToolbarWidget.h"
#include "ImGui/imgui.h"
#include "Level.h"
#include "JsonSerializer.h"
#include "SelectionManager.h"
#include "CameraActor.h"
#include "EditorEngine.h"
#include "UIManager.h"
#include "Windows/CameraBlendEditorWindow.h"
#include "Windows/UIWindow.h"
#include <commdlg.h>
#include <random>

IMPLEMENT_CLASS(UMainToolbarWidget)

UMainToolbarWidget::UMainToolbarWidget()
    : UWidget("Main Toolbar")
{
}

void UMainToolbarWidget::Initialize()
{
    LoadToolbarIcons();
}

void UMainToolbarWidget::Update()
{
    // 키보드 단축키 처리
    HandleKeyboardShortcuts();

    // 대기 중인 명령 처리
    ProcessPendingCommands();
}

void UMainToolbarWidget::RenderWidget()
{
    RenderToolbar();
}

void UMainToolbarWidget::LoadToolbarIcons()
{
    // 아이콘 로딩 (사용자가 파일을 제공할 예정)
    IconNew = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_New.png");
    IconSave = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Save.png");
    IconLoad = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Load.png");
    IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Play.png");
    IconStop = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_Stop.png");
    IconAddActor = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_AddActor.png");
    IconBlendEditor = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Toolbar_BlendEditor.png");
    LogoTexture = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Mundi_Logo.png");
}

void UMainToolbarWidget::RenderToolbar()
{
    // 툴바 윈도우 설정
    const float ToolbarHeight = 50.0f;
    ImVec2 ToolbarPos(0, 0);
    ImVec2 ToolbarSize(ImGui::GetIO().DisplaySize.x, ToolbarHeight);

    ImGui::SetNextWindowPos(ToolbarPos);
    ImGui::SetNextWindowSize(ToolbarSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("##MainToolbar", nullptr, flags))
    {
        // 상단 테두리 박스 렌더링
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        const float BoxHeight = 8.0f;

        ImGui::GetWindowDrawList()->AddRectFilled(
            windowPos,
            ImVec2(windowPos.x + windowSize.x, windowPos.y + BoxHeight),
            ImGui::GetColorU32(ImVec4(0.15f, 0.45f, 0.25f, 1.0f))  // 진한 초록색 악센트
        );

        // 수직 중앙 정렬
        float cursorY = (ToolbarHeight - IconSize) / 2.0f;
        ImGui::SetCursorPosY(cursorY);
        ImGui::SetCursorPosX(8.0f); // 왼쪽 여백

        // Scene 관리 버튼들
        RenderSceneButtons();

        // 구분선
        ImGui::SameLine(0, 12.0f);
        ImVec2 separatorStart = ImGui::GetCursorScreenPos();
        separatorStart.y += 4.0f;  // 5픽셀 아래로 이동
        ImGui::GetWindowDrawList()->AddLine(
            separatorStart,
            ImVec2(separatorStart.x, separatorStart.y + IconSize),
            ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 0.8f)),
            2.0f
        );
        ImGui::Dummy(ImVec2(2.0f, IconSize));

        // Actor Spawn 버튼
        ImGui::SameLine(0, 12.0f);
        RenderActorSpawnButton();

        // Blend Editor 버튼
        ImGui::SameLine(0, 12.0f);
        RenderBlendEditorButton();

        // 구분선
        ImGui::SameLine(0, 12.0f);
        separatorStart = ImGui::GetCursorScreenPos();
        separatorStart.y += 4.0f;  // 5픽셀 아래로 이동
        ImGui::GetWindowDrawList()->AddLine(
            separatorStart,
            ImVec2(separatorStart.x, separatorStart.y + IconSize),
            ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 0.8f)),
            2.0f
        );
        ImGui::Dummy(ImVec2(2.0f, IconSize));

        // PIE 제어 버튼들
        ImGui::SameLine(0, 12.0f);
        RenderPIEButtons();

        // 로고를 오른쪽에 배치
        if (LogoTexture && LogoTexture->GetShaderResourceView())
        {
            const float LogoHeight = ToolbarHeight * 0.9f;  // 툴바 높이의 70%
            const float LogoWidth = LogoHeight * 3.42f;     // 820:240 비율
            const float RightPadding = 16.0f;

            ImVec2 logoPos;
            logoPos.x = ImGui::GetWindowWidth() - LogoWidth - RightPadding;
            logoPos.y = (ToolbarHeight - LogoHeight) / 2.0f;

            ImGui::SetCursorPos(logoPos);
            ImGui::Image((void*)LogoTexture->GetShaderResourceView(), ImVec2(LogoWidth, LogoHeight));
        }
    }
    ImGui::End();
}

void UMainToolbarWidget::BeginButtonGroup()
{
    ImGui::BeginGroup();
}

void UMainToolbarWidget::EndButtonGroup()
{
    ImGui::EndGroup();

    // 그룹 영역 계산
    ImVec2 groupMin = ImGui::GetItemRectMin();
    ImVec2 groupMax = ImGui::GetItemRectMax();

    const float Padding = 1.0f;
    groupMin.x -= Padding;
    groupMin.y -= Padding;
    groupMax.x += Padding;
    groupMax.y += Padding;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 테두리만 그리기 (배경 제거)
    drawList->AddRect(
        groupMin,
        groupMax,
        ImGui::GetColorU32(ImVec4(0.4f, 0.45f, 0.5f, 0.8f)),
        4.0f,
        0,
        1.3f
    );
}

void UMainToolbarWidget::RenderSceneButtons()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 버튼 스타일 설정 (SViewportWindow 스타일)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(9, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    BeginButtonGroup();

    // New 버튼
    if (IconNew && IconNew->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##NewBtn", (void*)IconNew->GetShaderResourceView(), IconSizeVec))
        {
            PendingCommand = EToolbarCommand::NewScene;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("새 씬을 생성합니다 [Ctrl+N]");
    }

    ImGui::SameLine();

    // Save 버튼
    if (IconSave && IconSave->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SaveBtn", (void*)IconSave->GetShaderResourceView(), IconSizeVec))
        {
            PendingCommand = EToolbarCommand::SaveScene;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("현재 씬을 저장합니다 [Ctrl+S]");
    }

    ImGui::SameLine();

    // Load 버튼
    if (IconLoad && IconLoad->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##LoadBtn", (void*)IconLoad->GetShaderResourceView(), IconSizeVec))
        {
            PendingCommand = EToolbarCommand::LoadScene;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("씬을 불러옵니다 [Ctrl+O]");
    }

    EndButtonGroup();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void UMainToolbarWidget::RenderActorSpawnButton()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 드롭다운 버튼 스타일
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    ImGui::BeginGroup();

    // 아이콘과 화살표를 포함하는 버튼
    bool bButtonClicked = false;
    if (IconAddActor && IconAddActor->GetShaderResourceView())
    {
        // 커서 위치 저장
        ImVec2 buttonStartPos = ImGui::GetCursorScreenPos();

        // 아이콘+텍스트를 포함하는 투명 버튼 (전체 영역)
        const float ButtonWidth = IconSize + 20.0f;  // 아이콘 + 화살표 공간
        const float ButtonHeight = IconSize + 9.0f;

        if (ImGui::Button("##AddActorBtnInvisible", ImVec2(ButtonWidth, ButtonHeight)))
        {
            bButtonClicked = true;
        }
        bool bIsHovered = ImGui::IsItemHovered();

        // 버튼 위에 아이콘과 텍스트를 오버레이
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 아이콘 그리기
        ImVec2 iconPos = buttonStartPos;
        iconPos.x += 4.0f;
        iconPos.y += 4.0f;
        drawList->AddImage(
            (void*)IconAddActor->GetShaderResourceView(),
            iconPos,
            ImVec2(iconPos.x + IconSize, iconPos.y + IconSize)
        );

        // 화살표 그리기
        ImVec2 arrowPos = buttonStartPos;
        arrowPos.x += IconSize + 5.0f;
        arrowPos.y += (ButtonHeight - 20.0f) / 2.0f;

        ImVec4 textColor = bIsHovered ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        drawList->AddText(arrowPos, ImGui::GetColorU32(textColor), "∨");

        // 툴팁
        if (bIsHovered)
            ImGui::SetTooltip("액터를 월드에 추가합니다");
    }

    // Actor Spawn 버튼만 테두리를 아래로 5픽셀 추가 확장
    ImVec2 groupMin = ImGui::GetItemRectMin();
    ImVec2 groupMax = ImGui::GetItemRectMax();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRect(
        groupMin,
        groupMax,
        ImGui::GetColorU32(ImVec4(0.4f, 0.45f, 0.5f, 0.8f)),
        4.0f,
        0,
        1.3f
    );

    ImGui::EndGroup();

    // 팝업 열기
    if (bButtonClicked)
    {
        ImGui::OpenPopup("ActorSpawnPopup");
    }

    // Actor Spawn 팝업 렌더링
    if (ImGui::BeginPopup("ActorSpawnPopup"))
    {
        // 버튼 색상 스타일 설정 (테마에 맞게)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.18f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

        // 랜덤 스폰 설정 섹션
        ImGui::Checkbox("랜덤 스폰 모드", &RandomSpawnSettings.bEnabled);

        if (RandomSpawnSettings.bEnabled)
        {
            // 스폰 개수
            ImGui::Text("스폰 개수");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputInt("##SpawnCount", &RandomSpawnSettings.SpawnCount);
            if (RandomSpawnSettings.SpawnCount < 1) RandomSpawnSettings.SpawnCount = 1;
            if (RandomSpawnSettings.SpawnCount > 1000) RandomSpawnSettings.SpawnCount = 1000;

            // 스폰 범위 타입
            const char* rangeTypes[] = { "중심점 + 반경", "박스 영역", "선택 액터 주변", "뷰포트 중심" };
            int currentRangeType = static_cast<int>(RandomSpawnSettings.RangeType);
            ImGui::Text("스폰 범위");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::Combo("##SpawnRange", &currentRangeType, rangeTypes, IM_ARRAYSIZE(rangeTypes)))
            {
                RandomSpawnSettings.RangeType = static_cast<ESpawnRangeType>(currentRangeType);
            }

            // 범위별 파라미터
            switch (RandomSpawnSettings.RangeType)
            {
            case ESpawnRangeType::CenterRadius:
                ImGui::DragFloat3("중심점", &RandomSpawnSettings.Center.X, 10.0f);
                ImGui::DragFloat("반경", &RandomSpawnSettings.Radius, 10.0f, 0.0f, 10000.0f);
                break;

            case ESpawnRangeType::BoundingBox:
                ImGui::DragFloat3("최소 좌표", &RandomSpawnSettings.BoxMin.X, 10.0f);
                ImGui::DragFloat3("최대 좌표", &RandomSpawnSettings.BoxMax.X, 10.0f);
                break;

            case ESpawnRangeType::AroundSelection:
                ImGui::DragFloat("반경", &RandomSpawnSettings.Radius, 10.0f, 0.0f, 10000.0f);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "선택된 액터 주변에 스폰");
                break;

            case ESpawnRangeType::ViewportCenter:
                ImGui::DragFloat("반경", &RandomSpawnSettings.Radius, 10.0f, 0.0f, 10000.0f);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "뷰포트 중심에 스폰");
                break;
            }

            // 회전 축 설정
            ImGui::Text("랜덤 회전 축:");
            ImGui::Checkbox("Pitch (X축)", &RandomSpawnSettings.bRandomPitch);
            ImGui::SameLine();
            ImGui::Checkbox("Yaw (Z축)", &RandomSpawnSettings.bRandomYaw);
            ImGui::SameLine();
            ImGui::Checkbox("Roll (Y축)", &RandomSpawnSettings.bRandomRoll);

            ImGui::Separator();
        }

        ImGui::PopStyleColor(6);

        // 액터 리스트
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "액터 선택");
        ImGui::Separator();

        TArray<UClass*> SpawnableActors = UClass::GetAllSpawnableActors();
        for (UClass* ActorClass : SpawnableActors)
        {
            if (ActorClass && ActorClass->bIsSpawnable && ActorClass->DisplayName)
            {
                ImGui::PushID(ActorClass->DisplayName);
                if (ImGui::Selectable(ActorClass->DisplayName))
                {
                    // Actor 생성 명령 큐에 추가
                    PendingCommand = EToolbarCommand::SpawnActor;
                    PendingActorClass = ActorClass;
                    ImGui::CloseCurrentPopup();
                }
                if (ActorClass->Description && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", ActorClass->Description);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void UMainToolbarWidget::RenderPIEButtons()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 버튼 스타일
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    BeginButtonGroup();

#ifdef _EDITOR
    extern UEditorEngine GEngine;
    bool isPIE = GEngine.IsPIEActive();

    // Play 버튼
    ImGui::BeginDisabled(isPIE);
    if (IconPlay && IconPlay->GetShaderResourceView())
    {
        ImVec4 tint = isPIE ? ImVec4(0.5f, 0.5f, 0.5f, 0.5f) : ImVec4(1, 1, 1, 1);
        if (ImGui::ImageButton("##PlayBtn", (void*)IconPlay->GetShaderResourceView(),
                                IconSizeVec, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint))
        {
            PendingCommand = EToolbarCommand::StartPIE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play In Editor를 시작합니다 [F5]");
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Stop 버튼
    ImGui::BeginDisabled(!isPIE);
    if (IconStop && IconStop->GetShaderResourceView())
    {
        ImVec4 tint = !isPIE ? ImVec4(0.5f, 0.5f, 0.5f, 0.5f) : ImVec4(1, 1, 1, 1);
        if (ImGui::ImageButton("##StopBtn", (void*)IconStop->GetShaderResourceView(),
                                IconSizeVec, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint))
        {
            PendingCommand = EToolbarCommand::EndPIE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play In Editor를 종료합니다 [Shift+F5]");
    }
    ImGui::EndDisabled();
#endif

    EndButtonGroup();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void UMainToolbarWidget::OnNewScene()
{
    try
    {
        UWorld* CurrentWorld = GWorld;
        if (!CurrentWorld)
        {
            UE_LOG("MainToolbar: Cannot find World!");
            return;
        }

        // 로드 직전: Transform 위젯/선택 초기화
        UUIManager::GetInstance().ClearTransformWidgetSelection();
        GWorld->GetSelectionManager()->ClearSelection();

        // 새 레벨 생성 후 월드에 적용
        CurrentWorld->GetLightManager()->ClearAllLightList();
        CurrentWorld->SetLevel(ULevelService::CreateNewLevel());

        UE_LOG("MainToolbar: New scene created");
    }
    catch (const std::exception& Exception)
    {
        UE_LOG("MainToolbar: Create Error: %s", Exception.what());
    }
}

void UMainToolbarWidget::OnSaveScene()
{
    // Windows 파일 다이얼로그 열기
    std::filesystem::path selectedPath = OpenSaveFileDialog();
    if (selectedPath.empty())
        return;

    try
    {
        UWorld* CurrentWorld = GWorld;
        if (!CurrentWorld)
        {
            UE_LOG("MainToolbar: Cannot find World!");
            return;
        }

        // 파일 경로에서 씬 이름 추출
        FString SceneName = selectedPath.stem().string(); // 확장자 제외한 파일명

        // Scene 디렉토리가 없으면 생성
        namespace fs = std::filesystem;
        fs::path SceneDir = "Scene";
        if (!fs::exists(SceneDir))
        {
            fs::create_directories(SceneDir);
            UE_LOG("MainToolbar: Created Scene directory");
        }

        FString FilePath = "Scene/" + SceneName + ".Scene";

        // Scene JSON 생성
        JSON SceneJson;

        // Level 데이터 저장
        JSON LevelJson;
        CurrentWorld->GetLevel()->Serialize(false, LevelJson);
        SceneJson["Level"] = LevelJson;

        // World 설정 저장 (PIE 설정)
        JSON WorldSettingsJson;
        if (CurrentWorld->GetGameModeClass())
        {
            WorldSettingsJson["GameModeClass"] = CurrentWorld->GetGameModeClass()->Name;
        }
        if (CurrentWorld->GetDefaultPawnClass())
        {
            WorldSettingsJson["DefaultPawnClass"] = CurrentWorld->GetDefaultPawnClass()->Name;
        }
        if (CurrentWorld->GetPlayerControllerClass())
        {
            WorldSettingsJson["PlayerControllerClass"] = CurrentWorld->GetPlayerControllerClass()->Name;
        }

        FVector SpawnLoc = CurrentWorld->GetPlayerSpawnLocation();
        WorldSettingsJson["PlayerSpawnLocation"] = FJsonSerializer::VectorToJson(SpawnLoc);

        SceneJson["WorldSettings"] = WorldSettingsJson;

        // 파일 저장
        bool bSuccess = FJsonSerializer::SaveJsonToFile(SceneJson, FilePath);

        UE_LOG("MainToolbar: Scene saved: %s (with World settings)", SceneName.c_str());
    }
    catch (const std::exception& Exception)
    {
        UE_LOG("MainToolbar: Save Error: %s", Exception.what());
    }
}

void UMainToolbarWidget::OnLoadScene()
{
    // Windows 파일 다이얼로그 열기
    std::filesystem::path selectedPath = OpenLoadFileDialog();
    if (selectedPath.empty())
        return;

    try
    {
        FString InFilePath = selectedPath.string();

        // 파일명에서 씬 이름 추출
        FString SceneName = InFilePath;
        size_t LastSlash = SceneName.find_last_of("\\/");
        if (LastSlash != std::string::npos)
        {
            SceneName = SceneName.substr(LastSlash + 1);
        }
        size_t LastDot = SceneName.find_last_of(".");
        if (LastDot != std::string::npos)
        {
            SceneName = SceneName.substr(0, LastDot);
        }

        // World 가져오기
        UWorld* CurrentWorld = GWorld;
        if (!CurrentWorld)
        {
            UE_LOG("MainToolbar: Cannot find World!");
            return;
        }

        // 로드 직전: Transform 위젯/선택 초기화
        UUIManager::GetInstance().ClearTransformWidgetSelection();
        GWorld->GetSelectionManager()->ClearSelection();

        // Scene JSON 로드
        JSON SceneJson;
        if (!FJsonSerializer::LoadJsonFromFile(SceneJson, InFilePath))
        {
            UE_LOG("MainToolbar: Failed To Load Scene From: %s", InFilePath.c_str());
            return;
        }

        // Level 데이터 로드
        std::unique_ptr<ULevel> NewLevel = ULevelService::CreateDefaultLevel();
        JSON LevelJsonData;
        if (FJsonSerializer::ReadObject(SceneJson, "Level", LevelJsonData, nullptr, false))
        {
            NewLevel->Serialize(true, LevelJsonData);
        }
        else
        {
            // 구버전 호환: 직접 Level 데이터인 경우
            NewLevel->Serialize(true, SceneJson);
        }
        CurrentWorld->SetLevel(std::move(NewLevel));

        // World 설정 로드
        JSON WorldSettingsJson;
        if (FJsonSerializer::ReadObject(SceneJson, "WorldSettings", WorldSettingsJson, nullptr, false))
        {
            // Helper lambda: 클래스 이름으로부터 UClass 로드
            auto LoadClass = [&](const char* Key, auto SetterFunc) {
                FString ClassName;
                if (FJsonSerializer::ReadString(WorldSettingsJson, Key, ClassName, "", false))
                {
                    UClass* Class = UClass::FindClass(ClassName);
                    if (Class)
                    {
                        (CurrentWorld->*SetterFunc)(Class);
                        UE_LOG("MainToolbar: Loaded %s: %s", Key, ClassName.c_str());
                    }
                }
            };

            // 클래스 설정 로드
            LoadClass("GameModeClass", &UWorld::SetGameModeClass);
            LoadClass("DefaultPawnClass", &UWorld::SetDefaultPawnClass);
            LoadClass("PlayerControllerClass", &UWorld::SetPlayerControllerClass);

            // PlayerSpawnLocation 로드
            FVector SpawnLoc;
            if (FJsonSerializer::ReadVector(WorldSettingsJson, "PlayerSpawnLocation", SpawnLoc, FVector(0.0f, 0.0f, 100.0f), false))
            {
                CurrentWorld->SetPlayerSpawnLocation(SpawnLoc);
                UE_LOG("MainToolbar: Loaded PlayerSpawnLocation: (%.1f, %.1f, %.1f)",
                    SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);
            }
        }

        UE_LOG("MainToolbar: Scene loaded successfully: %s (with World settings)", InFilePath.c_str());
    }
    catch (const std::exception& Exception)
    {
        UE_LOG("MainToolbar: Load Error: %s", Exception.what());
    }
}

std::filesystem::path UMainToolbarWidget::OpenSaveFileDialog()
{
    using std::filesystem::path;

    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {};
    wchar_t szInitialDir[260] = {};

    // Scene 폴더를 기본 경로로 설정
    std::filesystem::path sceneDir = std::filesystem::current_path() / "Scene";
    wcscpy_s(szInitialDir, sceneDir.wstring().c_str());

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Scene Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrTitle = L"Save Scene File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    ofn.lpstrDefExt = L"scene";

    UE_LOG("MainToolbar: Opening Save Dialog (Modal)...");

    if (GetSaveFileNameW(&ofn) == TRUE)
    {
        UE_LOG("MainToolbar: Save Dialog Closed");
        return path(szFile);
    }

    UE_LOG("MainToolbar: Save Dialog Cancelled");
    return L"";
}

std::filesystem::path UMainToolbarWidget::OpenLoadFileDialog()
{
    using std::filesystem::path;

    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {};
    wchar_t szInitialDir[260] = {};

    // Scene 폴더를 기본 경로로 설정
    std::filesystem::path sceneDir = std::filesystem::current_path() / "Scene";
    wcscpy_s(szInitialDir, sceneDir.wstring().c_str());

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Scene Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrTitle = L"Load Scene File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLESIZING;

    UE_LOG("MainToolbar: Opening Load Dialog (Modal)...");

    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        UE_LOG("MainToolbar: Load Dialog Closed");
        return path(szFile);
    }

    UE_LOG("MainToolbar: Load Dialog Cancelled");
    return L"";
}

FVector UMainToolbarWidget::GetRandomPositionInRange() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());

    FVector position(0, 0, 0);

    switch (RandomSpawnSettings.RangeType)
    {
    case ESpawnRangeType::CenterRadius:
    {
        // 구형 분포 랜덤 위치
        std::uniform_real_distribution<float> distAngle(0.0f, 360.0f);
        std::uniform_real_distribution<float> distRadius(0.0f, RandomSpawnSettings.Radius);

        float yaw = distAngle(gen);
        float pitch = distAngle(gen);
        float radius = distRadius(gen);

        // 구형 좌표를 직교 좌표로 변환 (Z-Up)
        float radYaw = yaw * (3.14159265f / 180.0f);
        float radPitch = pitch * (3.14159265f / 180.0f);

        position.X = RandomSpawnSettings.Center.X + radius * cos(radPitch) * cos(radYaw);
        position.Y = RandomSpawnSettings.Center.Y + radius * cos(radPitch) * sin(radYaw);
        position.Z = RandomSpawnSettings.Center.Z + radius * sin(radPitch);
        break;
    }

    case ESpawnRangeType::BoundingBox:
    {
        // 박스 영역 내 균등 분포
        std::uniform_real_distribution<float> distX(RandomSpawnSettings.BoxMin.X, RandomSpawnSettings.BoxMax.X);
        std::uniform_real_distribution<float> distY(RandomSpawnSettings.BoxMin.Y, RandomSpawnSettings.BoxMax.Y);
        std::uniform_real_distribution<float> distZ(RandomSpawnSettings.BoxMin.Z, RandomSpawnSettings.BoxMax.Z);

        position.X = distX(gen);
        position.Y = distY(gen);
        position.Z = distZ(gen);
        break;
    }

    case ESpawnRangeType::AroundSelection:
    {
        // 선택된 액터 주변
        if (GWorld && GWorld->GetSelectionManager())
        {
            auto selectedActors = GWorld->GetSelectionManager()->GetSelectedActors();
            if (!selectedActors.empty())
            {
                AActor* selectedActor = selectedActors[0];
                FVector center = selectedActor->GetActorLocation();

                std::uniform_real_distribution<float> distAngle(0.0f, 360.0f);
                std::uniform_real_distribution<float> distRadius(0.0f, RandomSpawnSettings.Radius);

                float yaw = distAngle(gen);
                float radius = distRadius(gen);
                float radYaw = yaw * (3.14159265f / 180.0f);

                position.X = center.X + radius * cos(radYaw);
                position.Y = center.Y + radius * sin(radYaw);
                position.Z = center.Z;
            }
            else
            {
                // 선택 없으면 원점 기준
                position = FVector(0, 0, 0);
            }
        }
        break;
    }

    case ESpawnRangeType::ViewportCenter:
    {
        FVector center(0, 0, 0);

        if (GWorld && GWorld->GetCameraActor())
        {
            ACameraActor* camera = GWorld->GetCameraActor();
            FVector cameraPos = camera->GetActorLocation();
            FVector cameraForward = camera->GetForward();

            center = cameraPos + cameraForward * 10.0f;
        }

        std::uniform_real_distribution<float> distAngle(0.0f, 360.0f);
        std::uniform_real_distribution<float> distRadius(0.0f, RandomSpawnSettings.Radius);

        float yaw = distAngle(gen);
        float radius = distRadius(gen);
        float radYaw = yaw * (3.14159265f / 180.0f);

        position.X = center.X + radius * cos(radYaw);
        position.Y = center.Y + radius * sin(radYaw);
        position.Z = center.Z;
        break;
    }
    }

    return position;
}

FVector UMainToolbarWidget::GetRandomRotation() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 360.0f);

    FVector rotation(0, 0, 0);

    if (RandomSpawnSettings.bRandomPitch)
        rotation.X = dist(gen);

    if (RandomSpawnSettings.bRandomYaw)
        rotation.Y = dist(gen);

    if (RandomSpawnSettings.bRandomRoll)
        rotation.Z = dist(gen);

    return rotation;
}

void UMainToolbarWidget::ProcessPendingCommands()
{
    if (PendingCommand == EToolbarCommand::None)
        return;

    switch (PendingCommand)
    {
    case EToolbarCommand::NewScene:
        OnNewScene();
        break;

    case EToolbarCommand::SaveScene:
        OnSaveScene();
        break;

    case EToolbarCommand::LoadScene:
        OnLoadScene();
        break;

    case EToolbarCommand::SpawnActor:
        if (PendingActorClass && GWorld)
        {
            if (RandomSpawnSettings.bEnabled)
            {
                // 랜덤 스폰 모드
                int spawnCount = RandomSpawnSettings.SpawnCount;
                UE_LOG("MainToolbar: Random spawning %d actors of type %s", spawnCount, PendingActorClass->DisplayName);

                for (int i = 0; i < spawnCount; ++i)
                {
                    AActor* NewActor = GWorld->SpawnActor(PendingActorClass);
                    if (NewActor)
                    {
                        // 고유 이름 생성
                        FString ActorName = GWorld->GenerateUniqueActorName(PendingActorClass->DisplayName);
                        NewActor->SetName(ActorName);

                        // 랜덤 위치 설정
                        FVector randomPos = GetRandomPositionInRange();
                        NewActor->SetActorLocation(randomPos);

                        // 랜덤 회전 설정
                        FVector randomRot = GetRandomRotation();
                        NewActor->SetActorRotation(randomRot);
                    }
                }

                UE_LOG("MainToolbar: Spawned %d random actors", spawnCount);
            }
            else
            {
                // 일반 스폰 모드 - 뷰포트 중심 앞쪽에 스폰
                AActor* NewActor = GWorld->SpawnActor(PendingActorClass);
                if (NewActor)
                {
                    // 고유 이름 생성
                    FString ActorName = GWorld->GenerateUniqueActorName(PendingActorClass->DisplayName);
                    NewActor->SetName(ActorName);

                    // 카메라 앞쪽에 배치
                    ACameraActor* Camera = GWorld->GetCameraActor();
                    if (Camera)
                    {
                        FVector cameraPos = Camera->GetActorLocation();
                        FVector cameraForward = Camera->GetForward();

                        // 카메라 앞쪽 10 유닛 위치에 스폰
                        FVector spawnPos = cameraPos + cameraForward * 10.0f;
                        NewActor->SetActorLocation(spawnPos);
                    }

                    // 생성된 액터를 선택 상태로 만들기
                    HandleActorSelection(NewActor);

                    UE_LOG("MainToolbar: Spawned actor %s", ActorName.c_str());
                }
            }
        }
        PendingActorClass = nullptr;
        break;

    case EToolbarCommand::StartPIE:
#ifdef _EDITOR
        {
            extern UEditorEngine GEngine;
            GEngine.StartPIE();
        }
#endif
        break;

    case EToolbarCommand::EndPIE:
#ifdef _EDITOR
        {
            extern UEditorEngine GEngine;
            GEngine.EndPIE();
        }
#endif
        break;

    default:
        break;
    }

    // 명령 처리 완료 후 초기화
    PendingCommand = EToolbarCommand::None;
}

void UMainToolbarWidget::HandleKeyboardShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // Ctrl+N: New Scene
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false))
    {
        PendingCommand = EToolbarCommand::NewScene;
    }

    // Ctrl+S: Save Scene
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        PendingCommand = EToolbarCommand::SaveScene;
    }

    // Ctrl+O: Open/Load Scene
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
    {
        PendingCommand = EToolbarCommand::LoadScene;
    }

#ifdef _EDITOR
    extern UEditorEngine GEngine;

    // F5: Start PIE
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false) && !GEngine.IsPIEActive())
    {
        PendingCommand = EToolbarCommand::StartPIE;
    }

    // Shift+F5: Stop PIE
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5, false) && GEngine.IsPIEActive())
    {
        PendingCommand = EToolbarCommand::EndPIE;
    }
#endif
}

void UMainToolbarWidget::HandleActorSelection(AActor* Actor)
{
    if (!Actor || !GWorld)
        return;

    // Clear previous selection and select this actor
    GWorld->GetSelectionManager()->ClearSelection();
    GWorld->GetSelectionManager()->SelectActor(Actor);
}

void UMainToolbarWidget::RenderBlendEditorButton()
{
    const ImVec2 IconSizeVec(IconSize, IconSize);

    // 버튼 스타일
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

    ImGui::BeginGroup();

    if (IconBlendEditor && IconBlendEditor->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##BlendEditorBtn", (void*)IconBlendEditor->GetShaderResourceView(),
                                IconSizeVec, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), ImVec4(1, 1, 1, 1)))
        {
            // UCameraBlendEditorWindow 찾기 또는 생성
            UUIWindow* BlendEditorWindow = UUIManager::GetInstance().FindUIWindow("Camera Blend Editor");
            if (!BlendEditorWindow)
            {
                // 윈도우가 없으면 생성 후 등록
                BlendEditorWindow = new UCameraBlendEditorWindow();
                BlendEditorWindow->Initialize();
                UUIManager::GetInstance().RegisterUIWindow(BlendEditorWindow);
            }

            // 윈도우 토글 (열려있으면 닫고, 닫혀있으면 열기)
            BlendEditorWindow->ToggleVisibility();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("카메라 블렌드 에디터를 엽니다");
        }
    }

    // 테두리 그리기
    ImVec2 groupMin = ImGui::GetItemRectMin();
    ImVec2 groupMax = ImGui::GetItemRectMax();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRect(
        groupMin,
        groupMax,
        ImGui::GetColorU32(ImVec4(0.4f, 0.45f, 0.5f, 0.8f)),
        4.0f,
        0,
        1.3f
    );

    ImGui::EndGroup();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}
