#include "pch.h"
#include "UCameraModifier_LetterBox.h"

IMPLEMENT_CLASS(UCameraModifier_LetterBox)

BEGIN_PROPERTIES(UCameraModifier_LetterBox)
    // 내부적으로 관리되므로 에디터에 노출하지 않음
END_PROPERTIES()

UCameraModifier_LetterBox::UCameraModifier_LetterBox()
	: StartLetterBoxSize(0.0f)
    , StartLetterBoxOpacity(1.0f)
    , CurrentLetterBoxSize(0.0f)
    , CurrentLetterBoxOpacity(1.0f)
    , TargetLetterBoxSize(0.0f)
    , TargetLetterBoxOpacity(1.0f)
{
    // 부모 클래스 기본값 설정
    AlphaInTime = 1.0f;
    AlphaOutTime = 1.0f;
    bDisabled = true;
	bUseRealDeltaTime = true;
}

void UCameraModifier_LetterBox::ModifyPostProcess(
    float DeltaTime,
    float& PostProcessBlendWeight,
    FPostProcessSettings& PostProcessSettings
)
{
    if (bDisabled)
    {
        return;
    }

    // Alpha 값에 따라 레터박스 크기와 불투명도를 보간
    CurrentLetterBoxSize = FMath::Lerp(StartLetterBoxSize, TargetLetterBoxSize, Alpha);
    CurrentLetterBoxOpacity = FMath::Lerp(StartLetterBoxOpacity, TargetLetterBoxOpacity, Alpha);

    // PostProcessSettings에 레터박스 설정 적용
    PostProcessSettings.LetterBoxSize = CurrentLetterBoxSize;
    PostProcessSettings.LetterBoxOpacity = CurrentLetterBoxOpacity;

    // Alpha 값을 블렌드 가중치로 설정(임시)
    PostProcessBlendWeight = Alpha;
}

void UCameraModifier_LetterBox::SetFadeIn(float InSize, float InOpacity, float InFadeInTime, float InStartSize)
{
	StartLetterBoxSize = InStartSize;
	StartLetterBoxOpacity = 1.0f;

    TargetLetterBoxSize = FMath::Clamp(InSize, 0.0f, 1.0f);
    TargetLetterBoxOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);

    SetAlphaInTime(InFadeInTime);
    EnableModifier();
    SetIsFadingIn(true);
    SetAlpha(0.0f);
}

void UCameraModifier_LetterBox::SetFadeOut(float InFadeOutTime, float InStartSize)
{
	StartLetterBoxSize = InStartSize;
	StartLetterBoxOpacity = 1.0f;

    TargetLetterBoxSize = 0.0f;
    TargetLetterBoxOpacity = 1.0f;

    SetAlphaInTime(InFadeOutTime);
    EnableModifier();
    SetIsFadingIn(true);
    SetAlpha(0.0f);
}