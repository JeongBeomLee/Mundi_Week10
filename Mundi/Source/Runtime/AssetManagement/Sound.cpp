#include "pch.h"
#include "Sound.h"
#include "SoundManager.h"
#include "fmod.hpp"
#include "fmod_errors.h"

// 클래스 팩토리에 등록
IMPLEMENT_CLASS(USound)

USound::USound()
	: FMODSound(nullptr)
	, bIs3D(true)
	, bIsLooping(false)
	, Duration(0.0f)
{
}

USound::~USound()
{
	Release();
}

void USound::Load(const FString& InFilePath, ID3D11Device* InDevice, bool bInIs3D, bool bInLoop)
{
	// 이미 로드되어 있으면 해제
	if (FMODSound)
	{
		Release();
	}

	// 속성 저장
	bIs3D = bInIs3D;
	bIsLooping = bInLoop;

	// USoundManager를 통해 사운드 로드
	USoundManager& SoundMgr = USoundManager::GetInstance();
	if (!SoundMgr.IsInitialized())
	{
		UE_LOG("[USound] SoundManager가 초기화되지 않았습니다.");
		return;
	}

	// 사운드 로드
	FMODSound = SoundMgr.LoadSound(InFilePath, bIs3D, bIsLooping);
	if (!FMODSound)
	{
		UE_LOG("[USound] 사운드 로드 실패: %s", InFilePath.c_str());
		return;
	}

	// 사운드 길이 가져오기
	unsigned int LengthMs = 0;
	FMOD_RESULT Result = FMODSound->getLength(&LengthMs, FMOD_TIMEUNIT_MS);
	if (Result == FMOD_OK)
	{
		Duration = LengthMs / 1000.0f; // 밀리초를 초로 변환
	}

	UE_LOG("[USound] 사운드 로드 성공: %s (길이: %.2f초)", InFilePath.c_str(), Duration);
}

void USound::Release()
{
	if (FMODSound)
	{
		// FMOD Sound 해제
		FMODSound->release();
		FMODSound = nullptr;
	}

	Duration = 0.0f;
}
