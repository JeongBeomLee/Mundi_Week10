#include "pch.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "RenderSettings.h"
#include "CameraActor.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "UIManager.h"
#include "FViewport.h"
#include "Picking.h"
#include "EditorEngine.h"

IMPLEMENT_CLASS(AGizmoActor)

AGizmoActor::AGizmoActor()
{
	Name = "Gizmo Actor";

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
	GizmoArrowComponents.Add(ArrowX);
	GizmoArrowComponents.Add(ArrowY);
	GizmoArrowComponents.Add(ArrowZ);

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
	GizmoRotateComponents.Add(RotateX);
	GizmoRotateComponents.Add(RotateY);
	GizmoRotateComponents.Add(RotateZ);

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
	GizmoScaleComponents.Add(ScaleX);
	GizmoScaleComponents.Add(ScaleY);
	GizmoScaleComponents.Add(ScaleZ);

	CurrentMode = EGizmoMode::Translate;

	// --- 드래그 상태 변수 초기화 ---
	DraggingAxis = 0;
	DragCamera = nullptr;
	HoverImpactPoint = FVector::Zero();
	DragImpactPoint = FVector::Zero();
	DragScreenVector = FVector2D::Zero();

	// 매니저 참조 초기화 (월드 소유)
	SelectionManager = GetWorld() ? GetWorld()->GetSelectionManager() : nullptr;
	InputManager = &UInputManager::GetInstance();
	UIManager = &UUIManager::GetInstance();
}

void AGizmoActor::Tick(float DeltaSeconds)
{
	if (!SelectionManager) SelectionManager = GetWorld() ? GetWorld()->GetSelectionManager() : nullptr;
	if (!InputManager) InputManager = &UInputManager::GetInstance();
	if (!UIManager) UIManager = &UUIManager::GetInstance();

	// 컴포넌트 활성화 상태 업데이트    
	if (SelectionManager && SelectionManager->HasSelection() && CameraActor)
	{
		USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();

		// 기즈모 위치를 선택된 액터 위치로 업데이트
		if (SelectedComponent)
		{
			// OnDrag 함수가 컴포넌트의 위치를 변경하면,
			// Tick 함수는 그 변경된 위치를 읽어 기즈모 액터 자신을 이동시킵니다.
			SetSpaceWorldMatrix(CurrentSpace, SelectedComponent);
			SetActorLocation(SelectedComponent->GetWorldLocation());
		}
	}
	UpdateComponentVisibility();
}

AGizmoActor::~AGizmoActor()
{
	// Components are centrally destroyed by AActor's destructor
	ArrowX = ArrowY = ArrowZ = nullptr;
	ScaleX = ScaleY = ScaleZ = nullptr;
	RotateX = RotateY = RotateZ = nullptr;
}

void AGizmoActor::SetMode(EGizmoMode NewMode)
{
	CurrentMode = NewMode;
}

EGizmoMode AGizmoActor::GetMode()
{
	return CurrentMode;
}

void AGizmoActor::SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* SelectedComponent)
{
	SetSpace(NewSpace);

	if (!SelectedComponent)
		return;

	if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
	{
		// 기즈모 액터 자체를 타겟의 회전으로 설정합니다.
		FQuat TargetRot = SelectedComponent->GetWorldRotation();
		SetActorRotation(TargetRot);
	}
	else if (NewSpace == EGizmoSpace::World)
	{
		// 기즈모 액터를 월드 축에 정렬 (단위 회전으로 설정)
		SetActorRotation(FQuat::Identity());
	}
}

void AGizmoActor::NextMode(EGizmoMode GizmoMode)
{
	CurrentMode = GizmoMode;
}

TArray<USceneComponent*>* AGizmoActor::GetGizmoComponents()
{
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
		return &GizmoArrowComponents;
	case EGizmoMode::Rotate:
		return &GizmoRotateComponents;
	case EGizmoMode::Scale:
		return &GizmoScaleComponents;
	}
	return nullptr;
}

EGizmoMode AGizmoActor::GetGizmoMode() const
{
	return CurrentMode;
}

// 개선된 축 투영 함수 - 수직 각도에서도 안정적
static FVector2D GetStableAxisDirection(const FVector& WorldAxis, const ACameraActor* Camera)
{
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	// 카메라와 축이 정렬될 때 불안정하게 튀는
	// "예외 처리" if 블록을 완전히 제거합니다.

	// "일반적인 경우"의 스크린 투영 로직만 사용합니다.
	float RightDot = FVector::Dot(WorldAxis, CameraRight);
	float UpDot = FVector::Dot(WorldAxis, CameraUp);

	// DirectX 스크린 좌표계 고려 (Y축 반전)
	FVector2D ScreenDirection = FVector2D(RightDot, -UpDot);

	// 안전한 정규화 (최소 길이 보장)
	float Length = ScreenDirection.Length();
	if (Length > 0.001f)
	{
		return ScreenDirection * (1.0f / Length);
	}

	// 투영된 길이가 0에 가까우면 (즉, 축이 카메라를 쳐다보면)
	// 이 코드가 실행되어 안정적인 기본 X축 방향을 반환합니다.
	return FVector2D(1.0f, 0.0f);
}

void AGizmoActor::OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport)
{
	// DraggingAxis == 0 이면 드래그 중이 아니므로 반환
	if (!Target || !Camera || DraggingAxis == 0)
		return;

	// MouseDeltaX/Y는 이제 드래그 시작점으로부터의 '총 변위(Total Offset)'입니다.
	FVector2D MouseOffset(MouseDeltaX, MouseDeltaY);

	// ────────────── 모드별 처리 (Stateful 방식) ──────────────
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

		// ────────────── 픽셀 당 월드 이동량 계산 (Translate/Scale용) ──────────────
		FVector2D ScreenAxis = GetStableAxisDirection(Axis, Camera);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X;
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
			Target->SetWorldLocation(DragStartLocation + Axis * TotalMovement);
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
			Target->SetWorldScale(NewScale);
		}
		break;
	}
	case EGizmoMode::Rotate:
	{
		// ==============================================================
		// UGizmoManager의 정밀 회전 로직 (Stateful, Total Offset 기반)
		// ==============================================================

		float RotationSpeed = 0.005f;

		// ProcessGizmoDragging에서 미리 계산해둔 2D 스크린 드래그 벡터(DragScreenVector) 사용
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
			// 월드 축(LocalAxisVector)을 기준으로 총 각도(TotalAngle)만큼 회전하는 델타 계산
			FQuat DeltaQuat = FQuat::FromAxisAngle(LocalAxisVector, TotalAngle);
			// '시작 회전 * 델타'로 최종 회전 계산
			NewRot = DeltaQuat * DragStartRotation;
		}
		else // Local
		{
			// 로컬 축을 드래그 시작 시점의 월드 축으로 변환
			FVector WorldSpaceRotationAxis = DragStartRotation.RotateVector(LocalAxisVector);

			// 월드 공간에서 변환된 축을 기준으로 총 각도만큼 회전하는 델타 계산
			FQuat DeltaQuat = FQuat::FromAxisAngle(WorldSpaceRotationAxis, TotalAngle);
			// '시작 회전 * 델타'로 최종 회전 계산
			NewRot = DeltaQuat * DragStartRotation;
		}

		Target->SetWorldRotation(NewRot);
		break;
	}
	}
}

void AGizmoActor::ProcessGizmoInteraction(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!SelectionManager)
	{
		return;
	}
	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!SelectedComponent || !Camera) return;

	// 기즈모 드래그
	ProcessGizmoDragging(Camera, Viewport, MousePositionX, MousePositionY);

	// 호버링 (드래그 중이 아닐 때만)
	if (!bIsDragging)
	{
		ProcessGizmoHovering(Camera, Viewport, MousePositionX, MousePositionY);
	}
}

void AGizmoActor::ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
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

	if (GizmoAxis > 0)	//기즈모 축이 0이상이라면 선택 된것 
	{
		bIsHovering = true;
	}
	else
	{
		bIsHovering = false;
	}
}

void AGizmoActor::ProcessGizmoDragging(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!SelectedComponent || !Camera) return;

	FVector2D CurrentMousePosition(MousePositionX, MousePositionY);

	// --- 1. Begin Drag (드래그 시작) ---
	if (InputManager->IsMouseButtonDown(LeftButton) && !bIsDragging && GizmoAxis > 0)
	{
		bIsDragging = true;
		DraggingAxis = GizmoAxis;
		DragCamera = Camera;

		// Alt 키가 눌려있으면 Actor 복제
		bool bIsAltDown = InputManager->IsKeyDown(VK_MENU);
		if (bIsAltDown && SelectionManager->IsActorMode())
		{
			AActor* SelectedActor = SelectionManager->GetSelectedActor();
			if (SelectedActor && World)
			{
				// Actor 복제
				AActor* DuplicatedActor = SelectedActor->Duplicate();
				if (DuplicatedActor)
				{
					// 원본과 같은 위치에 복제본 생성
					DuplicatedActor->SetActorLocation(SelectedActor->GetActorLocation());
					DuplicatedActor->SetActorRotation(SelectedActor->GetActorRotation());
					DuplicatedActor->SetActorScale(SelectedActor->GetActorScale());

					// 고유한 이름 생성
					FString ActorTypeName = SelectedActor->GetClass()->Name;
					FString UniqueName = World->GenerateUniqueActorName(ActorTypeName);
					DuplicatedActor->SetName(UniqueName);

					// World에 등록
					DuplicatedActor->SetWorld(World);
					World->OnActorSpawned(DuplicatedActor);

					// 복제본을 선택 (이제 복제본을 드래그함)
					SelectionManager->ClearSelection();
					SelectionManager->SelectActor(DuplicatedActor);

					// SelectedComponent를 복제본의 RootComponent로 업데이트
					SelectedComponent = DuplicatedActor->GetRootComponent();
				}
			}
		}

		// 드래그 시작 상태 저장
		DragStartLocation = SelectedComponent->GetWorldLocation();
		DragStartRotation = SelectedComponent->GetWorldRotation();
		DragStartScale = SelectedComponent->GetWorldScale();
		DragStartPosition = CurrentMousePosition;

		// 호버링 시점의 3D 충돌 지점을 드래그 시작 지점으로 래치
		DragImpactPoint = HoverImpactPoint;

		// (회전용) 드래그 시작 시점의 2D 드래그 벡터 계산
		if (CurrentMode == EGizmoMode::Rotate)
		{
			// ==============================================================
			// UGizmoManager의 정밀한 접선 계산 로직
			// ==============================================================
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

			// 1. 3D 충돌 지점(DragImpactPoint)과 기즈모 중심(DragStartLocation) 사이의 3D 벡터
			FVector ClickVector = DragImpactPoint - DragStartLocation;

			// 2. 3D 회전 축과 3D 클릭 벡터를 외적하여 3D 접선(Tangent)을 구함
			FVector Tangent3D = FVector::Cross(WorldAxis, ClickVector);

			// 3. 3D 접선을 2D 스크린에 투영 (GetStableAxisDirection의 로직 재활용)
			float RightDot = FVector::Dot(Tangent3D, DragCamera->GetRight());
			float UpDot = FVector::Dot(Tangent3D, DragCamera->GetUp());

			// DirectX 스크린 좌표계 (Y축 반전)를 고려하고 정규화
			DragScreenVector = FVector2D(RightDot, -UpDot);
			DragScreenVector = DragScreenVector.GetNormalized(); // 방향 벡터이므로 정규화
		}
	}

	// --- 2. Continue Drag (드래그 지속) ---
	if (InputManager->IsMouseButtonDown(LeftButton) && bIsDragging)
	{
		// 드래그 시작점으로부터의 '총 변위(Total Offset)' 계산
		FVector2D MouseOffset = CurrentMousePosition - DragStartPosition;

		// OnDrag 함수에 고정된 축(DraggingAxis)과 총 변위(MouseOffset)를 전달
		OnDrag(SelectedComponent, DraggingAxis, MouseOffset.X, MouseOffset.Y, Camera, Viewport);
	}

	// --- 3. End Drag (드래그 종료) ---
	if (InputManager->IsMouseButtonReleased(LeftButton))
	{
		if (bIsDragging)
		{
			bIsDragging = false;
			DraggingAxis = 0; // 고정된 축 해제
			DragCamera = nullptr;
			GizmoAxis = 0; // 하이라이트 해제
			SetSpaceWorldMatrix(CurrentSpace, SelectedComponent);
		}
	}
}

void AGizmoActor::ProcessGizmoModeSwitch()
{
	// 우클릭 드래그 중에는 기즈모 모드/스페이스 변경 불가
	if (InputManager->IsMouseButtonDown(RightButton) || bIsDragging) // 드래그 중 변경 불가
	{
		return;
	}

	// Q 키: Select 모드
	if (InputManager->IsKeyPressed('Q'))
	{
		SetMode(EGizmoMode::Select);
	}
	// W 키: Move (Translate) 모드
	else if (InputManager->IsKeyPressed('W'))
	{
		SetMode(EGizmoMode::Translate);
	}
	// E 키: Rotate 모드
	else if (InputManager->IsKeyPressed('E'))
	{
		SetMode(EGizmoMode::Rotate);
	}
	// R 키: Scale 모드
	else if (InputManager->IsKeyPressed('R'))
	{
		SetMode(EGizmoMode::Scale);
	}
	// 스페이스 키로 기즈모 모드 순환 전환 (기존 기능 유지)
	else if (InputManager->IsKeyPressed(VK_SPACE))
	{
		int GizmoModeIndex = static_cast<int>(GetMode());
		GizmoModeIndex = (GizmoModeIndex + 1) % static_cast<uint32>(EGizmoMode::Select);	// 3 = enum 개수
		EGizmoMode NewGizmoMode = static_cast<EGizmoMode>(GizmoModeIndex);
		NextMode(NewGizmoMode);
	}
	// Tab 키로 월드-로컬 모드 전환
	if (InputManager->IsKeyPressed(VK_TAB))
	{
		if (GetSpace() == EGizmoSpace::World)
		{
			SetSpace(EGizmoSpace::Local);
		}
		else
		{
			SetSpace(EGizmoSpace::World);
		}
	}
}

void AGizmoActor::UpdateComponentVisibility()
{
	// 선택된 액터가 없으면 모든 기즈모 컴포넌트를 비활성화
	bool bHasSelection = SelectionManager && SelectionManager->GetSelectedComponent();

	// 드래그 중일 때는 고정된 축(DraggingAxis)을, 아닐 때는 호버 축(GizmoAxis)을 사용
	uint32 HighlightAxis = bIsDragging ? DraggingAxis : GizmoAxis;

	// Arrow Components (Translate 모드)
	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	if (ArrowX) { ArrowX->SetActive(bShowArrows); ArrowX->SetHighlighted(HighlightAxis == 1, 1); }
	if (ArrowY) { ArrowY->SetActive(bShowArrows); ArrowY->SetHighlighted(HighlightAxis == 2, 2); }
	if (ArrowZ) { ArrowZ->SetActive(bShowArrows); ArrowZ->SetHighlighted(HighlightAxis == 3, 3); }

	// Rotate Components (Rotate 모드)
	bool bShowRotates = bHasSelection && (CurrentMode == EGizmoMode::Rotate);
	if (RotateX) { RotateX->SetActive(bShowRotates); RotateX->SetHighlighted(HighlightAxis == 1, 1); }
	if (RotateY) { RotateY->SetActive(bShowRotates); RotateY->SetHighlighted(HighlightAxis == 2, 2); }
	if (RotateZ) { RotateZ->SetActive(bShowRotates); RotateZ->SetHighlighted(HighlightAxis == 3, 3); }

	// Scale Components (Scale 모드)
	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);
	if (ScaleX) { ScaleX->SetActive(bShowScales); ScaleX->SetHighlighted(HighlightAxis == 1, 1); }
	if (ScaleY) { ScaleY->SetActive(bShowScales); ScaleY->SetHighlighted(HighlightAxis == 2, 2); }
	if (ScaleZ) { ScaleZ->SetActive(bShowScales); ScaleZ->SetHighlighted(HighlightAxis == 3, 3); }
}

void AGizmoActor::OnDrag(USceneComponent* SelectedComponent, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera)
{
	OnDrag(SelectedComponent, GizmoAxis, MouseDeltaX, MouseDeltaY, Camera, nullptr);
}