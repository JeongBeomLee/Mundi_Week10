#pragma once
#include "SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
public:
    DECLARE_CLASS(USpringArmComponent, USceneComponent)
    GENERATED_REFLECTION_BODY()

    USpringArmComponent();
    virtual ~USpringArmComponent() override;

    // Tick
    virtual void TickComponent(float DeltaSeconds) override;

    // ───────────────────────────────────────────
    // Spring Arm Settings
    // ───────────────────────────────────────────
    void SetTargetArmLength(float NewLength) { TargetArmLength = NewLength; }
    float GetTargetArmLength() const { return TargetArmLength; }

    void SetSocketOffset(const FVector& Offset) { SocketOffset = Offset; }
    FVector GetSocketOffset() const { return SocketOffset; }

    void SetTargetOffset(const FVector& Offset) { TargetOffset = Offset; }
    FVector GetTargetOffset() const { return TargetOffset; }

    // ───────────────────────────────────────────
    // Camera Lag (부드러운 따라가기)
    // ───────────────────────────────────────────
    void SetEnableCameraLag(bool bEnable) { bEnableCameraLag = bEnable; }
    bool GetEnableCameraLag() const { return bEnableCameraLag; }

    void SetCameraLagSpeed(float Speed) { CameraLagSpeed = Speed; }
    float GetCameraLagSpeed() const { return CameraLagSpeed; }

    void SetCameraLagMaxDistance(float MaxDistance) { CameraLagMaxDistance = MaxDistance; }
    float GetCameraLagMaxDistance() const { return CameraLagMaxDistance; }

    // ───────────────────────────────────────────
    // Camera Rotation Lag
    // ───────────────────────────────────────────
    void SetEnableCameraRotationLag(bool bEnable) { bEnableCameraRotationLag = bEnable; }
    bool GetEnableCameraRotationLag() const { return bEnableCameraRotationLag; }

    void SetCameraRotationLagSpeed(float Speed) { CameraRotationLagSpeed = Speed; }
    float GetCameraRotationLagSpeed() const { return CameraRotationLagSpeed; }

    // ───────────────────────────────────────────
    // Collision Test
    // ───────────────────────────────────────────
    void SetDoCollisionTest(bool bEnable) { bDoCollisionTest = bEnable; }
    bool GetDoCollisionTest() const { return bDoCollisionTest; }

    void SetProbeSize(float Size) { ProbeSize = Size; }
    float GetProbeSize() const { return ProbeSize; }

    // ───────────────────────────────────────────
    // Get Socket Transform
    // ───────────────────────────────────────────
    FVector GetSocketLocation() const;
    FRotator GetSocketRotation() const;
    FTransform GetSocketTransform() const;

    // ───────────────────────────────────────────
    // Serialization
    // ───────────────────────────────────────────
    virtual void OnSerialized() override;
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    // ───────────────────────────────────────────
    // Duplicate
    // ───────────────────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(USpringArmComponent)

protected:
    // Spring Arm 길이
    float TargetArmLength;
    float CurrentArmLength;

    // Offsets
    FVector SocketOffset;       // 소켓 오프셋 (카메라 위치)
    FVector TargetOffset;       // 타겟 오프셋 (중심점에서 이동)

    // Camera Lag (위치)
    bool bEnableCameraLag;
    float CameraLagSpeed;
    float CameraLagMaxDistance;
    FVector PreviousDesiredLocation;
    FVector PreviousActorLocation;

    // Camera Rotation Lag
    bool bEnableCameraRotationLag;
    float CameraRotationLagSpeed;
    FQuat PreviousDesiredRotation;

    // Collision
    bool bDoCollisionTest;
    float ProbeSize;

    // Socket Transform (자식 컴포넌트가 배치될 위치)
    FVector SocketLocation;
    FQuat SocketRotation;

private:
    void UpdateDesiredArmLocation(float DeltaTime, FVector& OutDesiredLocation, FQuat& OutDesiredRotation);
    bool DoCollisionTest(const FVector& DesiredLocation, FVector& OutLocation);
};
