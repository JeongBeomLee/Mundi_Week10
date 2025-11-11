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

// [COMMON] 컴포넌트 생성 로직은 AGizmoActor와 동일합니다.
// 향후 리팩토링 시 GizmoActorBase 클래스로 추출 가능합니다.
AOffscreenGizmoActor::AOffscreenGizmoActor()
{
	Name = "Offscreen Gizmo Actor";

	// [COMMON] 컴포넌트 생성 (AGizmoActor와 동일)
	const float GizmoTotalSize = 1.5f;

	//======= Arrow Component 생성 =======
	RootComponent = CreateDefaultSubobject<USceneComponent>("DefaultSceneComponent");

	ArrowX = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowY = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowZ = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");

	ArrowX->SetDirection(FVector(1.0f, 0.0f, 0.0f));//빨
	ArrowY->SetDirection(FVector(0.0f, 1.0f, 0.0f));//초
	ArrowZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));//파

	ArrowX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ArrowY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ArrowZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ArrowX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ArrowY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ArrowZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	ArrowX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ArrowY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ArrowZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

	ArrowX->SetRenderPriority(100);
	ArrowY->SetRenderPriority(100);
	ArrowZ->SetRenderPriority(100);

	AddOwnedComponent(ArrowX);
	AddOwnedComponent(ArrowY);
	AddOwnedComponent(ArrowZ);

	if (ArrowX) ArrowX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (ArrowY) ArrowY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (ArrowZ) ArrowZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	//======= Rotate Component 생성 =======
	RotateX = NewObject<UGizmoRotateComponent>();
	RotateY = NewObject<UGizmoRotateComponent>();
	RotateZ = NewObject<UGizmoRotateComponent>();

	RotateX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	RotateY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	RotateZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	RotateX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	RotateY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	RotateZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

	RotateX->SetRenderPriority(100);
	RotateY->SetRenderPriority(100);
	RotateZ->SetRenderPriority(100);

	AddOwnedComponent(RotateX);
	AddOwnedComponent(RotateY);
	AddOwnedComponent(RotateZ);

	if (RotateX) RotateX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (RotateY) RotateY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (RotateZ) RotateZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	//======= Scale Component 생성 =======
	ScaleX = NewObject<UGizmoScaleComponent>();
	ScaleY = NewObject<UGizmoScaleComponent>();
	ScaleZ = NewObject<UGizmoScaleComponent>();

	ScaleX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ScaleY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ScaleZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	ScaleX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ScaleY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ScaleZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

	ScaleX->SetRenderPriority(100);
	ScaleY->SetRenderPriority(100);
	ScaleZ->SetRenderPriority(100);

	if (ScaleX) ScaleX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (ScaleY) ScaleY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (ScaleZ) ScaleZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	AddOwnedComponent(ScaleX);
	AddOwnedComponent(ScaleY);
	AddOwnedComponent(ScaleZ);

	CurrentMode = EGizmoMode::Translate;

	// 드래그 상태 변수 초기화
	DraggingAxis = 0;
	DragCamera = nullptr;
	HoverImpactPoint = FVector::Zero();
	DragImpactPoint = FVector::Zero();
	DragScreenVector = FVector2D::Zero();
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
	// [COMMON] UpdateComponentVisibility만 호출 (AGizmoActor와 다르게 auto-follow 없음)
	UpdateComponentVisibility();
}

// [COMMON] Getter/Setter (AGizmoActor와 동일)
void AOffscreenGizmoActor::SetMode(EGizmoMode NewMode)
{
	CurrentMode = NewMode;
}

EGizmoMode AOffscreenGizmoActor::GetMode() const
{
	return CurrentMode;
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
		return;
	}

	// 기즈모 드래그 처리
	ProcessGizmoDraggingImGui(Camera, Viewport, MousePositionX, MousePositionY, bLeftMouseDown, bLeftMouseReleased);

	// [COMMON] 호버링 처리 (드래그 중이 아닐 때만)
	if (!bIsDragging)
	{
		ProcessGizmoHovering(Camera, Viewport, MousePositionX, MousePositionY);
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

		// CHANGED: OnDrag → UpdateSelfTransformFromDrag (semantic 명확화)
		UpdateSelfTransformFromDrag(DraggingAxis, MouseOffset.X, MouseOffset.Y, Camera, Viewport);
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
		}
	}
}

// RENAMED: OnDrag → UpdateSelfTransformFromDrag (자기자신 수정임을 명확히)
void AOffscreenGizmoActor::UpdateSelfTransformFromDrag(uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport)
{
	// DraggingAxis == 0 이면 드래그 중이 아니므로 반환
	if (!Camera || DraggingAxis == 0)
		return;

	// NOTE: Gizmo Actor 자신(this)의 Transform만 업데이트
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

		// [COMMON] 픽셀 당 월드 이동량 계산 (AGizmoActor와 동일)
		FVector2D ScreenAxis = AOffscreenGizmoActor::GetStableAxisDirection(Axis, Camera);
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
			// 로컬 축을 드래그 시작 시점의 월드 축으로 변환
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

// [COMMON] ProcessGizmoModeSwitch - 모드 순환 (Translate → Rotate → Scale)
void AOffscreenGizmoActor::ProcessGizmoModeSwitch()
{
	// 드래그 중에는 모드 변경 불가
	if (bIsDragging)
		return;

	// 모드 순환
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
		SetMode(EGizmoMode::Rotate);
		break;
	case EGizmoMode::Rotate:
		SetMode(EGizmoMode::Scale);
		break;
	case EGizmoMode::Scale:
		SetMode(EGizmoMode::Translate);
		break;
	default:
		SetMode(EGizmoMode::Translate);
		break;
	}
}

// [COMMON] UpdateComponentVisibility (AGizmoActor와 동일)
void AOffscreenGizmoActor::UpdateComponentVisibility()
{
	// bRender가 false이면 모든 컴포넌트 비활성화
	if (!bRender)
	{
		if (ArrowX) ArrowX->SetActive(false);
		if (ArrowY) ArrowY->SetActive(false);
		if (ArrowZ) ArrowZ->SetActive(false);
		if (RotateX) RotateX->SetActive(false);
		if (RotateY) RotateY->SetActive(false);
		if (RotateZ) RotateZ->SetActive(false);
		if (ScaleX) ScaleX->SetActive(false);
		if (ScaleY) ScaleY->SetActive(false);
		if (ScaleZ) ScaleZ->SetActive(false);
		return;
	}

	// 선택된 컴포넌트가 없으면 모든 기즈모 컴포넌트를 비활성화
	bool bHasSelection = SelectionManager && SelectionManager->GetSelectedComponent();
	uint32 HighlightAxis = bIsDragging ? DraggingAxis : GizmoAxis;

	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	if (ArrowX) { ArrowX->SetActive(bShowArrows); ArrowX->SetHighlighted(HighlightAxis == 1, 1); }
	if (ArrowY) { ArrowY->SetActive(bShowArrows); ArrowY->SetHighlighted(HighlightAxis == 2, 2); }
	if (ArrowZ) { ArrowZ->SetActive(bShowArrows); ArrowZ->SetHighlighted(HighlightAxis == 3, 3); }

	bool bShowRotates = bHasSelection && (CurrentMode == EGizmoMode::Rotate);
	if (RotateX) { RotateX->SetActive(bShowRotates); RotateX->SetHighlighted(HighlightAxis == 1, 1); }
	if (RotateY) { RotateY->SetActive(bShowRotates); RotateY->SetHighlighted(HighlightAxis == 2, 2); }
	if (RotateZ) { RotateZ->SetActive(bShowRotates); RotateZ->SetHighlighted(HighlightAxis == 3, 3); }

	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);
	if (ScaleX) { ScaleX->SetActive(bShowScales); ScaleX->SetHighlighted(HighlightAxis == 1, 1); }
	if (ScaleY) { ScaleY->SetActive(bShowScales); ScaleY->SetHighlighted(HighlightAxis == 2, 2); }
	if (ScaleZ) { ScaleZ->SetActive(bShowScales); ScaleZ->SetHighlighted(HighlightAxis == 3, 3); }
}

// [COMMON] ProcessGizmoHovering (AGizmoActor와 동일)
void AOffscreenGizmoActor::ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!Camera) return;

	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));
	FVector2D ViewportMousePos(static_cast<float>(MousePositionX) + ViewportOffset.X,
		static_cast<float>(MousePositionY) + ViewportOffset.Y);

	if (!bIsDragging)
	{
		GizmoAxis = CPickingSystem::IsHoveringGizmoForViewport(
			this,
			Camera,
			ViewportMousePos,
			ViewportSize,
			ViewportOffset,
			Viewport,
			HoverImpactPoint
		);
	}

	bIsHovering = (GizmoAxis > 0);
}

// [COMMON] GetStableAxisDirection (AGizmoActor와 동일, static)
FVector2D AOffscreenGizmoActor::GetStableAxisDirection(const FVector& WorldAxis, const ACameraActor* Camera)
{
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();

	float RightDot = FVector::Dot(WorldAxis, CameraRight);
	float UpDot = FVector::Dot(WorldAxis, CameraUp);

	FVector2D ScreenDirection = FVector2D(RightDot, -UpDot);

	float Length = ScreenDirection.Length();
	if (Length > 0.001f)
	{
		return ScreenDirection * (1.0f / Length);
	}

	return FVector2D(1.0f, 0.0f);
}
