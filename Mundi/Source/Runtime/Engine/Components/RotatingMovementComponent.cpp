#include "pch.h"
#include "RotatingMovementComponent.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(URotatingMovementComponent)

BEGIN_PROPERTIES(URotatingMovementComponent)
    MARK_AS_COMPONENT("회전 무브먼트 컴포넌트", "게임에서 액터를 회전하는 컴포넌트를 추가합니다")
    ADD_PROPERTY(FVector, RotationRate, "회전 컴포넌트", true, "회전 속도입니다")
    ADD_PROPERTY(FVector, PivotTranslation, "회전 컴포넌트", true, "피벗 오프셋입니다")
    ADD_PROPERTY(bool, bRotationInLocalSpace, "회전 컴포넌트", true, "로컬에서 회전합니다")
END_PROPERTIES()
    
URotatingMovementComponent::URotatingMovementComponent()
    : RotationRate(0.0f, 0.0f, 0.0f)
    , PivotTranslation(0.0f, 0.0f, 0.0f)
    , bRotationInLocalSpace(true)
{
    bCanEverTick = true;
}

URotatingMovementComponent::~URotatingMovementComponent()
{   
}

void URotatingMovementComponent::TickComponent(float DeltaSeconds)
{
    // Editor World에서는 Tick 안함.
    // + GetWorld로 GWorld 받아오면 Dangilng Pointer 버그있음
    if (!GWorld->bPie)
        return;

    Super_t::TickComponent(DeltaSeconds);

    if (!bIsActive || !bCanEverTick)
        return;

    if (!UpdatedComponent)
        return;

    // 회전 델타 계산 (초당 도 * 델타 시간)
    FVector RotationDelta = RotationRate * DeltaSeconds;

    // 오일러 각도로부터 쿼터니언 생성 (Pitch, Yaw, Roll)
    FQuat DeltaRotation = FQuat::MakeFromEulerZYX(FVector(RotationDelta.X, RotationDelta.Y, RotationDelta.Z));

    if (bRotationInLocalSpace)
    {
        // 로컬 공간에서 회전 적용
        if (PivotTranslation.IsZero())
        {
            // 컴포넌트 원점을 중심으로 단순 회전
            UpdatedComponent->AddLocalRotation(DeltaRotation);
        }
        else
        {
            // 피벗 지점을 중심으로 회전
            FVector CurrentLocation = UpdatedComponent->GetRelativeLocation();
            FQuat CurrentRotation = UpdatedComponent->GetRelativeRotation();

            // 피벗 주변 회전
            FVector OffsetFromPivot = CurrentLocation - PivotTranslation;
            FVector RotatedOffset = DeltaRotation.RotateVector(OffsetFromPivot);
            FVector NewLocation = PivotTranslation + RotatedOffset;

            // 새로운 회전과 위치 적용
            UpdatedComponent->SetRelativeRotation(DeltaRotation * CurrentRotation);
            UpdatedComponent->SetRelativeLocation(NewLocation);
        }
    }
    else
    {
        // 월드 공간에서 회전 적용
        if (PivotTranslation.IsZero())
        {
            // 컴포넌트 원점을 중심으로 단순 회전
            UpdatedComponent->AddWorldRotation(DeltaRotation);
        }
        else
        {
            // 월드 공간에서 피벗 지점을 중심으로 회전
            FVector WorldPivot = UpdatedComponent->GetWorldLocation() +
                UpdatedComponent->GetWorldRotation().RotateVector(PivotTranslation);

            FVector CurrentWorldLocation = UpdatedComponent->GetWorldLocation();
            FQuat CurrentWorldRotation = UpdatedComponent->GetWorldRotation();

            // 월드 피벗 주변 회전
            FVector OffsetFromPivot = CurrentWorldLocation - WorldPivot;
            FVector RotatedOffset = DeltaRotation.RotateVector(OffsetFromPivot);
            FVector NewWorldLocation = WorldPivot + RotatedOffset;

            // 새로운 회전과 위치 적용
            UpdatedComponent->SetWorldRotation(DeltaRotation * CurrentWorldRotation);
            UpdatedComponent->SetWorldLocation(NewWorldLocation);
        }
    }
}

void URotatingMovementComponent::SetRotationRate(const FVector& NewRotationRate)
{
    RotationRate = NewRotationRate;
}

void URotatingMovementComponent::SetPivotTranslation(const FVector& NewPivotTranslation)
{
    PivotTranslation = NewPivotTranslation;
}

void URotatingMovementComponent::SetRotationInLocalSpace(bool bNewRotationInLocalSpace)
{
    bRotationInLocalSpace = bNewRotationInLocalSpace;
}

void URotatingMovementComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // 모든 멤버가 값 타입이므로 추가적인 깊은 복사 불필요
    // UpdatedComponent는 부모 클래스에서 처리됨
}

void URotatingMovementComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}
