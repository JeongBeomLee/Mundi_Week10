// ────────────────────────────────────────────────────────────────────────────
// Controller.cpp
// Controller 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Controller.h"
#include "Pawn.h"

IMPLEMENT_CLASS(AController)

BEGIN_PROPERTIES(AController)
	//MARK_AS_SPAWNABLE("Controller", "Pawn을 빙의하여 제어하는 기본 Controller 클래스입니다.")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

AController::AController()
	: PossessedPawn(nullptr)
	, ControlRotation(FQuat::Identity())
{
}

AController::~AController()
{
	// 빙의 해제
	if (PossessedPawn)
	{
		UnPossess();
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AController::BeginPlay()
{
	Super::BeginPlay();
}

void AController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

// ────────────────────────────────────────────────────────────────────────────
// Pawn 빙의 관련
// ────────────────────────────────────────────────────────────────────────────

void AController::Possess(APawn* InPawn)
{
	if (!InPawn)
	{
		return;
	}

	// 이미 빙의한 Pawn이 있으면 먼저 빙의 해제
	if (PossessedPawn)
	{
		UnPossess();
	}

	// 새 Pawn 빙의
	PossessedPawn = InPawn;
	PossessedPawn->PossessedBy(this);

	// 이벤트 호출
	OnPossess(InPawn);
}

void AController::UnPossess()
{
	if (!PossessedPawn)
	{
		return;
	}

	// 이벤트 호출
	OnUnPossess();

	// Pawn에게 빙의 해제 알림
	PossessedPawn->UnPossessed();
	PossessedPawn = nullptr;
}

void AController::OnPossess(APawn* InPawn)
{
	// 기본 구현: 아무것도 하지 않음
	// 파생 클래스에서 오버라이드
}

void AController::OnUnPossess()
{
	// 기본 구현: 아무것도 하지 않음
	// 파생 클래스에서 오버라이드
}

// ────────────────────────────────────────────────────────────────────────────
// Control Rotation
// ────────────────────────────────────────────────────────────────────────────

void AController::SetControlRotation(const FQuat& NewRotation)
{
	ControlRotation = NewRotation.GetNormalized();
}

void AController::AddControlRotation(const FQuat& DeltaRotation)
{
	ControlRotation = (ControlRotation * DeltaRotation).GetNormalized();
}

void AController::AddYawInput(float DeltaYaw)
{
	if (DeltaYaw != 0.0f)
	{
		// Yaw 회전 적용
		FVector EulerAngles = ControlRotation.ToEulerZYXDeg();
		EulerAngles.Z += DeltaYaw; // Yaw

		// Yaw를 0~360 범위로 정규화
		while (EulerAngles.Z >= 360.0f) EulerAngles.Z -= 360.0f;
		while (EulerAngles.Z < 0.0f) EulerAngles.Z += 360.0f;

		ControlRotation = FQuat::MakeFromEulerZYX(EulerAngles);
	}
}

void AController::AddPitchInput(float DeltaPitch)
{
	if (DeltaPitch != 0.0f)
	{
		// Pitch 회전 적용
		FVector EulerAngles = ControlRotation.ToEulerZYXDeg();
		EulerAngles.Y += DeltaPitch; // Pitch

		// Pitch를 -90~90 범위로 클램프
		if (EulerAngles.Y > 89.0f) EulerAngles.Y = 89.0f;
		if (EulerAngles.Y < -89.0f) EulerAngles.Y = -89.0f;

		ControlRotation = FQuat::MakeFromEulerZYX(EulerAngles);
	}
}

void AController::AddRollInput(float DeltaRoll)
{
	if (DeltaRoll != 0.0f)
	{
		// Roll 회전 적용
		FVector EulerAngles = ControlRotation.ToEulerZYXDeg();
		EulerAngles.X += DeltaRoll; // Roll

		// Roll을 0~360 범위로 정규화
		while (EulerAngles.X >= 360.0f) EulerAngles.X -= 360.0f;
		while (EulerAngles.X < 0.0f) EulerAngles.X += 360.0f;

		ControlRotation = FQuat::MakeFromEulerZYX(EulerAngles);
	}
}
