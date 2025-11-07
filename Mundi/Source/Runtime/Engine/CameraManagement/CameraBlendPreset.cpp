#include "pch.h"
#include "CameraBlendPreset.h"

void FCameraBlendPreset::Serialize(JSON& InOutHandle, bool bIsLoading)
{
	if (bIsLoading)
	{
		// 로드
		FJsonSerializer::ReadString(InOutHandle, "PresetName", PresetName, "Unnamed");

		// BlendParams 직렬화
		if (InOutHandle.hasKey("BlendParams"))
		{
			JSON& ParamsJson = InOutHandle["BlendParams"];
			BlendParams.Serialize(ParamsJson, true);
		}
	}
	else
	{
		// 저장
		InOutHandle["PresetName"] = PresetName;

		// BlendParams 직렬화
		JSON ParamsJson = JSON::Make(JSON::Class::Object);
		BlendParams.Serialize(ParamsJson, false);
		InOutHandle["BlendParams"] = ParamsJson;
	}
}

bool FCameraBlendPreset::LoadFromFile(const FString& FilePath, FCameraBlendPreset& OutPreset)
{
	JSON RootJson;

	// JSON 파일 로드
	if (!FJsonSerializer::LoadJsonFromFile(RootJson, FilePath))
	{
		UE_LOG("FCameraBlendPreset: 파일 로드 실패 - %s", FilePath.c_str());
		return false;
	}

	// 프리셋 직렬화
	OutPreset.Serialize(RootJson, true);

	UE_LOG("FCameraBlendPreset: 프리셋 로드 성공 - %s (%s)", OutPreset.PresetName.c_str(), FilePath.c_str());
	return true;
}

bool FCameraBlendPreset::SaveToFile(const FString& FilePath) const
{
	JSON RootJson = JSON::Make(JSON::Class::Object);

	// 프리셋 직렬화
	FCameraBlendPreset& NonConstThis = const_cast<FCameraBlendPreset&>(*this);
	NonConstThis.Serialize(RootJson, false);

	// JSON 파일 저장
	if (!FJsonSerializer::SaveJsonToFile(RootJson, FilePath))
	{
		UE_LOG("FCameraBlendPreset: 파일 저장 실패 - %s", FilePath.c_str());
		return false;
	}

	UE_LOG("FCameraBlendPreset: 프리셋 저장 성공 - %s (%s)", PresetName.c_str(), FilePath.c_str());
	return true;
}
