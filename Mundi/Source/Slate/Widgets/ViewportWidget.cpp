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
						// World 모드: Gizmo의 회전을 타겟의 원래 회전에 곱함
						TargetTransform->Rotation = GizmoTransform.Rotation * DragStartRotation;
					}
					else
					{
						// Local 모드: Gizmo의 절대 회전 사용
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
}
