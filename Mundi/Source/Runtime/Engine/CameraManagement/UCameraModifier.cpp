#include "pch.h"
#include "UCameraModifier.h"
#include "PlayerCameraManager.h"

IMPLEMENT_CLASS(UCameraModifier)

BEGIN_PROPERTIES(UCameraModifier)
	ADD_PROPERTY_RANGE(float, AlphaInTime, "Camera Modifier", 0.0f, 10.0f, false, "블렌드 인 시간")
	ADD_PROPERTY_RANGE(float, AlphaOutTime, "Camera Modifier", 0.0f, 10.0f, false, "블렌드 아웃 시간")
	ADD_PROPERTY_RANGE(float, Alpha, "Camera Modifier", 0.0f, 1.0f, false, "현재 알파 값")
	ADD_PROPERTY(bool, bIsFadingIn, "Camera Modifier", false, "페이드 인 중 여부")
	ADD_PROPERTY(bool, bDisabled, "Camera Modifier", false, "비활성화 여부")
	ADD_PROPERTY_RANGE(int32, Priority, "Camera Modifier", 0, 255, false, "우선순위")
END_PROPERTIES()

void UCameraModifier::AddedToCamera( APlayerCameraManager* Camera ) 
{
    CameraOwner = Camera;
}

void UCameraModifier::ModifyCamera(
    float DeltaTime,
    FVector& InOutViewLocation,
    FQuat& InOutViewRotation,
    float& InOutFOV
)
{
    if (CameraOwner)
    {
        // note: pushing these through the cached PP blend system in the camera to get
        // proper layered blending, rather than letting subsequent mods stomp over each other in the 
        // InOutPOV struct.
        {
            float PPBlendWeight = 0.f;
            FPostProcessSettings PPSettings = CameraOwner->GetViewTarget_Internal().PostProcessSettings;
			
            //  Let native code modify the post process settings.
            ModifyPostProcess(
                DeltaTime,
                PPBlendWeight,
                PPSettings);

            if (PPBlendWeight > 0.f)
            {
                 CameraOwner->SetPostProcessSettings(PPSettings);
            }
        }
    }
}

void UCameraModifier::ModifyPostProcess(
    float DeltaTime,
    float& PostProcessBlendWeight,
    FPostProcessSettings& PostProcessSettings
) {}

float UCameraModifier::GetTargetAlpha()
{
    return bIsFadingIn ? 1.f : 0.f;  // bIsFadingIn이 true면 1.0을 향해 증가
}

bool UCameraModifier::IsDisabled() const
{
    return bDisabled;
}

void UCameraModifier::EnableModifier()
{
    bDisabled = false;
}

void UCameraModifier::DisableModifier()
{
    bDisabled = true;
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
    float const TargetAlpha = GetTargetAlpha();
    float const BlendTime = (TargetAlpha == 0.f) ? AlphaOutTime : AlphaInTime;

    // interpolate!
    if (BlendTime <= 0.f)
    {
        // no blendtime means no blending, just go directly to target alpha
        Alpha = TargetAlpha;
    }
    else if (Alpha > TargetAlpha)
    {
        // interpolate downward to target, while protecting against overshooting
        Alpha = std::max<float>(Alpha - DeltaTime / BlendTime, TargetAlpha);
    }
    else
    {
        // interpolate upward to target, while protecting against overshooting
        Alpha = std::min<float>(Alpha + DeltaTime / BlendTime, TargetAlpha);
    }

    // Alpha가 1.0에 도달하면 바로 비활성화
    if (bIsFadingIn && Alpha >= 1.f)
    {
        DisableModifier();
    }
}

float UCameraModifier::GetAlphaInTime() const
{
    return AlphaInTime;
}
void UCameraModifier::SetAlphaInTime(const float InAlphaInTime)
{
    AlphaInTime = InAlphaInTime;
}

float UCameraModifier::GetAlphaOutTime() const
{
    return AlphaOutTime;
}
void UCameraModifier::SetAlphaOutTime(const float InAlphaOutTime)
{
    AlphaOutTime = InAlphaOutTime;
}

float UCameraModifier::GetAlpha() const
{
    return Alpha;
}
void UCameraModifier::SetAlpha(const float InAlpha)
{
    Alpha = InAlpha;
}

bool UCameraModifier::IsFadingIn() const
{
    return bIsFadingIn;
}
void UCameraModifier::SetIsFadingIn(const bool InIsFadingIn)
{
    bIsFadingIn = InIsFadingIn;
}

uint8 UCameraModifier::GetPriority() const
{
    return Priority;
}
void UCameraModifier::SetPriority(const uint8 InPriority)
{
    Priority = InPriority;
}