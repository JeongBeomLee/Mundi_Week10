// ────────────────────────────────────────────────────────────────────────────
// ProjectileActor.h
// 발사체 Actor 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Actor.h"
#include "StaticMeshComponent.h"
#include "ProjectileMovementComponent.h"
#include "CollisionComponent/SphereComponent.h"

/**
 * AProjectileActor
 *
 * 발사체 Actor 클래스입니다.
 *
 * 주요 기능:
 * - ProjectileMovement 컴포넌트를 사용한 발사체 움직임
 * - 충돌 감지 및 처리
 * - 자동 파괴 (생명주기)
 */
class AProjectileActor : public AActor
{
public:
	DECLARE_CLASS(AProjectileActor, AActor)
	GENERATED_REFLECTION_BODY()
	DECLARE_DUPLICATE(AProjectileActor)

	AProjectileActor();
	virtual ~AProjectileActor() override;

	// ────────────────────────────────────────────────
	// 컴포넌트 접근
	// ────────────────────────────────────────────────

	/** StaticMesh 컴포넌트를 반환합니다 */
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	/** ProjectileMovement 컴포넌트를 반환합니다 */
	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

	/** Collision 컴포넌트를 반환합니다 */
	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }

	// ────────────────────────────────────────────────
	// 발사체 제어
	// ────────────────────────────────────────────────

	/**
	 * 특정 방향으로 발사체를 발사합니다
	 * @param Direction - 발사 방향 벡터 (정규화됨)
	 * @param Speed - 발사 속도 (cm/s)
	 */
	void FireInDirection(const FVector& Direction, float Speed = 2000.0f);

	/**
	 * 발사체의 초기 속도를 설정합니다
	 * @param Speed - 속도 (cm/s)
	 */
	void SetInitialSpeed(float Speed);

	/**
	 * 발사체의 중력 스케일을 설정합니다
	 * @param GravityScale - 중력 배율 (1.0 = 기본 중력)
	 */
	void SetGravityScale(float GravityScale);

	/**
	 * 발사체의 생명 시간을 설정합니다
	 * @param Lifespan - 생명 시간 (초)
	 */
	void SetLifespan(float Lifespan);

	virtual void BeginPlay() override;
protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void Tick(float DeltaTime) override;

	// ────────────────────────────────────────────────
	// 충돌 처리
	// ────────────────────────────────────────────────

	/**
	 * 충돌 발생 시 호출됩니다
	 */
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	                      UPrimitiveComponent* OtherComp, const FVector& ContactPoint, float PenetrationDepth);

	// ────────────────────────────────────────────────
	// 복제 (Duplication)
	// ────────────────────────────────────────────────

	virtual void DuplicateSubObjects() override;

	// ────────────────────────────────────────────────
	// 컴포넌트
	// ────────────────────────────────────────────────

	/** 발사체 메시 */
	UStaticMeshComponent* MeshComponent;

	/** 발사체 움직임 컴포넌트 */
	UProjectileMovementComponent* ProjectileMovement;

	/** 충돌 감지 컴포넌트 */
	USphereComponent* CollisionComponent;

	// ────────────────────────────────────────────────
	// 설정값
	// ────────────────────────────────────────────────

	/** 발사체 생명 시간 (초, 0이면 무제한) */
	float ProjectileLifespan;

	/** 현재 생존 시간 */
	float CurrentLifetime;
};
