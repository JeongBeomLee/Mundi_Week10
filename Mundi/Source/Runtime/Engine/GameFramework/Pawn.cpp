// ────────────────────────────────────────────────────────────────────────────
// Pawn.cpp
// Pawn 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Pawn.h"
#include "Controller.h"
#include "InputComponent.h"
#include "ObjectFactory.h"
#include "CameraComponent.h"
#include "InputManager.h"

IMPLEMENT_CLASS(APawn)

BEGIN_PROPERTIES(APawn)
	MARK_AS_SPAWNABLE("Pawn", "Controller에 의해 빙의될 수 있는 기본 Pawn 클래스입니다.")
END_PROPERTIES()
// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

APawn::APawn()
	: CameraComponent(nullptr)
	, Controller(nullptr)
	, InputComponent(nullptr)
	, PendingMovementInput(FVector())
	, bNormalizeMovementInput(true)
	, MovementSpeed(25.0f)        // 에디터 기본 속도: 10.0f * 2.5f = 25.0f
	, RotationSensitivity(0.05f)  // 에디터 마우스 감도: 0.05f
	, CurrentPitch(0.0f)
	, MinPitch(-89.0f)            // 거의 아래를 볼 수 있도록
	, MaxPitch(89.0f)             // 거의 위를 볼 수 있도록
{
	// InputComponent 생성
	InputComponent = ObjectFactory::NewObject<UInputComponent>();
	if (InputComponent)
	{
		InputComponent->SetOwner(this);
	}

	// CameraComponent 생성 및 RootComponent로 설정
	CameraComponent = ObjectFactory::NewObject<UCameraComponent>();
	if (CameraComponent)
	{
		CameraComponent->SetOwner(this);
		AddOwnedComponent(CameraComponent);

		// CameraComponent를 RootComponent로 설정
		SetRootComponent(CameraComponent);

		// 기본 카메라 설정
		CameraComponent->SetFOV(90.0f);
		CameraComponent->SetClipPlanes(1.0f, 10000.0f);
	}
}

APawn::~APawn()
{
	// CameraComponent 정리
	if (CameraComponent)
	{
		ObjectFactory::DeleteObject(CameraComponent);
		CameraComponent = nullptr;
	}

	// InputComponent 정리
	if (InputComponent)
	{
		ObjectFactory::DeleteObject(InputComponent);
		InputComponent = nullptr;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void APawn::BeginPlay()
{
	Super::BeginPlay();
}

void APawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 기본 폰 액터 카메라 입력 처리
	// Character 등은 CapsuleComponent가 Root이므로 이 로직이 실행되지 않음
	if (CameraComponent && CameraComponent == RootComponent && Controller)
	{
		UInputManager& InputMgr = UInputManager::GetInstance();

		// 마우스 우클릭으로 카메라 회전
		if (InputMgr.IsMouseButtonDown(EMouseButton::RightButton))
		{
			FVector2D MouseDelta = InputMgr.GetMouseDelta();

			// 마우스 델타가 있으면 회전
			if (MouseDelta.X != 0.0f || MouseDelta.Y != 0.0f)
			{
				AddControllerYawInput(MouseDelta.X);
				AddControllerPitchInput(MouseDelta.Y);
			}
		}

		// WASD + QE 이동 처리
		FVector MoveInput(0.0f, 0.0f, 0.0f);

		// WASD로 전후좌우 이동
		if (InputMgr.IsKeyDown('W')) MoveInput.X += 1.0f;  // 전진
		if (InputMgr.IsKeyDown('S')) MoveInput.X -= 1.0f;  // 후진
		if (InputMgr.IsKeyDown('D')) MoveInput.Y += 1.0f;  // 우측
		if (InputMgr.IsKeyDown('A')) MoveInput.Y -= 1.0f;  // 좌측

		// QE로 상하 이동
		if (InputMgr.IsKeyDown('E')) MoveInput.Z += 1.0f;  // 위로
		if (InputMgr.IsKeyDown('Q')) MoveInput.Z -= 1.0f;  // 아래로

		// 이동 입력이 있으면 처리
		if (MoveInput.SizeSquared() > 0.0f)
		{
			// 입력 정규화
			MoveInput = MoveInput.GetNormalized();

			// 카메라 기준 이동 방향 계산
			FVector Forward = CameraComponent->GetForward();
			FVector Right = CameraComponent->GetRight();
			FVector Up = FVector(0.0f, 0.0f, 1.0f);

			// 이동 벡터 계산 (카메라 기준)
			FVector MovementVector =
				Forward * MoveInput.X +  // 전후 이동
				Right * MoveInput.Y +    // 좌우 이동
				Up * MoveInput.Z;        // 상하 이동

			// 실제 이동량 계산
			FVector DeltaMovement = MovementVector * MovementSpeed * DeltaSeconds;

			// 액터 위치 업데이트
			SetActorLocation(GetActorLocation() + DeltaMovement);
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Controller 관련
// ────────────────────────────────────────────────────────────────────────────

void APawn::PossessedBy(AController* NewController)
{
	Controller = NewController;
}

void APawn::UnPossessed()
{
	Controller = nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 관련
// ────────────────────────────────────────────────────────────────────────────

void APawn::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	// 기본 APawn은 입력 바인딩을 하지 않음
	// Character, DefaultPawn 등 파생 클래스에서 필요한 입력만 바인딩

	// 에디터 카메라 스타일로 동작하는 기본 Pawn이 필요한 경우:
	// - World Settings의 Default Pawn Class를 APawn으로 설정하면
	// - Tick()의 마우스 회전 + WASD 이동 로직이 자동으로 작동함
	// - 입력 바인딩은 Tick()에서 InputManager를 직접 사용하므로 불필요
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void APawn::AddMovementInput(FVector WorldDirection, float ScaleValue)
{
	// 입력이 유효하지 않으면 무시
	if (ScaleValue == 0.0f || WorldDirection.SizeSquared() == 0.0f)
	{
		return;
	}

	// 방향 정규화
	FVector NormalizedDirection = WorldDirection.GetNormalized();

	// 스케일 적용하여 누적
	PendingMovementInput += NormalizedDirection * ScaleValue;
}

FVector APawn::ConsumeMovementInput()
{
	FVector Result = PendingMovementInput;

	// 정규화 옵션이 켜져 있고, 입력이 있으면 정규화
	if (bNormalizeMovementInput && Result.SizeSquared() > 0.0f)
	{
		Result = Result.GetNormalized();
	}

	// 입력 초기화
	PendingMovementInput = FVector();

	return Result;
}

void APawn::AddControllerYawInput(float DeltaYaw)
{
	if (DeltaYaw != 0.0f && CameraComponent)
	{
		// Yaw는 월드 Z축 기준으로 회전 (좌우 회전)
		DeltaYaw *= RotationSensitivity;

		// 현재 회전에 Yaw 추가
		FQuat CurrentRotation = GetActorRotation();
		FVector EulerAngles = CurrentRotation.ToEulerZYXDeg();

		EulerAngles.Z += DeltaYaw; // Yaw

		// Yaw를 0~360 범위로 정규화
		while (EulerAngles.Z >= 360.0f) EulerAngles.Z -= 360.0f;
		while (EulerAngles.Z < 0.0f) EulerAngles.Z += 360.0f;

		// Pitch 유지하면서 Yaw만 업데이트
		EulerAngles.Y = CurrentPitch; // Pitch
		EulerAngles.X = 0.0f;         // Roll (사용 안 함)

		FQuat NewRotation = FQuat::MakeFromEulerZYX(EulerAngles);
		SetActorRotation(NewRotation);
	}
}

void APawn::AddControllerPitchInput(float DeltaPitch)
{
	if (DeltaPitch != 0.0f && CameraComponent)
	{
		// Pitch는 로컬 Y축 기준으로 회전 (상하 회전)
		DeltaPitch *= RotationSensitivity;

		// CurrentPitch에 추가
		CurrentPitch += DeltaPitch;

		// Pitch 제한
		CurrentPitch = FMath::Clamp(CurrentPitch, MinPitch, MaxPitch);

		// 현재 Yaw 유지하면서 Pitch 업데이트
		FQuat CurrentRotation = GetActorRotation();
		FVector EulerAngles = CurrentRotation.ToEulerZYXDeg();

		EulerAngles.Y = CurrentPitch; // Pitch
		EulerAngles.X = 0.0f;         // Roll (사용 안 함)
		// EulerAngles.Z는 현재 Yaw 유지

		FQuat NewRotation = FQuat::MakeFromEulerZYX(EulerAngles);
		SetActorRotation(NewRotation);
	}
}

void APawn::AddControllerRollInput(float DeltaRoll)
{
	// 기본 구현: 아무것도 하지 않음
	// 비행기 등에서 필요 시 오버라이드
}
