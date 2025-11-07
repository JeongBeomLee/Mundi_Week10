#include "pch.h"
#include "CameraBlendPresetLibrary.h"

IMPLEMENT_CLASS(UCameraBlendPresetLibrary)

UCameraBlendPresetLibrary* UCameraBlendPresetLibrary::Instance = nullptr;

UCameraBlendPresetLibrary::UCameraBlendPresetLibrary()
{
}

UCameraBlendPresetLibrary* UCameraBlendPresetLibrary::GetInstance()
{
	if (!Instance)
	{
		Instance = NewObject<UCameraBlendPresetLibrary>();
		Instance->Initialize();
	}
	return Instance;
}

void UCameraBlendPresetLibrary::Initialize()
{
	if (bIsInitialized)
	{
		UE_LOG("CameraBlendPresetLibrary: 이미 초기화되었습니다.");
		return;
	}

	UE_LOG("CameraBlendPresetLibrary: 초기화 시작...");

	// 기본 프리셋 등록
	RegisterDefaultPresets();

	// Data/CameraBlendPresets 디렉토리에서 프리셋 로드
	FString PresetDirectory = "Data/CameraBlendPresets";
	LoadPresetsFromDirectory(PresetDirectory);

	bIsInitialized = true;

	UE_LOG("CameraBlendPresetLibrary: 초기화 완료 - %d개의 프리셋 등록됨", static_cast<int>(PresetMap.size()));
}

void UCameraBlendPresetLibrary::RegisterPreset(const FString& PresetName, const FCameraBlendPreset& Preset)
{
	PresetMap[PresetName] = Preset;
	UE_LOG("CameraBlendPresetLibrary: 프리셋 등록 - %s", PresetName.c_str());
}

void UCameraBlendPresetLibrary::RegisterPreset(const FCameraBlendPreset& Preset)
{
	RegisterPreset(Preset.PresetName, Preset);
}

bool UCameraBlendPresetLibrary::FindPreset(const FString& PresetName, FCameraBlendPreset& OutPreset) const
{
	auto It = PresetMap.find(PresetName);
	if (It != PresetMap.end())
	{
		OutPreset = It->second;
		return true;
	}
	return false;
}

const FCameraBlendPreset* UCameraBlendPresetLibrary::GetPreset(const FString& PresetName) const
{
	auto It = PresetMap.find(PresetName);
	if (It != PresetMap.end())
	{
		return &It->second;
	}
	return nullptr;
}

bool UCameraBlendPresetLibrary::HasPreset(const FString& PresetName) const
{
	return PresetMap.find(PresetName) != PresetMap.end();
}

TArray<FString> UCameraBlendPresetLibrary::GetAllPresetNames() const
{
	TArray<FString> Names;
	for (const auto& Pair : PresetMap)
	{
		Names.push_back(Pair.first);
	}
	return Names;
}

void UCameraBlendPresetLibrary::LoadPresetsFromDirectory(const FString& DirectoryPath)
{
	namespace fs = std::filesystem;

	// 디렉토리 존재 확인
	fs::path DirPath(DirectoryPath);
	if (!fs::exists(DirPath))
	{
		UE_LOG("CameraBlendPresetLibrary: 디렉토리가 존재하지 않음 - %s", DirectoryPath.c_str());
		// 디렉토리 생성
		try
		{
			fs::create_directories(DirPath);
			UE_LOG("CameraBlendPresetLibrary: 디렉토리 생성 - %s", DirectoryPath.c_str());
		}
		catch (const std::exception& e)
		{
			UE_LOG("CameraBlendPresetLibrary: 디렉토리 생성 실패 - %s", e.what());
		}
		return;
	}

	// .json 파일 스캔
	int LoadedCount = 0;
	for (const auto& Entry : fs::directory_iterator(DirPath))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == ".json")
		{
			FString FilePath = Entry.path().string();
			FCameraBlendPreset Preset;

			if (FCameraBlendPreset::LoadFromFile(FilePath, Preset))
			{
				RegisterPreset(Preset);
				LoadedCount++;
			}
		}
	}

	UE_LOG("CameraBlendPresetLibrary: %d개의 프리셋을 디렉토리에서 로드 - %s", LoadedCount, DirectoryPath.c_str());
}

void UCameraBlendPresetLibrary::RegisterDefaultPresets()
{
	// Linear (선형 블렌드)
	{
		FViewTargetTransitionParams Params(1.0f, EViewTargetBlendFunction::VTBlend_Linear);
		FCameraBlendPreset Preset("Linear", Params);
		RegisterPreset(Preset);
	}

	// Cubic (큐빅 보간)
	{
		FViewTargetTransitionParams Params(1.0f, EViewTargetBlendFunction::VTBlend_Cubic);
		FCameraBlendPreset Preset("Cubic", Params);
		RegisterPreset(Preset);
	}

	// EaseIn (천천히 시작)
	{
		FViewTargetTransitionParams Params(1.0f, EViewTargetBlendFunction::VTBlend_EaseIn, 2.0f);
		FCameraBlendPreset Preset("EaseIn", Params);
		RegisterPreset(Preset);
	}

	// EaseOut (천천히 종료)
	{
		FViewTargetTransitionParams Params(1.0f, EViewTargetBlendFunction::VTBlend_EaseOut, 2.0f);
		FCameraBlendPreset Preset("EaseOut", Params);
		RegisterPreset(Preset);
	}

	// EaseInOut (천천히 시작하고 종료)
	{
		FViewTargetTransitionParams Params(1.0f, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.0f);
		FCameraBlendPreset Preset("EaseInOut", Params);
		RegisterPreset(Preset);
	}

	// Smooth (매끄러운 전환 - 커스텀 베지어)
	{
		FViewTargetTransitionParams Params(1.5f, EViewTargetBlendFunction::VTBlend_BezierCustom);
		// 부드러운 S자 커브
		Params.LocationCurve = FBezierControlPoints(0.0f, 0.1f, 0.9f, 1.0f);
		Params.RotationCurve = FBezierControlPoints(0.0f, 0.1f, 0.9f, 1.0f);
		Params.FOVCurve = FBezierControlPoints(0.0f, 0.1f, 0.9f, 1.0f);
		Params.SpringArmCurve = FBezierControlPoints(0.0f, 0.1f, 0.9f, 1.0f);
		FCameraBlendPreset Preset("Smooth", Params);
		RegisterPreset(Preset);
	}

	// Cinematic (시네마틱 - 느린 전환)
	{
		FViewTargetTransitionParams Params(2.0f, EViewTargetBlendFunction::VTBlend_BezierCustom);
		// 매우 부드러운 영화 같은 커브
		Params.LocationCurve = FBezierControlPoints(0.0f, 0.05f, 0.95f, 1.0f);
		Params.RotationCurve = FBezierControlPoints(0.0f, 0.05f, 0.95f, 1.0f);
		Params.FOVCurve = FBezierControlPoints(0.0f, 0.05f, 0.95f, 1.0f);
		Params.SpringArmCurve = FBezierControlPoints(0.0f, 0.05f, 0.95f, 1.0f);
		Params.bLockOutgoing = true;
		FCameraBlendPreset Preset("Cinematic", Params);
		RegisterPreset(Preset);
	}

	// Snap (거의 즉시 전환)
	{
		FViewTargetTransitionParams Params(0.1f, EViewTargetBlendFunction::VTBlend_Linear);
		FCameraBlendPreset Preset("Snap", Params);
		RegisterPreset(Preset);
	}

	UE_LOG("CameraBlendPresetLibrary: 8개의 기본 프리셋 등록 완료");
}

void UCameraBlendPresetLibrary::UnregisterPreset(const FString& PresetName)
{
	auto It = PresetMap.find(PresetName);
	if (It != PresetMap.end())
	{
		PresetMap.erase(It);
		UE_LOG("CameraBlendPresetLibrary: 프리셋 제거 - %s", PresetName.c_str());
	}
}

void UCameraBlendPresetLibrary::ClearAllPresets()
{
	PresetMap.clear();
	UE_LOG("CameraBlendPresetLibrary: 모든 프리셋 제거");
}

void UCameraBlendPresetLibrary::PrintAllPresets() const
{
	UE_LOG("=== 등록된 카메라 블렌드 프리셋 목록 (%d개) ===", static_cast<int>(PresetMap.size()));
	for (const auto& Pair : PresetMap)
	{
		const FCameraBlendPreset& Preset = Pair.second;
		UE_LOG("  [%s] (BlendTime: %.2fs)",
			Preset.PresetName.c_str(),
			Preset.BlendParams.BlendTime);
	}
}

BEGIN_PROPERTIES(UCameraBlendPresetLibrary)
END_PROPERTIES()
