#include "pch.h"
#include "ViewportWidget.h"
#include "ImGui/imgui.h"
#include "FOffscreenViewport.h"
#include "FOffscreenViewportClient.h"
#include "Gizmo/OffscreenGizmoActor.h"

IMPLEMENT_CLASS(UViewportWidget)

UViewportWidget::UViewportWidget()
	: UWidget("Viewport")
{
}

void UViewportWidget::Initialize()
{
	// 외부에서 주입된 데이터를 사용하므로 초기화 불필요
	// 아이콘 텍스처는 RenderViewportToolbar()에서 지연 로드
}

void UViewportWidget::Update()
{
	// Gizmo를 타겟 Transform에 동기화
	if (BoneGizmo && TargetTransform)
	{
		// 드래그 중이 아닐 때만 Gizmo Transform 업데이트
		// (드래그 중에는 OffscreenGizmoActor가 자체적으로 Transform 관리)
		bool bIsDragging = BoneGizmo->GetbIsDragging();
		if (!bIsDragging)
		{
			// Gizmo를 타겟 위치로 이동
			BoneGizmo->SetActorLocation(TargetTransform->Translation);

			// Gizmo 회전은 Space 모드에 따라 설정
			if (CurrentGizmoSpace == EGizmoSpace::Local || CurrentGizmoMode == EGizmoMode::Scale)
			{
				// Local 모드 또는 Scale 모드: 타겟의 회전을 따라감
				BoneGizmo->SetActorRotation(TargetTransform->Rotation);
			}
			else // World 모드
			{
				// World 모드: 월드 축에 정렬 (Identity 회전)
				BoneGizmo->SetActorRotation(FQuat::Identity());
			}
		}

		BoneGizmo->SetbRender(true);
	}
	else if (BoneGizmo)
	{
		// 타겟이 없으면 Gizmo 숨김
		BoneGizmo->SetbRender(false);
	}
}

void UViewportWidget::RenderWidget()
{
	if (!EmbeddedViewport || !ViewportClient)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Viewport not initialized!");
		return;
	}

	// Toolbar 렌더링 (Viewport 위에 오버레이)
	RenderViewportToolbar();
	ImGui::Spacing();

	// ImGui 영역 크기 가져오기
	ImVec2 availSize = ImGui::GetContentRegionAvail();
	uint32 NewWidth = static_cast<uint32>(availSize.x);
	uint32 NewHeight = static_cast<uint32>(availSize.y);

	// 최소 크기 보장 (너무 작으면 렌더링 문제 발생)
	if (NewWidth < 64) NewWidth = 64;
	if (NewHeight < 64) NewHeight = 64;

	// Viewport 크기 조정 (크기가 변경되었을 때만)
	if (NewWidth != EmbeddedViewport->GetSizeX() || NewHeight != EmbeddedViewport->GetSizeY())
	{
		EmbeddedViewport->Resize(0, 0, NewWidth, NewHeight);
	}

	// 3D 씬 렌더링
	EmbeddedViewport->Render();

	// ImGui::Image로 렌더링 결과 표시
	ID3D11ShaderResourceView* SRV = EmbeddedViewport->GetShaderResourceView();
	if (SRV)
	{
		// 이미지 시작 위치 저장 (마우스 좌표 변환용)
		ImVec2 ImagePos = ImGui::GetCursorScreenPos();

		ImGui::Image((void*)SRV, ImVec2((float)NewWidth, (float)NewHeight));

		// InvisibleButton으로 마우스 이벤트 캡처 (CameraBlendEditor 패턴)
		ImGui::SetCursorScreenPos(ImagePos);
		ImGui::InvisibleButton("ViewportCanvas", ImVec2((float)NewWidth, (float)NewHeight));

		// 뷰포트 호버 여부 확인 (뒤에 다른 위젯이 있어도 감지)
		bool bIsHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		bool bIsRightMouseDown = bIsHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

		// FOffscreenViewportClient를 통해 ImGui 입력 처리
		ImGuiIO& IO = ImGui::GetIO();

		// Gizmo 드래그 상태 확인
		bool bIsGizmoDragging = BoneGizmo ? BoneGizmo->GetbIsDragging() : false;

		// 에디터가 켜져있으면 항상 키보드 입력 캡처 (메인 뷰포트로 전달 방지)
		// 단, 우클릭 카메라 회전 중에는 뷰포트 호버 시에만 캡처
		if (!bIsRightMouseDown)
		{
			IO.WantCaptureKeyboard = true;
		}
		else if (bIsHovered)
		{
			IO.WantCaptureKeyboard = true;
		}

		// 뷰포트 중앙 좌표 계산 (스크린 좌표)
		int ViewportCenterX = static_cast<int>(ImagePos.x + NewWidth * 0.5f);
		int ViewportCenterY = static_cast<int>(ImagePos.y + NewHeight * 0.5f);

		// ViewportClient에는 우클릭만 전달 (카메라 회전은 우클릭만)
		ViewportClient->ProcessImGuiInput(IO, bIsRightMouseDown, ViewportCenterX, ViewportCenterY);

		// 우클릭 카메라 회전 중에만 마우스 커서 숨김 (무한 스크롤)
		// 좌클릭 Gizmo 드래그 시에는 커서를 보여서 사용자가 범위를 인지할 수 있도록 함
		bool bLeftMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		if (bIsRightMouseDown)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		}

		// Gizmo 모드/공간 전환 키 (에디터가 켜져있으면 항상 작동, 우클릭 중 제외)
		if (BoneGizmo && !bIsRightMouseDown)
		{
			// Tab: World/Local 공간 전환
			if (ImGui::IsKeyPressed(ImGuiKey_Tab))
			{
				CurrentGizmoSpace = (CurrentGizmoSpace == EGizmoSpace::Local) ? EGizmoSpace::World : EGizmoSpace::Local;
			}

			// 스페이스: 모드 순환 (Translate → Rotate → Scale)
			if (ImGui::IsKeyPressed(ImGuiKey_Space))
			{
				static const EGizmoMode CycleModes[] = { EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale };
				constexpr int NumModes = sizeof(CycleModes) / sizeof(CycleModes[0]);

				// 현재 모드 인덱스 찾기
				int CurrentIndex = 0;
				for (int i = 0; i < NumModes; ++i)
				{
					if (CycleModes[i] == CurrentGizmoMode)
					{
						CurrentIndex = i;
						break;
					}
				}

				// 다음 모드로 순환
				CurrentIndex = (CurrentIndex + 1) % NumModes;
				CurrentGizmoMode = CycleModes[CurrentIndex];
				BoneGizmo->SetMode(CurrentGizmoMode);
			}

			// 개별 모드 단축키 (Q/W/E/R)
			struct { ImGuiKey Key; EGizmoMode Mode; } ModeBindings[] = {
				{ ImGuiKey_Q, EGizmoMode::Select },
				{ ImGuiKey_W, EGizmoMode::Translate },
				{ ImGuiKey_E, EGizmoMode::Rotate },
				{ ImGuiKey_R, EGizmoMode::Scale }
			};

			for (const auto& Binding : ModeBindings)
			{
				if (ImGui::IsKeyPressed(Binding.Key))
				{
					CurrentGizmoMode = Binding.Mode;
					BoneGizmo->SetMode(Binding.Mode);
					break;
				}
			}
		}

		// 뷰포트 밖에서 마우스를 떼면 드래그 강제 종료
		if (!bIsHovered && BoneGizmo && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (BoneGizmo->GetbIsDragging())
			{
				// 드래그 종료 처리 (false, true로 호출하여 Released 이벤트 전달)
				BoneGizmo->ProcessGizmoInteractionImGui(
					ViewportClient->GetCamera(),
					EmbeddedViewport,
					0, 0, false, true
				);
			}
		}

		// Gizmo 입력 처리 (우클릭 드래그 중이 아닐 때만, 타겟이 있을 때만)
		if (!bIsRightMouseDown && BoneGizmo && bIsHovered && TargetTransform)
		{
			// ImGui 좌표 → Viewport 상대 좌표로 변환
			ImVec2 MousePos = ImGui::GetMousePos();
			float RelativeMouseX = MousePos.x - ImagePos.x;
			float RelativeMouseY = MousePos.y - ImagePos.y;

			// ImGui 입력 상태 가져오기 (bLeftMouseDown은 이미 위에서 선언됨)
			bool bLeftMouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

			// OffscreenGizmoActor의 ImGui 입력 처리
			BoneGizmo->ProcessGizmoInteractionImGui(
				ViewportClient->GetCamera(),
				EmbeddedViewport,
				RelativeMouseX,
				RelativeMouseY,
				bLeftMouseDown,
				bLeftMouseReleased
			);

			// Gizmo 드래그 중 실시간으로 타겟 Transform 업데이트
			bool bIsDragging = BoneGizmo->GetbIsDragging();
			static bool bWasDragging = false;

			// 드래그 시작 시 타겟의 원래 회전 저장
			if (bIsDragging && !bWasDragging && TargetTransform)
			{
				DragStartRotation = TargetTransform->Rotation;
			}

			if (bIsDragging && TargetTransform)
			{
				// Gizmo Transform 가져오기
				FTransform GizmoTransform = BoneGizmo->GetActorTransform();

				// Gizmo 모드에 따라 변경된 컴포넌트만 업데이트
				if (CurrentGizmoMode == EGizmoMode::Translate)
				{
					// Translate 모드: 위치만 변경
					TargetTransform->Translation = GizmoTransform.Translation;
				}
				else if (CurrentGizmoMode == EGizmoMode::Rotate)
				{
					// Rotate 모드: 회전 변경
					if (CurrentGizmoSpace == EGizmoSpace::World)
					{
						// World 모드: Gizmo 회전(델타)을 원래 회전에 적용
						// (Gizmo는 드래그 시작 시 Identity였으므로, 현재 Gizmo 회전 = 델타)
						TargetTransform->Rotation = GizmoTransform.Rotation * DragStartRotation;
					}
					else // Local 모드
					{
						// Local 모드: Gizmo가 최종 회전을 직접 계산했으므로 그대로 사용
						// (Gizmo는 드래그 시작 시 본 회전과 같았고, 로컬 축 기준으로 회전함)
						TargetTransform->Rotation = GizmoTransform.Rotation;
					}
				}
				else if (CurrentGizmoMode == EGizmoMode::Scale)
				{
					// Scale 모드: 스케일만 변경
					TargetTransform->Scale3D = GizmoTransform.Scale3D;
				}
			}

			bWasDragging = bIsDragging;
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Viewport SRV is null!");
	}
}

void UViewportWidget::LoadToolbarIcons()
{
	// 이미 로드되었으면 스킵
	if (IconSelect)
		return;

	// Viewport가 준비되지 않았으면 스킵
	if (!EmbeddedViewport)
		return;

	// pch.h에 extern 선언되어 있음
	ID3D11Device* Device = GEngine.GetRenderer()->GetRHIDevice()->GetDevice();

	if (!Device)
		return;

	// 메인 뷰포트와 동일한 아이콘 로드
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

	// 뷰모드 아이콘 로드
	IconViewMode_Lit = NewObject<UTexture>();
	IconViewMode_Lit->Load(GDataDir + "/Icon/Viewport_ViewMode_Lit.png", Device);

	IconViewMode_Unlit = NewObject<UTexture>();
	IconViewMode_Unlit->Load(GDataDir + "/Icon/Viewport_ViewMode_Unlit.png", Device);

	IconViewMode_Wireframe = NewObject<UTexture>();
	IconViewMode_Wireframe->Load(GDataDir + "/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconViewMode_BufferVis = NewObject<UTexture>();
	IconViewMode_BufferVis->Load(GDataDir + "/Icon/Viewport_ViewMode_BufferVis.png", Device);
}

void UViewportWidget::RenderViewportToolbar()
{
	if (!BoneGizmo)
		return;

	// 툴바 아이콘 지연 로드 (최초 1회만)
	LoadToolbarIcons();
	const ImVec2 IconSize(17, 17);  // 메인 뷰포트와 동일한 크기

	// 메인 뷰포트와 동일한 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));  // 간격 좁히기
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);        // 모서리 둥글게

	// 툴바 배경색과 동일한 버튼 배경색 설정 (호버되지 않았을 때 자연스럽게 녹아듦)
	ImVec4 ToolbarBgColor = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
	ImGui::PushStyleColor(ImGuiCol_Button, ToolbarBgColor);

	// Select 버튼
	bool bIsSelectActive = (CurrentGizmoMode == EGizmoMode::Select);
	ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconSelect && IconSelect->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
		{
			CurrentGizmoMode = EGizmoMode::Select;
			BoneGizmo->SetMode(EGizmoMode::Select);
		}
	}
	else
	{
		if (ImGui::Button("Select", ImVec2(60, 0)))
		{
			CurrentGizmoMode = EGizmoMode::Select;
			BoneGizmo->SetMode(EGizmoMode::Select);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Select mode [Q]");

	ImGui::SameLine();

	// Translate 버튼
	bool bIsTranslateActive = (CurrentGizmoMode == EGizmoMode::Translate);
	ImVec4 TranslateTintColor = bIsTranslateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconMove && IconMove->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##TranslateBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TranslateTintColor))
		{
			CurrentGizmoMode = EGizmoMode::Translate;
			BoneGizmo->SetMode(EGizmoMode::Translate);
		}
	}
	else
	{
		if (ImGui::Button("Move", ImVec2(60, 0)))
		{
			CurrentGizmoMode = EGizmoMode::Translate;
			BoneGizmo->SetMode(EGizmoMode::Translate);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Move bone [W]");

	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentGizmoMode == EGizmoMode::Rotate);
	ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconRotate && IconRotate->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
		{
			CurrentGizmoMode = EGizmoMode::Rotate;
			BoneGizmo->SetMode(EGizmoMode::Rotate);
		}
	}
	else
	{
		if (ImGui::Button("Rotate", ImVec2(60, 0)))
		{
			CurrentGizmoMode = EGizmoMode::Rotate;
			BoneGizmo->SetMode(EGizmoMode::Rotate);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Rotate bone [E]");

	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentGizmoMode == EGizmoMode::Scale);
	ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconScale && IconScale->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
		{
			CurrentGizmoMode = EGizmoMode::Scale;
			BoneGizmo->SetMode(EGizmoMode::Scale);
		}
	}
	else
	{
		if (ImGui::Button("Scale", ImVec2(60, 0)))
		{
			CurrentGizmoMode = EGizmoMode::Scale;
			BoneGizmo->SetMode(EGizmoMode::Scale);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Scale bone [R]");

	ImGui::SameLine();

	// 구분선 (Gizmo 모드와 공간 전환 사이)
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// World/Local 공간 전환 버튼 (아이콘)
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* SpaceIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	ImVec4 SpaceTintColor = ImVec4(1, 1, 1, 1);  // 항상 흰색 (선택 상태 구분 없음)

	if (SpaceIcon && SpaceIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SpaceBtn", (void*)SpaceIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SpaceTintColor))
		{
			CurrentGizmoSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
			BoneGizmo->SetSpace(CurrentGizmoSpace);
		}
	}
	else
	{
		const char* SpaceLabel = bIsWorldSpace ? "World" : "Local";
		if (ImGui::Button(SpaceLabel, ImVec2(60, 0)))
		{
			CurrentGizmoSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
			BoneGizmo->SetSpace(CurrentGizmoSpace);
		}
	}

	if (ImGui::IsItemHovered())
	{
		const char* SpaceTooltip = bIsWorldSpace ? "World space [Tab]" : "Local space [Tab]";
		ImGui::SetTooltip(SpaceTooltip);
	}

	// 스타일 복원
	ImGui::PopStyleColor();  // Button 배경색
	ImGui::PopStyleVar(2);   // ItemSpacing, FrameRounding

	// View Mode 드롭다운 (우측 정렬)
	// 먼저 ViewMode 버튼 너비를 계산해야 함
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

	// 같은 라인에 배치
	ImGui::SameLine();

	// 사용 가능한 전체 너비 계산
	float AvailableWidth = ImGui::GetContentRegionAvail().x;
	float CursorStartX = ImGui::GetCursorPosX();

	// ViewMode는 오른쪽 끝에 배치
	float ViewModeX = CursorStartX + AvailableWidth - ViewModeButtonWidth;

	// ViewMode 드롭다운 렌더링 (X 위치만 우측으로 이동)
	ImGui::SetCursorPosX(ViewModeX);
	RenderViewModeDropdown();
}

void UViewportWidget::RenderViewModeDropdown()
{
	if (!ViewportClient)
		return;

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

	// 드롭다운 버튼 생성 (아이콘 + 텍스트) - 높이를 약간 줄임
	const float ButtonHeight = ImGui::GetFrameHeight() * 0.9f;  // 10% 높이 감소
	ImVec2 ButtonSize(DropdownWidth, ButtonHeight);
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
	float ButtonContentStartY = ButtonCursorPos.y + (ButtonHeight - IconSize.y) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonContentStartY);
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
