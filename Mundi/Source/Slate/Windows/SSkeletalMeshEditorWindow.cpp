#include "pch.h"
#include "SSkeletalMeshEditorWindow.h"
#include "ImGui/imgui.h"
#include "Widgets/SkeletalMeshEditorWidget.h"
#include "SkeletalMeshComponent.h"

IMPLEMENT_CLASS(SSkeletalMeshEditorWindow)

SSkeletalMeshEditorWindow::SSkeletalMeshEditorWindow()
	: UUIWindow()
{
	// 윈도우 설정 구성
	FUIWindowConfig Config;
	Config.WindowTitle = "Skeletal Mesh Editor";
	Config.DefaultSize = ImVec2(1200, 800);
	Config.DefaultPosition = ImVec2(100, 100);
	Config.MinSize = ImVec2(800, 600);
	Config.WindowFlags = ImGuiWindowFlags_MenuBar;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;
	Config.InitialState = EUIWindowState::Hidden; // 기본적으로 숨김

	SetConfig(Config);
}

SSkeletalMeshEditorWindow::~SSkeletalMeshEditorWindow()
{
	// EditorWidget은 GUObjectArray가 관리하므로 명시적 삭제 불필요
}

void SSkeletalMeshEditorWindow::Initialize()
{
	// 부모 클래스 초기화
	UUIWindow::Initialize();

	// Widget 생성
	EditorWidget = NewObject<USkeletalMeshEditorWidget>();
	EditorWidget->Initialize();
}

void SSkeletalMeshEditorWindow::SetTargetComponent(USkeletalMeshComponent* Component)
{
	if (EditorWidget)
	{
		EditorWidget->SetTargetComponent(Component);
	}
}

bool SSkeletalMeshEditorWindow::OnWindowClose()
{
	// 저장되지 않은 변경사항이 있으면 플래그 설정하고 닫기 취소
	if (EditorWidget && EditorWidget->HasUnsavedChanges())
	{
		UE_LOG("SkeletalMeshEditor: Has unsaved changes, setting flag from OnWindowClose");
		bRequestCloseWithUnsavedChanges = true;
		return false;  // 닫기 취소 (모달에서 선택할 때까지)
	}

	// 변경사항 없으면 닫기 허용
	UE_LOG("SkeletalMeshEditor: No unsaved changes, closing from OnWindowClose");
	return true;
}

void SSkeletalMeshEditorWindow::RenderContent()
{
	// UUIWindow가 ImGui::Begin/End를 처리하므로 내용만 렌더링

	// 메뉴바
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Close"))
			{
				// 저장되지 않은 변경사항이 있으면 플래그 설정
				if (EditorWidget && EditorWidget->HasUnsavedChanges())
				{
					UE_LOG("SkeletalMeshEditor: Has unsaved changes, setting flag from menu");
					bRequestCloseWithUnsavedChanges = true;
				}
				else
				{
					UE_LOG("SkeletalMeshEditor: No unsaved changes, closing window");
					SetWindowState(EUIWindowState::Hidden);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// 닫기 요청 플래그가 설정되면 팝업 열기
	if (bRequestCloseWithUnsavedChanges)
	{
		ImGui::OpenPopup("Unsaved Changes##SkeletalMeshEditor");
		bRequestCloseWithUnsavedChanges = false;  // 플래그 리셋
		UE_LOG("SkeletalMeshEditor: Opening popup from RenderContent");
	}

	// 저장 안하고 닫기 확인 모달
	RenderUnsavedChangesModal();

	// Widget 렌더링
	if (EditorWidget)
	{
		EditorWidget->RenderWidget();
	}

	// Widget 업데이트는 UUIWindow::RenderWindow()에서 자동으로 Update() 호출됨
	if (EditorWidget)
	{
		EditorWidget->Update();
	}
}

void SSkeletalMeshEditorWindow::RenderUnsavedChangesModal()
{
	if (ImGui::BeginPopupModal("Unsaved Changes##SkeletalMeshEditor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You have unsaved changes.");
		ImGui::Text("What do you want to do?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Apply & Close 버튼
		if (ImGui::Button("Apply & Close", ImVec2(120, 0)))
		{
			if (EditorWidget)
			{
				EditorWidget->ApplyChanges();
			}
			ImGui::CloseCurrentPopup();
			SetWindowState(EUIWindowState::Hidden);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Save changes and close editor");
		}

		ImGui::SameLine();

		// Discard 버튼
		if (ImGui::Button("Discard", ImVec2(120, 0)))
		{
			if (EditorWidget)
			{
				EditorWidget->CancelChanges();
			}
			ImGui::CloseCurrentPopup();
			SetWindowState(EUIWindowState::Hidden);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Discard changes and close editor");
		}

		ImGui::SameLine();

		// Cancel 버튼
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Keep editor open");
		}

		ImGui::EndPopup();
	}
}
