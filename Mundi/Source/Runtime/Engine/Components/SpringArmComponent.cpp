#include "pch.h"
#include "SpringArmComponent.h"
#include "Actor.h"
#include "World.h"

IMPLEMENT_CLASS(USpringArmComponent)

BEGIN_PROPERTIES(USpringArmComponent)
    MARK_AS_COMPONENT("스프링암 컴포넌트", "카메라 추적을 위한 스프링암 컴포넌트입니다. 카메라 Lag, 충돌 감지 등을 지원합니다.")
    ADD_PROPERTY_RANGE(float, TargetArmLength, "SpringArm", 0.0f, 10000.0f, true, "스프링암의 목표 길이입니다.")
    ADD_PROPERTY(FVector, SocketOffset, "SpringArm", true, "카메라 소켓의 오프셋입니다.")
    ADD_PROPERTY(FVector, TargetOffset, "SpringArm", true, "타겟 위치의 오프셋입니다.")
    ADD_PROPERTY(bool, bEnableCameraLag, "CameraLag", true, "카메라 위치에 부드러운 따라가기(Lag)를 적용할지 여부입니다.")
    ADD_PROPERTY_RANGE(float, CameraLagSpeed, "CameraLag", 0.0f, 100.0f, true, "카메라 위치 Lag의 보간 속도입니다.")
    ADD_PROPERTY_RANGE(float, CameraLagMaxDistance, "CameraLag", 0.0f, 10000.0f, true, "카메라 Lag의 최대 거리 제한입니다. 0이면 제한 없음.")
    ADD_PROPERTY(bool, bEnableCameraRotationLag, "CameraLag", true, "카메라 회전에 부드러운 따라가기(Lag)를 적용할지 여부입니다.")
    ADD_PROPERTY_RANGE(float, CameraRotationLagSpeed, "CameraLag", 0.0f, 100.0f, true, "카메라 회전 Lag의 보간 속도입니다.")
    ADD_PROPERTY(bool, bDoCollisionTest, "Collision", true, "스프링암이 장애물과의 충돌을 테스트할지 여부입니다.")
    ADD_PROPERTY_RANGE(float, ProbeSize, "Collision", 0.0f, 100.0f, true, "충돌 테스트에 사용되는 프로브의 크기입니다.")
END_PROPERTIES()

USpringArmComponent::USpringArmComponent()
    : TargetArmLength(300.0f)
    , CurrentArmLength(300.0f)
    , SocketOffset(FVector())
    , TargetOffset(FVector())
    , bEnableCameraLag(false)
    , CameraLagSpeed(10.0f)
    , CameraLagMaxDistance(0.0f)
    , PreviousDesiredLocation(FVector())
    , PreviousActorLocation(FVector())
    , bEnableCameraRotationLag(false)
    , CameraRotationLagSpeed(10.0f)
    , PreviousDesiredRotation(FQuat::Identity())
    , bDoCollisionTest(true)
    , ProbeSize(12.0f)
    , SocketLocation(FVector())
    , SocketRotation(FQuat::Identity())
{
    bCanEverTick = true;  // 틱 지원 활성화
    bTickEnabled = true;  // 틱 시작 시 켜짐
}

USpringArmComponent::~USpringArmComponent()
{
}

void USpringArmComponent::TickComponent(float DeltaSeconds)
{
    USceneComponent::TickComponent(DeltaSeconds);

    FVector DesiredSocketLocation;
    FQuat DesiredSocketRotation;

    UpdateDesiredArmLocation(DeltaSeconds, DesiredSocketLocation, DesiredSocketRotation);

    // Collision Test
    FVector FinalSocketLocation = DesiredSocketLocation;
    if (bDoCollisionTest)
    {
        DoCollisionTest(DesiredSocketLocation, FinalSocketLocation);
    }

    // Socket 위치/회전 저장 (GetSocketLocation/Rotation에서 사용)
    SocketLocation = FinalSocketLocation;
    SocketRotation = DesiredSocketRotation;

    // 자식 컴포넌트(카메라)를 Socket 위치로 이동
    for (USceneComponent* Child : GetAttachChildren())
    {
        if (Child)
        {
            Child->SetWorldLocation(SocketLocation);
            Child->SetWorldRotation(SocketRotation);
        }
    }
}

// ───────────────────────────────────────────
// Get Socket Transform
// ───────────────────────────────────────────

FVector USpringArmComponent::GetSocketLocation() const
{
    return SocketLocation;
}

FRotator USpringArmComponent::GetSocketRotation() const
{
    return SocketRotation.ToRotator();
}

FTransform USpringArmComponent::GetSocketTransform() const
{
    return FTransform(SocketLocation, SocketRotation, FVector(1, 1, 1));
}

// ───────────────────────────────────────────
// Internal Functions
// ───────────────────────────────────────────

void USpringArmComponent::UpdateDesiredArmLocation(float DeltaTime, FVector& OutDesiredLocation, FQuat& OutDesiredRotation)
{
    if (!GetOwner())
    {
        OutDesiredLocation = GetWorldLocation();
        OutDesiredRotation = GetWorldRotation();
        return;
    }

    // 회전 - Owner(캐릭터)의 월드 회전을 따라감
    FQuat OwnerRotation = GetOwner()->GetActorRotation();

    // 기본 위치: Owner의 위치 + TargetOffset (Owner의 로컬 좌표계 기준)
    FVector OwnerLocation = GetOwner()->GetActorLocation();
    FVector RotatedTargetOffset = OwnerRotation.RotateVector(TargetOffset);
    FVector TargetLocation = OwnerLocation + RotatedTargetOffset;

    // Spring Arm 방향 계산 (뒤쪽)
    FVector ArmDirection = OwnerRotation.GetForwardVector() * -1.0f; // Backward direction

    // SocketOffset도 Owner 회전 적용
    FVector RotatedSocketOffset = OwnerRotation.RotateVector(SocketOffset);

    // Desired Location
    FVector UnlaggedDesiredLocation = TargetLocation + ArmDirection * TargetArmLength + RotatedSocketOffset;

    // Camera Lag 적용
    if (bEnableCameraLag)
    {
        // 이전 위치가 초기값이면 즉시 설정
        if (PreviousDesiredLocation.IsZero())
        {
            PreviousDesiredLocation = UnlaggedDesiredLocation;
            PreviousActorLocation = OwnerLocation;
        }

        // Lag 적용
        FVector LagVector = UnlaggedDesiredLocation - PreviousDesiredLocation;
        float LagDistance = LagVector.Size();

        // MaxDistance 제한
        if (CameraLagMaxDistance > 0.0f && LagDistance > CameraLagMaxDistance)
        {
            PreviousDesiredLocation = UnlaggedDesiredLocation - LagVector.GetNormalized() * CameraLagMaxDistance;
        }

        // Lerp
        OutDesiredLocation = FVector::Lerp(PreviousDesiredLocation, UnlaggedDesiredLocation,
                                           FMath::Min(1.0f, DeltaTime * CameraLagSpeed));
        PreviousDesiredLocation = OutDesiredLocation;
        PreviousActorLocation = OwnerLocation;
    }
    else
    {
        OutDesiredLocation = UnlaggedDesiredLocation;
    }

    // Rotation Lag 적용
    if (bEnableCameraRotationLag)
    {
        if (PreviousDesiredRotation.IsIdentity())
        {
            PreviousDesiredRotation = OwnerRotation;
        }

        float Alpha = FMath::Min(1.0f, DeltaTime * CameraRotationLagSpeed);
        OutDesiredRotation = FQuat::Slerp(PreviousDesiredRotation, OwnerRotation, Alpha);
        PreviousDesiredRotation = OutDesiredRotation;
    }
    else
    {
        OutDesiredRotation = OwnerRotation;
    }
}

bool USpringArmComponent::DoCollisionTest(const FVector& DesiredLocation, FVector& OutLocation)
{
    if (!GetOwner() || !GetOwner()->GetWorld())
    {
        OutLocation = DesiredLocation;
        return false;
    }

    // 간단한 Collision Test 구현
    // 실제로는 World의 CollisionManager를 사용하여 Raycast 수행
    // 여기서는 기본 구현만 제공

    FVector OwnerLocation = GetOwner()->GetActorLocation();
    FVector Direction = DesiredLocation - OwnerLocation;
    float Distance = Direction.Size();

    // TODO: Raycast 구현
    // 현재는 단순히 DesiredLocation 반환
    OutLocation = DesiredLocation;
    CurrentArmLength = TargetArmLength;

    return false;
}

// ───────────────────────────────────────────
// Serialization
// ───────────────────────────────────────────

void USpringArmComponent::OnSerialized()
{
    USceneComponent::OnSerialized();
}

void USpringArmComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    USceneComponent::Serialize(bInIsLoading, InOutHandle);
}

// ───────────────────────────────────────────
// Duplicate
// ───────────────────────────────────────────

void USpringArmComponent::DuplicateSubObjects()
{
    USceneComponent::DuplicateSubObjects();
}


