// ────────────────────────────────────────────────────────────────────────────
// Pawn.cpp
// Pawn 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Pawn.h"
#include "Controller.h"
#include "InputComponent.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(APawn)

BEGIN_PROPERTIES(APawn)
	MARK_AS_SPAWNABLE("Pawn", "Controller에 의해 빙의될 수 있는 기본 Pawn 클래스입니다.")
END_PROPERTIES()
// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

APawn::APawn()
	: Controller(nullptr)
	, InputComponent(nullptr)
	, PendingMovementInput(FVector())
	, bNormalizeMovementInput(true)
{
	// InputComponent 생성
	InputComponent = ObjectFactory::NewObject<UInputComponent>();
	if (InputComponent)
	{
		InputComponent->SetOwner(this);
	}
}

APawn::~APawn()
{
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
	// 기본 구현: 아무것도 하지 않음
	// 파생 클래스에서 오버라이드하여 입력 바인딩을 추가합니다
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
	if (DeltaYaw != 0.0f)
	{
		// Pawn의 현재 Yaw에 DeltaYaw 추가
		FQuat CurrentRotation = GetActorRotation();
		FVector EulerAngles = CurrentRotation.ToEulerZYXDeg();

		EulerAngles.Z += DeltaYaw; // Yaw

		// Yaw를 0~360 범위로 정규화
		while (EulerAngles.Z >= 360.0f) EulerAngles.Z -= 360.0f;
		while (EulerAngles.Z < 0.0f) EulerAngles.Z += 360.0f;

		FQuat NewRotation = FQuat::MakeFromEulerZYX(EulerAngles);
		SetActorRotation(NewRotation);
	}
}

void APawn::AddControllerPitchInput(float DeltaPitch)
{
	// 기본 구현: 아무것도 하지 않음
	// 카메라 컴포넌트가 있는 Pawn에서 오버라이드
}

void APawn::AddControllerRollInput(float DeltaRoll)
{
	// 기본 구현: 아무것도 하지 않음
	// 비행기 등에서 필요 시 오버라이드
}
