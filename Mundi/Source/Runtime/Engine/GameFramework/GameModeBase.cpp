#include "pch.h"
#include "GameModeBase.h"
#include "GameStateBase.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "World.h"
#include "DeltaTimeManager.h"
#include "PlayerCameraManager.h"
#include "CameraBlendPresetLibrary.h"

// AGameModeBase를 ObjectFactory에 등록
IMPLEMENT_CLASS(AGameModeBase)

// 프로퍼티 등록
BEGIN_PROPERTIES(AGameModeBase)
	// PlayerSpawnLocation 프로퍼티 등록
	ADD_PROPERTY(FVector, PlayerSpawnLocation, "GameMode", true, "플레이어 스폰 위치입니다.")
END_PROPERTIES()

AGameModeBase::AGameModeBase()
	: GameState(nullptr)
	, PlayerSpawnLocation(FVector(0.0f, 0.0f, 100.0f))
	, DefaultPawnClass(nullptr)
	, PlayerControllerClass(nullptr)
	, PlayerController(nullptr)
	, bGameStarted(false)
	, bAutoSpawnPlayer(true)
{
	// 기본 클래스 설정
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = APawn::StaticClass();
}

void AGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG("[GameMode] BeginPlay");

	// 카메라 블렌드 프리셋 라이브러리 초기화
	UCameraBlendPresetLibrary* PresetLibrary = UCameraBlendPresetLibrary::GetInstance();
	if (PresetLibrary)
	{
		PresetLibrary->PrintAllPresets();  // 디버그: 등록된 프리셋 목록 출력
	}

	// World의 PIE 설정을 GameMode에 적용
	if (World)
	{
		// World 설정이 있으면 우선 사용
		if (World->GetDefaultPawnClass())
		{
			DefaultPawnClass = World->GetDefaultPawnClass();
			UE_LOG("[GameMode] DefaultPawnClass set from World: %s", DefaultPawnClass->Name);
		}

		if (World->GetPlayerControllerClass())
		{
			PlayerControllerClass = World->GetPlayerControllerClass();
			UE_LOG("[GameMode] PlayerControllerClass set from World: %s", PlayerControllerClass->Name);
		}

		PlayerSpawnLocation = World->GetPlayerSpawnLocation();
		UE_LOG("[GameMode] PlayerSpawnLocation set from World: (%.1f, %.1f, %.1f)",
			PlayerSpawnLocation.X, PlayerSpawnLocation.Y, PlayerSpawnLocation.Z);
	}

	// GameState 자동 생성 (없는 경우)
	if (!GameState.IsValid())
	{
		if (World)
		{
			AGameStateBase* NewGameState = World->SpawnActor<AGameStateBase>();
			if (NewGameState)
			{
				GameState = TWeakPtr<AGameStateBase>(NewGameState);
				UE_LOG("[GameMode] GameState 자동 생성됨");
			}
		}
	}

	// 플레이어 초기화
	if (bAutoSpawnPlayer)
	{
		InitPlayer();
	}
}

void AGameModeBase::EndPlay(EEndPlayReason Reason)
{
	USoundManager::GetInstance().StopAllSounds();
}

void AGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 추가 게임 로직 필요 시 여기에 구현
}

void AGameModeBase::StartGame()
{
	if (bGameStarted)
	{
		UE_LOG("GameMode: 게임이 이미 시작되었습니다.");
		return;
	}

	// 이전 게임에서 적용되었던 모든 델타타임 효과 취소
	GWorld->GetDeltaTimeManager()->CancelAllEffects();

	// 이전 게임에서 적용되었던 모든 포스트 프로세스 설정 초기화
	PlayerController->GetPlayerCameraManager()->ResetPostProcessSettings();

	bGameStarted = true;

	// GameState 상태 변경
	if (GameState.IsValid())
	{
		GameState.Get()->SetGameState(EGameState::Playing);
	}

	// 델리게이트 브로드캐스트
	OnGameStarted.Broadcast();

	UE_LOG("GameMode: 게임 시작");
}

void AGameModeBase::EndGame(bool bVictory)
{
	if (!bGameStarted)
	{
		return;
	}

	// GameState 상태 변경
	if (GameState.IsValid())
	{
		if (bVictory)
		{
			GameState.Get()->SetGameState(EGameState::Victory);
		}
		else
		{
			GameState.Get()->SetGameState(EGameState::GameOver);
		}
	}

	// 델리게이트 브로드캐스트
	OnGameEnded.Broadcast(bVictory);
}

void AGameModeBase::RestartGame()
{
	// GameState 초기화
	if (GameState.IsValid())
	{
		GameState.Get()->SetGameState(EGameState::NotStarted);
		GameState.Get()->SetScore(0);
		GameState.Get()->ResetTimer();
	}

	bGameStarted = false;

	// 델리게이트 브로드캐스트
	OnGameRestarted.Broadcast();

	UE_LOG("GameMode: 게임 재시작");

	// 자동으로 게임 시작
	StartGame();
}

void AGameModeBase::PauseGame()
{
	if (!bGameStarted)
	{
		UE_LOG("GameMode: 게임이 시작되지 않았습니다.");
		return;
	}
	bGameStarted = false;
	// GameState 상태 변경
	if (GameState.IsValid())
	{
		GameState.Get()->SetGameState(EGameState::Paused);
	}

	// 델리게이트 브로드캐스트
	OnGamePaused.Broadcast();

	UE_LOG("GameMode: 게임 일시정지!");
}

void AGameModeBase::ResumeGame()
{
	bGameStarted = true;
	if (!bGameStarted)
	{
		UE_LOG("GameMode: 게임이 시작되지 않았습니다.");
		return;
	}

	// GameState 상태 변경
	if (GameState.IsValid())
	{
		GameState.Get()->SetGameState(EGameState::Playing);
	}

	// 델리게이트 브로드캐스트
	OnGameResumed.Broadcast();

	UE_LOG("GameMode: 게임 재개!");
}

FVector AGameModeBase::GetPlayerSpawnLocation() const
{
	return PlayerSpawnLocation;
}

void AGameModeBase::SetPlayerSpawnLocation(const FVector& Location)
{
	PlayerSpawnLocation = Location;
}

void AGameModeBase::SetGameState(AGameStateBase* NewGameState)
{
	if (NewGameState)
	{
		GameState = TWeakPtr<AGameStateBase>(NewGameState);
		UE_LOG("GameMode: GameState 설정됨");
	}
}

void AGameModeBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	// GameMode 복제 시 게임 상태 초기화
	bGameStarted = false;
}

// ────────────────────────────────────────────────────────────────────────────
// 플레이어 초기화
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::InitPlayer()
{
	UE_LOG("[GameMode] Initializing Player...");

	// 1. PlayerController 생성
	PlayerController = SpawnPlayerController();

	if (!PlayerController)
	{
		UE_LOG("[GameMode] ERROR: Failed to spawn PlayerController!");
		return;
	}

	// 2. Pawn 스폰 위치 결정
	FTransform SpawnTransform;
	SpawnTransform.Translation = PlayerSpawnLocation;
	SpawnTransform.Rotation = FQuat::Identity();
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

	// 3. Pawn 스폰 및 빙의
	APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, SpawnTransform);

	if (SpawnedPawn)
	{
		UE_LOG("[GameMode] Player initialized successfully!");
		UE_LOG("  PlayerController: %s", PlayerController->GetName().ToString());
		UE_LOG("  Pawn: %s", SpawnedPawn->GetName().ToString());
	}
	else
	{
		UE_LOG("[GameMode] ERROR: Failed to spawn Pawn!");
	}
}

APlayerController* AGameModeBase::SpawnPlayerController()
{
	if (!World || !PlayerControllerClass)
	{
		return nullptr;
	}

	// PlayerController 스폰 (위치는 중요하지 않음)
	AActor* NewActor = World->SpawnActor(PlayerControllerClass);
	APlayerController* NewController = Cast<APlayerController>(NewActor);

	if (NewController)
	{
		UE_LOG("[GameMode] PlayerController spawned: %s", NewController->GetName().ToString());
	}

	return NewController;
}

APawn* AGameModeBase::SpawnDefaultPawnFor(APlayerController* NewPlayer, const FTransform& SpawnTransform)
{
	if (!NewPlayer || !World)
	{
		return nullptr;
	}

	// Pawn 클래스 결정
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);

	if (!PawnClass)
	{
		UE_LOG("[GameMode] ERROR: No DefaultPawnClass set!");
		return nullptr;
	}

	// Pawn 스폰
	AActor* NewActor = World->SpawnActor(PawnClass, SpawnTransform);

	APawn* NewPawn = Cast<APawn>(NewActor);

	if (NewPawn)
	{
		UE_LOG("[GameMode] Pawn spawned: %s at (%.1f, %.1f, %.1f)",
			   NewPawn->GetName().ToString(),
			   SpawnTransform.Translation.X,
			   SpawnTransform.Translation.Y,
			   SpawnTransform.Translation.Z);

		// PlayerController가 Pawn을 빙의
		NewPlayer->Possess(NewPawn);
	}

	return NewPawn;
}

UClass* AGameModeBase::GetDefaultPawnClassForController(APlayerController* InController)
{
	return DefaultPawnClass;
}

void AGameModeBase::RestartPlayer(APlayerController* Player)
{
	if (!Player)
	{
		return;
	}

	UE_LOG("[GameMode] Restarting player...");

	// 기존 Pawn 제거
	APawn* OldPawn = Player->GetPawn();
	if (OldPawn)
	{
		Player->UnPossess();
		World->DestroyActor(OldPawn);  // UnPossess 전에 저장한 OldPawn 사용
		//UE_LOG("[GameMode] Old pawn destroyed: %s", OldPawn->GetName().ToString());
	}

	// 새 Pawn 스폰
	FTransform SpawnTransform;
	SpawnTransform.Translation = PlayerSpawnLocation;
	SpawnTransform.Rotation = FQuat::Identity();
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

	APawn* NewPawn = SpawnDefaultPawnFor(Player, SpawnTransform);

	if (NewPawn)
	{
		// PIE 모드에서 런타임 스폰된 Pawn은 BeginPlay를 수동으로 호출해야 함
		NewPawn->BeginPlay();
		UE_LOG("[GameMode] Player restarted!");
	}
}
