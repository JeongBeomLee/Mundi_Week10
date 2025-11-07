#pragma once
#include "UCameraModifier.h"
#include "CameraBlendPreset.h"

// 프리셋을 사용하여 카메라에 일회성 블렌드 효과를 적용하는 모디파이어
// ViewTarget 전환 없이 카메라 위치/회전/FOV를 블렌딩
class UCameraModifier_CustomBlend : public UCameraModifier
{
public:
	DECLARE_CLASS(UCameraModifier_CustomBlend, UCameraModifier)
	GENERATED_REFLECTION_BODY()

	UCameraModifier_CustomBlend();
	virtual ~UCameraModifier_CustomBlend() = default;

	// 프리셋 이름으로 블렌드 시작
	void ApplyBlendPreset(
		const FString& PresetName,
		const FVector& TargetLocationOffset,
		const FQuat& TargetRotationOffset,
		float TargetFOVOffset
	);

	// 프리셋 객체로 블렌드 시작
	void ApplyBlendPreset(
		const FCameraBlendPreset& Preset,
		const FVector& TargetLocationOffset,
		const FQuat& TargetRotationOffset,
		float TargetFOVOffset
	);

	// 반복 블렌드 설정 (위아래 흔들림 등)
	void SetLooping(bool bInLoop) { bLooping = bInLoop; }
	bool IsLooping() const { return bLooping; }

	// UCameraModifier 인터페이스
	virtual void ModifyCamera(
		float DeltaTime,
		FVector& InOutViewLocation,
		FQuat& InOutViewRotation,
		float& InOutFOV
	) override;

private:
	// 블렌드 파라미터
	FViewTargetTransitionParams BlendParams;

	// 시작/목표 상태
	FVector StartLocation;
	FQuat StartRotation;
	float StartFOV;

	FVector TargetLocationOffset;  // 오프셋 (절대 위치가 아님)
	FQuat TargetRotationOffset;
	float TargetFOVOffset;

	// 블렌드 상태
	bool bIsBlending = false;
	float BlendTimeElapsed = 0.0f;

	// 반복 재생
	bool bLooping = false;
};
