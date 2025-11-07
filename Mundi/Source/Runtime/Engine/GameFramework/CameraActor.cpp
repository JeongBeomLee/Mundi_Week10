#include "pch.h"
#include "CameraActor.h"
#include "ObjectFactory.h"
#include "CameraComponent.h"
#include "UIManager.h"
#include "InputManager.h"
#include "Vector.h"
#include "USlateManager.h"

// 예전 World에서 사용하던 전역 변수들 (임시)
static float MouseSensitivity = 0.05f;  // 적당한 값으로 조정

IMPLEMENT_CLASS(ACameraActor)

BEGIN_PROPERTIES(ACameraActor)
	MARK_AS_SPAWNABLE("카메라", "씬을 렌더링하는 카메라 액터입니다.")
END_PROPERTIES()

ACameraActor::ACameraActor()
{

    Name = "Camera Actor";
    // 카메라 컴포넌트
    //CameraComponent = NewObject<UCameraComponent>();
    RootComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
    CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
    CameraComponent->SetupAttachment(RootComponent);
    //SceneComponents.Add(CameraComponent);

    if(EditorINI.count("CameraSpeed"))
    {
        try
        {
            float temp = std::stof(EditorINI["CameraSpeed"]);
            SetCameraSpeed(temp);
        }
        catch (...)
        {
            SetCameraSpeed(10.f);
        }
    }
    else
    {
        SetCameraSpeed(10.f);
    }
}
void ACameraActor::SetPerspectiveCameraInput(bool InPerspectiveCameraInput) {
    PerspectiveCameraInput = InPerspectiveCameraInput;
}

void ACameraActor::Tick(float DeltaSeconds)
{
    //if (PerspectiveCameraInput) {
       // ProcessEditorCameraInput(DeltaSeconds);

    //}
    // 우클릭 드래그 종료시 UI와 동기화
    UInputManager& InputManager = UInputManager::GetInstance();
    if (InputManager.IsMouseButtonReleased(RightButton))
    {
        UUIManager& UIManager = UUIManager::GetInstance();
        FVector UICameraDeg = UIManager.GetTempCameraRotation();
        CameraYawDeg = UICameraDeg.Y;
        CameraPitchDeg = UICameraDeg.X;
    }
}

static inline float ClampPitch(float P)
{
    // 입력 경로와 동일한 클램프 적용
    return std::clamp(P, -89.9f, 89.9f);
}

/**
 * @brief 입력받은 오일러 각도(Roll, Pitch, Yaw)로 액터의 회전을 설정합니다.
 */
void ACameraActor::SetRotationFromEulerAngles(FVector InEulerAngles)
{
    // Roll은 사용하지 않으므로 Pitch, Yaw만 추출하여 설정합니다.
    SetAnglesImmediate(InEulerAngles.Y, InEulerAngles.Z);
}

void ACameraActor::SetAnglesImmediate(float InPitchDeg, float InYawDeg)
{
    // 내부 상태 갱신
    CameraPitchDeg = ClampPitch(InPitchDeg);
    CameraYawDeg = InYawDeg;

    // 입력 경로와 동일한 축/순서로 쿼터니언 조립
    // 사용 중인 좌표계: Pitch = Y축, Yaw = Z축 (질문에서 언급하신 매핑)
    const float RadPitch = DegreesToRadians(CameraPitchDeg);
    const float RadYaw = DegreesToRadians(CameraYawDeg);

    const FQuat QYaw = FQuat::FromAxisAngle(FVector{ 0, 0, 1 }, RadYaw);
    const FQuat QPitch = FQuat::FromAxisAngle(FVector{ 0, 1, 0 }, RadPitch);

    // 조립 순서도 입력 경로와 동일하게
    const FQuat FinalRot = QYaw * QPitch;

    SetActorRotation(FinalRot);

    // 스무딩/보간 캐시가 있다면 현재 상태로 초기화
    SyncRotationCache();
}

void ACameraActor::SyncRotationCache()
{
    // 만약 ProcessCameraRotation에서 이전 각도/쿼터니언을 보간에 사용한다면
    // 그 캐시를 현재 상태로 맞춰 1프레임 스냅을 방지
    // 예시:
    // LastYawDeg = CameraYawDeg;
    // LastPitchDeg = CameraPitchDeg;
    // LastRotQuat = GetActorRotation();
}

ACameraActor::~ACameraActor()
{
    if (CameraComponent)
    {
        ObjectFactory::DeleteObject(CameraComponent);
    }
    CameraComponent = nullptr;
}

FMatrix ACameraActor::GetViewMatrix() const
{
    return CameraComponent ? CameraComponent->GetViewMatrix() : FMatrix::Identity();
}

FMatrix ACameraActor::GetProjectionMatrix() const
{
    return CameraComponent ? CameraComponent->GetProjectionMatrix() : FMatrix::Identity();
}

FMatrix ACameraActor::GetProjectionMatrix(float ViewportAspectRatio) const
{
    return CameraComponent ? CameraComponent->GetProjectionMatrix(ViewportAspectRatio) : FMatrix::Identity();
}
FMatrix ACameraActor::GetProjectionMatrix(float ViewportAspectRatio,FViewport* Viewport) const
{
    return CameraComponent ? CameraComponent->GetProjectionMatrix(ViewportAspectRatio, Viewport) : FMatrix::Identity();
}


FMatrix ACameraActor::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

FVector ACameraActor::GetForward() const
{
    return CameraComponent ? CameraComponent->GetForward() : FVector(1, 0, 0);
}

FVector ACameraActor::GetRight() const
{
    return CameraComponent ? CameraComponent->GetRight() : FVector(0, 1, 0);
}

FVector ACameraActor::GetUp() const
{
    return CameraComponent ? CameraComponent->GetUp() : FVector(0, 0, 1);
}

void ACameraActor::ProcessEditorCameraInput(float DeltaSeconds)
{
    UInputManager& InputManager = UInputManager::GetInstance();

    bool bRightButtonDown = InputManager.IsMouseButtonDown(RightButton);

    // 우클릭 드래그로 카메라 회전 및 이동
    if (bRightButtonDown)
    {
        ProcessCameraRotation(DeltaSeconds);
        ProcessCameraMovement(DeltaSeconds);
    }
}

void ACameraActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    // 수동 직렬화 (프로퍼티로 등록되지 않은 변수들)
    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "MouseSensitivity", MouseSensitivity, 0.1f);
        FJsonSerializer::ReadFloat(InOutHandle, "CameraMoveSpeed", CameraMoveSpeed, 10.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "CameraYawDeg", CameraYawDeg, 0.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "CameraPitchDeg", CameraPitchDeg, 0.0f);
        FJsonSerializer::ReadBool(InOutHandle, "PerspectiveCameraInput", PerspectiveCameraInput, false);

        // CameraComponent 포인터는 DuplicateSubObjects()에서 복원됨
        for (UActorComponent* Component : OwnedComponents)
        {
            if (UCameraComponent* CameraComp = Cast<UCameraComponent>(Component))
            {
                CameraComponent = CameraComp;
                break;
            }
        }
    }
    else
    {
        InOutHandle["MouseSensitivity"] = MouseSensitivity;
        InOutHandle["CameraMoveSpeed"] = CameraMoveSpeed;
        InOutHandle["CameraYawDeg"] = CameraYawDeg;
        InOutHandle["CameraPitchDeg"] = CameraPitchDeg;
        InOutHandle["PerspectiveCameraInput"] = PerspectiveCameraInput;
    }
}

void ACameraActor::OnSerialized()
{
    Super::OnSerialized();

    

}

void ACameraActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (UCameraComponent* CameraComp = Cast<UCameraComponent>(Component))
        {
            CameraComponent = CameraComp;
            break;
        }
    }
}

static inline float Clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void ACameraActor::ProcessCameraRotation(float DeltaSeconds)
{
    UInputManager& InputManager = UInputManager::GetInstance();
    UUIManager& UIManager = UUIManager::GetInstance();
    
    FVector2D MouseDelta = InputManager.GetMouseDelta();
    
    if (MouseDelta.X == 0.0f && MouseDelta.Y == 0.0f) return;

    // 1) Pitch/Yaw만 누적 (Roll은 UIManager에서 관리하는 값으로 고정)
    CameraYawDeg += MouseDelta.X * MouseSensitivity;
    CameraPitchDeg += MouseDelta.Y * MouseSensitivity;

    // 각도 정규화 및 Pitch 제한
    CameraYawDeg = NormalizeAngleDeg(CameraYawDeg); // -180 ~ 180 범위로 정규화
    CameraPitchDeg = Clamp(CameraPitchDeg, -89.0f, 89.0f); // Pitch는 짐벌락 방지를 위해 제한

    // 2) UIManager의 저장된 Roll 값을 가져와서 축별 쿼터니언 합성
    float CurrentRoll = UIManager.GetStoredRoll();

    // 축별 개별 쿼터니언 생성
    FQuat PitchQuat = FQuat::FromAxisAngle(FVector(0, 1, 0), DegreesToRadians(CameraPitchDeg));
    FQuat YawQuat = FQuat::FromAxisAngle(FVector(0, 0, 1), DegreesToRadians(CameraYawDeg));
    FQuat RollQuat = FQuat::FromAxisAngle(FVector(1, 0, 0), DegreesToRadians(CurrentRoll));

    // RzRxRy 순서로 회전 합성 (Roll(Z) → Pitch(X) → Yaw(Y))
    FQuat FinalRotation = YawQuat * PitchQuat * RollQuat;
    FinalRotation.Normalize();

    SetActorRotation(FinalRotation);

    // 3) UIManager에 마우스로 변경된 Pitch/Yaw 값 동기화
    UIManager.UpdateMouseRotation(CameraPitchDeg, CameraYawDeg);
}

static inline FVector RotateByQuat(const FVector& Vector, const FQuat& Quat)
{
    return Quat.RotateVector(Vector);
}

void ACameraActor::ProcessCameraMovement(float DeltaSeconds)
{
    UInputManager& InputManager = UInputManager::GetInstance();
    
    FVector Move(0, 0, 0);

    // 1) 카메라 회전(쿼터니언)에서 로컬 기저 추출 (스케일 영향 제거)
    const FQuat Quat = GetActorRotation(); // (x,y,z,w)
    // DirectX LH 기준: Right=+X, Up=+Y, Forward=+Z
    const FVector Right = Quat.RotateVector(FVector(0, 1, 0)).GetNormalized();
    const FVector Up = Quat.RotateVector(FVector(0, 0, 1)).GetNormalized();
    const FVector Forward = Quat.RotateVector(FVector(1, 0, 0)).GetNormalized();

    // 2) 입력 누적 (WASD + QE)
    if (InputManager.IsKeyDown('W')) Move += Forward;
    if (InputManager.IsKeyDown('S')) Move -= Forward;
    if (InputManager.IsKeyDown('D')) Move += Right;
    if (InputManager.IsKeyDown('A')) Move -= Right;
    if (InputManager.IsKeyDown('E')) Move += Up;
    if (InputManager.IsKeyDown('Q')) Move -= Up;

    // 3) 이동 적용
    if (Move.SizeSquared() > 0.0f)
    {
        const float speed = CameraMoveSpeed * DeltaSeconds * 2.5f;
        Move = Move.GetNormalized() * speed;

        const FVector P = GetActorLocation();
        SetActorLocation(P + Move);
    }
}
