// ────────────────────────────────────────────────────────────────────────────
// Character.cpp
// Character 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Character.h"
#include "CharacterMovementComponent.h"
#include "SceneComponent.h"
#include "StaticMeshComponent.h"
#include "InputComponent.h"
#include "ObjectFactory.h"
#include "GameModeBase.h"
#include "World.h"

IMPLEMENT_CLASS(ACharacter)

BEGIN_PROPERTIES(ACharacter)
	MARK_AS_SPAWNABLE("Character", "이동, 점프 등의 기능을 가진 Character 클래스입니다.")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

ACharacter::ACharacter()
	: CharacterMovement(nullptr)
	, MeshComponent(nullptr)
	, StaticMeshComponent(nullptr)
	, bIsCrouched(false)
	, CrouchedHeightRatio(0.5f)

{
	// CharacterMovementComponent 생성
	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovement");
	if (CharacterMovement)
	{
		CharacterMovement->SetOwner(this);
	}

	// Mesh 컴포넌트 생성 (일단 빈 SceneComponent)
	MeshComponent = CreateDefaultSubobject<USceneComponent>("MeshComponent");
	if (MeshComponent)
	{
		MeshComponent->SetOwner(this);
		SetRootComponent(MeshComponent);
	}

	// StaticMesh 컴포넌트 생성 (시각적 표현용)
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	if (StaticMeshComponent)
	{
		StaticMeshComponent->SetOwner(this);
		StaticMeshComponent->SetupAttachment(MeshComponent);
		UE_LOG("[Character] StaticMeshComponent created!");
	}
	
}

ACharacter::~ACharacter()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACharacter::Tick(float DeltaSeconds)
{
	// 게임이 시작되지 않았으면 Lua Tick을 실행하지 않음
	if (!bGameStarted)
	{
		// 입력 벡터 초기화
		if (CharacterMovement)
		{
			CharacterMovement->ConsumeInputVector();
		}
		// Super::Tick 호출 안 함 = Lua Tick 실행 안 됨
		return;
	}

	// 게임 시작됨 - 정상 Tick (Lua Tick 포함)
	Super::Tick(DeltaSeconds);
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	// 입력 바인딩은 파생 클래스(예: RunnerCharacter)의 Lua 스크립트에서 처리
	// C++에서는 기본 바인딩을 하지 않음
	UE_LOG("[Character] SetupPlayerInputComponent called - input bindings should be done in Lua");
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::AddMovementInput(FVector WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
		CharacterMovement->AddInputVector(WorldDirection, ScaleValue);
	}
}

void ACharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// Forward 방향으로 이동
		FVector Forward = GetActorForward();
		AddMovementInput(Forward, Value);
	}
}

void ACharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// Right 방향으로 이동
		FVector Right = GetActorRight();
		AddMovementInput(Right, Value);
	}
}

void ACharacter::Turn(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerYawInput(Value);
	}
}

void ACharacter::LookUp(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerPitchInput(Value);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Character 동작
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->Jump();
	}
}

void ACharacter::StopJumping()
{
	if (CharacterMovement)
	{
		CharacterMovement->StopJumping();
	}
}

bool ACharacter::CanJump() const
{
	return CharacterMovement && CharacterMovement->bCanJump && IsGrounded();
}

void ACharacter::Crouch()
{
	if (bIsCrouched)
	{
		return;
	}

	bIsCrouched = true;

	// 웅크릴 때 이동 속도 감소 (옵션)
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed *= CrouchedHeightRatio;
	}
}

void ACharacter::UnCrouch()
{
	if (!bIsCrouched)
	{
		return;
	}

	bIsCrouched = false;

	// 이동 속도 복원
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed /= CrouchedHeightRatio;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 상태 조회
// ────────────────────────────────────────────────────────────────────────────

FVector ACharacter::GetVelocity() const
{
	if (CharacterMovement)
	{
		return CharacterMovement->GetVelocity();
	}

	return FVector();
}

bool ACharacter::IsGrounded() const
{
	return CharacterMovement && CharacterMovement->IsGrounded();
}

bool ACharacter::IsFalling() const
{
	return CharacterMovement && CharacterMovement->IsFalling();
}

