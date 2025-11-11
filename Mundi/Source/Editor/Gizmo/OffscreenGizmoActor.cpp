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

	TArray<UGizmoArrowComponent**> ArrowPtrs = { &ArrowX, &ArrowY, &ArrowZ };
	TArray<FVector> ArrowDirections = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
	TArray<FVector> ArrowColors = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
	TArray<FVector> ArrowRotations = { FVector(0, 0, 0), FVector(0, 0, 90), FVector(0, -90, 0) };

	for (int i = 0; i < 3; ++i)
	{
		*ArrowPtrs[i] = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
		(*ArrowPtrs[i])->SetDirection(ArrowDirections[i]);
		(*ArrowPtrs[i])->SetColor(ArrowColors[i]);
		(*ArrowPtrs[i])->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
		(*ArrowPtrs[i])->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
		(*ArrowPtrs[i])->SetRenderPriority(100);
		(*ArrowPtrs[i])->SetRelativeRotation(FQuat::MakeFromEulerZYX(ArrowRotations[i]));
		AddOwnedComponent(*ArrowPtrs[i]);
	}

	//======= Rotate Component 생성 =======
	TArray<UGizmoRotateComponent**> RotatePtrs = { &RotateX, &RotateY, &RotateZ };
	TArray<FVector> RotateDirections = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
	TArray<FVector> RotateColors = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
	TArray<FVector> RotateRotations = { FVector(0, 0, 0), FVector(0, 0, 90), FVector(0, -90, 0) };

	for (int i = 0; i < 3; ++i)
	{
		*RotatePtrs[i] = NewObject<UGizmoRotateComponent>();
		(*RotatePtrs[i])->SetDirection(RotateDirections[i]);
		(*RotatePtrs[i])->SetColor(RotateColors[i]);
		(*RotatePtrs[i])->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
		(*RotatePtrs[i])->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
		(*RotatePtrs[i])->SetRenderPriority(100);
		(*RotatePtrs[i])->SetRelativeRotation(FQuat::MakeFromEulerZYX(RotateRotations[i]));
		AddOwnedComponent(*RotatePtrs[i]);
	}

	//======= Scale Component 생성 =======
	TArray<UGizmoScaleComponent**> ScalePtrs = { &ScaleX, &ScaleY, &ScaleZ };
	TArray<FVector> ScaleDirections = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
	TArray<FVector> ScaleColors = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
	TArray<FVector> ScaleRotations = { FVector(0, 0, 0), FVector(0, 0, 90), FVector(0, -90, 0) };

	for (int i = 0; i < 3; ++i)
	{
		*ScalePtrs[i] = NewObject<UGizmoScaleComponent>();
		(*ScalePtrs[i])->SetDirection(ScaleDirections[i]);
		(*ScalePtrs[i])->SetColor(ScaleColors[i]);
		(*ScalePtrs[i])->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
		(*ScalePtrs[i])->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
		(*ScalePtrs[i])->SetRenderPriority(100);
		(*ScalePtrs[i])->SetRelativeRotation(FQuat::MakeFromEulerZYX(ScaleRotations[i]));
		AddOwnedComponent(*ScalePtrs[i]);
	}

	// 모든 Gizmo 컴포넌트를 에디터 헬퍼로 설정 (섀도우 생성 방지, EditorPrimitives로 처리)
	TArray<UActorComponent*> AllGizmoComponents = {
		ArrowX, ArrowY, ArrowZ,
		RotateX, RotateY, RotateZ,
		ScaleX, ScaleY, ScaleZ
	};
	for (UActorComponent* GizmoComp : AllGizmoComponents)
	{
		if (GizmoComp)
		{
			GizmoComp->SetEditability(false);
		}
	}

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
	TArray<UActorComponent*> AllComponents = {
		ArrowX, ArrowY, ArrowZ,
		RotateX, RotateY, RotateZ,
		ScaleX, ScaleY, ScaleZ
	};
	for (UActorComponent* Comp : AllComponents)
	{
		if (Comp)
		{
			Comp->SetActive(false);
		}
	}
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

void AOffscreenGizmoActor::SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* Target)
{
	SetSpace(NewSpace);

	if (!Target)
		return;

	if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
	{
		// Local 모드: Gizmo를 타겟의 회전으로 설정
		FQuat TargetRot = Target->GetWorldRotation();
		SetActorRotation(TargetRot);
	}
	else if (NewSpace == EGizmoSpace::World)
	{
		// World 모드: Gizmo를 월드 축에 정렬 (단위 회전)
		SetActorRotation(FQuat::Identity());
	}
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
		TArray<UActorComponent*> AllComponents = {
			ArrowX, ArrowY, ArrowZ,
			RotateX, RotateY, RotateZ,
			ScaleX, ScaleY, ScaleZ
		};
		for (UActorComponent* Comp : AllComponents)
		{
			if (Comp)
			{
				Comp->SetActive(false);
			}
		}
		return;
	}

	// 선택된 컴포넌트가 없으면 모든 기즈모 컴포넌트를 비활성화
	bool bHasSelection = SelectionManager && SelectionManager->GetSelectedComponent();
	uint32 HighlightAxis = bIsDragging ? DraggingAxis : GizmoAxis;

	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	bool bShowRotates = bHasSelection && (CurrentMode == EGizmoMode::Rotate);
	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);

	TArray<UGizmoArrowComponent*> ArrowComponents = { ArrowX, ArrowY, ArrowZ };
	TArray<UGizmoRotateComponent*> RotateComponents = { RotateX, RotateY, RotateZ };
	TArray<UGizmoScaleComponent*> ScaleComponents = { ScaleX, ScaleY, ScaleZ };

	for (int i = 0; i < 3; ++i)
	{
		uint32 AxisIndex = i + 1; // 1, 2, 3

		if (ArrowComponents[i])
		{
			ArrowComponents[i]->SetActive(bShowArrows);
			ArrowComponents[i]->SetHighlighted(HighlightAxis == AxisIndex, AxisIndex);
		}

		if (RotateComponents[i])
		{
			RotateComponents[i]->SetActive(bShowRotates);
			RotateComponents[i]->SetHighlighted(HighlightAxis == AxisIndex, AxisIndex);
		}

		if (ScaleComponents[i])
		{
			ScaleComponents[i]->SetActive(bShowScales);
			ScaleComponents[i]->SetHighlighted(HighlightAxis == AxisIndex, AxisIndex);
		}
	}
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
