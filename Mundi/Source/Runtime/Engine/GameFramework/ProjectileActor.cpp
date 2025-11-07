// ────────────────────────────────────────────────────────────────────────────
// ProjectileActor.cpp
// 발사체 Actor 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "ProjectileActor.h"
#include "StaticMeshComponent.h"
#include "ProjectileMovementComponent.h"
#include "CollisionComponent/SphereComponent.h"
#include "ObjectFactory.h"
#include "World.h"
#include "GameModeBase.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

IMPLEMENT_CLASS(AProjectileActor)

BEGIN_PROPERTIES(AProjectileActor)
	MARK_AS_SPAWNABLE("프로젝타일", "발사체 Actor입니다. ProjectileMovement를 사용합니다.")
END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

AProjectileActor::AProjectileActor()
	: MeshComponent(nullptr)
	, ProjectileMovement(nullptr)
	, CollisionComponent(nullptr)
	, ProjectileLifespan(3.0f)
	, CurrentLifetime(0.0f)
{
	Name = "Projectile Actor";

	// CollisionComponent 생성 (루트 컴포넌트)
	CollisionComponent = CreateDefaultSubobject<USphereComponent>("CollisionComponent");
	if (CollisionComponent)
	{
		CollisionComponent->SetOwner(this);
		CollisionComponent->SetSphereRadius(2.5f); // 반지름 25cm
		RootComponent = CollisionComponent;

		// 충돌 이벤트는 Lua에서 처리하도록 바인딩 제거
		// Lua의 OnOverlap이 자동으로 연결됩니다
	}

	// MeshComponent 생성
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComponent");
	if (MeshComponent)
	{
		MeshComponent->SetOwner(this);
		if (CollisionComponent)
		{
			MeshComponent->SetupAttachment(CollisionComponent);
		}
	}

	// ProjectileMovement 생성
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	if (ProjectileMovement)
	{
		ProjectileMovement->SetOwner(this);
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);
		ProjectileMovement->SetInitialSpeed(2000.0f); // 기본 속도 20m/s
		ProjectileMovement->SetGravity(-980.0f); // 기본 중력
		ProjectileMovement->SetRotationFollowsVelocity(true); // 속도 방향으로 회전
		ProjectileMovement->SetProjectileLifespan(ProjectileLifespan);
		ProjectileMovement->SetAutoDestroyWhenLifespanExceeded(true);
		ProjectileMovement->SetActive(true);
	}
}

AProjectileActor::~AProjectileActor()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AProjectileActor::BeginPlay()
{
	UE_LOG("[ProjectileActor] BeginPlay - Attaching Lua script first");

	// Lua 스크립트 자동 연결 (Super::BeginPlay() 전에!)
	FLuaLocalValue LuaLocalValue;
	LuaLocalValue.MyActor = this;
	LuaLocalValue.GameMode = World ? World->GetGameMode() : nullptr;

	try
	{
		UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, "Projectile.lua");
		UE_LOG("[ProjectileActor] Auto-attached Lua script: Projectile.lua");
	}
	catch (std::exception& e)
	{
		UE_LOG("[ProjectileActor] ERROR: Lua script attachment failed: %s", e.what());
	}

	// 이제 Super::BeginPlay() 호출 → Actor::BeginPlay()에서 Lua BeginPlay 호출됨
	Super::BeginPlay();

	UE_LOG("[ProjectileActor] BeginPlay complete - Projectile spawned");
}

void AProjectileActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 생명주기 체크 (ProjectileMovement에서도 하지만, 여기서도 추가 체크)
	if (ProjectileLifespan > 0.0f)
	{
		CurrentLifetime += DeltaTime;
		if (CurrentLifetime >= ProjectileLifespan)
		{
			// 발사체 파괴
			SetActorHiddenInGame(true);
			// World에서 삭제 예약
			if (World)
			{
				World->MarkActorForDestruction(this);
			}
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 발사체 제어
// ────────────────────────────────────────────────────────────────────────────

void AProjectileActor::FireInDirection(const FVector& Direction, float Speed)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->SetInitialSpeed(Speed);
		ProjectileMovement->FireInDirection(Direction);
		UE_LOG("[ProjectileActor] Fired in direction: (%.2f, %.2f, %.2f) at speed %.2f",
		       Direction.X, Direction.Y, Direction.Z, Speed);
	}
}

void AProjectileActor::SetInitialSpeed(float Speed)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->SetInitialSpeed(Speed);
	}
}

void AProjectileActor::SetGravityScale(float GravityScale)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->SetGravity(-980.0f * GravityScale);
	}
}

void AProjectileActor::SetLifespan(float Lifespan)
{
	ProjectileLifespan = Lifespan;
	if (ProjectileMovement)
	{
		ProjectileMovement->SetProjectileLifespan(Lifespan);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 충돌 처리
// ────────────────────────────────────────────────────────────────────────────

void AProjectileActor::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, const FVector& ContactPoint,
                                        float PenetrationDepth)
{
	// 자기 자신은 무시
	if (OtherActor == this)
		return;

	UE_LOG("[ProjectileActor] Hit: %s", OtherActor ? OtherActor->GetName().ToString() : "nullptr");

	// TODO: 충돌 시 파티클 효과, 사운드 재생 등
	// TODO: 데미지 처리 등

	// 충돌 시 발사체 파괴
	SetActorHiddenInGame(true);
	if (World)
	{
		World->MarkActorForDestruction(this);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 복제 (Duplication)
// ────────────────────────────────────────────────────────────────────────────

void AProjectileActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 컴포넌트 복제 후 다시 연결
	for (UActorComponent* Component : OwnedComponents)
	{
		if (USphereComponent* SphereComp = Cast<USphereComponent>(Component))
		{
			CollisionComponent = SphereComp;
		}
		else if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
		{
			MeshComponent = MeshComp;
		}
		else if (UProjectileMovementComponent* MovementComp = Cast<UProjectileMovementComponent>(Component))
		{
			ProjectileMovement = MovementComp;
		}
	}
}
