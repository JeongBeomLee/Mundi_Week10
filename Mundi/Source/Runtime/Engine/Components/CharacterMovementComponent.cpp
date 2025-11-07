// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.cpp
// Character 이동 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CharacterMovementComponent.h"
#include "Character.h"
#include "RunnerCharacter.h"
#include "CollisionComponent/BoxComponent.h"
#include "GravityWall.h"
#include "RunnerGameMode.h"
#include "World.h"

IMPLEMENT_CLASS(UCharacterMovementComponent)

BEGIN_PROPERTIES(UCharacterMovementComponent)
	ADD_PROPERTY(float, MaxWalkSpeed, "Movement", true, "최대 걷기 속도 (cm/s)")
	ADD_PROPERTY(float, JumpZVelocity, "Movement", true, "점프 초기 속도 (cm/s)")
	ADD_PROPERTY(float, GravityScale, "Movement", true, "중력 스케일 (1.0 = 기본 중력)")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCharacterMovementComponent::UCharacterMovementComponent()
	: CharacterOwner(nullptr)
	, Velocity(FVector())
	, PendingInputVector(FVector())
	, MovementMode(EMovementMode::Falling)
	, TimeInAir(0.0f)
	, bIsJumping(false)
	, bIsRotating(false)
	// 이동 설정
	, MaxWalkSpeed(30.0f)           // 0.3 m/s
	, MaxAcceleration(2048.0f)       // 20.48 m/s²
	, GroundFriction(8.0f)
	, AirControl(0.05f)
	, BrakingDeceleration(2048.0f)
	// 중력 설정
	, GravityScale(1.0f)
	, GravityDirection(0.0f, 0.0f, -1.0f) // 기본값: 아래 방향
	// 점프 설정
	, JumpZVelocity(420.0f)          // 4.2 m/s
	, MaxAirTime(2.0f)
	, bCanJump(true)
{
	bCanEverTick = true;
}

UCharacterMovementComponent::~UCharacterMovementComponent()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Owner를 Character로 캐스팅
	CharacterOwner = Cast<ACharacter>(GetOwner());
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	// 사망한 상태면 조기 리턴
	UWorld* World = CharacterOwner->GetWorld();
	ARunnerGameMode* GameMode = nullptr;
	if (World)
	{
		GameMode = Cast<ARunnerGameMode>(World->GetGameMode());
		if (GameMode &&
			GameMode->GetGameState()->GetGameState() == EGameState::GameOver)
		{
			return;
		}
	}
	
	if (!CharacterOwner)
	{
		return;
	}

	// 1. 속도 업데이트 (입력, 마찰, 가속)
	UpdateVelocity(DeltaTime);

	// 2. 중력 적용
	ApplyGravity(DeltaTime);

	// 3. 위치 업데이트
	MoveUpdatedComponent(DeltaTime);

	// 4. 지면 체크
	bool bWasGrounded = IsGrounded();
	bool bIsNowGrounded = CheckGround();

	// 5. 이동 모드 업데이트
	if (bIsNowGrounded && !bWasGrounded)
	{
		// 착지 - 중력 방향의 속도 성분 제거
		SetMovementMode(EMovementMode::Walking);
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity -= GravityDirection * VerticalSpeed;
		TimeInAir = 0.0f;
		bIsJumping = false;
	}
	else if (!bIsNowGrounded && bWasGrounded)
	{
		// 낙하 시작
		SetMovementMode(EMovementMode::Falling);
	}

	// 6. 공중 시간 체크
	if (IsFalling())
	{
		TimeInAir += DeltaTime;

		// 너무 오래 공중에 있으면 GameOver 처리
		if (TimeInAir > MaxAirTime)
		{
			if (GameMode)
			{
				GameMode->OnPlayerDeath(CharacterOwner);
			}
			TimeInAir = 0.0f;
		}
	}

	// 입력 초기화
	PendingInputVector = FVector();
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 함수
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::AddInputVector(FVector WorldDirection, float ScaleValue)
{
	if (ScaleValue == 0.0f || WorldDirection.SizeSquared() == 0.0f)
	{
		return;
	}

	// 중력 방향에 수직인 평면으로 입력 제한
	// 입력 벡터에서 중력 방향 성분을 제거 (투영 후 빼기)
	float DotWithGravity = FVector::Dot(WorldDirection, GravityDirection);
	FVector HorizontalDirection = WorldDirection - (GravityDirection * DotWithGravity);

	// 방향 벡터가 0이면 무시
	if (HorizontalDirection.SizeSquared() < 0.0001f)
	{
		return;
	}

	FVector NormalizedDirection = HorizontalDirection.GetNormalized();
 	PendingInputVector += NormalizedDirection * ScaleValue;
}

bool UCharacterMovementComponent::Jump()
{
	// 점프 가능 조건 체크
	if (!bCanJump || !IsGrounded())
	{
		return false;
	}

	// 중력 반대 방향으로 점프 속도 적용
	FVector JumpVelocity = GravityDirection * -1.0f * JumpZVelocity;
	Velocity += JumpVelocity;

	// 이동 모드 변경
	SetMovementMode(EMovementMode::Falling);
	bIsJumping = true;

	return true;
}

void UCharacterMovementComponent::StopJumping()
{
	// 점프 키를 뗐을 때 상승 속도를 줄임
	// 중력 반대 방향으로의 속도만 감소
	FVector UpDirection = GravityDirection * -1.0f;
	float UpwardSpeed = FVector::Dot(Velocity, UpDirection);

	if (bIsJumping && UpwardSpeed > 0.0f)
	{
		// 상승 속도 성분만 감소
		Velocity -= UpDirection * (UpwardSpeed * 0.5f);
	}
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMode)
{
	if (MovementMode == NewMode)
	{
		return;
	}

	EMovementMode PrevMode = MovementMode;
	MovementMode = NewMode;

	// 모드 전환 시 처리
	if (MovementMode == EMovementMode::Walking)
	{
		// 착지 시 중력 방향의 속도 성분 제거
		FVector UpDirection = GravityDirection * -1.0f;
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity -= GravityDirection * VerticalSpeed;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 이동 로직
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::UpdateVelocity(float DeltaTime)
{
	// 회전 중에는 입력 처리 안 함
	if (bIsRotating)
	{
		return;
	}

	if (PendingInputVector.SizeSquared() > 0.0f)
	{
		// 입력이 있으면 가속
		FVector InputDirection = PendingInputVector.GetNormalized();
		float CurrentControl = IsGrounded() ? 1.0f : AirControl;
		float AccelRate = MaxAcceleration * CurrentControl;

		// 목표 속도
		FVector TargetVelocity = InputDirection * MaxWalkSpeed;

		// 중력 방향에 수직인 평면으로 속도 분리
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		FVector HorizontalVelocity = Velocity - (GravityDirection * VerticalSpeed);

		float TargetVerticalSpeed = FVector::Dot(TargetVelocity, GravityDirection);
		FVector HorizontalTarget = TargetVelocity - (GravityDirection * TargetVerticalSpeed);

		// 가속도 적용
		FVector Delta = HorizontalTarget - HorizontalVelocity;
		float DeltaSize = Delta.Size();

		if (DeltaSize > 0.0f)
		{
			FVector AccelDir = Delta / DeltaSize;
			float AccelAmount = FMath::Min(DeltaSize, AccelRate * DeltaTime);

			HorizontalVelocity += AccelDir * AccelAmount;
		}

		// 최대 속도 제한
		if (HorizontalVelocity.Size() > MaxWalkSpeed)
		{
			HorizontalVelocity = HorizontalVelocity.GetNormalized() * MaxWalkSpeed;
		}

		// 수평 속도와 수직 속도 결합
		Velocity = HorizontalVelocity + (GravityDirection * VerticalSpeed);
	}
	else if (IsGrounded())
	{
		// 입력이 없으면 마찰 적용 (지면에서만)
		// 중력 방향에 수직인 속도 성분만 추출
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		FVector HorizontalVelocity = Velocity - (GravityDirection * VerticalSpeed);
		float CurrentSpeed = HorizontalVelocity.Size();

		if (CurrentSpeed > 0.0f)
		{
			float FrictionAmount = GroundFriction * BrakingDeceleration * DeltaTime;
			float NewSpeed = FMath::Max(0.0f, CurrentSpeed - FrictionAmount);
			float SpeedRatio = NewSpeed / CurrentSpeed;

			HorizontalVelocity *= SpeedRatio;
		}

		// 수평 속도와 수직 속도 결합
		Velocity = HorizontalVelocity + (GravityDirection * VerticalSpeed);
	}
}

void UCharacterMovementComponent::ApplyGravity(float DeltaTime)
{
	// 회전 중에는 중력 적용 안 함
	if (bIsRotating)
	{
		return;
	}

	// 지면에 있으면 중력 적용 안 함
	if (IsGrounded())
	{
		return;
	}

	// 중력 가속도 적용 (방향 벡터 사용)
	float GravityMagnitude = DefaultGravity * GravityScale;
	FVector GravityVector = GravityDirection * GravityMagnitude;
	Velocity += GravityVector * DeltaTime;

	// 최대 낙하 속도 제한 (터미널 속도)
	// 중력 방향으로의 속도 성분을 체크
	float VelocityInGravityDir = FVector::Dot(Velocity, GravityDirection);
	constexpr float MaxFallSpeed = 40.0f; // 40 m/s
	if (VelocityInGravityDir > MaxFallSpeed)
	{
		// 중력 방향 속도 성분만 제한
		FVector GravityComponent = GravityDirection * VelocityInGravityDir;
		FVector OtherComponent = Velocity - GravityComponent;
		Velocity = OtherComponent + GravityDirection * MaxFallSpeed;
	}
}

void UCharacterMovementComponent::SetGravityDirection(const FVector& NewDirection)
{
	// 벡터를 정규화하여 저장
	if (NewDirection.SizeSquared() > 0.0f)
	{
		GravityDirection = NewDirection.GetNormalized();
	}
	else
	{
		// 유효하지 않은 방향이면 기본값으로
		GravityDirection = FVector(0.0f, 0.0f, -1.0f);
	}
}

void UCharacterMovementComponent::SetIsRotating(bool bInIsRotating)
{
	bIsRotating = bInIsRotating;

	// 회전 시작 시: 현재 중력 방향의 속도 성분을 제거 (낙하 중이었어도 멈춤)
	if (bIsRotating)
	{
		// 중력 방향의 속도 성분 계산
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);

		// 중력 방향 속도 제거 (수평 속도만 유지)
		Velocity -= GravityDirection * VerticalSpeed;
	}
}

void UCharacterMovementComponent::MoveUpdatedComponent(float DeltaTime)
{
	if (Velocity.SizeSquared() == 0.0f)
	{
		return;
	}

	// 새 위치 계산
	FVector CurrentLocation = CharacterOwner->GetActorLocation();
	FVector Delta = Velocity * DeltaTime;
	FVector NewLocation = CurrentLocation + Delta;

	// 위치 업데이트
	CharacterOwner->SetActorLocation(NewLocation);

	// 벽 충돌 체크
	if (CheckWallCollision())
	{
		// 이전 위치로 복원
		CharacterOwner->SetActorLocation(CurrentLocation);

		// 중력 방향에 수직인 속도만 초기화 (중력/점프 영향은 유지)
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity = GravityDirection * VerticalSpeed;
	}
}

bool UCharacterMovementComponent::CheckWallCollision()
{
	if (!CharacterOwner)
	{
		return false;
	}

	// RunnerCharacter의 CollisionBox 가져오기
	ARunnerCharacter* RunnerChar = Cast<ARunnerCharacter>(CharacterOwner);
	if (!RunnerChar)
	{
		return false;
	}

	UBoxComponent* CollisionBox = RunnerChar->GetCollisionBox();
	if (!CollisionBox)
	{
		return false;
	}

	// 중력 방향에 수직인 이동 방향 계산
	float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
	FVector HorizontalVelocity = Velocity - (GravityDirection * VerticalSpeed);
	if (HorizontalVelocity.SizeSquared() < 0.01f)
	{
		// 이동하지 않으면 충돌 검사 안 함
		return false;
	}

	FVector MovementDirection = HorizontalVelocity.GetNormalized();

	// CollisionBox의 오버랩 정보 확인
	for (const FOverlapInfo& Info : CollisionBox->OverlapInfos)
	{
		// GravityWall과 충돌했는지 확인
		AGravityWall* Wall = Cast<AGravityWall>(Info.OtherActor);
		if (Wall)
		{
			FVector WallNormal = Wall->GetWallNormal();

			// 벽 Normal과 이동 방향의 내적 계산
			// Normal이 이동 방향과 반대면 내적이 음수 (벽이 앞을 가로막음)
			float DotProduct = FVector::Dot(WallNormal, MovementDirection);
			if (DotProduct < -0.3f)
			{
				// 측면 충돌 체크: WallNormal이 중력 방향과 거의 수직인가?
				float GravityDot = FVector::Dot(WallNormal, GravityDirection);
				if (FMath::Abs(GravityDot) < 0.3f)
				{
					// 측면 충돌! Lua 콜백 호출
					if (WallCollisionLuaCallback.valid())
					{
						WallCollisionLuaCallback(WallNormal);
					}
				}

				return true;
			}
		}
	}

	return false;
}

void UCharacterMovementComponent::SetOnWallCollisionCallback(sol::function Callback)
{
	WallCollisionLuaCallback = Callback;
}

bool UCharacterMovementComponent::CheckGround()
{
	if (!CharacterOwner)
	{
		return false;
	}

	// RunnerCharacter의 CollisionBox 가져오기
	ARunnerCharacter* RunnerChar = Cast<ARunnerCharacter>(CharacterOwner);
	if (!RunnerChar)
	{
		return false;
	}

	UBoxComponent* CollisionBox = RunnerChar->GetCollisionBox();
	if (!CollisionBox)
	{
		return false;
	}

	// CollisionBox의 오버랩 정보 확인
	for (const FOverlapInfo& Info : CollisionBox->OverlapInfos)
	{
		// GravityWall과 충돌했는지 확인
		AGravityWall* Wall = Cast<AGravityWall>(Info.OtherActor);
		if (Wall)
		{
			FVector WallNormal = Wall->GetWallNormal();

			// WallNormal과 중력 방향의 내적 계산
			// Normal이 중력 방향과 반대면 바닥 (내적이 충분히 음수)
			float DotProduct = FVector::Dot(WallNormal, GravityDirection);
			if (DotProduct <= -0.9f)
			{
				// 중력 방향으로의 속도 성분 계산
				float VelocityInGravityDir = FVector::Dot(Velocity, GravityDirection);

				// 중력 반대 방향으로 이동 중이면 지면으로 인식하지 않음 (점프 허용)
				if (VelocityInGravityDir < 0.0f)
				{
					return false;
				}

				// 중력 방향으로 이동 중이거나 정지 상태일 때만 바닥에 스냅
				if (VelocityInGravityDir >= 0.0f)
				{
					// 바닥의 표면 위치 계산
					UBoxComponent* WallBox = Wall->GetBoxComponent();
					if (WallBox)
					{
						FVector WallLocation = Wall->GetActorLocation();
						FVector WallScale = Wall->GetActorScale();
						FVector WallBoxExtent = WallBox->GetBoxExtent();

						// WallNormal 방향으로의 Wall 크기 계산
						float WallExtentInNormalDir =
							FMath::Abs(WallBoxExtent.X * WallScale.X * WallNormal.X) +
							FMath::Abs(WallBoxExtent.Y * WallScale.Y * WallNormal.Y) +
							FMath::Abs(WallBoxExtent.Z * WallScale.Z * WallNormal.Z);

						// 바닥 표면 위치 = Wall 중심 + (Normal 방향 크기)
						FVector FloorSurface = WallLocation + WallNormal * WallExtentInNormalDir;

						// 캐릭터의 BoxComponent 크기 계산
						FVector CharBoxExtent = CollisionBox->GetBoxExtent();
						FVector CharScale = CharacterOwner->GetActorScale();

						// 중력 반대 방향으로의 캐릭터 크기
						float CharExtentInGravityDir =
							FMath::Abs(CharBoxExtent.X * CharScale.X * GravityDirection.X) +
							FMath::Abs(CharBoxExtent.Y * CharScale.Y * GravityDirection.Y) +
							FMath::Abs(CharBoxExtent.Z * CharScale.Z * GravityDirection.Z);

						// 현재 캐릭터 위치를 가져옴
						FVector CurrentCharLocation = CharacterOwner->GetActorLocation();

						// 목표 바닥 위치 = 바닥 표면 - (중력 방향 × 캐릭터 크기)
						FVector TargetFloorPos = FloorSurface - GravityDirection * CharExtentInGravityDir;

						// 중력 방향으로 투영한 현재 위치
						float CurrentPosInGravityDir = FVector::Dot(CurrentCharLocation, GravityDirection);
						float TargetPosInGravityDir = FVector::Dot(TargetFloorPos, GravityDirection);

						// 중력 방향으로만 위치 조정 (수평 위치는 유지)
						FVector PositionAdjustment = GravityDirection * (TargetPosInGravityDir - CurrentPosInGravityDir);
						FVector NewCharLocation = CurrentCharLocation + PositionAdjustment;
						CharacterOwner->SetActorLocation(NewCharLocation);

						// 중력 방향 속도 성분 초기화
						FVector GravityComponent = GravityDirection * VelocityInGravityDir;
						Velocity -= GravityComponent;
					}
				}
				return true;
			}
		}
	}
	return false;
}
