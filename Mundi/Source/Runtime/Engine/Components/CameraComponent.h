#pragma once
#include "SceneComponent.h"

enum class ECameraProjectionMode
{
    Perspective,
    Orthographic
};

class UCameraComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UCameraComponent, USceneComponent)
    GENERATED_REFLECTION_BODY()

    UCameraComponent();

protected:
    ~UCameraComponent() override;

public:
    // Projection settings
    void SetFOV(float NewFOV) { FieldOfView = NewFOV; }
    void SetAspectRatio(float NewAspect) { AspectRatio = NewAspect; }
    void SetClipPlanes(float NewNear, float NewFar) { NearClip = NewNear; FarClip = NewFar; }
    void SetNearClipPlane(float NewNear) { NearClip = NewNear; }
    void SetFarClipPlane(float NewFar) { FarClip = NewFar; }
    void SetProjectionMode(ECameraProjectionMode Mode) { ProjectionMode = Mode; }
    void SetZoomFactor(float InZoomFactor) { ZooMFactor = InZoomFactor; };
    
    float GetFOV() const { return FieldOfView; }
    float GetAspectRatio() const { return AspectRatio; }
    float GetNearClip() const { return NearClip; }
    float GetFarClip() const { return FarClip; }
    float GetZoomFactor()const { return ZooMFactor; };
    ECameraProjectionMode GetProjectionMode() const { return ProjectionMode; }

    // Matrices
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio) const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const;

    // Directions in world space
    FVector GetForward() const;
    FVector GetRight() const;
    FVector GetUp() const;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UCameraComponent)

    // Serialization
    virtual void OnSerialized() override;
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;


private:
    float FieldOfView;   // degrees
    float AspectRatio;
    float NearClip;
    float FarClip;

    float ZooMFactor;


    ECameraProjectionMode ProjectionMode;
};

