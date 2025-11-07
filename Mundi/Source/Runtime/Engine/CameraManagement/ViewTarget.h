#pragma once
#include "Vector.h"

class AActor;

struct FPostProcessSettings
{
    // Letter Box
    float LetterBoxSize = 0.0f;
    float LetterBoxOpacity = 1.0f;

    // Vignetting
    float VignettingRadius = 0.5f;
    float VignettingSoftness = 0.5f;
	FVector4 VignettingColor{};

    //// Color Grading
    //float Saturation = 1.0f;
    //float Contrast = 1.0f;
    //FLinearColor ColorTint = FLinearColor(1, 1, 1, 1);

    //// Bloom
    //float BloomIntensity = 0.0f;
    //float BloomThreshold = 1.0f;

    //// Depth of Field
    //float DOFDistance = 100.0f;
    //float DOFBlurAmount = 0.0f;
};

// ViewTarget 정보를 담는 구조체
// 현재 카메라가 바라보는 대상과 최종 POV(Point of View) 정보를 저장
struct FViewTarget
{
	// 타겟 액터 (일반적으로 플레이어의 Pawn)
	AActor* Target = nullptr;

	// 최종 카메라 POV (Point of View)
	FVector POV_Location;
	FQuat POV_Rotation;
	float POV_FOV = 60.0f;

	FPostProcessSettings PostProcessSettings;

	FViewTarget()
		: Target(nullptr)
		, POV_Location(0.0f, 0.0f, 0.0f)
		, POV_Rotation(FQuat::Identity())
		, POV_FOV(60.0f)
	{
	}
};
