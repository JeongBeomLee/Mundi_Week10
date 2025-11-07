#pragma once
#include "CameraBlendPreset.h"

// 카메라 블렌드 프리셋 라이브러리 (싱글톤)
// 프리셋을 중앙에서 관리하고 이름 기반으로 빠르게 검색 가능
class UCameraBlendPresetLibrary : public UObject
{
public:
	DECLARE_CLASS(UCameraBlendPresetLibrary, UObject)
	GENERATED_REFLECTION_BODY()

	UCameraBlendPresetLibrary();

	// 싱글톤 인스턴스 접근
	static UCameraBlendPresetLibrary* GetInstance();

	// 라이브러리 초기화 (게임 시작 시 호출)
	void Initialize();

	// 프리셋 등록
	void RegisterPreset(const FString& PresetName, const FCameraBlendPreset& Preset);
	void RegisterPreset(const FCameraBlendPreset& Preset);  // Preset.PresetName 사용

	// 프리셋 검색
	bool FindPreset(const FString& PresetName, FCameraBlendPreset& OutPreset) const;
	const FCameraBlendPreset* GetPreset(const FString& PresetName) const;

	// 프리셋 존재 확인
	bool HasPreset(const FString& PresetName) const;

	// 모든 프리셋 이름 가져오기
	TArray<FString> GetAllPresetNames() const;

	// 디렉토리에서 프리셋 자동 로드
	void LoadPresetsFromDirectory(const FString& DirectoryPath);

	// 기본 프리셋 등록 (Linear, Cubic, EaseIn, EaseOut 등)
	void RegisterDefaultPresets();

	// 프리셋 제거
	void UnregisterPreset(const FString& PresetName);

	// 모든 프리셋 제거
	void ClearAllPresets();

	// 디버그: 등록된 프리셋 목록 출력
	void PrintAllPresets() const;

private:
	virtual ~UCameraBlendPresetLibrary() = default;

	static UCameraBlendPresetLibrary* Instance;
	TMap<FString, FCameraBlendPreset> PresetMap;

	bool bIsInitialized = false;
};
