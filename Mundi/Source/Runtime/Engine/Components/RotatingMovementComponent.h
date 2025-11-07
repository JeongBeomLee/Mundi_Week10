#pragma once
#include "MovementComponent.h"
#include "Vector.h"

/**
 * URotatingMovementComponent
 * 지정된 속도로 UpdatedComponent를 회전시키는 컴포넌트
 * 로컬 공간과 월드 공간 회전을 모두 지원
 */
class URotatingMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)
    GENERATED_REFLECTION_BODY()
    URotatingMovementComponent();

protected:
    ~URotatingMovementComponent() override;

public:
    // Life Cycle
    virtual void TickComponent(float DeltaSeconds) override;

    // 회전 API
    void SetRotationRate(const FVector& NewRotationRate);
    FVector GetRotationRate() const { return RotationRate; }

    void SetPivotTranslation(const FVector& NewPivotTranslation);
    FVector GetPivotTranslation() const { return PivotTranslation; }

    void SetRotationInLocalSpace(bool bNewRotationInLocalSpace);
    bool IsRotationInLocalSpace() const { return bRotationInLocalSpace; }

    DECLARE_DUPLICATE(URotatingMovementComponent)
    void DuplicateSubObjects() override;

    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    // [PIE] 값 복사
    // 초당 회전 속도 (도 단위, Pitch/Yaw/Roll)
    FVector RotationRate;

    // 컴포넌트 위치로부터의 피벗 오프셋
    FVector PivotTranslation;

    // true면 로컬 공간에서 회전, false면 월드 공간에서 회전
    bool bRotationInLocalSpace;
};
