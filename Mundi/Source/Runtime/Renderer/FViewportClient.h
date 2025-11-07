#pragma once
#include "Vector.h"
#include"Enums.h"

class FViewport;
class UWorld;
class UCameraComponent;


/**
 * @brief 뷰포트 클라이언트 - UE의 FViewportClient를 모방
 */
class FViewportClient
{
public:
    FViewportClient();
    virtual ~FViewportClient();

    // 렌더링
    virtual void Draw(FViewport* Viewport);
    virtual void Tick(float DeltaTime);

    // 입력 처리
    virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y);
    virtual void MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button);
    virtual void MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button);
    virtual void MouseWheel(float DeltaSeconds);
    virtual void KeyDown(FViewport* Viewport, int32 KeyCode) {}
    virtual void KeyUp(FViewport* Viewport, int32 KeyCode) {}

    // 뷰포트 설정
    void SetViewportType(EViewportType InType) { ViewportType = InType; }
    EViewportType GetViewportType() const { return ViewportType; }

    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

    void SetCamera(ACameraActor* InCamera) { Camera = InCamera; }
    ACameraActor* GetCamera() const { return Camera; }

    // 뷰포트 활성화 상태
    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    // 카메라 매트릭스 계산
    FMatrix GetViewMatrix() const;


    // 뷰포트별 카메라 설정
    void SetupCameraMode();
    void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }

    EViewModeIndex GetViewModeIndex() { return ViewModeIndex;}

    // ========== Piloting (카메라 오버라이드) ==========
    void StartPiloting(AActor* TargetActor);
    void StopPiloting();
    bool IsPiloting() const { return bIsPiloting; }
    AActor* GetPilotActor() const { return PilotTargetActor; }

protected:
    EViewportType ViewportType = EViewportType::Perspective;
    UWorld* World = nullptr;
    ACameraActor* Camera = nullptr;
    int32 MouseLastX{};
    int32 MouseLastY{};
    bool bIsMouseButtonDown = false;
    bool bIsMouseRightButtonDown = false;
    bool bIsActive = false;  // 뷰포트 활성화 상태
    static FVector CameraAddPosition;


    // 직교 뷰용 카메라 설정
    uint32 OrthographicAddXPosition;
    uint32  OrthographicAddYPosition;
    float OrthographicZoom = 30.0f;
    //뷰모드
    EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Lit;

    //원근 투영 기본값
    bool PerspectiveCameraInput = false;
    FVector PerspectiveCameraPosition = FVector(-5.0f, 5.0f, 5.0f);
    FVector PerspectiveCameraRotation = FVector(0.0f, 22.5f, -45.0f);
    float PerspectiveCameraFov=60;

    // ========== Piloting 상태 ==========
    AActor* PilotTargetActor = nullptr;     // 현재 부착된 Actor
    bool bIsPiloting = false;               // Piloting 중인지 여부

    // 원본 카메라 상태 저장
    FVector SavedCameraPosition;
    FVector SavedCameraRotation;
    float SavedCameraFov;
};