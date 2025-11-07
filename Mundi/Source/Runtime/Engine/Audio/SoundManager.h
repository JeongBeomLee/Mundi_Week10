#pragma once
#include "Object.h"
#include "UEContainer.h"

// FMOD 전방 선언
namespace FMOD
{
	class System;
	class Sound;
	class Channel;
	class ChannelGroup;
}

// ESoundChannelType
// 사운드 채널 타입
enum class ESoundChannelType : uint8
{
	Master = 0,  // 마스터 채널
	Music,       // 배경음악
	SFX,         // 효과음
	Count
};

// FMOD Core API 사운드 시스템 매니저
class USoundManager : public UObject
{
public:
	DECLARE_CLASS(USoundManager, UObject)

	static USoundManager& GetInstance();

	USoundManager();
	void Initialize();
	void Update(float DeltaTime);
	void Shutdown();

	// 사운드 파일 경로를 받아서 2D 사운드로 재생
	// 반환값: 재생 중인 Channel 포인터 (볼륨 조절 등에 사용 가능)
	FMOD::Channel* PlaySound2D(const FString& SoundPath, bool bLoop = false, ESoundChannelType ChannelType = ESoundChannelType::SFX);

	// 특정 위치에서 3D 사운드 재생
	FMOD::Channel* PlaySound3D(const FString& SoundPath, const FVector& Location, bool bLoop = false, float MinDistance = 100.0f, float MaxDistance = 10000.0f, ESoundChannelType ChannelType = ESoundChannelType::SFX);

	// 사운드 파일을 미리 로드하여 캐시에 저장 (자주 사용하는 효과음은 미리 로드 권장)
	FMOD::Sound* LoadSound(const FString& SoundPath, bool bIs3D = true, bool bLoop = false);

	// 캐시에서 사운드 가져오기 (없으면 로드)
	FMOD::Sound* GetOrLoadSound(const FString& SoundPath, bool bIs3D = true, bool bLoop = false);

	// 3D 사운드를 듣는 리스너의 위치와 방향 설정
	void SetListenerPosition(const FVector& Position, const FVector& Forward, const FVector& Up);

	// BGM(배경음악) 재생 (자동으로 루프 재생, Music 채널 사용)
	void PlayBGM(const FString& SoundPath);

	// BGM 정지
	void StopBGM();

	// 개별 채널 정지 (PlaySound2D/3D 반환값으로 받은 채널을 정지)
	void StopChannel(FMOD::Channel* Channel);

	// 모든 사운드 정지
	void StopAllSounds();

	// BGM이 재생 중인지 확인
	bool IsBGMPlaying() const;

	// 특정 채널이 재생 중인지 확인
	bool IsChannelPlaying(FMOD::Channel* Channel) const;

	// 볼륨 조절
	void SetMasterVolume(float Volume);
	float GetMasterVolume() const { return MasterVolume; }

	void SetMusicVolume(float Volume);
	float GetMusicVolume() const { return MusicVolume; }

	void SetSFXVolume(float Volume);
	float GetSFXVolume() const { return SFXVolume; }

	// FMOD System 접근자
	FMOD::System* GetSystem() const { return FMODSystem; }

	// 상태 확인
	bool IsInitialized() const { return bIsInitialized; }

protected:
	virtual ~USoundManager() override;

	// 복사 방지
	USoundManager(const USoundManager&) = delete;
	USoundManager& operator=(const USoundManager&) = delete;

	// FMOD 결과 코드 체크 및 로깅
	bool CheckFMODResult(int Result, const char* FunctionName);

	// FVector를 FMOD_VECTOR로 변환
	void VectorToFMOD(const FVector& InVector, struct FMOD_VECTOR& OutFMODVector);

private:
	// FMOD 시스템
	FMOD::System* FMODSystem = nullptr;

	// 채널 그룹
	FMOD::ChannelGroup* MasterChannelGroup = nullptr;
	FMOD::ChannelGroup* MusicChannelGroup = nullptr;
	FMOD::ChannelGroup* SFXChannelGroup = nullptr;

	// BGM 전용 채널
	FMOD::Channel* BGMChannel = nullptr;

	// 로딩된 사운드를 경로별로 캐싱
	TMap<FString, FMOD::Sound*> LoadedSounds;

	// 볼륨 설정
	float MasterVolume = 1.0f;
	float MusicVolume = 0.7f;
	float SFXVolume = 1.0f;

	// 초기화 상태
	bool bIsInitialized = false;

	// 최대 채널 수
	static constexpr int MaxChannels = 512;
};
