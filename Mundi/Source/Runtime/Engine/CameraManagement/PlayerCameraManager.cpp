#include "pch.h"
#include "PlayerCameraManager.h"
#include "ObjectFactory.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "UCameraModifier_CameraShake.h"
#include "SpringArmComponent.h"
#include "CameraBlendPresetLibrary.h"

IMPLEMENT_CLASS(APlayerCameraManager)

BEGIN_PROPERTIES(APlayerCameraManager)
	// PlayerCameraManager는 스폰 UI에 노출하지 않음 + 디테일 패널에도 노출 X (PlayerController가 자동 생성)
	ADD_PROPERTY(FLinearColor, FadeColor, "Fade", false, "Fade 색상")
	ADD_PROPERTY(float, FadeAmount, "Fade", false, "현재 Fade 양")
	ADD_PROPERTY(float, FadeAlphaFrom, "Fade", false, "Fade 시작 알파")
	ADD_PROPERTY(float, FadeAlphaTo, "Fade", false, "Fade 종료 알파")
	ADD_PROPERTY(float, FadeTime, " Fade", false, "Fade 총 시간")
	ADD_PROPERTY(float, FadeTimeRemaining, "Fade", false, "Fade 남은 시간")
	ADD_PROPERTY(bool, bFading, "Fade", false, "Fade 진행 중 여부")
	ADD_PROPERTY(FName, CameraStyle, "Camera", false, "카메라 스타일 프리셋")
END_PROPERTIES()

APlayerCameraManager::APlayerCameraManager()
	: FadeColor(FLinearColor(0.f, 0.f, 0.f))
	, FadeAmount(0.0f)
	, FadeAlphaFrom(0.0f)
	, FadeAlphaTo(0.0f)
	, FadeTime(0.0f)
	, FadeTimeRemaining(0.0f)
	, bFading(false)
	, CameraStyle("Default")
{
	Name = "PlayerCameraManager";
	RootComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
}

APlayerCameraManager::~APlayerCameraManager()
{
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	if (!ViewTarget.Target)
	{
		return;
	}

	// ViewTarget 업데이트
	UpdateViewTarget(DeltaTime);

	// ViewTarget 블렌딩 업데이트
	UpdateViewTargetBlending(DeltaTime);

	// Fade 효과 업데이트
	UpdateFade(DeltaTime);

	// 카메라 모디파이어 적용 (추후 구현)
	ApplyCameraModifiers(DeltaTime, ViewTarget.POV_Location, ViewTarget.POV_Rotation, ViewTarget.POV_FOV);
}

void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget)
{
	ViewTarget.Target = NewViewTarget;

	// 초기 POV 설정
	if (NewViewTarget)
	{
		UCameraComponent* CameraComp = FindCameraComponent(NewViewTarget);
		if (CameraComp)
		{
			ViewTarget.POV_Location = CameraComp->GetWorldLocation();
			ViewTarget.POV_Rotation = CameraComp->GetWorldRotation();
			ViewTarget.POV_FOV = CameraComp->GetFOV();
		}
		else
		{
			// CameraComponent가 없으면 액터의 Transform 사용
			ViewTarget.POV_Location = NewViewTarget->GetActorLocation();
			ViewTarget.POV_Rotation = NewViewTarget->GetActorRotation();
			ViewTarget.POV_FOV = 60.0f; // 기본값
		}
	}
}

UCameraComponent* APlayerCameraManager::GetCameraComponentForRendering() const
{
	if (!ViewTarget.Target)
	{
		return nullptr;
	}

	return FindCameraComponent(ViewTarget.Target);
}

UCameraComponent* APlayerCameraManager::FindCameraComponent(AActor* InActor) const
{
	if (!InActor)
	{
		return nullptr;
	}

	// 1. CameraActor인 경우: 내장 CameraComponent 반환
	if (ACameraActor* CameraActor = Cast<ACameraActor>(InActor))
	{
		return CameraActor->GetCameraComponent();
	}

	// 2. Character/Pawn인 경우: OwnedComponents에서 CameraComponent 검색
	const TSet<UActorComponent*>& Components = InActor->GetOwnedComponents();
	for (UActorComponent* Component : Components)
	{
		if (UCameraComponent* CameraComp = Cast<UCameraComponent>(Component))
		{
			return CameraComp;
		}
	}

	// 3. 찾지 못함
	return nullptr;
}

void APlayerCameraManager::UpdateViewTarget(float DeltaTime)
{
	if (!ViewTarget.Target)
	{
		return;
	}

	// ViewTarget의 카메라로부터 POV 업데이트
	UCameraComponent* CameraComp = FindCameraComponent(ViewTarget.Target);
	if (CameraComp)
	{
		ViewTarget.POV_Location = CameraComp->GetWorldLocation();
		ViewTarget.POV_Rotation = CameraComp->GetWorldRotation();
		ViewTarget.POV_FOV = CameraComp->GetFOV();
	}
	else
	{
		// CameraComponent가 없으면 액터의 Transform 사용
		ViewTarget.POV_Location = ViewTarget.Target->GetActorLocation();
		ViewTarget.POV_Rotation = ViewTarget.Target->GetActorRotation();
	}
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FVector& InOutLocation, FQuat& InOutRotation, float& InOutFOV)
{
	for (UCameraModifier* Modifier : ModifierList)
	{
	    if (Modifier && !Modifier->IsDisabled())
	    {
			if(Modifier->IsUsingRealDeltaTime())
			{
				// 실제 델타타임 사용
				Modifier->UpdateAlpha(World->GetRealDeltaTime());
			}
			else
			{
				// 고정 델타타임 사용
				Modifier->UpdateAlpha(DeltaTime);
			}

	        Modifier->ModifyCamera(
	        	DeltaTime,
	        	InOutLocation,
	        	InOutRotation,
	        	InOutFOV
	        );
	    }
	}
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FLinearColor& Color)
{
	FadeAlphaFrom = FromAlpha;
	FadeAlphaTo = ToAlpha;
	FadeTime = Duration;
	FadeTimeRemaining = Duration;
	FadeColor = Color;
	bFading = true;
}

void APlayerCameraManager::StopCameraFade()
{
	bFading = false;
	FadeTimeRemaining = 0.0f;
	FadeAmount = 0.0f;
}

float APlayerCameraManager::GetFadeAmount() const
{
	return FadeAmount;
}

void APlayerCameraManager::UpdateFade(float DeltaTime)
{
	if (!bFading)
	{
		return;
	}

	FadeTimeRemaining -= DeltaTime;

	if (FadeTimeRemaining <= 0.0f)
	{
		// Fade 완료 - 최종 알파값 유지, StopCameraFade() 호출 전까지 bFading은 true 유지
		FadeAmount = FadeAlphaTo;
		FadeTimeRemaining = 0.0f;
		// bFading은 true로 유지하여 페이드 효과 계속 렌더링
	}
	else
	{
		// 선형 보간
		float Alpha = 1.0f - (FadeTimeRemaining / FadeTime);
		FadeAmount = FadeAlphaFrom + (FadeAlphaTo - FadeAlphaFrom) * Alpha;
	}
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* NewModifier)
{
	if (!NewModifier)
	{
		return;
	}

	// 이미 리스트에 있는지 확인
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier == NewModifier)
		{
			return;
		}
	}

	// 모디파이어 추가 및 초기화
	ModifierList.Add(NewModifier);
	NewModifier->AddedToCamera(this);
}

void APlayerCameraManager::RemoveCameraModifier(UCameraModifier* ModifierToRemove)
{
	if (!ModifierToRemove)
	{
		return;
	}

	// 리스트에서 제거
	for (int32 i = 0; i < ModifierList.size(); ++i)
	{
		if (ModifierList[i] == ModifierToRemove)
		{
			ModifierList.RemoveAt(i);
			return;
		}
	}
}

void APlayerCameraManager::ResetPostProcessSettings()
{
	ViewTarget.PostProcessSettings = FPostProcessSettings();
}

void APlayerCameraManager::RemoveDisabledCameraModifiers()
{
	// 역순으로 순회하면서 비활성화된 모디파이어 제거
	for (int32 i = ModifierList.size() - 1; i >= 0; --i)
	{
		if (ModifierList[i] && ModifierList[i]->IsDisabled())
		{
			ModifierList.RemoveAt(i);
		}
	}
}

TArray<UCameraModifier*>& APlayerCameraManager::GetModifierList()
{
	return ModifierList;
}

void APlayerCameraManager::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
}

void APlayerCameraManager::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// ViewTarget은 복제하지 않음 (런타임에 설정됨)
	// Todo: ModifierList는 복제 필요시 추가
}

void APlayerCameraManager::SetViewTargetWithBlend(
	AActor* NewViewTarget,
	float BlendTime,
	EViewTargetBlendFunction BlendFunc,
	float BlendExp,
	bool bLockOutgoing
)
{
	// 블렌드 시간이 0이거나 음수면 즉시 전환
	if (BlendTime <= 0.0f)
	{
		SetViewTarget(NewViewTarget);
		StopBlending();
		return;
	}

	// PendingViewTarget 설정
	PendingViewTarget.Target = NewViewTarget;

	// PendingViewTarget의 초기 POV 설정
	if (NewViewTarget)
	{
		UCameraComponent* CameraComp = FindCameraComponent(NewViewTarget);
		if (CameraComp)
		{
			PendingViewTarget.POV_Location = CameraComp->GetWorldLocation();
			PendingViewTarget.POV_Rotation = CameraComp->GetWorldRotation();
			PendingViewTarget.POV_FOV = CameraComp->GetFOV();
		}
		else
		{
			PendingViewTarget.POV_Location = NewViewTarget->GetActorLocation();
			PendingViewTarget.POV_Rotation = NewViewTarget->GetActorRotation();
			PendingViewTarget.POV_FOV = 60.0f;
		}
	}

	// 블렌드 파라미터 설정
	BlendParams = FViewTargetTransitionParams(BlendTime, BlendFunc, BlendExp, bLockOutgoing);

	// 블렌딩 시작 시점의 상태 저장
	BlendStartLocation = ViewTarget.POV_Location;
	BlendStartRotation = ViewTarget.POV_Rotation;
	BlendStartFOV = ViewTarget.POV_FOV;

	// SpringArm 길이 저장
	USpringArmComponent* SpringArm = FindSpringArmComponent(ViewTarget.Target);
	if (SpringArm)
	{
		BlendStartSpringArmLength = SpringArm->GetTargetArmLength();
	}
}

void APlayerCameraManager::SetViewTargetWithBezierBlend(
	AActor* NewViewTarget,
	const FViewTargetTransitionParams& CustomBlendParams
)
{
	// 블렌드 시간이 0이거나 음수면 즉시 전환
	if (CustomBlendParams.BlendTime <= 0.0f)
	{
		SetViewTarget(NewViewTarget);
		StopBlending();
		return;
	}

	// PendingViewTarget 설정
	PendingViewTarget.Target = NewViewTarget;

	// PendingViewTarget의 초기 POV 설정
	if (NewViewTarget)
	{
		UCameraComponent* CameraComp = FindCameraComponent(NewViewTarget);
		if (CameraComp)
		{
			PendingViewTarget.POV_Location = CameraComp->GetWorldLocation();
			PendingViewTarget.POV_Rotation = CameraComp->GetWorldRotation();
			PendingViewTarget.POV_FOV = CameraComp->GetFOV();
		}
		else
		{
			PendingViewTarget.POV_Location = NewViewTarget->GetActorLocation();
			PendingViewTarget.POV_Rotation = NewViewTarget->GetActorRotation();
			PendingViewTarget.POV_FOV = 60.0f;
		}
	}

	// 커스텀 블렌드 파라미터 복사
	BlendParams = CustomBlendParams;
	BlendParams.BlendTimeRemaining = CustomBlendParams.BlendTime;

	// 블렌딩 시작 시점의 상태 저장
	BlendStartLocation = ViewTarget.POV_Location;
	BlendStartRotation = ViewTarget.POV_Rotation;
	BlendStartFOV = ViewTarget.POV_FOV;

	// SpringArm 길이 저장
	USpringArmComponent* SpringArm = FindSpringArmComponent(ViewTarget.Target);
	if (SpringArm)
	{
		BlendStartSpringArmLength = SpringArm->GetTargetArmLength();
	}
}

void APlayerCameraManager::SetViewTargetWithBlendPreset(
	AActor* NewViewTarget,
	const FString& PresetName
)
{
	// 라이브러리에서 프리셋 검색
	UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
	if (!Library)
	{
		UE_LOG("PlayerCameraManager: CameraBlendPresetLibrary가 초기화되지 않았습니다.");
		// Fallback: Cubic 블렌드 사용
		SetViewTargetWithBlend(NewViewTarget, 1.0f, EViewTargetBlendFunction::VTBlend_Cubic);
		return;
	}

	const FCameraBlendPreset* Preset = Library->GetPreset(PresetName);
	if (!Preset)
	{
		UE_LOG("PlayerCameraManager: 프리셋을 찾을 수 없음 - %s", PresetName.c_str());
		// Fallback: Cubic 블렌드 사용
		SetViewTargetWithBlend(NewViewTarget, 1.0f, EViewTargetBlendFunction::VTBlend_Cubic);
		return;
	}

	// 프리셋 적용
	SetViewTargetWithBlendPreset(NewViewTarget, *Preset);
}

void APlayerCameraManager::SetViewTargetWithBlendPreset(
	AActor* NewViewTarget,
	const FCameraBlendPreset& Preset
)
{
	// 프리셋의 BlendParams를 사용하여 베지어 블렌드 호출
	SetViewTargetWithBezierBlend(NewViewTarget, Preset.BlendParams);

	UE_LOG("PlayerCameraManager: 프리셋 '%s'로 ViewTarget 블렌드 시작 (%.2fs)",
		Preset.PresetName.c_str(),
		Preset.BlendParams.BlendTime);
}

void APlayerCameraManager::BlendToTransform(
	const FVector& TargetLocation,
	const FQuat& TargetRotation,
	float TargetFOV,
	float BlendTime,
	EViewTargetBlendFunction BlendFunc,
	float BlendExp
)
{
	// 블렌드 시간이 0이거나 음수면 즉시 전환
	if (BlendTime <= 0.0f)
	{
		ViewTarget.POV_Location = TargetLocation;
		ViewTarget.POV_Rotation = TargetRotation;
		ViewTarget.POV_FOV = TargetFOV;
		ViewTarget.Target = nullptr; // 액터 없음
		StopBlending();
		return;
	}

	// PendingViewTarget 설정 (액터 없이 고정 Transform 사용)
	PendingViewTarget.Target = nullptr;
	PendingViewTarget.POV_Location = TargetLocation;
	PendingViewTarget.POV_Rotation = TargetRotation;
	PendingViewTarget.POV_FOV = TargetFOV;

	// 블렌드 파라미터 설정
	BlendParams = FViewTargetTransitionParams(BlendTime, BlendFunc, BlendExp, false);

	// 블렌딩 시작 시점의 상태 저장
	BlendStartLocation = ViewTarget.POV_Location;
	BlendStartRotation = ViewTarget.POV_Rotation;
	BlendStartFOV = ViewTarget.POV_FOV;

	// SpringArm 길이 저장 (고정 위치로 이동할 때는 SpringArm 블렌딩 비활성화)
	USpringArmComponent* SpringArm = FindSpringArmComponent(ViewTarget.Target);
	if (SpringArm)
	{
		BlendStartSpringArmLength = SpringArm->GetTargetArmLength();
	}
}

void APlayerCameraManager::BlendToTransformWithBezier(
	const FVector& TargetLocation,
	const FQuat& TargetRotation,
	float TargetFOV,
	const FViewTargetTransitionParams& CustomBlendParams
)
{
	// 블렌드 시간이 0이거나 음수면 즉시 전환
	if (CustomBlendParams.BlendTime <= 0.0f)
	{
		ViewTarget.POV_Location = TargetLocation;
		ViewTarget.POV_Rotation = TargetRotation;
		ViewTarget.POV_FOV = TargetFOV;
		ViewTarget.Target = nullptr;
		StopBlending();
		return;
	}

	// PendingViewTarget 설정 (액터 없이 고정 Transform 사용)
	PendingViewTarget.Target = nullptr;
	PendingViewTarget.POV_Location = TargetLocation;
	PendingViewTarget.POV_Rotation = TargetRotation;
	PendingViewTarget.POV_FOV = TargetFOV;

	// 커스텀 블렌드 파라미터 복사
	BlendParams = CustomBlendParams;
	BlendParams.BlendTimeRemaining = CustomBlendParams.BlendTime;

	// 블렌딩 시작 시점의 상태 저장
	BlendStartLocation = ViewTarget.POV_Location;
	BlendStartRotation = ViewTarget.POV_Rotation;
	BlendStartFOV = ViewTarget.POV_FOV;

	// SpringArm 길이 저장
	USpringArmComponent* SpringArm = FindSpringArmComponent(ViewTarget.Target);
	if (SpringArm)
	{
		BlendStartSpringArmLength = SpringArm->GetTargetArmLength();
	}
}

void APlayerCameraManager::BlendToTransformWithPreset(
	const FVector& TargetLocation,
	const FQuat& TargetRotation,
	float TargetFOV,
	const FString& PresetName
)
{
	// 라이브러리에서 프리셋 검색
	UCameraBlendPresetLibrary* Library = UCameraBlendPresetLibrary::GetInstance();
	if (!Library)
	{
		UE_LOG("PlayerCameraManager: CameraBlendPresetLibrary가 초기화되지 않았습니다.");
		// Fallback: Cubic 블렌드 사용
		BlendToTransform(TargetLocation, TargetRotation, TargetFOV, 1.0f, EViewTargetBlendFunction::VTBlend_Cubic);
		return;
	}

	const FCameraBlendPreset* Preset = Library->GetPreset(PresetName);
	if (!Preset)
	{
		UE_LOG("PlayerCameraManager: 프리셋을 찾을 수 없음 - %s", PresetName.c_str());
		// Fallback: Cubic 블렌드 사용
		BlendToTransform(TargetLocation, TargetRotation, TargetFOV, 1.0f, EViewTargetBlendFunction::VTBlend_Cubic);
		return;
	}

	// 프리셋 적용
	BlendToTransformWithPreset(TargetLocation, TargetRotation, TargetFOV, *Preset);
}

void APlayerCameraManager::BlendToTransformWithPreset(
	const FVector& TargetLocation,
	const FQuat& TargetRotation,
	float TargetFOV,
	const FCameraBlendPreset& Preset
)
{
	// 프리셋의 BlendParams를 사용하여 베지어 블렌드 호출
	BlendToTransformWithBezier(TargetLocation, TargetRotation, TargetFOV, Preset.BlendParams);

	UE_LOG("PlayerCameraManager: 프리셋 '%s'로 고정 Transform 블렌드 시작 (%.2fs)",
		Preset.PresetName.c_str(),
		Preset.BlendParams.BlendTime);
}

void APlayerCameraManager::StopBlending()
{
	BlendParams.BlendTimeRemaining = 0.0f;
	PendingViewTarget.Target = nullptr;
}

void APlayerCameraManager::SetCameraTransform(const FVector& Location, const FQuat& Rotation, float FOV)
{
	// 현재 ViewTarget의 카메라 위치/회전/FOV를 즉시 설정
	ViewTarget.POV_Location = Location;
	ViewTarget.POV_Rotation = Rotation;
	ViewTarget.POV_FOV = FOV;
}

void APlayerCameraManager::UpdateViewTargetBlending(float DeltaTime)
{
	// 블렌딩 중이 아니면 리턴
	if (!BlendParams.IsBlending())
	{
		return;
	}

	// 블렌드 파라미터 업데이트
	BlendParams.UpdateBlend(DeltaTime);

	// PendingViewTarget이 액터를 가리키는 경우, 해당 액터의 현재 POV로 업데이트
	if (PendingViewTarget.Target)
	{
		// bLockOutgoing이 false인 경우, 현재 ViewTarget도 계속 업데이트
		if (!BlendParams.bLockOutgoing && ViewTarget.Target)
		{
			UCameraComponent* CurrentCamera = FindCameraComponent(ViewTarget.Target);
			if (CurrentCamera)
			{
				BlendStartLocation = CurrentCamera->GetWorldLocation();
				BlendStartRotation = CurrentCamera->GetWorldRotation();
				BlendStartFOV = CurrentCamera->GetFOV();
			}
		}

		UCameraComponent* PendingCamera = FindCameraComponent(PendingViewTarget.Target);
		if (PendingCamera)
		{
			PendingViewTarget.POV_Location = PendingCamera->GetWorldLocation();
			PendingViewTarget.POV_Rotation = PendingCamera->GetWorldRotation();
			PendingViewTarget.POV_FOV = PendingCamera->GetFOV();
		}
	}

	// 블렌딩 완료 확인
	if (!BlendParams.IsBlending())
	{
		// 블렌딩 완료: PendingViewTarget을 ViewTarget으로 전환
		ViewTarget = PendingViewTarget;
		PendingViewTarget.Target = nullptr;

		// 블렌딩 완료 델리게이트 호출
		if (OnBlendComplete.IsBound())
		{
			OnBlendComplete.Broadcast();
		}
		return;
	}

	// 각 채널별 알파 계산
	float LocationAlpha = BlendParams.CalculateLocationAlpha();
	float RotationAlpha = BlendParams.CalculateRotationAlpha();
	float FOVAlpha = BlendParams.CalculateFOVAlpha();

	// 위치 블렌딩 (선형 보간)
	ViewTarget.POV_Location = FVector::Lerp(
		BlendStartLocation,
		PendingViewTarget.POV_Location,
		LocationAlpha
	);

	// 회전 블렌딩 (구면 선형 보간)
	ViewTarget.POV_Rotation = FQuat::Slerp(
		BlendStartRotation,
		PendingViewTarget.POV_Rotation,
		RotationAlpha
	);

	// FOV 블렌딩
	ViewTarget.POV_FOV = FMath::Lerp(
		BlendStartFOV,
		PendingViewTarget.POV_FOV,
		FOVAlpha
	);

	// SpringArm 길이 블렌딩 (옵션)
	if (BlendParams.bBlendSpringArmLength)
	{
		float SpringArmAlpha = BlendParams.CalculateSpringArmAlpha();

		// 현재 ViewTarget의 SpringArm (있다면)
		USpringArmComponent* SpringArm = FindSpringArmComponent(ViewTarget.Target);
		if (SpringArm)
		{
			float BlendedLength = FMath::Lerp(
				BlendStartSpringArmLength,
				BlendParams.TargetSpringArmLength,
				SpringArmAlpha
			);
			SpringArm->SetTargetArmLength(BlendedLength);
		}
	}
}

USpringArmComponent* APlayerCameraManager::FindSpringArmComponent(AActor* InActor) const
{
	if (!InActor)
	{
		return nullptr;
	}

	// OwnedComponents에서 SpringArmComponent 검색
	const TSet<UActorComponent*>& Components = InActor->GetOwnedComponents();
	for (UActorComponent* Component : Components)
	{
		if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Component))
		{
			return SpringArm;
		}
	}

	return nullptr;
}
