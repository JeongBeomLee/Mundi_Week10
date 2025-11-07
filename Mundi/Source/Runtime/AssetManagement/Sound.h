#pragma once
#include "ResourceBase.h"
#include "UEContainer.h"

// FMOD 전방 선언
namespace FMOD
{
	class Sound;
}

// USound
// FMOD Sound 객체를 래핑하는 리소스 클래스
class USound : public UResourceBase
{
public:
	DECLARE_CLASS(USound, UResourceBase)

	USound();
	virtual ~USound() override;

	// 리소스 로드
	// InFilePath: 사운드 파일 경로 (.wav, .mp3, .ogg 등)
	// InDevice: 사용하지 않음 (FMOD는 USoundManager가 관리)
	// bIs3D: 3D 사운드 여부
	// bLoop: 루프 재생 여부
	void Load(const FString& InFilePath, ID3D11Device* InDevice, bool bIs3D = true, bool bLoop = false);

	// 리소스 해제
	void Release();

	// FMOD Sound 접근자
	FMOD::Sound* GetFMODSound() const { return FMODSound; }

	// 사운드 속성
	bool Is3D() const { return bIs3D; }
	bool IsLooping() const { return bIsLooping; }
	float GetDuration() const { return Duration; }

protected:
	// FMOD Sound 핸들
	FMOD::Sound* FMODSound = nullptr;

	// 사운드 속성
	bool bIs3D = true;          // 3D 사운드 여부
	bool bIsLooping = false;    // 루프 재생 여부
	float Duration = 0.0f;      // 사운드 길이 (초)
};
