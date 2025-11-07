#include "pch.h"
#include "WorldDetailsWidget.h"
#include "ImGui/imgui.h"
#include "World.h"
#include "GameModeBase.h"
#include "Pawn.h"
#include "Character.h"
#include "PlayerController.h"
#include "Object.h"

IMPLEMENT_CLASS(UWorldDetailsWidget)

UWorldDetailsWidget::UWorldDetailsWidget()
{
}

void UWorldDetailsWidget::Initialize()
{
	UE_LOG("WorldDetailsWidget: Initialized");
}

void UWorldDetailsWidget::Update()
{
	// 매 프레임 World 상태 체크
}

void UWorldDetailsWidget::RenderWidget()
{
	if (!World)
	{
		ImGui::TextDisabled("No World selected");
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

	// === World Settings Header ===
	if (ImGui::CollapsingHeader("World Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(10.0f);

		// PIE 모드인지 확인
		if (World->bPie)
		{
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "PIE Mode - Settings are read-only");
			ImGui::TextWrapped("Edit settings in Editor mode before starting PIE");
			ImGui::Separator();

			// PIE에서는 실제 GameMode 정보 표시
			AGameModeBase* GameMode = World->GetGameMode();
			if (GameMode)
			{
				ImGui::Text("Active GameMode: %s", GameMode->GetClass()->Name);
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Editor Mode - Configure PIE Settings");
			ImGui::Separator();
		}

		RenderGameModeSettings();
		ImGui::Unindent(10.0f);
	}

	ImGui::PopStyleVar();
}

void UWorldDetailsWidget::SetWorld(UWorld* InWorld)
{
	World = InWorld;

	if (World)
	{
		UpdateAvailableClasses();
		//UE_LOG("WorldDetailsWidget: World set to %s", World->GetName().ToString());
	}
}

void UWorldDetailsWidget::RenderGameModeSettings()
{
	if (!World)
		return;

	// PIE 모드에서는 읽기 전용
	bool bReadOnly = World->bPie;

	// GameModeClass 선택
	RenderGameModeClassSelector();

	ImGui::Spacing();

	// DefaultPawnClass 선택
	RenderPawnClassSelector();

	ImGui::Spacing();

	// PlayerControllerClass 선택
	RenderControllerClassSelector();

	ImGui::Spacing();
	ImGui::Separator();

	// Player Spawn Location
	RenderSpawnLocationEditor();
}

void UWorldDetailsWidget::RenderGameModeClassSelector()
{
	if (!World)
		return;

	ImGui::Text("GameMode Class:");

	// 현재 World 설정에서 클래스 가져오기
	UClass* CurrentGameModeClass = World->GetGameModeClass();

	if (CurrentGameModeClass)
	{
		// 현재 클래스가 목록에 있는지 찾기
		for (int i = 0; i < AvailableGameModeClasses.size(); ++i)
		{
			if (AvailableGameModeClasses[i] == CurrentGameModeClass)
			{
				SelectedGameModeClassIndex = i;
				break;
			}
		}
	}

	// Combo Box
	if (!GameModeClassNames.empty())
	{
		// PIE 모드에서는 비활성화
		if (World->bPie)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Combo("##GameModeClass", &SelectedGameModeClassIndex,
			GameModeClassNames.data(), static_cast<int>(GameModeClassNames.size())))
		{
			// 선택 변경됨 - World 설정에 반영
			if (SelectedGameModeClassIndex >= 0 && SelectedGameModeClassIndex < AvailableGameModeClasses.size())
			{
				World->SetGameModeClass(AvailableGameModeClasses[SelectedGameModeClassIndex]);
				UE_LOG("WorldDetailsWidget: GameMode class set to %s",
					AvailableGameModeClasses[SelectedGameModeClassIndex]->Name);
			}
		}

		if (World->bPie)
		{
			ImGui::EndDisabled();
		}
	}
	else
	{
		ImGui::TextDisabled("No available GameMode classes");
	}

	// 현재 선택된 클래스 설명
	if (CurrentGameModeClass && CurrentGameModeClass->Description)
	{
		ImGui::TextWrapped("%s", CurrentGameModeClass->Description);
	}
}

void UWorldDetailsWidget::RenderPawnClassSelector()
{
	if (!World)
		return;

	ImGui::Text("Default Pawn Class:");

	// 현재 World 설정에서 클래스 가져오기
	UClass* CurrentPawnClass = World->GetDefaultPawnClass();

	if (CurrentPawnClass)
	{
		// 현재 클래스가 목록에 있는지 찾기
		for (int i = 0; i < AvailablePawnClasses.size(); ++i)
		{
			if (AvailablePawnClasses[i] == CurrentPawnClass)
			{
				SelectedPawnClassIndex = i;
				break;
			}
		}
	}

	// Combo Box
	if (!PawnClassNames.empty())
	{
		// PIE 모드에서는 비활성화
		if (World->bPie)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Combo("##PawnClass", &SelectedPawnClassIndex,
			PawnClassNames.data(), static_cast<int>(PawnClassNames.size())))
		{
			// 선택 변경됨 - World 설정에 반영
			if (SelectedPawnClassIndex >= 0 && SelectedPawnClassIndex < AvailablePawnClasses.size())
			{
				World->SetDefaultPawnClass(AvailablePawnClasses[SelectedPawnClassIndex]);
				UE_LOG("WorldDetailsWidget: Default Pawn class set to %s",
					AvailablePawnClasses[SelectedPawnClassIndex]->Name);
			}
		}

		if (World->bPie)
		{
			ImGui::EndDisabled();
		}
	}
	else
	{
		ImGui::TextDisabled("No available Pawn classes");
	}

	// 현재 선택된 클래스 설명
	if (CurrentPawnClass && CurrentPawnClass->Description)
	{
		ImGui::TextWrapped("%s", CurrentPawnClass->Description);
	}
}

void UWorldDetailsWidget::RenderControllerClassSelector()
{
	if (!World)
		return;

	ImGui::Text("Player Controller Class:");

	// 현재 World 설정에서 클래스 가져오기
	UClass* CurrentControllerClass = World->GetPlayerControllerClass();

	if (CurrentControllerClass)
	{
		// 현재 클래스가 목록에 있는지 찾기
		for (int i = 0; i < AvailableControllerClasses.size(); ++i)
		{
			if (AvailableControllerClasses[i] == CurrentControllerClass)
			{
				SelectedControllerClassIndex = i;
				break;
			}
		}
	}

	// Combo Box
	if (!ControllerClassNames.empty())
	{
		// PIE 모드에서는 비활성화
		if (World->bPie)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Combo("##ControllerClass", &SelectedControllerClassIndex,
			ControllerClassNames.data(), static_cast<int>(ControllerClassNames.size())))
		{
			// 선택 변경됨 - World 설정에 반영
			if (SelectedControllerClassIndex >= 0 && SelectedControllerClassIndex < AvailableControllerClasses.size())
			{
				World->SetPlayerControllerClass(AvailableControllerClasses[SelectedControllerClassIndex]);
				UE_LOG("WorldDetailsWidget: Player Controller class set to %s",
					AvailableControllerClasses[SelectedControllerClassIndex]->Name);
			}
		}

		if (World->bPie)
		{
			ImGui::EndDisabled();
		}
	}
	else
	{
		ImGui::TextDisabled("No available Controller classes");
	}
}

void UWorldDetailsWidget::RenderSpawnLocationEditor()
{
	if (!World)
		return;

	ImGui::Text("Player Spawn Location:");

	// 현재 World 설정에서 SpawnLocation 가져오기
	FVector CurrentSpawnLocation = World->GetPlayerSpawnLocation();
	EditSpawnLocation = CurrentSpawnLocation;

	bool bChanged = false;

	// PIE 모드에서는 비활성화
	if (World->bPie)
	{
		ImGui::BeginDisabled();
	}

	// X
	ImGui::Text("X:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::DragFloat("##SpawnX", &EditSpawnLocation.X, 1.0f))
	{
		bChanged = true;
	}

	// Y
	ImGui::Text("Y:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::DragFloat("##SpawnY", &EditSpawnLocation.Y, 1.0f))
	{
		bChanged = true;
	}

	// Z
	ImGui::Text("Z:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::DragFloat("##SpawnZ", &EditSpawnLocation.Z, 1.0f))
	{
		bChanged = true;
	}

	if (World->bPie)
	{
		ImGui::EndDisabled();
	}

	// 변경되면 World 설정에 반영
	if (bChanged)
	{
		World->SetPlayerSpawnLocation(EditSpawnLocation);
		UE_LOG("WorldDetailsWidget: Player spawn location set to (%.1f, %.1f, %.1f)",
			EditSpawnLocation.X, EditSpawnLocation.Y, EditSpawnLocation.Z);
	}
}

void UWorldDetailsWidget::UpdateAvailableClasses()
{
	// Clear previous data
	AvailableGameModeClasses.clear();
	AvailablePawnClasses.clear();
	AvailableControllerClasses.clear();
	GameModeClassNames.clear();
	PawnClassNames.clear();
	ControllerClassNames.clear();

	// 모든 클래스 찾기
	TArray<UClass*> AllClasses = UClass::GetAllClasses();

	for (UClass* Class : AllClasses)
	{
		if (!Class)
			continue;

		// GameModeBase 및 자식 클래스 찾기
		if (Class->IsChildOf(AGameModeBase::StaticClass()))
		{
			AvailableGameModeClasses.push_back(Class);
			GameModeClassNames.push_back(Class->Name);
		}

		// Pawn 및 자식 클래스 찾기
		if (Class->IsChildOf(APawn::StaticClass()))
		{
			AvailablePawnClasses.push_back(Class);
			PawnClassNames.push_back(Class->Name);
		}

		// PlayerController 및 자식 클래스 찾기
		if (Class->IsChildOf(APlayerController::StaticClass()))
		{
			AvailableControllerClasses.push_back(Class);
			ControllerClassNames.push_back(Class->Name);
		}
	}

	UE_LOG("WorldDetailsWidget: Found %d GameMode classes, %d Pawn classes, %d Controller classes",
		AvailableGameModeClasses.size(), AvailablePawnClasses.size(), AvailableControllerClasses.size());
}
