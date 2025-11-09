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

		// 뷰포트에 호버 중이거나 Gizmo 드래그 중일 때 키보드 입력 캡처 (메인 뷰포트로 전달 방지)
		if (bIsHovered || bIsGizmoDragging)
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

		// Gizmo 모드 전환 키 (뷰포트에 호버 중이거나 Gizmo 드래그 중일 때, 우클릭 드래그 중 아닐 때)
		// NOTE: 우클릭 중에는 Q/W/E가 카메라 이동 키로 사용되므로 Gizmo 모드 전환 비활성화
		if ((bIsHovered || bIsGizmoDragging) && BoneGizmo && !bIsRightMouseDown && GizmoModePtr)
		{
			// 스페이스: 모드 순환 (Translate → Rotate → Scale → Translate)
			if (ImGui::IsKeyPressed(ImGuiKey_Space))
			{
				BoneGizmo->ProcessGizmoModeSwitch();
				*GizmoModePtr = BoneGizmo->GetMode();
			}

			// Q: Translate 모드
			if (ImGui::IsKeyPressed(ImGuiKey_Q))
			{
				*GizmoModePtr = EGizmoMode::Translate;
				BoneGizmo->SetMode(EGizmoMode::Translate);
			}

			// W: Rotate 모드
			if (ImGui::IsKeyPressed(ImGuiKey_W))
			{
				*GizmoModePtr = EGizmoMode::Rotate;
				BoneGizmo->SetMode(EGizmoMode::Rotate);
			}

			// E: Scale 모드
			if (ImGui::IsKeyPressed(ImGuiKey_E))
			{
				*GizmoModePtr = EGizmoMode::Scale;
				BoneGizmo->SetMode(EGizmoMode::Scale);
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

			if (bIsDragging && *SelectedBoneIndexPtr >= 0)
			{
				// 드래그 중 매 프레임 Gizmo Transform을 본에 적용 (실시간 피드백)
				FTransform NewWorldTransform = BoneGizmo->GetActorTransform();
				FBoneTransformCalculator::SetBoneWorldTransform(PreviewComponent, *SelectedBoneIndexPtr, NewWorldTransform);
			}
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Viewport SRV is null!");
	}
}

void UViewportWidget::RenderViewportToolbar()
{
	if (!BoneGizmo || !GizmoModePtr || !GizmoSpacePtr)
		return;

	// Gizmo 모드 버튼
	EGizmoMode CurrentMode = BoneGizmo->GetMode();

	// Translate 버튼
	bool bIsTranslateActive = (CurrentMode == EGizmoMode::Translate);
	if (bIsTranslateActive)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

	if (ImGui::Button("Translate (W)", ImVec2(100, 0)))
	{
		*GizmoModePtr = EGizmoMode::Translate;
		BoneGizmo->SetMode(EGizmoMode::Translate);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Move bone [W]");

	if (bIsTranslateActive)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentMode == EGizmoMode::Rotate);
	if (bIsRotateActive)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

	if (ImGui::Button("Rotate (E)", ImVec2(100, 0)))
	{
		*GizmoModePtr = EGizmoMode::Rotate;
		BoneGizmo->SetMode(EGizmoMode::Rotate);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Rotate bone [E]");

	if (bIsRotateActive)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentMode == EGizmoMode::Scale);
	if (bIsScaleActive)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

	if (ImGui::Button("Scale (R)", ImVec2(100, 0)))
	{
		*GizmoModePtr = EGizmoMode::Scale;
		BoneGizmo->SetMode(EGizmoMode::Scale);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Scale bone [R]");

	if (bIsScaleActive)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// Local/World 공간 전환 버튼
	const char* SpaceLabel = (*GizmoSpacePtr == EGizmoSpace::Local) ? "Local" : "World";
	if (ImGui::Button(SpaceLabel, ImVec2(60, 0)))
	{
		*GizmoSpacePtr = (*GizmoSpacePtr == EGizmoSpace::Local) ? EGizmoSpace::World : EGizmoSpace::Local;
		BoneGizmo->SetSpace(*GizmoSpacePtr);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Local/World space");
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

	// Gizmo를 본 위치로 이동
	BoneGizmo->SetActorLocation(BoneWorldTransform.Translation);

	// Gizmo Space에 따라 회전 설정
	if (*GizmoSpacePtr == EGizmoSpace::Local || *GizmoModePtr == EGizmoMode::Scale)
	{
		// Local 모드 또는 Scale 모드: 본의 회전을 Gizmo에 적용
		BoneGizmo->SetActorRotation(BoneWorldTransform.Rotation);
	}
	else // World 모드
	{
		// World 모드: 월드 축에 정렬 (항등 회전)
		BoneGizmo->SetActorRotation(FQuat::Identity());
	}

	BoneGizmo->SetbRender(true);
}
