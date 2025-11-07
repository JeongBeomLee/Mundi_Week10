#include "pch.h"
#include "UCameraModifier_CustomBlend.h"
#include "CameraBlendPresetLibrary.h"

IMPLEMENT_CLASS(UCameraModifier_CustomBlend)

BEGIN_PROPERTIES(UCameraModifier_CustomBlend)
	// 내부 상태만 관리하므로 프로퍼티 노출 없음
END_PROPERTIES()

UCameraModifier_CustomBlend::UCameraModifier_CustomBlend()
{
	// 기본 설정
	SetAlpha(1.0f);  // 항상 활성화
	EnableModifier();
	SetPriority(1);  // CameraShake보다 높은 우선순위
}

void UCameraModifier_CustomBlend::ApplyBlendPreset(
	const FString& PresetName,
	const FVector& InTargetLocationOffset,
	const FQuat& InTargetRotationOffset,
	float InTargetFOVOffset
)
{
	// 라이브러리에서 프리셋 검색
	UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
	if (!Library)
	{
		UE_LOG("UCameraModifier_CustomBlend: 라이브러리가 초기화되지 않았습니다.");
		return;
	}

	const FCameraBlendPreset* Preset = Library->GetPreset(PresetName);
	if (!Preset)
	{
		UE_LOG("UCameraModifier_CustomBlend: 프리셋을 찾을 수 없음 - %s", PresetName.c_str());
		return;
	}

	// 프리셋 적용
	ApplyBlendPreset(*Preset, InTargetLocationOffset, InTargetRotationOffset, InTargetFOVOffset);
}

void UCameraModifier_CustomBlend::ApplyBlendPreset(
	const FCameraBlendPreset& Preset,
	const FVector& InTargetLocationOffset,
	const FQuat& InTargetRotationOffset,
	float InTargetFOVOffset
)
{
	// 블렌드 파라미터 설정
	BlendParams = Preset.BlendParams;

	// 목표 오프셋 저장
	TargetLocationOffset = InTargetLocationOffset;
	TargetRotationOffset = InTargetRotationOffset;
	TargetFOVOffset = InTargetFOVOffset;

	// 블렌드 시작
	bIsBlending = true;
	BlendTimeElapsed = 0.0f;

	UE_LOG("UCameraModifier_CustomBlend: 프리셋 '%s' 블렌드 시작 (%.2fs, Loop: %s)",
		Preset.PresetName.c_str(),
		Preset.BlendParams.BlendTime,
		bLooping ? "Yes" : "No");
}

void UCameraModifier_CustomBlend::ModifyCamera(
	float DeltaTime,
	FVector& InOutViewLocation,
	FQuat& InOutViewRotation,
	float& InOutFOV
)
{
	// 부모 클래스 호출
	UCameraModifier::ModifyCamera(DeltaTime, InOutViewLocation, InOutViewRotation, InOutFOV);

	if (!bIsBlending)
		return;

	// 첫 프레임: 시작 상태 저장
	if (BlendTimeElapsed == 0.0f)
	{
		StartLocation = InOutViewLocation;
		StartRotation = InOutViewRotation;
		StartFOV = InOutFOV;
	}

	// 시간 업데이트
	BlendTimeElapsed += DeltaTime;

	// 블렌드 진행률 계산 (0~1)
	float BlendAlpha = 0.0f;
	if (BlendParams.BlendTime > 0.0f)
	{
		if (bLooping)
		{
			// 주파수 = 1 / BlendTime (1사이클 = BlendTime초)
			float Frequency = 1.0f / BlendParams.BlendTime;
			float Phase = BlendTimeElapsed * Frequency * 2.0f * 3.14159265f; // 2*PI*f*t

			// sin() 결과: -1~1 → 0~1로 변환
			float SineWave = sinf(Phase);
			BlendAlpha = (SineWave + 1.0f) * 0.5f;  // 0~1 범위
		}
		else
		{
			// 일반 모드: 선형 시간 진행
			BlendAlpha = FMath::Clamp(BlendTimeElapsed / BlendParams.BlendTime, 0.0f, 1.0f);
		}
	}
	else
	{
		BlendAlpha = 1.0f;
	}

	// 프리셋 커브를 BlendAlpha에 적용
	// BlendParams를 임시로 업데이트 (커브 계산용)
	float OriginalBlendTimeRemaining = BlendParams.BlendTimeRemaining;
	BlendParams.BlendTimeRemaining = BlendParams.BlendTime * (1.0f - BlendAlpha);

	float LocationAlpha = BlendParams.CalculateLocationAlpha();
	float RotationAlpha = BlendParams.CalculateRotationAlpha();
	float FOVAlpha = BlendParams.CalculateFOVAlpha();

	// 복원
	BlendParams.BlendTimeRemaining = OriginalBlendTimeRemaining;

	// Location 블렌딩 (오프셋 적용)
	FVector TargetLocation = StartLocation + TargetLocationOffset;
	InOutViewLocation = FMath::Lerp(StartLocation, TargetLocation, LocationAlpha);

	// Rotation 블렌딩 (오프셋 적용)
	FQuat TargetRotation = StartRotation * TargetRotationOffset;
	InOutViewRotation = FQuat::Slerp(StartRotation, TargetRotation, RotationAlpha);

	// FOV 블렌딩 (오프셋 적용)
	float TargetFOV = StartFOV + TargetFOVOffset;
	InOutFOV = FMath::Lerp(StartFOV, TargetFOV, FOVAlpha);

	// 블렌드 완료 처리 (Looping이 아닐 때만)
	if (!bLooping && BlendAlpha >= 1.0f)
	{
		// 반복 없음: 블렌딩 종료
		bIsBlending = false;
		DisableModifier();  // 모디파이어 비활성화
	}

	// BlendParams의 블렌드 시간 업데이트는 Looping이 아닐 때만
	if (!bLooping)
	{
		BlendParams.UpdateBlend(DeltaTime);
	}
}
