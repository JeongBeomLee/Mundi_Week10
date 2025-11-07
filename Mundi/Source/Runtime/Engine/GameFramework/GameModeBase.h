#pragma once
#include "Info.h"
#include "GameStateBase.h"
// Forward Declarations

class UWorld;
class APawn;
class APlayerController;

// 게임 시작 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnGameStarted);

// 게임 종료 델리게이트 (bVictory)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameEnded, bool);

// 게임 재시작 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnGameRestarted);

// 게임 일시정지 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnGamePaused);

// 게임 재개 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnGameResumed);

// 게임 모드를 관리하는 클래스
// 게임 규칙, 플레이어 스폰, 게임 상태 전환 등을 담당
class AGameModeBase : public AInfo
{
public:
	DECLARE_CLASS(AGameModeBase, AInfo)
	GENERATED_REFLECTION_BODY()

	AGameModeBase();
	virtual ~AGameModeBase() = default;

	// AActor 오버라이드
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason Reason) override;
	virtual void Tick(float DeltaTime) override;

	// 게임 라이프사이클
	virtual void StartGame();
	virtual void EndGame(bool bVictory);
	virtual void RestartGame();
	virtual void PauseGame();
	virtual void ResumeGame();

	// 플레이어 관리
	virtual FVector GetPlayerSpawnLocation() const;
	virtual void SetPlayerSpawnLocation(const FVector& Location);

	/** PlayerController 생성 */
	virtual APlayerController* SpawnPlayerController();

	/** 플레이어를 위한 기본 Pawn 스폰 */
	virtual APawn* SpawnDefaultPawnFor(APlayerController* NewPlayer, const FTransform& SpawnTransform);

	/** 플레이어를 위한 기본 Pawn 클래스 반환 */
	virtual UClass* GetDefaultPawnClassForController(APlayerController* InController);

	/** 플레이어 리스폰 */
	virtual void RestartPlayer(APlayerController* Player);

	/** 메인 플레이어 컨트롤러 반환 */
	APlayerController* GetPlayerController() const { return PlayerController; }

	// GameState 접근자
	AGameStateBase* GetGameState() const { return GameState.Get(); }
	void SetGameState(AGameStateBase* NewGameState);

	// 델리게이트 접근자
	FOnGameRestarted& GetOnGameRestarted() { return OnGameRestarted; }

	// 델리게이트
	FOnGameStarted OnGameStarted;
	FOnGameEnded OnGameEnded;
	FOnGameRestarted OnGameRestarted;
	FOnGamePaused OnGamePaused;
	FOnGameResumed OnGameResumed;

	DECLARE_DUPLICATE(AGameModeBase)
	void DuplicateSubObjects() override;

protected:
	// GameState 참조
	TWeakPtr<AGameStateBase> GameState;

	// 플레이어 스폰 위치
	FVector PlayerSpawnLocation;

	// DefaultPawn, PlayerController 클래스
	UClass* DefaultPawnClass;
	UClass* PlayerControllerClass;

	// 메인 플레이어 컨트롤러 인스턴스
	APlayerController* PlayerController;

	// 게임 시작 여부
	bool bGameStarted;

	// 자동으로 플레이어 스폰 여부
	bool bAutoSpawnPlayer;

	/** 플레이어 초기화 */
	virtual void InitPlayer();
};
