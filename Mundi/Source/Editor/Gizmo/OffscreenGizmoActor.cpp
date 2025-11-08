#include "pch.h"
#include "OffscreenGizmoActor.h"
#include "GizmoArrowComponent.h"
#include "GizmoScaleComponent.h"
#include "GizmoRotateComponent.h"
#include "CameraActor.h"
#include "SelectionManager.h"
#include "FViewport.h"
#include "Picking.h"

IMPLEMENT_CLASS(AOffscreenGizmoActor)

AOffscreenGizmoActor::AOffscreenGizmoActor()
	: AGizmoActor()
{
	Name = "Offscreen Gizmo Actor";
}

void AOffscreenGizmoActor::PostActorCreated()
{
	Super::PostActorCreated();

	// World가 설정된 후 SelectionManager 초기화
	if (GetWorld())
	{
		SelectionManager = GetWorld()->GetSelectionManager();
		UE_LOG("OffscreenGizmoActor::PostActorCreated - SelectionManager initialized: %p", SelectionManager);
	}
	else
	{
		UE_LOG("OffscreenGizmoActor::PostActorCreated - Warning: GetWorld() returned null");
	}

	// 초기 컴포넌트 가시성 설정 (모두 비활성화)
	if (ArrowX) ArrowX->SetActive(false);
	if (ArrowY) ArrowY->SetActive(false);
	if (ArrowZ) ArrowZ->SetActive(false);
	if (RotateX) RotateX->SetActive(false);
	if (RotateY) RotateY->SetActive(false);
	if (RotateZ) RotateZ->SetActive(false);
	if (ScaleX) ScaleX->SetActive(false);
	if (ScaleY) ScaleY->SetActive(false);
	if (ScaleZ) ScaleZ->SetActive(false);
}

void AOffscreenGizmoActor::Tick(float DeltaSeconds)
{
	// 디버깅: SelectionManager 상태 확인
	static int TickCount = 0;
	if (TickCount++ % 60 == 0)  // 1초마다 로그
	{
		USceneComponent* SelectedComp = SelectionManager ? SelectionManager->GetSelectedComponent() : nullptr;
		UE_LOG("OffscreenGizmo::Tick - SelectionMgr=%p, SelectedComp=%p, Mode=%d, bRender=%d",
			SelectionManager, SelectedComp, (int)CurrentMode, GetbRender());
	}

	// 컴포넌트 가시성 업데이트만 수행 (기즈모 위치는 외부에서 UpdateGizmoForSelectedBone()으로 설정)
	UpdateComponentVisibility();
}

void AOffscreenGizmoActor::ProcessGizmoInteractionImGui(
	ACameraActor* Camera,
	FViewport* Viewport,
	float MousePositionX,
	float MousePositionY,
	bool bLeftMouseDown,
	bool bLeftMouseReleased
)
{
	if (!SelectionManager)
	{
		UE_LOG("OffscreenGizmo: SelectionManager is null!");
		return;
	}
	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!SelectedComponent || !Camera)
	{
		static int LogCounter = 0;
		if (LogCounter++ % 60 == 0)
		{
			UE_LOG("OffscreenGizmo: SelectedComponent=%p, Camera=%p", SelectedComponent, Camera);
		}
		return;
	}

	// 기즈모 드래그 처리
	ProcessGizmoDraggingImGui(Camera, Viewport, MousePositionX, MousePositionY, bLeftMouseDown, bLeftMouseReleased);

	// 호버링 처리 (드래그 중이 아닐 때만)
	if (!bIsDragging)
	{
		ProcessGizmoHovering(Camera, Viewport, MousePositionX, MousePositionY);

		// 디버깅: GizmoAxis 값 로그
		static uint32 LastGizmoAxis = 0;
		if (GizmoAxis != LastGizmoAxis)
		{
			UE_LOG("OffscreenGizmo: GizmoAxis changed from %d to %d (Hovering=%d)",
				LastGizmoAxis, GizmoAxis, bIsHovering);
			LastGizmoAxis = GizmoAxis;
		}
	}
}

void AOffscreenGizmoActor::ProcessGizmoDraggingImGui(
	ACameraActor* Camera,
	FViewport* Viewport,
	float MousePositionX,
	float MousePositionY,
	bool bLeftMouseDown,
	bool bLeftMouseReleased
)
{
	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!SelectedComponent || !Camera) return;

	FVector2D CurrentMousePosition(MousePositionX, MousePositionY);

	// --- 1. Begin Drag (드래그 시작) ---
	if (bLeftMouseDown && !bIsDragging && GizmoAxis > 0)
	{
		bIsDragging = true;
		DraggingAxis = GizmoAxis;
		DragCamera = Camera;

		// 드래그 시작 상태 저장 (Gizmo Actor 자신의 Transform)
		DragStartLocation = GetActorLocation();
		DragStartRotation = GetActorRotation();
		DragStartScale = GetActorScale();
		DragStartPosition = CurrentMousePosition;

		// 호버링 시점의 3D 충돌 지점을 드래그 시작 지점으로 래치
		DragImpactPoint = HoverImpactPoint;

		// (회전용) 드래그 시작 시점의 2D 드래그 벡터 계산
		if (CurrentMode == EGizmoMode::Rotate)
		{
			FVector WorldAxis(0.f);
			FVector LocalAxisVector(0.f);
			switch (DraggingAxis)
			{
			case 1: LocalAxisVector = FVector(1, 0, 0); break;
			case 2: LocalAxisVector = FVector(0, 1, 0); break;
			case 3: LocalAxisVector = FVector(0, 0, 1); break;
			}

			if (CurrentSpace == EGizmoSpace::World)
			{
				WorldAxis = LocalAxisVector;
			}
			else // Local
			{
				WorldAxis = DragStartRotation.RotateVector(LocalAxisVector);
			}

			// 3D 접선 계산
			FVector ClickVector = DragImpactPoint - DragStartLocation;
			FVector Tangent3D = FVector::Cross(WorldAxis, ClickVector);

			// 2D 스크린에 투영
			float RightDot = FVector::Dot(Tangent3D, DragCamera->GetRight());
			float UpDot = FVector::Dot(Tangent3D, DragCamera->GetUp());

			DragScreenVector = FVector2D(RightDot, -UpDot);
			DragScreenVector = DragScreenVector.GetNormalized();
		}
	}

	// --- 2. Continue Drag (드래그 지속) ---
	if (bLeftMouseDown && bIsDragging)
	{
		// 드래그 시작점으로부터의 '총 변위(Total Offset)' 계산
		FVector2D MouseOffset = CurrentMousePosition - DragStartPosition;

		// OnDrag 함수에 고정된 축(DraggingAxis)과 총 변위(MouseOffset)를 전달
		OnDrag(SelectedComponent, DraggingAxis, MouseOffset.X, MouseOffset.Y, Camera, Viewport);
	}

	// --- 3. End Drag (드래그 종료) ---
	if (bLeftMouseReleased)
	{
		if (bIsDragging)
		{
			bIsDragging = false;
			DraggingAxis = 0; // 고정된 축 해제
			DragCamera = nullptr;
			GizmoAxis = 0; // 하이라이트 해제
			SetSpaceWorldMatrix(CurrentSpace, this->GetRootComponent());  // Gizmo Actor 자신 기준
		}
	}
}

void AOffscreenGizmoActor::OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport)
{
	// DraggingAxis == 0 이면 드래그 중이 아니므로 반환
	if (!Camera || DraggingAxis == 0)
		return;

	// NOTE: Target은 무시하고 Gizmo Actor 자신(this)의 Transform만 업데이트
	// 본의 Transform은 SkeletalMeshEditorWidget::RenderViewport()에서 별도로 동기화됨

	// MouseDeltaX/Y는 드래그 시작점으로부터의 '총 변위(Total Offset)'
	FVector2D MouseOffset(MouseDeltaX, MouseDeltaY);

	// ────────────── 모드별 처리 ──────────────
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
	case EGizmoMode::Scale:
	{
		// --- 드래그 시작 시점의 축 계산 ---
		FVector Axis{};
		if (CurrentSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
		{
			switch (DraggingAxis)
			{
			case 1: Axis = DragStartRotation.RotateVector(FVector(1, 0, 0)); break;
			case 2: Axis = DragStartRotation.RotateVector(FVector(0, 1, 0)); break;
			case 3: Axis = DragStartRotation.RotateVector(FVector(0, 0, 1)); break;
			}
		}
		else if (CurrentSpace == EGizmoSpace::World)
		{
			switch (DraggingAxis)
			{
			case 1: Axis = FVector(1, 0, 0); break;
			case 2: Axis = FVector(0, 1, 0); break;
			case 3: Axis = FVector(0, 0, 1); break;
			}
		}

		// ────────────── 픽셀 당 월드 이동량 계산 ──────────────
		FVector2D ScreenAxis = AGizmoActor::GetStableAxisDirection(Axis, Camera);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : 600.0f;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : 800.0f;
		float aspect = w / h;
		FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
		bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
		float worldPerPixel = 0.0f;
		if (bOrtho)
		{
			float halfH = 1.0f / Proj.M[1][1];
			worldPerPixel = (2.0f * halfH) / h;
		}
		else
		{
			float yScale = Proj.M[1][1];
			FVector camPos = Camera->GetActorLocation();
			FVector gizPos = GetActorLocation();
			FVector camF = Camera->GetForward();
			float z = FVector::Dot(gizPos - camPos, camF);
			if (z < 1.0f) z = 1.0f;
			worldPerPixel = (2.0f * z) / (h * yScale);
		}

		float ProjectedPx = (MouseOffset.X * ScreenAxis.X + MouseOffset.Y * ScreenAxis.Y);
		float TotalMovement = ProjectedPx * worldPerPixel;

		if (CurrentMode == EGizmoMode::Translate)
		{
			// Gizmo Actor 자신을 이동
			SetActorLocation(DragStartLocation + Axis * TotalMovement);
		}
		else // Scale
		{
			FVector NewScale = DragStartScale;
			switch (DraggingAxis)
			{
			case 1: NewScale.X += TotalMovement; break;
			case 2: NewScale.Y += TotalMovement; break;
			case 3: NewScale.Z += TotalMovement; break;
			}
			// Gizmo Actor 자신의 스케일 변경
			SetActorScale(NewScale);
		}
		break;
	}
	case EGizmoMode::Rotate:
	{
		float RotationSpeed = 0.005f;

		// ProcessGizmoDragging에서 미리 계산해둔 2D 스크린 드래그 벡터 사용
		float ProjectedAmount = (MouseOffset.X * DragScreenVector.X + MouseOffset.Y * DragScreenVector.Y);

		// 총 회전 각도 계산
		float TotalAngle = ProjectedAmount * RotationSpeed;

		// 회전의 기준이 될 로컬 축 벡터
		FVector LocalAxisVector;
		switch (DraggingAxis)
		{
		case 1: LocalAxisVector = FVector(1, 0, 0); break;
		case 2: LocalAxisVector = FVector(0, 1, 0); break;
		case 3: LocalAxisVector = FVector(0, 0, 1); break;
		default: LocalAxisVector = FVector(1, 0, 0);
		}

		FQuat NewRot;
		if (CurrentSpace == EGizmoSpace::World)
		{
			FQuat DeltaQuat = FQuat::FromAxisAngle(LocalAxisVector, TotalAngle);
			NewRot = DeltaQuat * DragStartRotation;
		}
		else // Local
		{
			FVector WorldSpaceRotationAxis = DragStartRotation.RotateVector(LocalAxisVector);
			FQuat DeltaQuat = FQuat::FromAxisAngle(WorldSpaceRotationAxis, TotalAngle);
			NewRot = DeltaQuat * DragStartRotation;
		}

		// Gizmo Actor 자신의 회전 변경
		SetActorRotation(NewRot);
		break;
	}
	}
}
