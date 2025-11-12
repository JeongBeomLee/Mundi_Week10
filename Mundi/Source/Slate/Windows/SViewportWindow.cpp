#include "pch.h"
#include "SViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include "USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"

#include "CameraComponent.h"
#include "CameraActor.h"
#include "StatsOverlayD2D.h"
#include "ShadowManager.h"
#include "ShadowConfiguration.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow::SViewportWindow()
{
	ViewportType = EViewportType::Perspective;
	bIsActive = false;
	bIsMouseDown = false;
}

SViewportWindow::~SViewportWindow()
{
	if (Viewport)
	{
		delete Viewport;
		Viewport = nullptr;
	}

	if (ViewportClient)
	{
		delete ViewportClient;
		ViewportClient = nullptr;
	}

	IconSelect = nullptr;
	IconMove = nullptr;
	IconRotate = nullptr;
	IconScale = nullptr;
	IconWorldSpace = nullptr;
	IconLocalSpace = nullptr;

	IconCamera = nullptr;
	IconPerspective = nullptr;
	IconTop = nullptr;
	IconBottom = nullptr;
	IconLeft = nullptr;
	IconRight = nullptr;
	IconFront = nullptr;
	IconBack = nullptr;

	IconSpeed = nullptr;
	IconFOV = nullptr;
	IconNearClip = nullptr;
	IconFarClip = nullptr;

	IconViewMode_Lit = nullptr;
	IconViewMode_Unlit = nullptr;
	IconViewMode_Wireframe = nullptr;
	IconViewMode_BufferVis = nullptr;

	IconShowFlag = nullptr;
	IconRevert = nullptr;
	IconStats = nullptr;
	IconHide = nullptr;
	IconBVH = nullptr;
	IconGrid = nullptr;
	IconDecal = nullptr;
	IconStaticMesh = nullptr;
	IconBillboard = nullptr;
	IconFog = nullptr;
	IconCollision = nullptr;
	IconAntiAliasing = nullptr;
	IconTile = nullptr;
	IconShadowMap = nullptr;
	IconSkeletalMesh = nullptr;

	IconSingleToMultiViewport = nullptr;
	IconMultiToSingleViewport = nullptr;
}

bool SViewportWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType)
{
	ViewportType = InViewportType;

	// 이름 설정
	switch (ViewportType)
	{
	case EViewportType::Perspective:		ViewportName = "원근"; break;
	case EViewportType::Orthographic_Front: ViewportName = "정면"; break;
	case EViewportType::Orthographic_Left:  ViewportName = "왼쪽"; break;
	case EViewportType::Orthographic_Top:   ViewportName = "상단"; break;
	case EViewportType::Orthographic_Back:	ViewportName = "후면"; break;
	case EViewportType::Orthographic_Right:  ViewportName = "오른쪽"; break;
	case EViewportType::Orthographic_Bottom:   ViewportName = "하단"; break;
	}

	// FViewport 생성
	Viewport = new FViewport();
	if (!Viewport->Initialize(StartX, StartY, Width, Height, Device))
	{
		delete Viewport;
		Viewport = nullptr;
		return false;
	}

	// FViewportClient 생성
	ViewportClient = new FViewportClient();
	ViewportClient->SetViewportType(ViewportType);
	ViewportClient->SetWorld(World); // 전역 월드 연결 (이미 있다고 가정)

	// 양방향 연결
	Viewport->SetViewportClient(ViewportClient);

	// 툴바 아이콘 로드
	LoadToolbarIcons(Device);

	return true;
}

void SViewportWindow::OnRender()
{
#ifndef STANDALONE_BUILD
	// Slate(UI)만 처리하고 렌더는 FViewport에 위임 (Editor 전용)
	RenderToolbar();
#endif

	if (Viewport)
		Viewport->Render();
}

void SViewportWindow::OnUpdate(float DeltaSeconds)
{
	if (!Viewport)
		return;

	if (!Viewport) return;

	// 툴바 높이만큼 뷰포트 영역 조정

	uint32 NewStartX = static_cast<uint32>(Rect.Left);
	uint32 NewStartY = static_cast<uint32>(Rect.Top);
	uint32 NewWidth = static_cast<uint32>(Rect.Right - Rect.Left);
	uint32 NewHeight = static_cast<uint32>(Rect.Bottom - Rect.Top);

	Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);
	ViewportClient->Tick(DeltaSeconds);
}

void SViewportWindow::OnMouseMove(FVector2D MousePos)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
}

void SViewportWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	// 이 뷰포트를 활성화하고 다른 모든 뷰포트 비활성화
	USlateManager::GetInstance().SetActiveViewport(this);

	// 툴바 영역 아래에서만 마우스 이벤트 처리
	bIsMouseDown = true;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, Button);

}

void SViewportWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	bIsMouseDown = false;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, Button);
}

void SViewportWindow::SetVClientWorld(UWorld* InWorld)
{
	if (ViewportClient && InWorld)
	{
		ViewportClient->SetWorld(InWorld);
	}
}

void SViewportWindow::RenderToolbar()
{
	if (!Viewport) return;

	// 툴바 영역 크기
	float ToolbarHeight = 35.0f;
	ImVec2 ToolbarPosition(Rect.Left, Rect.Top);
	ImVec2 ToolbarSize(Rect.Right - Rect.Left, ToolbarHeight);

	// 툴바 위치 지정
	ImGui::SetNextWindowPos(ToolbarPosition);
	ImGui::SetNextWindowSize(ToolbarSize);

	// 뷰포트별 고유한 윈도우 ID
	char WindowId[64];
	sprintf_s(WindowId, "ViewportToolbar_%p", this);

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin(WindowId, nullptr, WindowFlags))
	{
		// 기즈모 버튼 스타일 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));      // 간격 좁히기
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);            // 모서리 둥글게
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // 배경 투명
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // 호버 배경

		// 기즈모 모드 버튼들 렌더링
		RenderGizmoModeButtons();

		// 구분선
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
		ImGui::SameLine();

		// 기즈모 스페이스 버튼 렌더링
		RenderGizmoSpaceButton();

		// 기즈모 버튼 스타일 복원
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);

		// === 오른쪽 정렬 버튼들 ===
		// Switch, ShowFlag, ViewMode는 오른쪽 고정, Camera는 ViewMode 너비에 따라 왼쪽으로 밀림

		const float SwitchButtonWidth = 33.0f;  // Switch 버튼
		const float ShowFlagButtonWidth = 50.0f;  // ShowFlag 버튼 (대략)
		const float ButtonSpacing = 8.0f;

		// 현재 ViewMode 이름으로 실제 너비 계산
		const char* CurrentViewModeName = "뷰모드";
		if (ViewportClient)
		{
			EViewModeIndex CurrentViewMode = ViewportClient->GetViewModeIndex();
			switch (CurrentViewMode)
			{
			case EViewModeIndex::VMI_Lit:
			case EViewModeIndex::VMI_Lit_Gouraud:
			case EViewModeIndex::VMI_Lit_Lambert:
			case EViewModeIndex::VMI_Lit_Phong:
				CurrentViewModeName = "라이팅 포함";
				break;
			case EViewModeIndex::VMI_Unlit:
				CurrentViewModeName = "언릿";
				break;
			case EViewModeIndex::VMI_Wireframe:
				CurrentViewModeName = "와이어프레임";
				break;
			case EViewModeIndex::VMI_WorldNormal:
				CurrentViewModeName = "월드 노멀";
				break;
			case EViewModeIndex::VMI_SceneDepth:
				CurrentViewModeName = "씬 뎁스";
				break;
			}
		}

		// ViewMode 버튼의 실제 너비 계산
		char viewModeText[64];
		sprintf_s(viewModeText, "%s %s", CurrentViewModeName, "∨");
		ImVec2 viewModeTextSize = ImGui::CalcTextSize(viewModeText);
		const float ViewModeButtonWidth = 17.0f + 4.0f + viewModeTextSize.x + 16.0f;

		// Camera 버튼 너비 계산
		char cameraText[64];
		sprintf_s(cameraText, "%s %s", ViewportName.ToString().c_str(), "∨");
		ImVec2 cameraTextSize = ImGui::CalcTextSize(cameraText);
		const float CameraButtonWidth = 17.0f + 4.0f + cameraTextSize.x + 16.0f;

		// 사용 가능한 전체 너비와 현재 커서 위치
		float AvailableWidth = ImGui::GetContentRegionAvail().x;
		float CursorStartX = ImGui::GetCursorPosX();
		ImVec2 CurrentCursor = ImGui::GetCursorPos();

		// 오른쪽부터 역순으로 위치 계산
		// Switch는 오른쪽 끝
		float SwitchX = CursorStartX + AvailableWidth - SwitchButtonWidth;

		// ShowFlag는 Switch 왼쪽
		float ShowFlagX = SwitchX - ButtonSpacing - ShowFlagButtonWidth;

		// ViewMode는 ShowFlag 왼쪽 (실제 너비 사용)
		float ViewModeX = ShowFlagX - ButtonSpacing - ViewModeButtonWidth;

		// Camera는 ViewMode 왼쪽 (ViewMode 너비에 따라 위치 변동)
		float CameraX = ViewModeX - ButtonSpacing - CameraButtonWidth;

		// 버튼들을 순서대로 그리기 (Y 위치는 동일하게 유지)
		ImGui::SetCursorPos(ImVec2(CameraX, CurrentCursor.y));
		RenderCameraOptionDropdownMenu();

		ImGui::SetCursorPos(ImVec2(ViewModeX, CurrentCursor.y));
		RenderViewModeDropdownMenu();

		ImGui::SetCursorPos(ImVec2(ShowFlagX, CurrentCursor.y));
		RenderShowFlagDropdownMenu();

		ImGui::SetCursorPos(ImVec2(SwitchX, CurrentCursor.y));
		RenderViewportLayoutSwitchButton();
	}
	ImGui::End();
}

void SViewportWindow::LoadToolbarIcons(ID3D11Device* Device)
{
	if (!Device) return;

	// 기즈모 아이콘 텍스처 생성 및 로드
	IconSelect = NewObject<UTexture>();
	IconSelect->Load(GDataDir + "/Icon/Viewport_Toolbar_Select.png", Device);

	IconMove = NewObject<UTexture>();
	IconMove->Load(GDataDir + "/Icon/Viewport_Toolbar_Move.png", Device);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load(GDataDir + "/Icon/Viewport_Toolbar_Rotate.png", Device);

	IconScale = NewObject<UTexture>();
	IconScale->Load(GDataDir + "/Icon/Viewport_Toolbar_Scale.png", Device);

	IconWorldSpace = NewObject<UTexture>();
	IconWorldSpace->Load(GDataDir + "/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconLocalSpace = NewObject<UTexture>();
	IconLocalSpace->Load(GDataDir + "/Icon/Viewport_Toolbar_LocalSpace.png", Device);

	// 뷰포트 모드 아이콘 텍스처 로드
	IconCamera = NewObject<UTexture>();
	IconCamera->Load(GDataDir + "/Icon/Viewport_Mode_Camera.png", Device);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load(GDataDir + "/Icon/Viewport_Mode_Perspective.png", Device);

	IconTop = NewObject<UTexture>();
	IconTop->Load(GDataDir + "/Icon/Viewport_Mode_Top.png", Device);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load(GDataDir + "/Icon/Viewport_Mode_Bottom.png", Device);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load(GDataDir + "/Icon/Viewport_Mode_Left.png", Device);

	IconRight = NewObject<UTexture>();
	IconRight->Load(GDataDir + "/Icon/Viewport_Mode_Right.png", Device);

	IconFront = NewObject<UTexture>();
	IconFront->Load(GDataDir + "/Icon/Viewport_Mode_Front.png", Device);

	IconBack = NewObject<UTexture>();
	IconBack->Load(GDataDir + "/Icon/Viewport_Mode_Back.png", Device);

	// 뷰포트 설정 아이콘 텍스처 로드
	IconSpeed = NewObject<UTexture>();
	IconSpeed->Load(GDataDir + "/Icon/Viewport_Mode_Camera.png", Device);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load(GDataDir + "/Icon/Viewport_Setting_FOV.png", Device);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load(GDataDir + "/Icon/Viewport_Setting_NearClip.png", Device);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load(GDataDir + "/Icon/Viewport_Setting_FarClip.png", Device);

	// 뷰모드 아이콘 텍스처 로드
	IconViewMode_Lit = NewObject<UTexture>();
	IconViewMode_Lit->Load(GDataDir + "/Icon/Viewport_ViewMode_Lit.png", Device);

	IconViewMode_Unlit = NewObject<UTexture>();
	IconViewMode_Unlit->Load(GDataDir + "/Icon/Viewport_ViewMode_Unlit.png", Device);

	IconViewMode_Wireframe = NewObject<UTexture>();
	IconViewMode_Wireframe->Load(GDataDir + "/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconViewMode_BufferVis = NewObject<UTexture>();
	IconViewMode_BufferVis->Load(GDataDir + "/Icon/Viewport_ViewMode_BufferVis.png", Device);

	// ShowFlag 아이콘 텍스처 로드
	IconShowFlag = NewObject<UTexture>();
	IconShowFlag->Load(GDataDir + "/Icon/Viewport_ShowFlag.png", Device);

	IconRevert = NewObject<UTexture>();
	IconRevert->Load(GDataDir + "/Icon/Viewport_Revert.png", Device);

	IconStats = NewObject<UTexture>();
	IconStats->Load(GDataDir + "/Icon/Viewport_Stats.png", Device);

	IconHide = NewObject<UTexture>();
	IconHide->Load(GDataDir + "/Icon/Viewport_Hide.png", Device);

	IconBVH = NewObject<UTexture>();
	IconBVH->Load(GDataDir + "/Icon/Viewport_BVH.png", Device);

	IconGrid = NewObject<UTexture>();
	IconGrid->Load(GDataDir + "/Icon/Viewport_Grid.png", Device);

	IconDecal = NewObject<UTexture>();
	IconDecal->Load(GDataDir + "/Icon/Viewport_Decal.png", Device);

	IconStaticMesh = NewObject<UTexture>();
	IconStaticMesh->Load(GDataDir + "/Icon/Viewport_StaticMesh.png", Device);

	IconSkeletalMesh = NewObject<UTexture>();
	IconSkeletalMesh->Load(GDataDir + "/Icon/Viewport_SkeletalMesh.png", Device);

	IconBillboard = NewObject<UTexture>();
	IconBillboard->Load(GDataDir + "/Icon/Viewport_Billboard.png", Device);

	IconFog = NewObject<UTexture>();
	IconFog->Load(GDataDir + "/Icon/Viewport_Fog.png", Device);

	IconCollision = NewObject<UTexture>();
	IconCollision->Load(GDataDir + "/Icon/Viewport_Collision.png", Device);

	IconAntiAliasing = NewObject<UTexture>();
	IconAntiAliasing->Load(GDataDir + "/Icon/Viewport_AntiAliasing.png", Device);

	IconTile = NewObject<UTexture>();
	IconTile->Load(GDataDir + "/Icon/Viewport_Tile.png", Device);

	IconShadowMap = NewObject<UTexture>();
	IconShadowMap->Load(GDataDir + "/Icon/Viewport_ShadowMap.png", Device);

	// 뷰포트 레이아웃 전환 아이콘 로드
	IconSingleToMultiViewport = NewObject<UTexture>();
	IconSingleToMultiViewport->Load(GDataDir + "/Icon/Viewport_SingleToMultiViewport.png", Device);
	IconMultiToSingleViewport = NewObject<UTexture>();
	IconMultiToSingleViewport->Load(GDataDir + "/Icon/Viewport_MultiToSingleViewport.png", Device);
}

void SViewportWindow::RenderGizmoModeButtons()
{
	ImVec2 switchCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(switchCursorPos.y - 2.0f);

	const ImVec2 IconSize(17, 17);

	// GizmoActor에서 직접 현재 모드 가져오기
	EGizmoMode CurrentGizmoMode = EGizmoMode::Select;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoMode = GizmoActor->GetMode();
		}
	}

	// Select 버튼
	bool bIsSelectActive = (CurrentGizmoMode == EGizmoMode::Select);
	ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconSelect && IconSelect->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}
	else
	{
		if (ImGui::Button("Select", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택합니다. [Q]");
	}
	ImGui::SameLine();

	// Move 버튼
	bool bIsMoveActive = (CurrentGizmoMode == EGizmoMode::Translate);
	ImVec4 MoveTintColor = bIsMoveActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconMove && IconMove->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##MoveBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), MoveTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Move", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 이동시킵니다. [W]");
	}
	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentGizmoMode == EGizmoMode::Rotate);
	ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconRotate && IconRotate->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Rotate", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 회전시킵니다. [E]");
	}
	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentGizmoMode == EGizmoMode::Scale);
	ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconScale && IconScale->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}
	else
	{
		if (ImGui::Button("Scale", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 스케일을 조절합니다. [R]");
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderGizmoSpaceButton()
{
	const ImVec2 IconSize(17, 17);

	// GizmoActor에서 직접 현재 스페이스 가져오기
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::World;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoSpace = GizmoActor->GetSpace();
		}
	}

	// 현재 스페이스에 따라 적절한 아이콘 표시
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	const char* TooltipText = bIsWorldSpace ? "월드 스페이스 좌표 [Tab]" : "로컬 스페이스 좌표 [Tab]";

	// 선택 상태 tint (월드/로컬 모두 동일하게 흰색)
	ImVec4 TintColor = ImVec4(1, 1, 1, 1);

	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##GizmoSpaceBtn", (void*)CurrentIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TintColor))
		{
			// 버튼 클릭 시 스페이스 전환
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}
	else
	{
		// 아이콘이 없는 경우 텍스트 버튼
		const char* ButtonText = bIsWorldSpace ? "World" : "Local";
		if (ImGui::Button(ButtonText, ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", TooltipText);
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderCameraOptionDropdownMenu()
{
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 0.7f);

	const ImVec2 IconSize(17, 17);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", ViewportName.ToString().c_str(), "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float HorizontalPadding = 8.0f;
	const float CameraDropdownWidth = IconSize.x + 4.0f + TextSize.x + HorizontalPadding * 2.0f;

	// 드롭다운 버튼 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 드롭다운 버튼 생성 (카메라 아이콘 + 현재 모드명 + 화살표)
	ImVec2 ButtonSize(CameraDropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewportModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewportModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("카메라 옵션");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 현재 뷰포트 모드에 따라 아이콘 선택
	UTexture* CurrentModeIcon = nullptr;
	switch (ViewportType)
	{
	case EViewportType::Perspective:
		CurrentModeIcon = IconCamera;
		break;
	case EViewportType::Orthographic_Top:
		CurrentModeIcon = IconTop;
		break;
	case EViewportType::Orthographic_Bottom:
		CurrentModeIcon = IconBottom;
		break;
	case EViewportType::Orthographic_Left:
		CurrentModeIcon = IconLeft;
		break;
	case EViewportType::Orthographic_Right:
		CurrentModeIcon = IconRight;
		break;
	case EViewportType::Orthographic_Front:
		CurrentModeIcon = IconFront;
		break;
	case EViewportType::Orthographic_Back:
		CurrentModeIcon = IconBack;
		break;
	default:
		CurrentModeIcon = IconCamera;
		break;
	}

	if (CurrentModeIcon && CurrentModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰포트 모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewportModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션 1: 원근 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "원근");
		ImGui::Separator();

		bool bIsPerspective = (ViewportType == EViewportType::Perspective);
		const char* RadioIcon = bIsPerspective ? "●" : "○";

		// 원근 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
		ImVec2 SelectableSize(180, 20);
		ImVec2 SelectableCursorPos = ImGui::GetCursorPos();

		if (ImGui::Selectable("##Perspective", bIsPerspective, 0, SelectableSize))
		{
			ViewportType = EViewportType::Perspective;
			ViewportName = "원근";
			if (ViewportClient)
			{
				ViewportClient->SetViewportType(ViewportType);
				ViewportClient->SetupCameraMode();
			}
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("뷰포트를 원근 보기로 전환합니다.");
		}

		// Selectable 위에 내용 렌더링
		ImVec2 ContentPos = ImVec2(SelectableCursorPos.x + 4, SelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
		ImGui::SetCursorPos(ContentPos);

		ImGui::Text("%s", RadioIcon);
		ImGui::SameLine(0, 4);

		if (IconPerspective && IconPerspective->GetShaderResourceView())
		{
			ImGui::Image((void*)IconPerspective->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("원근");

		// --- 섹션 2: 직교 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "직교");
		ImGui::Separator();

		// 직교 모드 목록
		struct ViewportModeEntry {
			EViewportType type;
			const char* koreanName;
			UTexture** icon;
			const char* tooltip;
		};

		ViewportModeEntry orthographicModes[] = {
			{ EViewportType::Orthographic_Top, "상단", &IconTop, "뷰포트를 상단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Bottom, "하단", &IconBottom, "뷰포트를 하단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Left, "왼쪽", &IconLeft, "뷰포트를 왼쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Right, "오른쪽", &IconRight, "뷰포트를 오른쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Front, "정면", &IconFront, "뷰포트를 정면 보기로 전환합니다." },
			{ EViewportType::Orthographic_Back, "후면", &IconBack, "뷰포트를 후면 보기로 전환합니다." }
		};

		for (int i = 0; i < 6; i++)
		{
			const auto& mode = orthographicModes[i];
			bool bIsSelected = (ViewportType == mode.type);
			const char* RadioIcon = bIsSelected ? "●" : "○";

			// 직교 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
			char SelectableID[32];
			sprintf_s(SelectableID, "##Ortho%d", i);

			ImVec2 OrthoSelectableCursorPos = ImGui::GetCursorPos();

			if (ImGui::Selectable(SelectableID, bIsSelected, 0, SelectableSize))
			{
				ViewportType = mode.type;
				ViewportName = mode.koreanName;
				if (ViewportClient)
				{
					ViewportClient->SetViewportType(ViewportType);
					ViewportClient->SetupCameraMode();
				}
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", mode.tooltip);
			}

			// Selectable 위에 내용 렌더링
			ImVec2 OrthoContentPos = ImVec2(OrthoSelectableCursorPos.x + 4, OrthoSelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
			ImGui::SetCursorPos(OrthoContentPos);

			ImGui::Text("%s", RadioIcon);
			ImGui::SameLine(0, 4);

			if (*mode.icon && (*mode.icon)->GetShaderResourceView())
			{
				ImGui::Image((void*)(*mode.icon)->GetShaderResourceView(), IconSize);
				ImGui::SameLine(0, 4);
			}

			ImGui::Text("%s", mode.koreanName);
		}

		// --- 섹션 3: 이동 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "이동");
		ImGui::Separator();

		ACameraActor* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
		if (Camera)
		{
			if (IconSpeed && IconSpeed->GetShaderResourceView())
			{
				ImGui::Image((void*)IconSpeed->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("카메라 이동 속도");

			float speed = Camera->GetCameraSpeed();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##CameraSpeed", &speed, 1.0f, 100.0f, "%.1f"))
			{
				Camera->SetCameraSpeed(speed);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("WASD 키로 카메라를 이동할 때의 속도 (1-100)");
			}
		}

		// --- 섹션 4: 뷰 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰");
		ImGui::Separator();

		if (Camera && Camera->GetCameraComponent())
		{
			UCameraComponent* camComp = Camera->GetCameraComponent();

			// FOV
			if (IconFOV && IconFOV->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFOV->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("필드 오브 뷰");

			float fov = camComp->GetFOV();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FOV", &fov, 30.0f, 120.0f, "%.1f"))
			{
				camComp->SetFOV(fov);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라 시야각 (30-120도)\n값이 클수록 넓은 범위가 보입니다");
			}

			// 근평면
			if (IconNearClip && IconNearClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconNearClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("근평면");

			float nearClip = camComp->GetNearClip();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##NearClip", &nearClip, 0.01f, 10.0f, "%.2f"))
			{
				camComp->SetClipPlanes(nearClip, camComp->GetFarClip());
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 가까운 렌더링 거리 (0.01-10)\n이 값보다 가까운 오브젝트는 보이지 않습니다");
			}

			// 원평면
			if (IconFarClip && IconFarClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFarClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("원평면");

			float farClip = camComp->GetFarClip();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FarClip", &farClip, 100.0f, 10000.0f, "%.0f"))
			{
				camComp->SetClipPlanes(camComp->GetNearClip(), farClip);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 먼 렌더링 거리 (100-10000)\n이 값보다 먼 오브젝트는 보이지 않습니다");
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderViewModeDropdownMenu()
{
	if (!ViewportClient) return;

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	const ImVec2 IconSize(17, 17);

	// 현재 뷰모드 이름 및 아이콘 가져오기
	EViewModeIndex CurrentViewMode = ViewportClient->GetViewModeIndex();
	const char* CurrentViewModeName = "뷰모드";
	UTexture* CurrentViewModeIcon = nullptr;

	switch (CurrentViewMode)
	{
	case EViewModeIndex::VMI_Lit:
	case EViewModeIndex::VMI_Lit_Gouraud:
	case EViewModeIndex::VMI_Lit_Lambert:
	case EViewModeIndex::VMI_Lit_Phong:
		CurrentViewModeName = "라이팅 포함";
		CurrentViewModeIcon = IconViewMode_Lit;
		break;
	case EViewModeIndex::VMI_Unlit:
		CurrentViewModeName = "언릿";
		CurrentViewModeIcon = IconViewMode_Unlit;
		break;
	case EViewModeIndex::VMI_Wireframe:
		CurrentViewModeName = "와이어프레임";
		CurrentViewModeIcon = IconViewMode_Wireframe;
		break;
	case EViewModeIndex::VMI_WorldNormal:
		CurrentViewModeName = "월드 노멀";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	case EViewModeIndex::VMI_SceneDepth:
		CurrentViewModeName = "씬 뎁스";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	}

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", CurrentViewModeName, "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 드롭다운 버튼 생성 (아이콘 + 텍스트)
	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("뷰모드 선택");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 현재 뷰모드 아이콘 표시
	if (CurrentViewModeIcon && CurrentViewModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentViewModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션: 뷰모드 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰모드");
		ImGui::Separator();

		// ===== Lit 메뉴 (서브메뉴 포함) =====
		bool bIsLitMode = (CurrentViewMode == EViewModeIndex::VMI_Lit ||
			CurrentViewMode == EViewModeIndex::VMI_Lit_Gouraud ||
			CurrentViewMode == EViewModeIndex::VMI_Lit_Lambert ||
			CurrentViewMode == EViewModeIndex::VMI_Lit_Phong);

		const char* LitRadioIcon = bIsLitMode ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 LitCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 LitSelectableSize(180, IconSize.y);

		// 호버 감지용 투명 Selectable
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		bool bLitHovered = ImGui::Selectable("##LitHoverArea", false, ImGuiSelectableFlags_AllowItemOverlap, LitSelectableSize);
		ImGui::PopStyleColor();

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(LitCursorPos);

		ImGui::Text("%s", LitRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_Lit && IconViewMode_Lit->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Lit->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		if (ImGui::BeginMenu("라이팅포함"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "셰이딩 모델");
			ImGui::Separator();

			// DEFAULT
			bool bIsDefault = (CurrentViewMode == EViewModeIndex::VMI_Lit);
			const char* DefaultIcon = bIsDefault ? "●" : "○";
			char DefaultLabel[32];
			sprintf_s(DefaultLabel, "%s DEFAULT", DefaultIcon);
			if (ImGui::MenuItem(DefaultLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit);
				CurrentLitSubMode = 0;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("기본 셰이딩 모델 (PHONG)\n다른 셰이더 모델과 다르게 셰이더를 덮어쓰지 않습니다.");
			}

			// PHONG
			bool bIsPhong = (CurrentViewMode == EViewModeIndex::VMI_Lit_Phong);
			const char* PhongIcon = bIsPhong ? "●" : "○";
			char PhongLabel[32];
			sprintf_s(PhongLabel, "%s PHONG", PhongIcon);
			if (ImGui::MenuItem(PhongLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong);
				CurrentLitSubMode = 3;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("픽셀 단위 셰이딩 (Per-Pixel)\n스페큘러 하이라이트 포함");
			}

			// GOURAUD
			bool bIsGouraud = (CurrentViewMode == EViewModeIndex::VMI_Lit_Gouraud);
			const char* GouraudIcon = bIsGouraud ? "●" : "○";
			char GouraudLabel[32];
			sprintf_s(GouraudLabel, "%s GOURAUD", GouraudIcon);
			if (ImGui::MenuItem(GouraudLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud);
				CurrentLitSubMode = 1;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("정점 단위 셰이딩 (Per-Vertex)\n부드러운 색상 보간");
			}

			// LAMBERT
			bool bIsLambert = (CurrentViewMode == EViewModeIndex::VMI_Lit_Lambert);
			const char* LambertIcon = bIsLambert ? "●" : "○";
			char LambertLabel[32];
			sprintf_s(LambertLabel, "%s LAMBERT", LambertIcon);
			if (ImGui::MenuItem(LambertLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert);
				CurrentLitSubMode = 2;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Diffuse만 계산하는 간단한 셰이딩\n스페큘러 하이라이트 없음");
			}

			ImGui::EndMenu();
		}

		// ===== Unlit 메뉴 =====
		bool bIsUnlit = (CurrentViewMode == EViewModeIndex::VMI_Unlit);
		const char* UnlitRadioIcon = bIsUnlit ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 UnlitCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 UnlitSelectableSize(180, IconSize.y);

		if (ImGui::Selectable("##UnlitSelectableArea", false, 0, UnlitSelectableSize))
		{
			ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Unlit);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("조명 계산 없이 텍스처와 색상만 표시");
		}

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(UnlitCursorPos);

		ImGui::Text("%s", UnlitRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_Unlit && IconViewMode_Unlit->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Unlit->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("언릿");

		// ===== Wireframe 메뉴 =====
		bool bIsWireframe = (CurrentViewMode == EViewModeIndex::VMI_Wireframe);
		const char* WireframeRadioIcon = bIsWireframe ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 WireframeCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 WireframeSelectableSize(180, IconSize.y);

		if (ImGui::Selectable("##WireframeSelectableArea", false, 0, WireframeSelectableSize))
		{
			ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Wireframe);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("메시의 외곽선(에지)만 표시");
		}

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(WireframeCursorPos);

		ImGui::Text("%s", WireframeRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_Wireframe && IconViewMode_Wireframe->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Wireframe->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("와이어프레임");

		// ===== Buffer Visualization 메뉴 (서브메뉴 포함) =====
		bool bIsBufferVis = (CurrentViewMode == EViewModeIndex::VMI_WorldNormal ||
			CurrentViewMode == EViewModeIndex::VMI_SceneDepth);

		const char* BufferVisRadioIcon = bIsBufferVis ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 BufferVisCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 BufferVisSelectableSize(180, IconSize.y);

		// 호버 감지용 투명 Selectable
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		bool bBufferVisHovered = ImGui::Selectable("##BufferVisHoverArea", false, ImGuiSelectableFlags_AllowItemOverlap, BufferVisSelectableSize);
		ImGui::PopStyleColor();

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(BufferVisCursorPos);

		ImGui::Text("%s", BufferVisRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_BufferVis && IconViewMode_BufferVis->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_BufferVis->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		if (ImGui::BeginMenu("버퍼 시각화"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "버퍼 시각화");
			ImGui::Separator();

			// Scene Depth
			bool bIsSceneDepth = (CurrentViewMode == EViewModeIndex::VMI_SceneDepth);
			const char* SceneDepthIcon = bIsSceneDepth ? "●" : "○";
			char SceneDepthLabel[32];
			sprintf_s(SceneDepthLabel, "%s 씬 뎁스", SceneDepthIcon);
			if (ImGui::MenuItem(SceneDepthLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth);
				CurrentBufferVisSubMode = 0;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("씬의 깊이 정보를 그레이스케일로 표시\n어두울수록 카메라에 가까움");
			}

			// World Normal
			bool bIsWorldNormal = (CurrentViewMode == EViewModeIndex::VMI_WorldNormal);
			const char* WorldNormalIcon = bIsWorldNormal ? "●" : "○";
			char WorldNormalLabel[32];
			sprintf_s(WorldNormalLabel, "%s 월드 노멀", WorldNormalIcon);
			if (ImGui::MenuItem(WorldNormalLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_WorldNormal);
				CurrentBufferVisSubMode = 1;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("월드 공간의 노멀 벡터를 RGB로 표시\nR=X, G=Y, B=Z 축 방향");
			}

			ImGui::EndMenu();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderShowFlagDropdownMenu()
{
	if (!ViewportClient) return;

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	const ImVec2 IconSize(20, 20);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s", "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 드롭다운 버튼 생성 (아이콘 + 텍스트)
	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ShowFlagBtn", ButtonSize))
	{
		ImGui::OpenPopup("ShowFlagPopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Show Flag 설정");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// ShowFlag 아이콘 표시
	if (IconShowFlag && IconShowFlag->GetShaderResourceView())
	{
		ImGui::Image((void*)IconShowFlag->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== ShowFlag 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ShowFlagPopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		URenderSettings& RenderSettings = ViewportClient->GetWorld()->GetRenderSettings();

		// --- 디폴트 사용 (Reset) ---
		ImVec2 ResetCursorPos = ImGui::GetCursorScreenPos();
		if (ImGui::Selectable("##ResetDefault", false, 0, ImVec2(0, IconSize.y)))
		{
			// 기본 설정으로 복원
			RenderSettings.SetShowFlags(EEngineShowFlags::SF_DefaultEnabled);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("모든 Show Flag를 기본 설정으로 복원합니다.");
		}

		// Selectable 위에 아이콘과 텍스트 그리기
		ImGui::SetCursorScreenPos(ResetCursorPos);
		if (IconRevert && IconRevert->GetShaderResourceView())
		{
			ImGui::Image((void*)IconRevert->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text("디폴트 사용");

		ImGui::Separator();

		// --- 뷰포트 통계 (Viewport Stats with Submenu) ---
		// 아이콘
		if (IconStats && IconStats->GetShaderResourceView())
		{
			ImGui::Image((void*)IconStats->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		if (ImGui::BeginMenu("  뷰포트 통계"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰포트 통계");
			ImGui::Separator();

			// 모두 숨김
			ImVec2 HideAllCursorPos = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##HideAllStats", false, 0, ImVec2(0, IconSize.y)))
			{
				UStatsOverlayD2D::Get().SetShowFPS(false);
				UStatsOverlayD2D::Get().SetShowMemory(false);
				UStatsOverlayD2D::Get().SetShowPicking(false);
				UStatsOverlayD2D::Get().SetShowDecal(false);
				UStatsOverlayD2D::Get().SetShowTileCulling(false);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("모든 뷰포트 통계를 숨깁니다.");
			}

			// Selectable 위에 아이콘과 텍스트 그리기
			ImGui::SetCursorScreenPos(HideAllCursorPos);
			if (IconHide && IconHide->GetShaderResourceView())
			{
				ImGui::Image((void*)IconHide->GetShaderResourceView(), IconSize);
				ImGui::SameLine(0, 4);
			}
			ImGui::Text(" 모두 숨김");

			ImGui::Separator();

			// Individual stats checkboxes
			bool bFPS = UStatsOverlayD2D::Get().IsFPSVisible();
			if (ImGui::Checkbox(" FPS", &bFPS))
			{
				UStatsOverlayD2D::Get().ToggleFPS();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("프레임 속도와 프레임 시간을 표시합니다.");
			}

			bool bMemory = UStatsOverlayD2D::Get().IsMemoryVisible();
			if (ImGui::Checkbox(" MEMORY", &bMemory))
			{
				UStatsOverlayD2D::Get().ToggleMemory();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("메모리 사용량과 할당 횟수를 표시합니다.");
			}

			bool bPicking = UStatsOverlayD2D::Get().IsPickingVisible();
			if (ImGui::Checkbox(" PICKING", &bPicking))
			{
				UStatsOverlayD2D::Get().TogglePicking();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("오브젝트 선택 성능 통계를 표시합니다.");
			}

			bool bDecalStats = UStatsOverlayD2D::Get().IsDecalVisible();
			if (ImGui::Checkbox(" DECAL", &bDecalStats))
			{
				UStatsOverlayD2D::Get().ToggleDecal();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("데칼 렌더링 성능 통계를 표시합니다.");
			}

			bool bTileCullingStats = UStatsOverlayD2D::Get().IsTileCullingVisible();
			if (ImGui::Checkbox(" LIGHT", &bTileCullingStats))
			{
				UStatsOverlayD2D::Get().ToggleTileCulling();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일 기반 라이트 컬링 통계를 표시합니다.");
			}

			bool bShadowMapStats = UStatsOverlayD2D::Get().IsShadowMapVisible();
			if (ImGui::Checkbox(" SHADOW", &bShadowMapStats))
			{
				UStatsOverlayD2D::Get().ToggleShadowMap();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("쉐도우 맵 메모리 사용량 통계를 표시합니다.");
			}

			ImGui::EndMenu();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("뷰포트 성능 통계 표시 설정");
		}

		// --- 섹션: 일반 표시 플래그 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "일반 표시 플래그");
		ImGui::Separator();

		// BVH
		bool bBVH = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug);
		if (ImGui::Checkbox("##BVH", &bBVH))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BVHDebug);
		}
		ImGui::SameLine();
		if (IconBVH && IconBVH->GetShaderResourceView())
		{
			ImGui::Image((void*)IconBVH->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" BVH");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("BVH(Bounding Volume Hierarchy) 디버그 시각화를 표시합니다.");
		}

		// Collision
		bool bCollision = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Collision);
		if (ImGui::Checkbox("##Collision", &bCollision))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Collision);
		}
		ImGui::SameLine();
		ImGui::Text(" 충돌 컴포넌트");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("충돌 컴포넌트(Box, Sphere, Capsule)의 디버그 와이어프레임을 표시합니다.");
		}

		// Collision BVH
		bool bCollisionBVH = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_CollisionBVH);
		if (ImGui::Checkbox("##CollisionBVH", &bCollisionBVH))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_CollisionBVH);
		}
		ImGui::SameLine();
		ImGui::Text(" 충돌 BVH");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("충돌 시스템의 BVH(Bounding Volume Hierarchy) 디버그 시각화를 표시합니다.");
		}

		// Grid
		bool bGrid = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
		if (ImGui::Checkbox("##Grid", &bGrid))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
		}
		ImGui::SameLine();
		if (IconGrid && IconGrid->GetShaderResourceView())
		{
			ImGui::Image((void*)IconGrid->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 그리드");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("월드 그리드를 표시합니다.");
		}

		// Decal
		bool bDecal = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Decals);
		if (ImGui::Checkbox("##Decal", &bDecal))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Decals);
		}
		ImGui::SameLine();
		if (IconDecal && IconDecal->GetShaderResourceView())
		{
			ImGui::Image((void*)IconDecal->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 데칼");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("데칼 렌더링을 표시합니다.");
		}

		// Static Mesh
		bool bStaticMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
		if (ImGui::Checkbox("##StaticMesh", &bStaticMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_StaticMeshes);
		}
		ImGui::SameLine();
		if (IconStaticMesh && IconStaticMesh->GetShaderResourceView())
		{
			ImGui::Image((void*)IconStaticMesh->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 스태틱 메시");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("스태틱 메시 렌더링을 표시합니다.");
		}

		// Skeletal Mesh
		bool bSkeletalMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_SkeletalMesh);
		if (ImGui::Checkbox("##SkeletalMesh", &bSkeletalMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_SkeletalMesh);
		}
		ImGui::SameLine();
		if (IconSkeletalMesh && IconSkeletalMesh->GetShaderResourceView())
		{
			ImGui::Image((void*)IconSkeletalMesh->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 스켈레탈 메시");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("스켈레탈 메시 렌더링을 표시합니다.");
		}

		// Billboard
		bool bBillboard = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Billboard);
		if (ImGui::Checkbox("##Billboard", &bBillboard))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Billboard);
		}
		ImGui::SameLine();
		if (IconBillboard && IconBillboard->GetShaderResourceView())
		{
			ImGui::Image((void*)IconBillboard->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 빌보드");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("빌보드 텍스트를 표시합니다.");
		}

		// Fog
		bool bFog = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Fog);
		if (ImGui::Checkbox("##Fog", &bFog))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Fog);
		}
		ImGui::SameLine();
		if (IconFog && IconFog->GetShaderResourceView())
		{
			ImGui::Image((void*)IconFog->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 포그");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("포그 효과를 표시합니다.");
		}

		// Bounds
		bool bBounds = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
		if (ImGui::Checkbox("##Bounds", &bBounds))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BoundingBoxes);
		}
		ImGui::SameLine();
		if (IconCollision && IconCollision->GetShaderResourceView())
		{
			ImGui::Image((void*)IconCollision->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 바운딩 박스");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("바운딩 박스를 표시합니다.");
		}

		// --- 섹션: 그래픽스 기능 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "그래픽스 기능");
		ImGui::Separator();

		// FXAA (Anti-Aliasing)
		bool bFXAA = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);
		if (ImGui::Checkbox("##FXAA", &bFXAA))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_FXAA);
		}
		ImGui::SameLine();
		if (IconAntiAliasing && IconAntiAliasing->GetShaderResourceView())
		{
			ImGui::Image((void*)IconAntiAliasing->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		// 서브메뉴
		if (ImGui::BeginMenu(" 안티 에일리어싱"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "안티 에일리어싱");
			ImGui::Separator();

			// EdgeThresholdMin 슬라이더
			float edgeMin = RenderSettings.GetFXAAEdgeThresholdMin();
			ImGui::Text("엣지 감지 최소값");
			if (ImGui::SliderFloat("##EdgeMin", &edgeMin, 0.01f, 0.2f, "%.4f"))
			{
				RenderSettings.SetFXAAEdgeThresholdMin(edgeMin);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("낮을수록 더 많은 엣지를 감지합니다. (권장: 0.0833)");
			}

			// EdgeThresholdMax 슬라이더
			float edgeMax = RenderSettings.GetFXAAEdgeThresholdMax();
			ImGui::Text("엣지 감지 최대값");
			if (ImGui::SliderFloat("##EdgeMax", &edgeMax, 0.05f, 0.3f, "%.4f"))
			{
				RenderSettings.SetFXAAEdgeThresholdMax(edgeMax);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("높을수록 선명한 엣지만 감지합니다. (권장: 0.166)");
			}

			// QualitySubPix 슬라이더
			float subPix = RenderSettings.GetFXAAQualitySubPix();
			ImGui::Text("서브픽셀 품질");
			if (ImGui::SliderFloat("##SubPix", &subPix, 0.0f, 1.0f, "%.2f"))
			{
				RenderSettings.SetFXAAQualitySubPix(subPix);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("낮을수록 부드러워집니다. (권장: 0.75)");
			}

			// QualityIterations 슬라이더
			int iterations = RenderSettings.GetFXAAQualityIterations();
			ImGui::Text("탐색 반복 횟수");
			if (ImGui::SliderInt("##Iterations", &iterations, 4, 24))
			{
				RenderSettings.SetFXAAQualityIterations(iterations);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("높을수록 품질이 좋지만 성능이 떨어집니다. (권장: 12)");
			}

			ImGui::Separator();

			// 품질 프리셋 버튼
			ImGui::Text("품질 프리셋");

			// 고품질 버튼
			if (ImGui::Button("고품질", ImVec2(60, 0)))
			{
				RenderSettings.SetFXAAEdgeThresholdMin(0.0625f);
				RenderSettings.SetFXAAEdgeThresholdMax(0.125f);
				RenderSettings.SetFXAAQualitySubPix(0.75f);
				RenderSettings.SetFXAAQualityIterations(16);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("최고 품질 설정 (성능 낮음)\nEdgeMin: 0.0625, EdgeMax: 0.125, SubPix: 0.75, Iterations: 16");
			}

			ImGui::SameLine();

			// 중품질 버튼
			if (ImGui::Button("중품질", ImVec2(60, 0)))
			{
				RenderSettings.SetFXAAEdgeThresholdMin(0.0833f);
				RenderSettings.SetFXAAEdgeThresholdMax(0.166f);
				RenderSettings.SetFXAAQualitySubPix(0.75f);
				RenderSettings.SetFXAAQualityIterations(12);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("균형잡힌 설정 (기본값)\nEdgeMin: 0.0833, EdgeMax: 0.166, SubPix: 0.75, Iterations: 12");
			}

			ImGui::SameLine();

			// 저품질 버튼
			if (ImGui::Button("저품질", ImVec2(60, 0)))
			{
				RenderSettings.SetFXAAEdgeThresholdMin(0.125f);
				RenderSettings.SetFXAAEdgeThresholdMax(0.25f);
				RenderSettings.SetFXAAQualitySubPix(1.0f);
				RenderSettings.SetFXAAQualityIterations(8);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("빠른 처리 설정 (성능 높음)\nEdgeMin: 0.125, EdgeMax: 0.25, SubPix: 1.0, Iterations: 8");
			}

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("FXAA 상세 설정");
		}

		ImGui::Separator();

		// === Gamma Correction ===
		ImGui::Text("감마 보정 (Gamma Correction)");

		// 서브메뉴
		if (ImGui::BeginMenu(" 감마 보정 설정"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "감마 보정 설정");
			ImGui::Separator();

			// Gamma 슬라이더
			float gamma = RenderSettings.GetGamma();
			ImGui::Text("감마 값");
			if (ImGui::SliderFloat("##Gamma", &gamma, 1.0f, 3.0f, "%.2f"))
			{
				RenderSettings.SetGamma(gamma);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("감마 보정 값을 조정합니다. (표준: 2.2)\n낮을수록 어두워지고, 높을수록 밝아집니다.");
			}

			ImGui::Separator();

			// Brightness 슬라이더
			float brightness = RenderSettings.GetBrightness();
			ImGui::Text("밝기");
			if (ImGui::SliderFloat("##Brightness", &brightness, 0.5f, 2.0f, "%.2f"))
			{
				RenderSettings.SetBrightness(brightness);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("전체 화면 밝기를 조정합니다. (기본: 1.0)");
			}

			ImGui::Separator();

			// Saturation 슬라이더
			float saturation = RenderSettings.GetSaturation();
			ImGui::Text("채도");
			if (ImGui::SliderFloat("##Saturation", &saturation, 0.0f, 2.0f, "%.2f"))
			{
				RenderSettings.SetSaturation(saturation);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("색상 채도를 조정합니다. (기본: 1.0, 0.0 = 흑백)");
			}

			ImGui::Separator();

			// 프리셋 버튼
			ImGui::Text("프리셋");

			// 표준 sRGB 버튼
			if (ImGui::Button("표준 sRGB", ImVec2(100, 0)))
			{
				RenderSettings.SetGamma(2.2f);
				RenderSettings.SetBrightness(1.0f);
				RenderSettings.SetSaturation(1.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("표준 sRGB 설정 (Gamma: 2.2, Brightness: 1.0, Saturation: 1.0)");
			}

			ImGui::SameLine();

			// 밝게 버튼
			if (ImGui::Button("밝게", ImVec2(100, 0)))
			{
				RenderSettings.SetGamma(2.0f);
				RenderSettings.SetBrightness(1.2f);
				RenderSettings.SetSaturation(1.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("밝은 설정 (Gamma: 2.0, Brightness: 1.2)");
			}

			// 어둡게 버튼
			if (ImGui::Button("어둡게", ImVec2(100, 0)))
			{
				RenderSettings.SetGamma(2.4f);
				RenderSettings.SetBrightness(0.9f);
				RenderSettings.SetSaturation(1.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("어두운 설정 (Gamma: 2.4, Brightness: 0.9)");
			}

			ImGui::SameLine();

			// 흑백 버튼
			if (ImGui::Button("흑백", ImVec2(100, 0)))
			{
				RenderSettings.SetGamma(2.2f);
				RenderSettings.SetBrightness(1.0f);
				RenderSettings.SetSaturation(0.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("흑백 모드 (Saturation: 0.0)");
			}

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("감마 보정, 밝기, 채도 설정");
		}

		ImGui::Separator();

		// Tile-Based Light Culling
		bool bTileCulling = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCulling);
		if (ImGui::Checkbox("##TileCulling", &bTileCulling))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCulling);
		}
		ImGui::SameLine();
		if (IconTile && IconTile->GetShaderResourceView())
		{
			ImGui::Image((void*)IconTile->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		// 서브메뉴
		if (ImGui::BeginMenu(" 타일 기반 라이트 컬링"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "타일 기반 라이트 컬링");
			ImGui::Separator();

			// 디버그 시각화 체크박스
			bool bDebugVis = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCullingDebug);
			if (ImGui::Checkbox(" 디버그 시각화", &bDebugVis))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCullingDebug);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일 컬링 결과를 화면에 색상으로 시각화합니다.");
			}

			ImGui::Separator();

			// 타일 크기 입력
			static int tempTileSize = RenderSettings.GetTileSize();
			ImGui::Text("타일 크기 (픽셀)");
			ImGui::SetNextItemWidth(100);
			ImGui::InputInt("##TileSize", &tempTileSize, 1, 8);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일의 크기를 픽셀 단위로 설정합니다. (일반적: 8, 16, 32)");
			}

			// 적용 버튼
			ImGui::SameLine();
			if (ImGui::Button("적용"))
			{
				if (tempTileSize >= 4 && tempTileSize <= 64)
				{
					RenderSettings.SetTileSize(tempTileSize);
					// TileLightCuller는 매 프레임 생성되므로 다음 프레임에 자동 적용됨
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일 크기를 적용합니다. (4~64 사이 값)");
			}

			// 현재 설정값 표시
			ImGui::Text("현재 타일 크기: %d x %d", RenderSettings.GetTileSize(), RenderSettings.GetTileSize());

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("타일 기반 라이트 컬링 설정");
		}

		// 쉐도우맵 (Shadow Map)
		ImGui::Indent(33.0f);
		if (IconShadowMap && IconShadowMap->GetShaderResourceView())
		{
			ImGui::Image((void*)IconShadowMap->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		// 서브메뉴
		if (ImGui::BeginMenu(" 쉐도우맵"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "쉐도우 맵 해상도");
			ImGui::Separator();

			// 현재 쉐도우 맵 해상도 가져오기
			UWorld* World = ViewportClient->GetWorld();
			uint32 DirResolution = 1024;
			uint32 SpotResolution = 1024;
			uint32 PointResolution = 1024;
			if (World && World->GetShadowManager())
			{
				const FShadowConfiguration& Config = World->GetShadowManager()->GetShadowConfiguration();
				DirResolution = Config.DirectionalLightResolution;
				SpotResolution = Config.SpotLightResolution;
				PointResolution = Config.PointLightResolution;
			}

			// === Directional Light 서브메뉴 ===
			if (ImGui::BeginMenu("디렉셔널 라이트"))
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "해상도");
				ImGui::Separator();

				// 512x512
				bool bDirIs512 = (DirResolution == 512);
				const char* DirIcon512 = bDirIs512 ? "●" : "○";
				char DirLabel512[32];
				sprintf_s(DirLabel512, "%s 512 x 512##Dir", DirIcon512);
				if (ImGui::MenuItem(DirLabel512))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetDirectionalLightResolution(512);
				}

				// 1024x1024
				bool bDirIs1024 = (DirResolution == 1024);
				const char* DirIcon1024 = bDirIs1024 ? "●" : "○";
				char DirLabel1024[32];
				sprintf_s(DirLabel1024, "%s 1024 x 1024##Dir", DirIcon1024);
				if (ImGui::MenuItem(DirLabel1024))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetDirectionalLightResolution(1024);
				}

				// 2048x2048
				bool bDirIs2048 = (DirResolution == 2048);
				const char* DirIcon2048 = bDirIs2048 ? "●" : "○";
				char DirLabel2048[32];
				sprintf_s(DirLabel2048, "%s 2048 x 2048##Dir", DirIcon2048);
				if (ImGui::MenuItem(DirLabel2048))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetDirectionalLightResolution(2048);
				}

				// 4096x4096
				bool bDirIs4096 = (DirResolution == 4096);
				const char* DirIcon4096 = bDirIs4096 ? "●" : "○";
				char DirLabel4096[32];
				sprintf_s(DirLabel4096, "%s 4096 x 4096##Dir", DirIcon4096);
				if (ImGui::MenuItem(DirLabel4096))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetDirectionalLightResolution(4096);
				}

				ImGui::Separator();
				ImGui::Text("현재: %d x %d", DirResolution, DirResolution);

				ImGui::EndMenu();
			}

			// === Spot Light 서브메뉴 ===
			if (ImGui::BeginMenu("스포트 라이트"))
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "해상도");
				ImGui::Separator();

				// 512x512
				bool bSpotIs512 = (SpotResolution == 512);
				const char* SpotIcon512 = bSpotIs512 ? "●" : "○";
				char SpotLabel512[32];
				sprintf_s(SpotLabel512, "%s 512 x 512##Spot", SpotIcon512);
				if (ImGui::MenuItem(SpotLabel512))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetSpotLightResolution(512);
				}

				// 1024x1024
				bool bSpotIs1024 = (SpotResolution == 1024);
				const char* SpotIcon1024 = bSpotIs1024 ? "●" : "○";
				char SpotLabel1024[32];
				sprintf_s(SpotLabel1024, "%s 1024 x 1024##Spot", SpotIcon1024);
				if (ImGui::MenuItem(SpotLabel1024))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetSpotLightResolution(1024);
				}

				// 2048x2048
				bool bSpotIs2048 = (SpotResolution == 2048);
				const char* SpotIcon2048 = bSpotIs2048 ? "●" : "○";
				char SpotLabel2048[32];
				sprintf_s(SpotLabel2048, "%s 2048 x 2048##Spot", SpotIcon2048);
				if (ImGui::MenuItem(SpotLabel2048))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetSpotLightResolution(2048);
				}

				// 4096x4096
				bool bSpotIs4096 = (SpotResolution == 4096);
				const char* SpotIcon4096 = bSpotIs4096 ? "●" : "○";
				char SpotLabel4096[32];
				sprintf_s(SpotLabel4096, "%s 4096 x 4096##Spot", SpotIcon4096);
				if (ImGui::MenuItem(SpotLabel4096))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetSpotLightResolution(4096);
				}

				ImGui::Separator();
				ImGui::Text("현재: %d x %d", SpotResolution, SpotResolution);

				ImGui::EndMenu();
			}

			// === Point Light 서브메뉴 ===
			if (ImGui::BeginMenu("포인트 라이트"))
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "해상도");
				ImGui::Separator();

				// 512x512
				bool bPointIs512 = (PointResolution == 512);
				const char* PointIcon512 = bPointIs512 ? "●" : "○";
				char PointLabel512[32];
				sprintf_s(PointLabel512, "%s 512 x 512##Point", PointIcon512);
				if (ImGui::MenuItem(PointLabel512))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetPointLightResolution(512);
				}

				// 1024x1024
				bool bPointIs1024 = (PointResolution == 1024);
				const char* PointIcon1024 = bPointIs1024 ? "●" : "○";
				char PointLabel1024[32];
				sprintf_s(PointLabel1024, "%s 1024 x 1024##Point", PointIcon1024);
				if (ImGui::MenuItem(PointLabel1024))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetPointLightResolution(1024);
				}

				// 2048x2048
				bool bPointIs2048 = (PointResolution == 2048);
				const char* PointIcon2048 = bPointIs2048 ? "●" : "○";
				char PointLabel2048[32];
				sprintf_s(PointLabel2048, "%s 2048 x 2048##Point", PointIcon2048);
				if (ImGui::MenuItem(PointLabel2048))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetPointLightResolution(2048);
				}

				// 4096x4096
				bool bPointIs4096 = (PointResolution == 4096);
				const char* PointIcon4096 = bPointIs4096 ? "●" : "○";
				char PointLabel4096[32];
				sprintf_s(PointLabel4096, "%s 4096 x 4096##Point", PointIcon4096);
				if (ImGui::MenuItem(PointLabel4096))
				{
					if (World && World->GetShadowManager())
						World->GetShadowManager()->SetPointLightResolution(4096);
				}

				ImGui::Separator();
				ImGui::Text("현재: %d x %d", PointResolution, PointResolution);

				ImGui::EndMenu();
			}

			// === 필터링 서브메뉴 ===
			ImGui::Separator();
			if (ImGui::BeginMenu("필터링"))
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "필터링 타입");
				ImGui::Separator();

				if (World && World->GetShadowManager())
				{
					FShadowConfiguration& Config = const_cast<FShadowConfiguration&>(World->GetShadowManager()->GetShadowConfiguration());

					// 필터링 타입 선택 (라디오 버튼)
					int currentFilterType = static_cast<int>(Config.FilterType);

					if (ImGui::RadioButton("NONE (필터링 없음)", &currentFilterType, static_cast<int>(EShadowFilterType::NONE)))
					{
						World->GetShadowManager()->SetFilterType(EShadowFilterType::NONE);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("하드웨어 기본 비교 샘플링\n필터링 없음");
					}

					if (ImGui::RadioButton("PCF", &currentFilterType, static_cast<int>(EShadowFilterType::PCF)))
					{
						World->GetShadowManager()->SetFilterType(EShadowFilterType::PCF);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Percentage Closer Filtering\n소프트웨어 기반 다중 샘플링");
					}

					if (ImGui::RadioButton("VSM", &currentFilterType, static_cast<int>(EShadowFilterType::VSM)))
					{
						World->GetShadowManager()->SetFilterType(EShadowFilterType::VSM);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Variance Shadow Maps\n분산 기반 부드러운 그림자");
					}

					if (ImGui::RadioButton("ESM", &currentFilterType, static_cast<int>(EShadowFilterType::ESM)))
					{
						World->GetShadowManager()->SetFilterType(EShadowFilterType::ESM);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Exponential Shadow Maps\n지수 기반 부드러운 그림자");
					}

					if (ImGui::RadioButton("EVSM", &currentFilterType, static_cast<int>(EShadowFilterType::EVSM)))
					{
						World->GetShadowManager()->SetFilterType(EShadowFilterType::EVSM);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Exponential Variance Shadow Maps\nVSM + ESM 결합");
					}

					// PCF 설정
					if (Config.FilterType == EShadowFilterType::PCF)
					{
						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "PCF 샘플 수");
						ImGui::Separator();

						int currentPCFSample = static_cast<int>(Config.PCFSampleCount);

						if (ImGui::RadioButton("3x3 (9 샘플)", &currentPCFSample, static_cast<int>(EPCFSampleCount::PCF_3x3)))
						{
							Config.PCFSampleCount = EPCFSampleCount::PCF_3x3;
						}

						if (ImGui::RadioButton("4x4 (16 샘플)", &currentPCFSample, static_cast<int>(EPCFSampleCount::PCF_4x4)))
						{
							Config.PCFSampleCount = EPCFSampleCount::PCF_4x4;
						}

						if (ImGui::RadioButton("5x5 (25 샘플)", &currentPCFSample, static_cast<int>(EPCFSampleCount::PCF_5x5)))
						{
							Config.PCFSampleCount = EPCFSampleCount::PCF_5x5;
						}

						if (ImGui::RadioButton("Custom", &currentPCFSample, static_cast<int>(EPCFSampleCount::Custom)))
						{
							Config.PCFSampleCount = EPCFSampleCount::Custom;
						}

						// Custom 샘플 수 조절
						if (Config.PCFSampleCount == EPCFSampleCount::Custom)
						{
							ImGui::Indent(20.0f);
							ImGui::Text("샘플 수:");
							int customSample = static_cast<int>(Config.PCFCustomSampleCount);
							if (ImGui::SliderInt("##CustomPCFSample", &customSample, 2, 8))
							{
								Config.PCFCustomSampleCount = static_cast<uint32>(customSample);
							}
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip("%dx%d (%d 샘플)", customSample, customSample, customSample * customSample);
							}
							ImGui::Unindent(20.0f);
						}
					}

					// ESM 설정
					if (Config.FilterType == EShadowFilterType::ESM)
					{
						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ESM 파라미터");
						ImGui::Separator();

						ImGui::Text("Exponent");
						ImGui::SliderFloat("##ESMExp", &Config.ESMExponent, 40.0f, 100.0f, "%.1f");
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("ESM 지수 계수 (40.0 ~ 100.0)");
						}
					}

					// EVSM 설정
					if (Config.FilterType == EShadowFilterType::EVSM)
					{
						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "EVSM 파라미터");
						ImGui::Separator();

						ImGui::Text("Positive Exponent");
						ImGui::SliderFloat("##EVSMPosExp", &Config.EVSMPositiveExponent, 20.0f, 80.0f, "%.1f");

						ImGui::Text("Negative Exponent");
						ImGui::SliderFloat("##EVSMNegExp", &Config.EVSMNegativeExponent, 20.0f, 80.0f, "%.1f");

						ImGui::Text("Light Bleeding Reduction");
						ImGui::SliderFloat("##EVSMLightBleed", &Config.EVSMLightBleedingReduction, 0.0f, 1.0f, "%.2f");
					}
				}

				ImGui::EndMenu();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("섀도우 필터링 타입 및 파라미터 설정");
			}

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("쉐도우 맵 해상도 설정");
		}
		ImGui::Unindent(30.0f); // 오프셋 복원

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderViewportLayoutSwitchButton()
{
	ImVec2 switchCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(switchCursorPos.y - 0.7f);

	const ImVec2 IconSize(17, 17);

	// 현재 레이아웃 모드에 따라 아이콘 선택
	// FourSplit일 때 → SingleViewport 아이콘 표시 (클릭하면 단일로 전환)
	// SingleMain일 때 → MultiViewport 아이콘 표시 (클릭하면 멀티로 전환)
	EViewportLayoutMode CurrentMode = SLATE.GetCurrentLayoutMode();
	UTexture* SwitchIcon = (CurrentMode == EViewportLayoutMode::FourSplit) ? IconMultiToSingleViewport : IconSingleToMultiViewport;
	const char* TooltipText = (CurrentMode == EViewportLayoutMode::FourSplit) ? "단일 뷰포트로 전환" : "멀티 뷰포트로 전환";

	// 버튼 너비 계산 (아이콘 + 패딩)
	const float Padding = 8.0f;
	const float ButtonWidth = IconSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 버튼
	ImVec2 ButtonSize(ButtonWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	if (ImGui::Button("##SwitchLayout", ButtonSize))
	{
		SLATE.SwitchPanel(this);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(TooltipText);
	}

	// 버튼 위에 아이콘 렌더링 (중앙 정렬)
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - IconSize.x) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 아이콘 표시
	if (SwitchIcon && SwitchIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)SwitchIcon->GetShaderResourceView(), IconSize);
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);
}
