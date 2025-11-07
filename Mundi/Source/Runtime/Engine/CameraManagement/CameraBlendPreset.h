#pragma once
#include "CameraTypes.h"

// 카메라 블렌드 프리셋 구조체
// 에디터에서 저장한 블렌드 커브를 런타임에서 재사용하기 위한 래퍼
struct FCameraBlendPreset
{
	FString PresetName;                         // 프리셋 이름 (예: "SmoothCinematic", "FastAction")
	FViewTargetTransitionParams BlendParams;    // 블렌드 파라미터 (커브 포함)

	// 기본 생성자
	FCameraBlendPreset()
		: PresetName("Unnamed")
	{
	}

	// 이름과 파라미터로 생성
	FCameraBlendPreset(const FString& InName, const FViewTargetTransitionParams& InParams)
		: PresetName(InName)
		, BlendParams(InParams)
	{
	}

	// JSON 직렬화/역직렬화
	void Serialize(JSON& InOutHandle, bool bIsLoading);

	// 파일에서 프리셋 로드
	static bool LoadFromFile(const FString& FilePath, FCameraBlendPreset& OutPreset);

	// 파일로 프리셋 저장
	bool SaveToFile(const FString& FilePath) const;
};
