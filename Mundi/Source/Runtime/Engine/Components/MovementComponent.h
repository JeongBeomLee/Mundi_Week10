#pragma once
#include "ActorComponent.h"
#include "Vector.h"

class USceneComponent;

/**
 * UMovementComponent
 * 이동 로직을 처리하는 컴포넌트의 기본 클래스
 * 기본적인 속도, 가속도, 이동 상태 관리를 제공
 */
class UMovementComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UMovementComponent, UActorComponent)
    GENERATED_REFLECTION_BODY()
    UMovementComponent();

protected:
    ~UMovementComponent() override;

public:
    // Life Cycle
    virtual void InitializeComponent() override;
    virtual void TickComponent(float DeltaSeconds) override;

    void SetVelocity(const FVector& NewVelocity);
    FVector GetVelocity() const { return Velocity; }

    void SetAcceleration(const FVector& NewAcceleration);
    FVector GetAcceleration() const { return Acceleration; }

    // 속도와 가속도를 0으로 설정하여 이동 중지
    virtual void StopMovement();

    // 업데이트 대상 컴포넌트
    void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
    USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }

    // 이동 속성
    void SetUpdateOnlyIfRendered(bool bNewUpdateOnlyIfRendered) { bUpdateOnlyIfRendered = bNewUpdateOnlyIfRendered; }
    bool GetUpdateOnlyIfRendered() const { return bUpdateOnlyIfRendered; }

    DECLARE_DUPLICATE(UMovementComponent)
    void DuplicateSubObjects() override;

    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    // [PIE] Duplicate 복사 대상
    USceneComponent* UpdatedComponent = nullptr;

    // [PIE] 값 복사
    FVector Velocity;
    FVector Acceleration;
    bool bUpdateOnlyIfRendered = false;
};
