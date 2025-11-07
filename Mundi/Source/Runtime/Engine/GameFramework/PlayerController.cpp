// ────────────────────────────────────────────────────────────────────────────
// PlayerController.cpp
// 플레이어 입력 처리 Controller 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "InputComponent.h"
#include "InputManager.h"
#include "PlayerCameraManager.h"
#include "CameraComponent.h"
#include "World.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(APlayerController)

BEGIN_PROPERTIES(APlayerController)
	MARK_AS_SPAWNABLE("PlayerController", "플레이어 입력을 처리하는 Controller입니다.")
END_PROPERTIES()

APlayerController::APlayerController()
	: InputManager(nullptr)
	, bInputEnabled(true)
	, bShowMouseCursor(true)
	, bIsMouseLocked(false)
	, MouseSensitivity(1.0f)
	, PlayerCameraManager(nullptr)
{
}

APlayerController::~APlayerController()
{
	DeleteObject(PlayerCameraManager);
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::BeginPlay()
{
	Super::BeginPlay();

	// InputManager 참조 획득
	InputManager = &UInputManager::GetInstance();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->BeginPlay();
	}
}

void APlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 입력 처리
	if (bInputEnabled)
	{
		ProcessPlayerInput();
	}

	// 카메라 업데이트
	if (PlayerCameraManager)
	{
		PlayerCameraManager->UpdateCamera(DeltaSeconds);
	}
}

void APlayerController::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);
}

void APlayerController::UnPossess()
{
	Super::UnPossess();
}

void APlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		// Pawn의 InputComponent에 입력 바인딩 설정
		UInputComponent* InputComp = InPawn->GetInputComponent();
		if (InputComp)
		{
			InPawn->SetupPlayerInputComponent(InputComp);
		}

		// PlayerCameraManager 생성 (OnPossess가 BeginPlay보다 먼저 호출됨)
		if (!PlayerCameraManager && World && World->bPie)
		{
			PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		}

		// PlayerCameraManager의 ViewTarget 설정
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetViewTarget(InPawn);
		}
	}
}

void APlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::ProcessPlayerInput()
{
	if (!PossessedPawn)
	{
		return;
	}

	// Pawn의 InputComponent에 입력 전달
	UInputComponent* InputComp = PossessedPawn->GetInputComponent();
	if (InputComp)
	{
		InputComp->ProcessInput();
	}
}

void APlayerController::ShowMouseCursor()
{
	bShowMouseCursor = true;

	if (InputManager)
	{
		InputManager->SetCursorVisible(true);
	}
}

void APlayerController::HideMouseCursor()
{
	bShowMouseCursor = false;

	if (InputManager)
	{
		InputManager->SetCursorVisible(false);
	}
}

void APlayerController::LockMouseCursor()
{
	bIsMouseLocked = true;

	if (InputManager)
	{
		InputManager->LockCursor();
	}
}

void APlayerController::UnlockMouseCursor()
{
	bIsMouseLocked = false;

	if (InputManager)
	{
		InputManager->ReleaseCursor();
	}
}

UCameraComponent* APlayerController::GetCameraComponentForRendering() const
{
	if (PlayerCameraManager)
	{
		return PlayerCameraManager->GetCameraComponentForRendering();
	}
	return nullptr;
}
