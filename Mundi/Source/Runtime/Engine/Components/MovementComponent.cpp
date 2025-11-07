#include "pch.h"
#include "MovementComponent.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(UMovementComponent)

BEGIN_PROPERTIES(UMovementComponent)
    ADD_PROPERTY(FVector, Velocity, "Movement", true, "현재 속도입니다")
    ADD_PROPERTY(FVector, Acceleration, "Movement", true, "현재 가속도입니다")
    ADD_PROPERTY(bool, bUpdateOnlyIfRendered, "Movement", true, "렌더링될 때만 업데이트합니다")
END_PROPERTIES()

UMovementComponent::UMovementComponent()
    : UpdatedComponent(nullptr)
    , Velocity(0.0f, 0.0f, 0.0f)
    , Acceleration(0.0f, 0.0f, 0.0f)
    , bUpdateOnlyIfRendered(false)
{
    // Movement component는 기본적으로 Tick 가능
    bCanEverTick = true;
    // Owner는 컴포넌트 시스템에서 자동으로 설정됨
}

UMovementComponent::~UMovementComponent()
{
}

void UMovementComponent::InitializeComponent()
{
    Super_t::InitializeComponent();
    
    // 자신을 소유한 액터의 루트 컴포넌트를 UpdatedComponent로 설정
    if (!UpdatedComponent && Owner)
    {
        AActor* OwnerActor = Cast<AActor>(Owner);
        if (OwnerActor)
        {
            UpdatedComponent = OwnerActor->GetRootComponent();
        }
    }
}

void UMovementComponent::TickComponent(float DeltaSeconds)
{
    Super_t::TickComponent(DeltaSeconds);

    if (!bIsActive || !bCanEverTick)
        return;

    // 매 프레임 호출, 파생 클래스에서 오버라이드하여 이동 로직 구현
}

void UMovementComponent::SetVelocity(const FVector& NewVelocity)
{
    Velocity = NewVelocity;
}

void UMovementComponent::SetAcceleration(const FVector& NewAcceleration)
{
    Acceleration = NewAcceleration;
}

void UMovementComponent::StopMovement()
{
    Velocity = FVector(0.0f, 0.0f, 0.0f);
    Acceleration = FVector(0.0f, 0.0f, 0.0f);
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    UpdatedComponent = NewUpdatedComponent;
}

void UMovementComponent::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();

    // UpdatedComponent는 Owner의 RootComponent에 대한 참조이므로
    // InitializeComponent()에서 다시 할당되도록 nullptr로 리셋
    UpdatedComponent = nullptr;
}

void UMovementComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}
