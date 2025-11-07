#pragma once
#include "Actor.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"
class UCameraComponent;
class UUIManager;
class UInputManager;

class ACameraActor : public AActor
{
public:
    DECLARE_CLASS(ACameraActor, AActor)
    GENERATED_REFLECTION_BODY()

    ACameraActor();
   
    virtual void Tick(float DeltaSeconds) override;

    // 씬 로드 등 외부에서 카메라 각도를 즉시 세팅하고
    // 입력 경로와 동일한 방식으로 트랜스폼을 재조립
    void SetAnglesImmediate(float InPitchDeg, float InYawDeg);
    void SetRotationFromEulerAngles(FVector InAngles);

    // 선택: 스무딩/보간 캐시가 있다면 여기서 동기화
    void SyncRotationCache();

    void SetPerspectiveCameraInput(bool InPerspectiveCameraInput);

protected:
    ~ACameraActor() override;

public:
    UCameraComponent* GetCameraComponent() const { return CameraComponent; }
    
    // Matrices
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio) const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const;
    FMatrix GetViewProjectionMatrix() const;

    // Directions (world)
    FVector GetForward() const;
    FVector GetRight() const;
    FVector GetUp() const;

    // Camera control methods
    void SetMouseSensitivity(float Sensitivity) { MouseSensitivity = Sensitivity; }
    void SetMoveSpeed(float Speed) { CameraMoveSpeed = Speed; }
    
    // Camera state getters
    float GetCameraYaw() const { return CameraYawDeg; }
    float GetCameraPitch() const { return CameraPitchDeg; }
    void SetCameraYaw(float Yaw) { CameraYawDeg = Yaw; }
    void SetCameraPitch(float Pitch) { CameraPitchDeg = Pitch; }

    float GetCameraSpeed() { return CameraMoveSpeed; }
    void SetCameraSpeed(float InSpeed) { CameraMoveSpeed = InSpeed; EditorINI["CameraSpeed"] = std::to_string(CameraMoveSpeed); }

    void ProcessEditorCameraInput(float DeltaSeconds);

    // ───── 직렬화 관련 ────────────────────────────
    void OnSerialized() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_ACTOR_DUPLICATE(ACameraActor)
private:
    UCameraComponent* CameraComponent = nullptr;
    
    // Camera control parameters
    float MouseSensitivity = 0.1f;  // 기존 World에서 사용하던 값으로 조정
    float CameraMoveSpeed = 10.0f;
    
    // Camera rotation state (cumulative)
    float CameraYawDeg = 0.0f;   // World Up(Y) based Yaw (unlimited accumulation)
    float CameraPitchDeg = 0.0f; // Local Right based Pitch (limited)

    bool PerspectiveCameraInput = false;
    

    // Camera input processing methods
    void ProcessCameraRotation(float DeltaSeconds);
    void ProcessCameraMovement(float DeltaSeconds);
};

