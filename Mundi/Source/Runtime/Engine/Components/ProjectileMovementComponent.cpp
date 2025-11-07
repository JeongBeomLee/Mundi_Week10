#include "pch.h"
#include "ProjectileMovementComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(UProjectileMovementComponent)

BEGIN_PROPERTIES(UProjectileMovementComponent)
    MARK_AS_COMPONENT("발사체 무브먼트 컴포넌트", "발사체 무브먼트 컴포넌트를 추가합니다")
    // 물리 속성
    ADD_PROPERTY(float, Gravity, "발사체", true, "발사체 중력 가속도입니다")
    ADD_PROPERTY(float, InitialSpeed, "발사체", true, "발사체 초기 속도입니다")
    ADD_PROPERTY(float, MaxSpeed, "발사체", true, "발사체 최대 속도입니다")
    // 생명주기 속성
    ADD_PROPERTY(bool, bAutoDestroyWhenLifespanExceeded, "발사체", true, "생명 시간 초과시 발사체를 파괴합니다")
    ADD_PROPERTY(float, ProjectileLifespan, "발사체", true, "발사체 생명 시간입니다")
    ADD_PROPERTY(float, CurrentLifetime, "발사체", false, "현재 생존 시간입니다 (읽기 전용)")
    // 호밍 속성
    ADD_PROPERTY(bool, bIsHomingProjectile, "호밍", true, "호밍 기능을 활성화합니다")
    ADD_PROPERTY(float, HomingAccelerationMagnitude, "호밍", true, "호밍 가속도 크기입니다")
    // 회전 속성
    ADD_PROPERTY(bool, bRotationFollowsVelocity, "발사체", true, "속도 방향으로 회전합니다")
    // 상태
    ADD_PROPERTY(bool, bIsActive, "발사체", true, "발사체가 활성화 상태입니다")
END_PROPERTIES()

UProjectileMovementComponent::UProjectileMovementComponent()
    : Gravity(-9.8f)
    , GravityDirection(FVector(0.0f, 0.0f, 1.0f))  // 기본 중력 방향: 아래쪽
    , InitialSpeed(3.0f)  // 초기 속도
    , MaxSpeed(0.0f)  // 0 = 제한 없음
    , HomingTargetActor(nullptr)
    , HomingTargetComponent(nullptr)
    , HomingAccelerationMagnitude(0.0f)
    , bIsHomingProjectile(false)
    , bRotationFollowsVelocity(true)
    , ProjectileLifespan(0.0f)  // 0 = 무제한
    , CurrentLifetime(0.0f)
    , bAutoDestroyWhenLifespanExceeded(false)
    , bIsActive(true)
{
    bCanEverTick = true;
}

UProjectileMovementComponent::~UProjectileMovementComponent()
{
}

void UProjectileMovementComponent::TickComponent(float DeltaSeconds)
{
    // Editor World에서는 Tick 안함
    if (!GWorld->bPie)
        return;

    Super_t::TickComponent(DeltaSeconds);

    if (!bIsActive || !bCanEverTick || !UpdatedComponent)
        return;

    // 1. 생명주기 체크
    if (ProjectileLifespan > 0.0f)
    {
        CurrentLifetime += DeltaSeconds;
        if (CurrentLifetime >= ProjectileLifespan)
        {
            if (bAutoDestroyWhenLifespanExceeded)
            {
                // Owner Actor 파괴 (현재 지연 삭제 처리가 없어서 안보이게만 처리.)
                AActor* Owner = UpdatedComponent->GetOwner();
                if (Owner)
                {
                    //Owner->Destroy();
                    Owner->SetActorHiddenInGame(true);
                    return;
                }
            }
            bIsActive = false;
            return;
        }
    }

    // 2. 호밍 가속도 계산
    if (bIsHomingProjectile)
    {
        ComputeHomingAcceleration(DeltaSeconds);
    }

    // 3. 중력 적용 (방향 벡터 사용)
    // Gravity는 음수일 수 있으므로 절댓값 사용
    FVector GravityAcceleration = GravityDirection * abs(Gravity);
    Velocity += GravityAcceleration * DeltaSeconds;

    // 4. 가속도 적용
    Velocity += Acceleration * DeltaSeconds;

    // 5. 속도 제한
    LimitVelocity();

    // 6. 위치 업데이트
    FVector Delta = Velocity * DeltaSeconds;
    if (!Delta.IsZero())
    {
        UpdatedComponent->AddWorldOffset(Delta);
    }

    // 7. 회전 업데이트 (속도 방향 추적)
    if (bRotationFollowsVelocity)
    {
        UpdateRotationFromVelocity();
    }
}

void UProjectileMovementComponent::FireInDirection(const FVector& ShootDirection)
{
    // 방향 벡터를 정규화하고 InitialSpeed를 곱해 속도 설정
    FVector NormalizedDirection = ShootDirection;
    NormalizedDirection.Normalize();

    Velocity = NormalizedDirection * InitialSpeed;

    // 상태 초기화
    bIsActive = true;
    CurrentLifetime = 0.0f;
}

void UProjectileMovementComponent::SetVelocityInLocalSpace(const FVector& NewVelocity)
{
    if (!UpdatedComponent)
        return;

    // 로컬 공간 속도를 월드 공간으로 변환
    FQuat WorldRotation = UpdatedComponent->GetWorldRotation();
    Velocity = WorldRotation.RotateVector(NewVelocity);
}

void UProjectileMovementComponent::SetHomingTarget(AActor* Target)
{
    HomingTargetActor = Target;
    HomingTargetComponent = nullptr;  // Component가 우선순위가 높으므로 초기화
}

void UProjectileMovementComponent::SetHomingTarget(USceneComponent* Target)
{
    HomingTargetComponent = Target;
    HomingTargetActor = nullptr;
}

void UProjectileMovementComponent::LimitVelocity()
{
    if (MaxSpeed <= 0.0f)
        return;

    float CurrentSpeed = Velocity.Size();
    if (CurrentSpeed > MaxSpeed)
    {
        Velocity.Normalize();
        Velocity *= MaxSpeed;
    }
}

void UProjectileMovementComponent::ComputeHomingAcceleration(float DeltaTime)
{
    if (HomingAccelerationMagnitude <= 0.0f)
        return;

    if (!UpdatedComponent)
        return;

    FVector TargetLocation;
    bool bHasValidTarget = false;

    // Component가 Actor보다 우선순위가 높음
    if (HomingTargetComponent)
    {
        TargetLocation = HomingTargetComponent->GetWorldLocation();
        bHasValidTarget = true;
    }
    else if (HomingTargetActor)
    {
        TargetLocation = HomingTargetActor->GetActorLocation();
        bHasValidTarget = true;
    }

    if (!bHasValidTarget)
        return;

    FVector CurrentLocation = UpdatedComponent->GetWorldLocation();
    FVector ToTarget = TargetLocation - CurrentLocation;

    // 타겟까지의 거리가 너무 가까우면 호밍 중지
    if (ToTarget.Size() < 10.0f)  // 10cm 이내
        return;

    ToTarget.Normalize();

    // 호밍 가속도를 타겟 방향으로 설정
    Acceleration = ToTarget * HomingAccelerationMagnitude;
}

void UProjectileMovementComponent::UpdateRotationFromVelocity()
{
    if (!UpdatedComponent)
        return;

    if (Velocity.IsZero())
        return;

    // TODO: 속도 방향으로 회전 기능 구현 필요
    // FQuat::FromDirectionVector() 또는 유사한 유틸리티 함수가 필요합니다
    // 현재는 수학 라이브러리 구현 대기 중으로 비활성화되어 있습니다
}

void UProjectileMovementComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // 호밍 타겟은 일시적인 런타임 참조이므로
    // 게임 로직에서 필요시 다시 설정하도록 nullptr로 리셋
    HomingTargetActor = nullptr;
    HomingTargetComponent = nullptr;

    // 런타임 상태 초기화
    CurrentLifetime = 0.0f;
    bIsActive = true;
}

void UProjectileMovementComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}