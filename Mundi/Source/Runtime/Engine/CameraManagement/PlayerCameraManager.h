#pragma once
#include "Actor.h"
#include "ViewTarget.h"
#include "Color.h"
#include "CameraTypes.h"

class UCameraComponent;
class UCameraModifier;
class USpringArmComponent;

// 블렌딩 완료 델리게이트 (파라미터 없음)
DECLARE_MULTICAST_DELEGATE(FOnBlendComplete);

// 플레이어 카메라를 관리하는 매니저 클래스
// PlayerController가 소유하며, ViewTarget의 카메라를 렌더링에 제공
class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)
	GENERATED_REFLECTION_BODY()

	APlayerCameraManager();

protected:
	~APlayerCameraManager() override;

public:
	// 매 프레임 카메라 업데이트 (PlayerController::Tick에서 호출)
	virtual void UpdateCamera(float DeltaTime);

	// ViewTarget 설정 (현재 렌더링할 액터, 일반적으로 플레이어의 Pawn)
	void SetViewTarget(AActor* NewViewTarget);
	AActor* GetViewTarget() const { return ViewTarget.Target; }

	// 렌더링용 카메라 컴포넌트 반환
	UCameraComponent* GetCameraComponentForRendering() const;

	// ViewTarget POV 접근자
	const FViewTarget& GetViewTarget_Internal() const { return ViewTarget; }
	FVector GetCameraLocation() const { return ViewTarget.POV_Location; }
	FQuat GetCameraRotation() const { return ViewTarget.POV_Rotation; }
	float GetCameraFOV() const { return ViewTarget.POV_FOV; }

	// 카메라 스타일 설정 (프리셋)
	void SetCameraStyle(const FName& NewStyle) { CameraStyle = NewStyle; }
	FName GetCameraStyle() const { return CameraStyle; }

	// 블렌딩과 함께 ViewTarget 설정
	void SetViewTargetWithBlend(
		AActor* NewViewTarget,
		float BlendTime = 0.0f,
		EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Cubic,
		float BlendExp = 2.0f,
		bool bLockOutgoing = false
	);

	// 커스텀 베지어 커브로 ViewTarget 블렌딩
	void SetViewTargetWithBezierBlend(
		AActor* NewViewTarget,
		const FViewTargetTransitionParams& CustomBlendParams
	);

	// 프리셋 이름으로 ViewTarget 블렌딩
	void SetViewTargetWithBlendPreset(
		AActor* NewViewTarget,
		const FString& PresetName
	);

	// 프리셋 객체로 ViewTarget 블렌딩
	void SetViewTargetWithBlendPreset(
		AActor* NewViewTarget,
		const struct FCameraBlendPreset& Preset
	);

	// 고정 위치/회전으로 블렌딩 (더미 액터 불필요)
	void BlendToTransform(
		const FVector& TargetLocation,
		const FQuat& TargetRotation,
		float TargetFOV,
		float BlendTime,
		EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Cubic,
		float BlendExp = 2.0f
	);

	// 커스텀 베지어로 고정 위치 블렌딩
	void BlendToTransformWithBezier(
		const FVector& TargetLocation,
		const FQuat& TargetRotation,
		float TargetFOV,
		const FViewTargetTransitionParams& CustomBlendParams
	);

	// 프리셋 이름으로 고정 위치 블렌딩
	void BlendToTransformWithPreset(
		const FVector& TargetLocation,
		const FQuat& TargetRotation,
		float TargetFOV,
		const FString& PresetName
	);

	// 프리셋 객체로 고정 위치 블렌딩
	void BlendToTransformWithPreset(
		const FVector& TargetLocation,
		const FQuat& TargetRotation,
		float TargetFOV,
		const struct FCameraBlendPreset& Preset
	);

	// 블렌딩 중단
	void StopBlending();

	// 초기 카메라 위치/회전/FOV를 즉시 설정
	void SetCameraTransform(const FVector& Location, const FQuat& Rotation, float FOV);

	// 블렌딩 진행 중인지 확인
	bool IsBlending() const { return BlendParams.IsBlending(); }

	// 현재 블렌드 파라미터 접근
	const FViewTargetTransitionParams& GetBlendParams() const { return BlendParams; }

	// 블렌딩 완료 델리게이트 (ViewTarget 전환 완료 시 호출)
	FOnBlendComplete OnBlendComplete;

	// Fade 효과 (구조만 구현, 실제 렌더링 연동은 추후)
	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FLinearColor& Color);
	void StopCameraFade();

	float GetFadeAmount() const;
	bool IsFading() const { return bFading; }
	FLinearColor GetFadeColor() const { return FadeColor; }

	// 카메라 모디파이어 관리
	void AddCameraModifier(UCameraModifier* NewModifier);
	void RemoveCameraModifier(UCameraModifier* ModifierToRemove);
	void RemoveDisabledCameraModifiers(); // 비활성화된 모디파이어 자동 제거
	TArray<UCameraModifier*>& GetModifierList();

	// 카메라 포스트 프로세싱 관리
	void SetPostProcessSettings(const FPostProcessSettings& NewSettings)
	{
		ViewTarget.PostProcessSettings = NewSettings;
	}
	void ResetPostProcessSettings();

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(APlayerCameraManager)

protected:
	// ViewTarget 업데이트
	virtual void UpdateViewTarget(float DeltaTime);

	// ViewTarget 블렌딩 업데이트
	void UpdateViewTargetBlending(float DeltaTime);

	// Fade 효과 업데이트
	void UpdateFade(float DeltaTime);

	// 카메라 모디파이어 적용 (추후 팀원 구현과 통합)
	void ApplyCameraModifiers(float DeltaTime, FVector& InOutLocation, FQuat& InOutRotation, float& InOutFOV);

	// ViewTarget으로부터 카메라 컴포넌트를 찾아서 반환
	UCameraComponent* FindCameraComponent(AActor* InActor) const;

	// ViewTarget으로부터 SpringArm 컴포넌트를 찾아서 반환
	USpringArmComponent* FindSpringArmComponent(AActor* InActor) const;

protected:
	// Fade 효과 관련 변수
	FLinearColor FadeColor;
	float FadeAmount;
	float FadeAlphaFrom; // Fade 시작 알파
	float FadeAlphaTo;   // Fade 종료 알파
	float FadeTime;
	float FadeTimeRemaining;
	bool bFading;

	// 카메라 스타일 (프리셋)
	FName CameraStyle;

	// ViewTarget (현재 렌더링 대상)
	FViewTarget ViewTarget;

	// 블렌딩 관련 변수
	FViewTarget PendingViewTarget;              // 전환할 대상 ViewTarget
	FViewTargetTransitionParams BlendParams;    // 블렌드 파라미터

	// 블렌딩 시작 시점의 상태 저장
	FVector BlendStartLocation;
	FQuat BlendStartRotation;
	float BlendStartFOV;
	float BlendStartSpringArmLength;

	// 카메라 모디파이어 리스트
	TArray<UCameraModifier*> ModifierList;
};
