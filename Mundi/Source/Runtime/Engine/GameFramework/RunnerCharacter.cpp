// ────────────────────────────────────────────────────────────────────────────
// RunnerCharacter.cpp
// Runner 게임용 캐릭터 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "RunnerCharacter.h"
#include "CharacterMovementComponent.h"
#include "InputComponent.h"
#include "CameraComponent.h"
#include "World.h"
#include "GameModeBase.h"
#include "CollisionComponent/BoxComponent.h"
#include "ObjectFactory.h"
#include "SpringArmComponent.h"
#include "ParticleComponent.h"

IMPLEMENT_CLASS(ARunnerCharacter)

BEGIN_PROPERTIES(ARunnerCharacter)
	MARK_AS_SPAWNABLE("RunnerCharacter", "Runner 게임 전용 캐릭터 클래스입니다. Lua 스크립트로 제어됩니다.")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

ARunnerCharacter::ARunnerCharacter()
	: CollisionBox(nullptr)
	, CameraComponent(nullptr)
	, CameraOffset(-5.0f, 0.0f, 5.0f)  // 플레이어 뒤쪽 5미터, 위쪽 5미터
{

	 SpriteComponent = CreateDefaultSubobject<UParticleComponent>("ParticleComp");
	 if (SpriteComponent)
	 {
		 SpriteComponent->SetOwner(this);

		 // MeshComponent가 있다면 그것에 부착, 없으면 RootComponent에 부착
		 if (MeshComponent)
		 {
			 SpriteComponent->SetupAttachment(MeshComponent);
		 }
		 else
		 {
			 SpriteComponent->SetupAttachment(GetRootComponent());
		 }
	 }
	// BoxComponent 생성 및 설정
	CollisionBox = CreateDefaultSubobject<UBoxComponent>("CollisionBox");
	if (CollisionBox)
	{
		CollisionBox->SetOwner(this);

		// MeshComponent가 있다면 그것에 부착, 없으면 RootComponent에 부착
		if (MeshComponent)
		{
			CollisionBox->SetupAttachment(MeshComponent);
		}
		else
		{
			CollisionBox->SetupAttachment(GetRootComponent());
		}

		CollisionBox->SetBoxExtent(FVector(0.50f, 0.50f, 0.50f));
	}
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	if (SpringArmComponent)
	{
		SpringArmComponent->SetOwner(this);
		SpringArmComponent->SetupAttachment(GetRootComponent());

		// 기본 설정값
		SpringArmComponent->SetTargetArmLength(5.0f);
		SpringArmComponent->SetSocketOffset(FVector(0, 0, 2.5))  ;  // 캐릭터 머리 위
		SpringArmComponent->SetEnableCameraLag(true) ;
		SpringArmComponent->SetCameraLagSpeed(10.0f);
		SpringArmComponent->SetDoCollisionTest(true);
		SpringArmComponent->SetProbeSize(12.0f);
	}

	// CameraComponent 생성 및 설정
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	if (CameraComponent)
	{
		CameraComponent->SetOwner(this);
		if (SpringArmComponent)
			CameraComponent->SetupAttachment(SpringArmComponent);
		else
			CameraComponent->SetupAttachment(GetRootComponent());
		CameraComponent->SetFOV(60.0f);
	}
	bTickInEditor = true;
	// 모든 이동 설정은 Lua에서 관리 (RunnerCharacter.lua의 Config)
}

ARunnerCharacter::~ARunnerCharacter()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void ARunnerCharacter::BeginPlay()
{
	UE_LOG("[RunnerCharacter] BeginPlay - Attaching Lua script first");

	// Lua 스크립트 자동 연결 (Super::BeginPlay() 전에!)
	FLuaLocalValue LuaLocalValue;
	LuaLocalValue.MyActor = this;
	LuaLocalValue.GameMode = World ? World->GetGameMode() : nullptr;

	try
	{
		UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, "RunnerCharacter.lua");
		UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, "MapGenerator.lua");
		UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, "ObstacleGenerator.lua");
		UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, "CoinGenerator.lua");
		UE_LOG("[RunnerCharacter] Auto-attached Lua script: RunnerCharacter.lua, MapGenerator.lua, ObstacleGenerator.lua");
	}
	catch (std::exception& e)
	{
		UE_LOG("[RunnerCharacter] ERROR: Lua script attachment failed: %s", e.what());
		// 스크립트 로드 실패 시에도 프로그램은 계속 실행됨
	}

	// 이제 Super::BeginPlay() 호출 → Actor::BeginPlay()에서 Lua BeginPlay 호출됨
	Super::BeginPlay();

	UE_LOG("[RunnerCharacter] BeginPlay complete");
}

void ARunnerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SpringArmComponent->TickComponent(DeltaSeconds);
	// 카메라 위치 업데이트
	//UpdateCameraPosition();
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩
// ────────────────────────────────────────────────────────────────────────────

void ARunnerCharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	// 모든 입력 바인딩은 Lua에서 처리 (RunnerCharacter.lua의 SetupInputBindings)
	// Lua에서 A/D (좌우 이동), Space (점프) 등을 바인딩함
	UE_LOG("[RunnerCharacter] SetupPlayerInputComponent called - All input bindings handled in Lua");
}

// ────────────────────────────────────────────────────────────────────────────
// 유틸리티 함수
// ────────────────────────────────────────────────────────────────────────────

FVector ARunnerCharacter::GetUpDirection() const
{
	// Actor의 회전 상태에서 로컬 Z축(위쪽)을 월드 좌표로 변환
	FQuat Rotation = GetActorRotation();
	FVector LocalUp(0.0f, 0.0f, 1.0f);  // 로컬 좌표계의 Z축
	FVector WorldUp = Rotation.RotateVector(LocalUp);
	return WorldUp;
}

FVector ARunnerCharacter::GetRightDirection() const
{
	FVector ForwardDir = GetForwardDirection();
	FVector UpDir = GetUpDirection();

	// 전진 방향과 위쪽에 수직인 벡터 = 우측 방향
	FVector RightDir = FVector::Cross(UpDir, ForwardDir);

	// 만약 평행하면 다른 축 사용
	if (RightDir.SizeSquared() < 0.01f)
	{
		RightDir = FVector::Cross(UpDir, FVector(0.0f, 1.0f, 0.0f));
	}

	FVector Result = RightDir.GetNormalized();

	return Result;
}

FVector ARunnerCharacter::GetForwardDirection() const
{
	// Actor의 회전 상태에서 로컬 X축(전진)을 월드 좌표로 변환
	FQuat Rotation = GetActorRotation();
	FVector LocalForward(1.0f, 0.0f, 0.0f);  // 로컬 좌표계의 X축
	FVector WorldForward = Rotation.RotateVector(LocalForward);
	return WorldForward;
}

// ────────────────────────────────────────────────────────────────────────────
// 중력 방향 제어
// ────────────────────────────────────────────────────────────────────────────

void ARunnerCharacter::SetGravityDirection(const FVector& NewGravityDir)
{
	if (CharacterMovement)
	{
		CharacterMovement->SetGravityDirection(NewGravityDir);
		UE_LOG("[RunnerCharacter] Gravity direction changed to: (%.2f, %.2f, %.2f)",
			NewGravityDir.X, NewGravityDir.Y, NewGravityDir.Z);
	}
}

FVector ARunnerCharacter::GetGravityDirection() const
{
	if (CharacterMovement)
	{
		return CharacterMovement->GetGravityDirection();
	}
	return FVector(0.0f, 0.0f, -1.0f);
}

// ────────────────────────────────────────────────────────────────────────────
// 카메라 제어
// ────────────────────────────────────────────────────────────────────────────

void ARunnerCharacter::UpdateCameraPosition()
{
	if (!CameraComponent)
		return;

	// TODO: SpringArm
	// 캐릭터의 로컬 좌표계 기준으로 카메라 오프셋 적용
	FQuat CharacterRotation = GetActorRotation();
	FVector WorldOffset = CharacterRotation.RotateVector(CameraOffset);
	FVector CameraLocation = GetActorLocation() + WorldOffset;

	CameraComponent->SetWorldLocation(CameraLocation);
	CameraComponent->SetWorldRotation(CharacterRotation);
}

// ────────────────────────────────────────────────────────────────────────────
// 복제 (Duplication)
// ────────────────────────────────────────────────────────────────────────────

void ARunnerCharacter::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

