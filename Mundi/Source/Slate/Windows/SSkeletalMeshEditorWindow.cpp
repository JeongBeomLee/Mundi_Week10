#include "pch.h"
#include "SSkeletalMeshEditorWindow.h"
#include "ImGui/imgui.h"
#include "Widgets/SkeletalMeshEditorWidget.h"
#include "SkeletalMesh.h"

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

void SSkeletalMeshEditorWindow::SetTargetSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	if (EditorWidget)
	{
		EditorWidget->SetTargetSkeletalMesh(SkeletalMesh);
	}
}

bool SSkeletalMeshEditorWindow::OnWindowClose()
{
	// EditorWidget 정리 (복제본 삭제, Actor 정리)
	if (EditorWidget)
	{
		EditorWidget->Cleanup();
	}

	// 창 닫기 허용
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
				// EditorWidget 정리
				if (EditorWidget)
				{
					EditorWidget->Cleanup();
				}
				// 창 숨기기
				SetWindowState(EUIWindowState::Hidden);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

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

