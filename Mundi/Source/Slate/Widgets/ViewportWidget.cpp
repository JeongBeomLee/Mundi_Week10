#include "pch.h"
#include "ViewportWidget.h"
#include "ImGui/imgui.h"
#include "FOffscreenViewport.h"
#include "FOffscreenViewportClient.h"
#include "Gizmo/OffscreenGizmoActor.h"
#include "SkeletalMeshComponent.h"
#include "World.h"
#include "Actor.h"
#include "SelectionManager.h"
#include "BoneTransformCalculator.h"

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
	// Gizmo 업데이트 (선택된 본 위치로 이동)
	UpdateGizmoForSelectedBone();
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
		if (BoneGizmo && !bIsRightMouseDown && GizmoModePtr && GizmoSpacePtr)
		{
			// Tab: World/Local 공간 전환
			if (ImGui::IsKeyPressed(ImGuiKey_Tab))
			{
				*GizmoSpacePtr = (*GizmoSpacePtr == EGizmoSpace::Local) ? EGizmoSpace::World : EGizmoSpace::Local;
				// Space 변경 시 UpdateGizmoForSelectedBone()에서 회전을 업데이트함
			}

			// 스페이스: 모드 순환 (Translate → Rotate → Scale, Select 제외)
			if (ImGui::IsKeyPressed(ImGuiKey_Space))
			{
				// Translate(0) → Rotate(1) → Scale(2) 순환
				if (*GizmoModePtr == EGizmoMode::Translate)
				{
					*GizmoModePtr = EGizmoMode::Rotate;
				}
				else if (*GizmoModePtr == EGizmoMode::Rotate)
				{
					*GizmoModePtr = EGizmoMode::Scale;
				}
				else  // Scale 또는 Select인 경우 → Translate로
				{
					*GizmoModePtr = EGizmoMode::Translate;
				}
				BoneGizmo->SetMode(*GizmoModePtr);
			}

			// Q: Select 모드
			if (ImGui::IsKeyPressed(ImGuiKey_Q))
			{
				*GizmoModePtr = EGizmoMode::Select;
				BoneGizmo->SetMode(EGizmoMode::Select);
			}

			// W: Translate 모드
			if (ImGui::IsKeyPressed(ImGuiKey_W))
			{
				*GizmoModePtr = EGizmoMode::Translate;
				BoneGizmo->SetMode(EGizmoMode::Translate);
			}

			// E: Rotate 모드
			if (ImGui::IsKeyPressed(ImGuiKey_E))
			{
				*GizmoModePtr = EGizmoMode::Rotate;
				BoneGizmo->SetMode(EGizmoMode::Rotate);
			}

			// R: Scale 모드
			if (ImGui::IsKeyPressed(ImGuiKey_R))
			{
				*GizmoModePtr = EGizmoMode::Scale;
				BoneGizmo->SetMode(EGizmoMode::Scale);
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

		// Gizmo 입력 처리 (우클릭 드래그 중이 아닐 때만)
		if (!bIsRightMouseDown && BoneGizmo && bIsHovered && PreviewComponent && SelectedBoneIndexPtr)
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

			// Gizmo 드래그 중 실시간으로 본 Transform 업데이트
			bool bIsDragging = BoneGizmo->GetbIsDragging();
			static bool bWasDragging = false;

			// 드래그 시작 시 본의 원래 회전 저장
			if (bIsDragging && !bWasDragging && *SelectedBoneIndexPtr >= 0)
			{
				FTransform CurrentBoneWorldTransform = FBoneTransformCalculator::GetBoneWorldTransform(PreviewComponent, *SelectedBoneIndexPtr);
				DragStartBoneRotation = CurrentBoneWorldTransform.Rotation;
			}

			if (bIsDragging && *SelectedBoneIndexPtr >= 0)
			{
				// 현재 본의 World Transform 가져오기
				FTransform CurrentBoneWorldTransform = FBoneTransformCalculator::GetBoneWorldTransform(PreviewComponent, *SelectedBoneIndexPtr);
				FTransform NewWorldTransform = CurrentBoneWorldTransform;

				// Gizmo 모드에 따라 변경된 컴포넌트만 업데이트
				FTransform GizmoWorldTransform = BoneGizmo->GetActorTransform();

				EGizmoMode CurrentMode = BoneGizmo->GetMode();
				if (CurrentMode == EGizmoMode::Translate)
				{
					// Translate 모드: 위치만 변경
					NewWorldTransform.Translation = GizmoWorldTransform.Translation;
				}
				else if (CurrentMode == EGizmoMode::Rotate)
				{
					// Rotate 모드: 회전 변경
					if (GizmoSpacePtr && *GizmoSpacePtr == EGizmoSpace::World)
					{
						// World 모드: Gizmo의 회전을 본의 원래 회전에 곱함
						NewWorldTransform.Rotation = GizmoWorldTransform.Rotation * DragStartBoneRotation;
					}
					else
					{
						// Local 모드: Gizmo의 절대 회전 사용
						NewWorldTransform.Rotation = GizmoWorldTransform.Rotation;
					}
				}
				else if (CurrentMode == EGizmoMode::Scale)
				{
					// Scale 모드: 스케일만 변경
					NewWorldTransform.Scale3D = GizmoWorldTransform.Scale3D;
				}

				// 변경된 Transform 적용
				FBoneTransformCalculator::SetBoneWorldTransform(PreviewComponent, *SelectedBoneIndexPtr, NewWorldTransform);
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
	if (!BoneGizmo || !GizmoModePtr || !GizmoSpacePtr)
		return;

	// 툴바 아이콘 지연 로드 (최초 1회만)
	LoadToolbarIcons();

	EGizmoMode CurrentMode = BoneGizmo->GetMode();
	const ImVec2 IconSize(17, 17);  // 메인 뷰포트와 동일한 크기

	// 메인 뷰포트와 동일한 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));  // 간격 좁히기
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);        // 모서리 둥글게

	// 툴바 배경색과 동일한 버튼 배경색 설정 (호버되지 않았을 때 자연스럽게 녹아듦)
	ImVec4 ToolbarBgColor = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
	ImGui::PushStyleColor(ImGuiCol_Button, ToolbarBgColor);

	// Select 버튼
	bool bIsSelectActive = (CurrentMode == EGizmoMode::Select);
	ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconSelect && IconSelect->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
		{
			*GizmoModePtr = EGizmoMode::Select;
			BoneGizmo->SetMode(EGizmoMode::Select);
		}
	}
	else
	{
		if (ImGui::Button("Select", ImVec2(60, 0)))
		{
			*GizmoModePtr = EGizmoMode::Select;
			BoneGizmo->SetMode(EGizmoMode::Select);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Select mode [Q]");

	ImGui::SameLine();

	// Translate 버튼
	bool bIsTranslateActive = (CurrentMode == EGizmoMode::Translate);
	ImVec4 TranslateTintColor = bIsTranslateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconMove && IconMove->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##TranslateBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TranslateTintColor))
		{
			*GizmoModePtr = EGizmoMode::Translate;
			BoneGizmo->SetMode(EGizmoMode::Translate);
		}
	}
	else
	{
		if (ImGui::Button("Move", ImVec2(60, 0)))
		{
			*GizmoModePtr = EGizmoMode::Translate;
			BoneGizmo->SetMode(EGizmoMode::Translate);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Move bone [W]");

	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentMode == EGizmoMode::Rotate);
	ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconRotate && IconRotate->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
		{
			*GizmoModePtr = EGizmoMode::Rotate;
			BoneGizmo->SetMode(EGizmoMode::Rotate);
		}
	}
	else
	{
		if (ImGui::Button("Rotate", ImVec2(60, 0)))
		{
			*GizmoModePtr = EGizmoMode::Rotate;
			BoneGizmo->SetMode(EGizmoMode::Rotate);
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Rotate bone [E]");

	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentMode == EGizmoMode::Scale);
	ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconScale && IconScale->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
		{
			*GizmoModePtr = EGizmoMode::Scale;
			BoneGizmo->SetMode(EGizmoMode::Scale);
		}
	}
	else
	{
		if (ImGui::Button("Scale", ImVec2(60, 0)))
		{
			*GizmoModePtr = EGizmoMode::Scale;
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
	bool bIsWorldSpace = (*GizmoSpacePtr == EGizmoSpace::World);
	UTexture* SpaceIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	ImVec4 SpaceTintColor = ImVec4(1, 1, 1, 1);  // 항상 흰색 (선택 상태 구분 없음)

	if (SpaceIcon && SpaceIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SpaceBtn", (void*)SpaceIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SpaceTintColor))
		{
			*GizmoSpacePtr = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
			BoneGizmo->SetSpace(*GizmoSpacePtr);
		}
	}
	else
	{
		const char* SpaceLabel = bIsWorldSpace ? "World" : "Local";
		if (ImGui::Button(SpaceLabel, ImVec2(60, 0)))
		{
			*GizmoSpacePtr = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
			BoneGizmo->SetSpace(*GizmoSpacePtr);
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

void UViewportWidget::UpdateGizmoForSelectedBone()
{
	if (!BoneGizmo || !PreviewComponent || !EditorWorld || !SelectedBoneIndexPtr || !GizmoModePtr || !GizmoSpacePtr)
		return;

	USelectionManager* SelectionManager = EditorWorld->GetSelectionManager();
	if (!SelectionManager)
		return;

	// 본이 선택되지 않았으면 Gizmo만 숨김 (PreviewActor는 선택 상태 유지하여 디버그 본 표시)
	if (*SelectedBoneIndexPtr < 0 || *SelectedBoneIndexPtr >= PreviewComponent->EditableBones.size())
	{
		BoneGizmo->SetbRender(false);
		// PreviewActor는 선택 상태로 유지 (RenderDebugVolume 계속 호출되도록)
		SelectionManager->ClearSelection();
		if (PreviewActor)
		{
			SelectionManager->SelectActor(PreviewActor);
		}
		return;
	}

	// EditorWorld의 SelectionManager에 PreviewComponent 선택
	// (GizmoActor의 UpdateComponentVisibility가 이를 확인하여 Gizmo를 표시함)
	SelectionManager->ClearSelection();
	SelectionManager->SelectComponent(PreviewComponent);

	// 선택된 본의 World Transform 계산
	FTransform BoneWorldTransform = FBoneTransformCalculator::GetBoneWorldTransform(PreviewComponent, *SelectedBoneIndexPtr);

	// Gizmo를 본 위치로 이동 (위치는 항상 본을 따라감)
	BoneGizmo->SetActorLocation(BoneWorldTransform.Translation);

	// Gizmo 회전은 Space 모드에 따라 설정
	if (*GizmoSpacePtr == EGizmoSpace::Local || *GizmoModePtr == EGizmoMode::Scale)
	{
		// Local 모드 또는 Scale 모드: 본의 회전을 따라감
		BoneGizmo->SetActorRotation(BoneWorldTransform.Rotation);
	}
	else // World 모드
	{
		// World 모드: 월드 축에 정렬 (Identity 회전)
		BoneGizmo->SetActorRotation(FQuat::Identity());
	}

	BoneGizmo->SetbRender(true);
}
