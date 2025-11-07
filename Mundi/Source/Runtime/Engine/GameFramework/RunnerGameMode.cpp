// ────────────────────────────────────────────────────────────────────────────
// RunnerGameMode.cpp
// 런2 게임 모드 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "RunnerGameMode.h"
#include "Character.h"
#include "RunnerCharacter.h"
#include "PlayerController.h"
#include "GameStateBase.h"
#include "PlayerCameraManager.h"
#include "CameraBlendPresetLibrary.h"
#include "UCameraModifier_CustomBlend.h"

IMPLEMENT_CLASS(ARunnerGameMode)

BEGIN_PROPERTIES(ARunnerGameMode)
	MARK_AS_SPAWNABLE("RunnerGameMode", "런2 게임 전용 GameMode입니다.")
	ADD_PROPERTY(float, DifficultyIncreaseRate, "Difficulty", true, "난이도 증가율 (10초마다)")
	ADD_PROPERTY(int32, JumpScore, "Score", true, "점프 시 획득 점수")
	ADD_PROPERTY(int32, CoinScore, "Score", true, "코인 획득 점수")
	ADD_PROPERTY(int32, AvoidScore, "Score", true, "장애물 회피 점수")
END_PROPERTIES()

ARunnerGameMode::ARunnerGameMode()
{
	// RunnerGameState 사용 (나중에 추가)
	// GameStateClass = ARunnerGameState::StaticClass();

	// 기본 PlayerController 사용
	PlayerControllerClass = APlayerController::StaticClass();

	// Character를 기본 Pawn으로 설정
	DefaultPawnClass = ARunnerCharacter::StaticClass();

	// 플레이어 스폰 위치 (런2 게임 시작 위치)
	PlayerSpawnLocation = FVector(0.0f, 0.0f, 3.0f);

	// 자동 스폰 활성화
	bAutoSpawnPlayer = true;

	UE_LOG("[RunnerGameMode] Constructor - DefaultPawnClass set to ACharacter");
}

ARunnerGameMode::~ARunnerGameMode()
{
}

void ARunnerGameMode::BeginPlay()
{
	Super::BeginPlay();  // ← 여기서 InitPlayer()가 호출되어 Character 스폰됨

	// RunnerCharacter가 자체적으로 스크립트를 연결하므로 여기서는 불필요

	// GameState의 상태 변경 델리게이트 바인딩
	if (GameState)
	{
		GameState->OnGameStateChanged.AddDynamic(this, &ARunnerGameMode::OnGameStateChanged);
		UE_LOG("[RunnerGameMode] GameState delegate bound");
	}

	// 카메라 트랜지션 효과 적용 테스트 코드
	// PlayerController와 CameraManager 가져오기
	APlayerController* PlayerController = GetPlayerController();
	if (PlayerController)
	{
		APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager();
		if (!CameraManager)
		{
			UE_LOG("[RunnerGameMode] WARNING: CameraManager not found");
			return;
		}

		// 플레이어 캐릭터 위치 가져오기
		APawn* PlayerPawn = PlayerController->GetPawn();
		if (PlayerPawn)
		{
			FVector PlayerLocation = PlayerPawn->GetActorLocation();

			// 시작 카메라 위치: 플레이어 위 + 뒤에서 내려다보는 시점
			FVector StartLocation = PlayerLocation + FVector(-8.0f, 0.0f, 6.0f);
			FQuat StartRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, -45.0f, 0.0f)); // 45도 아래로 (Pitch)
			float StartFOV = 90.0f;

			// 1. ViewTarget을 플레이어로 설정 (Target 설정)
			CameraManager->SetViewTarget(PlayerPawn);

			// 2. 카메라 초기 위치 덮어쓰기 (블렌딩 없이)
			CameraManager->SetCameraTransform(StartLocation, StartRotation, StartFOV);

			// 3. 프리셋을 사용하여 플레이어 카메라로 부드럽게 전환, "Cinematic" 프리셋 사용
			CameraManager->SetViewTargetWithBlendPreset(PlayerPawn, "Cinematic");

			// 4. 블렌딩 완료 후 카메라 흔들림 시작하도록 델리게이트 바인딩
			CameraManager->OnBlendComplete.AddDynamic(this, &ARunnerGameMode::OnCameraBlendComplete);

			UE_LOG("[RunnerGameMode] Camera transition started with 'Cinematic' preset");
		}
	}

	OnPlayerHealthDecreased.Broadcast(PLAYER_HEALTH);
}

void ARunnerGameMode::OnCameraBlendComplete()
{
	if (GameState.Get()->IsGameStarted() == true)
	{
		return;
	}

	// PlayerController와 CameraManager 가져오기
	UE_LOG("[RunnerGameMode] Camera blend complete! Starting camera shake...");
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		UE_LOG("[RunnerGameMode] ERROR: PlayerController not found");
		return;
	}

	APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager();
	if (!CameraManager)
	{
		UE_LOG("[RunnerGameMode] ERROR: CameraManager not found");
		return;
	}

	// CustomBlend 모디파이어 생성 및 추가
	HeadBobModifier = NewObject<UCameraModifier_CustomBlend>();
	CameraManager->AddCameraModifier(HeadBobModifier);

	// 리듬타
	FVector UpDownOffset(0.0f, 0.0f, 0.2f);
	FQuat PitchRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 15.0f, 0.0f));
	float NoFOVChange = -20.0f;

	HeadBobModifier->SetLooping(true);
	HeadBobModifier->ApplyBlendPreset("BounceCurve", UpDownOffset, PitchRotation, NoFOVChange);

	UE_LOG("[RunnerGameMode] Head bob started: Z=0.2m, Pitch=13.5deg, EaseInOut");
}

void ARunnerGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 사운드 지연 재생 처리
	if (bWaitingForSecondSound)
	{
		SoundDelayTimer += DeltaSeconds;
		if (SoundDelayTimer >= 1.5f)
		{
			// 두 번째 사운드 재생
			USoundManager::GetInstance().PlaySound2D("Data/Sounds/InfinityRunner/GameOverSoundFx.mp3", false, ESoundChannelType::SFX);

			bWaitingForSecondSound = false;
			SoundDelayTimer = 0.0f;
		}
	}
}

void ARunnerGameMode::RestartGame()
{
	Super::RestartGame();

	OnPlayerHealthDecreased.Broadcast(PLAYER_HEALTH);
}

// ────────────────────────────────────────────────────────────────────────────
// 게임 이벤트 (나중에 구현)
// ────────────────────────────────────────────────────────────────────────────

void ARunnerGameMode::OnDecreasePlayerHealth(ACharacter* Player, uint32 DamageAmount)
{
	// 현재 체력은 Lua의 RunnerCharacter.lua에서 관리되므로,
	// Lua에서 직접 현재 체력을 파라미터로 전달받음
	int32 CurrentHealth = static_cast<int32>(DamageAmount); // 여기서는 DamageAmount를 현재 체력으로 사용
	
	// 델리게이트 브로드캐스트 (현재 남은 체력을 전달)
	OnPlayerHealthDecreased.Broadcast(CurrentHealth);
	
	UE_LOG("[RunnerGameMode] Player health decreased to %d!", CurrentHealth);
}

void ARunnerGameMode::OnPlayerDeath(ACharacter* Player)
{
	UE_LOG("[RunnerGameMode] Player Died!");

	// 첫 번째 사운드 즉시 재생
	USoundManager::GetInstance().PlaySound2D("Data/Sounds/InfinityRunner/FailedSoundFx.mp3", false, ESoundChannelType::SFX);

	// 1.5초 후 두 번째 사운드 재생을 위한 타이머 시작
	bWaitingForSecondSound = true;
	SoundDelayTimer = 0.0f;

	// GameState를 GameOver로 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::GameOver);
	}
}

void ARunnerGameMode::OnCoinCollected(int32 CoinValue)
{
	
	// TODO: GameState 업데이트
	CoinScore += CoinValue;
	if (GameState)
	{
		GameState->SetScore(CoinScore);
		UE_LOG("[RunnerGameMode] Coin Collected! Value: %d, CoinScore: %d", CoinValue, CoinScore);
	}
	else
	{
		UE_LOG("[RunnerGameMode] ERROR: GameState is null when collecting coin!");
	}
}

void ARunnerGameMode::OnObstacleAvoided()
{
	UE_LOG("[RunnerGameMode] Obstacle Avoided!");
	// TODO: GameState 업데이트
}

void ARunnerGameMode::OnPlayerJump()
{
	UE_LOG("[RunnerGameMode] Player Jumped!");
	// TODO: GameState 업데이트
}

void ARunnerGameMode::OnGameStateChanged(EGameState OldState, EGameState NewState)
{
	UE_LOG("[RunnerGameMode] Game state changed: %d -> %d", (int32)OldState, (int32)NewState);

	// NotStarted -> Playing 전환 시 카메라 재설정 및 흔들림 종료
	if (OldState == EGameState::NotStarted && NewState == EGameState::Playing)
	{
		UE_LOG("[RunnerGameMode] Game started! Resetting camera and stopping head bob...");

		// PlayerController와 CameraManager 가져오기
		APlayerController* PlayerController = GetPlayerController();
		if (!PlayerController)
		{
			UE_LOG("[RunnerGameMode] ERROR: PlayerController not found");
			return;
		}

		APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager();
		if (!CameraManager)
		{
			UE_LOG("[RunnerGameMode] ERROR: CameraManager not found");
			return;
		}

		APawn* PlayerPawn = PlayerController->GetPawn();
		if (!PlayerPawn)
		{
			UE_LOG("[RunnerGameMode] ERROR: PlayerPawn not found");
			return;
		}

		// 1. HeadBob 모디파이어 제거
		if (HeadBobModifier)
		{
			CameraManager->RemoveCameraModifier(HeadBobModifier);
			HeadBobModifier = nullptr;
			UE_LOG("[RunnerGameMode] Head bob modifier removed");
		}

		// 2. 카메라 타겟을 플레이어로 재설정 (블렌딩 없이 즉시)
		CameraManager->SetViewTarget(PlayerPawn);
		UE_LOG("[RunnerGameMode] Camera target reset to player");
	}
}
