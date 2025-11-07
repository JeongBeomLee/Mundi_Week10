#include "pch.h"
#include "ObjManager.h"
#include "PathUtils.h"

#include "ObjectIterator.h"
#include "StaticMesh.h"
#include "Enums.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

TMap<FString, FStaticMesh*> FObjManager::ObjStaticMeshMap;

// 파일 유틸 함수
namespace
{
	/**
	 * .mtl 텍스처 맵 라인에서 모든 옵션 토큰과 마지막 파일 경로를 분리하여 추출합니다.
	 * 예: "-bm 1.0 path/to/file.png" -> OutOptions = ["-bm", "1.0"], OutFilePath = "path/to/file.png"
	 * * @param line - 파싱할 전체 라인 문자열
	 * @param substr_index - 키워드("map_Kd ") 이후부터 파싱을 시작할 인덱스
	 * @param OutOptions - (출력) 파일 경로를 제외한 모든 옵션 토큰이 저장될 벡터
	 * @param OutFilePath - (출력) 마지막 토큰인 파일 경로
	 */
	void ParseTextureMapLine(const FString& InLine, int InSubstrIndex, TArray<FString>& OutOptions, FString& OutFilePath)
	{
		// 출력 변수 초기화
		OutOptions.clear();
		OutFilePath.clear();

		std::stringstream wss(InLine.substr(InSubstrIndex));
		FString token;
		TArray<FString> all_tokens;

		// 라인을 공백 기준으로 모든 토큰으로 분리
		while (wss >> token)
		{
			all_tokens.push_back(token);
		}

		if (all_tokens.empty())
		{
			return; // 라인에 토큰이 없음 (예: "map_Kd ")
		}

		// 파일명은 항상 마지막 토큰으로 가정
		FString TextureRel = all_tokens.back().c_str();
		OutFilePath = NormalizePath(TextureRel);

		// 파일명을 제외한 나머지 토큰들을 OutOptions에 복사
		if (all_tokens.size() > 1)
		{
			OutOptions.assign(all_tokens.begin(), all_tokens.end() - 1);
		}
	}

	/**
	 * .mtl 텍스처 맵 옵션 벡터에서 특정 float 옵션의 값을 찾습니다.
	 * @param Options - ParseTextureMapLine에서 추출된 옵션 토큰 벡터
	 * @param OptionFlag - 찾고자 하는 옵션 플래그 (예: "-bm")
	 * @param DefaultValue - 옵션을 찾지 못했거나 값이 유효하지 않을 때 반환할 기본값
	 * @return 찾은 값 (float) 또는 기본값
	 */
	float GetFloatOption(const TArray<FString>& InOptions, const FString& InOptionFlag, float InDefaultValue)
	{
		for (size_t i = 0; i < InOptions.size(); ++i)
		{
			if (InOptions[i] == InOptionFlag)
			{
				// 플래그 다음 토큰이 값이어야 함
				if (i + 1 < InOptions.size())
				{
					try
					{
						// std::stof는 FString을 인자로 받음
						return std::stof(InOptions[i + 1]);
					}
					catch (...)
					{
						// float 변환 실패 시 기본값 반환
						return InDefaultValue;
					}
				}
			}
		}
		// 옵션 플래그를 찾지 못한 경우
		return InDefaultValue;
	}
}

/**
 * .obj 파일을 파싱하여 'mtllib' 라인에 명시된 .mtl 파일의 상대 경로를 찾습니다.
 * @param InObjPath - 검사할 .obj 파일의 경로
 * @return .mtl 파일의 경로를 담은 FString. 찾지 못하면 비어있는 문자열을 반환합니다.
 */
static FString FindMtlFilePath(const FString& InObjPath)
{
	// 한글 경로 지원: UTF-8 → UTF-16 변환 후 파일 열기
	FWideString WPath = UTF8ToWide(InObjPath);
	std::ifstream FileIn(WPath);
	if (!FileIn)
	{
		return "";
	}

	FString Line;
	while (std::getline(FileIn, Line))
	{
		if (Line.rfind("mtllib ", 0) == 0)
		{
			size_t pos = InObjPath.find_last_of("/\\");
			FString objDir = (pos == FString::npos) ? "" : InObjPath.substr(0, pos + 1);
			return objDir + Line.substr(7);
		}
	}

	return "";
}

/**
 * @brief .obj 파일을 빠르게 스캔하여 참조된 모든 .mtl 파일의 전체 경로 목록을 반환합니다.
 * 이 함수는 전체 3D 데이터를 파싱하지 않고 'mtllib' 지시어만 효율적으로 찾습니다.
 * @param ObjPath 원본 .obj 파일의 경로입니다.
 * @param OutMtlFilePaths[out] 발견된 .mtl 파일들의 전체 경로가 저장될 배열입니다.
 * @return 스캔에 성공하면 true, 파일 열기에 실패하면 false를 반환합니다.
 */
bool GetMtlDependencies(const FString& ObjPath, TArray<FString>& OutMtlFilePaths)
{
	// 한글 경로 지원: UTF-8 → UTF-16 변환 후 파일 열기
	FWideString WPath = UTF8ToWide(ObjPath);
	std::ifstream InFile(WPath);
	if (!InFile.is_open())
	{
		UE_LOG("Failed to open .obj file for dependency scan: %s", ObjPath.c_str());
		return false;
	}

	fs::path BaseDir = fs::path(ObjPath).parent_path();
	FString Line;

	while (std::getline(InFile, Line))
	{
		// 라인 앞뒤의 공백을 제거하여 안정성을 높입니다.
		Line.erase(0, Line.find_first_not_of(" \t\r\n"));
		Line.erase(Line.find_last_not_of(" \t\r\n") + 1);

		if (Line.rfind("mtllib ", 0) == 0) // "mtllib "으로 시작하는지 확인
		{
			// "mtllib " 다음의 모든 문자열을 경로로 추출합니다.
			FString MtlFileName = Line.substr(7);
			if (!MtlFileName.empty())
			{
				fs::path FullPath = fs::weakly_canonical(BaseDir / MtlFileName);
				FString PathStr = FullPath.string();
				std::replace(PathStr.begin(), PathStr.end(), '\\', '/');
				OutMtlFilePaths.AddUnique(NormalizePath(PathStr));
			}
		}
	}
	return true;
}

/**
 * @brief 캐시 파일이 원본(.obj) 및 모든 의존성(.mtl) 파일보다 최신인지 검사합니다.
 * @param ObjPath 원본 .obj 파일의 경로입니다.
 * @param BinPath 메쉬 데이터 캐시(.obj.bin) 파일의 경로입니다.
 * @param MatBinPath 머티리얼 데이터 캐시(.mtl.bin) 파일의 경로입니다.
 * @return 캐시를 다시 생성해야 하면 true, 캐시가 유효하면 false를 반환합니다.
 */
bool ShouldRegenerateCache(const FString& ObjPath, const FString& BinPath, const FString& MatBinPath)
{
	// 캐시 파일 중 하나라도 존재하지 않으면 무조건 재생성해야 합니다.
	if (!fs::exists(BinPath) || !fs::exists(MatBinPath))
	{
		return true;
	}

	try
	{
		auto BinTimestamp = fs::last_write_time(BinPath);

		// 1. 원본 .obj 파일의 수정 시간을 캐시 파일과 비교합니다.
		if (fs::last_write_time(ObjPath) > BinTimestamp)
		{
			return true;
		}

		// 2. .obj 파일을 빠르게 스캔하여 의존하는 .mtl 파일 목록을 가져옵니다.
		TArray<FString> MtlDependencies;
		if (!GetMtlDependencies(ObjPath, MtlDependencies))
		{
			return true; // .obj 파일을 읽을 수 없으면 안전을 위해 캐시를 재생성합니다.
		}

		// 3. 각 .mtl 파일의 수정 시간을 캐시 파일과 비교합니다.
		for (const FString& MtlPath : MtlDependencies)
		{
			// .mtl 파일이 존재하지 않거나 캐시보다 최신 버전이면 재생성합니다.
			if (!fs::exists(MtlPath) || fs::last_write_time(MtlPath) > BinTimestamp)
			{
				return true;
			}
		}
	}
	catch (const fs::filesystem_error& e)
	{
		// 파일 시스템 오류(예: 접근 권한 없음) 발생 시 안전하게 캐시를 재생성합니다.
		UE_LOG("Filesystem error during cache validation: %s. Forcing regeneration.", e.what());
		return true;
	}

	// 모든 검사를 통과하면 캐시는 유효합니다.
	return false;
}

void FObjManager::Preload()
{
	const fs::path DataDir(GDataDir);

	if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
	{
		UE_LOG("FObjManager::Preload: Data directory not found: %s", DataDir.string().c_str());
		return;
	}

	size_t LoadedCount = 0;
	std::unordered_set<FString> ProcessedFiles; // 중복 로딩 방지

	for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
	{
		if (!Entry.is_regular_file())
			continue;

		const fs::path& Path = Entry.path();
		FString Extension = Path.extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (Extension == ".obj")
		{
			FString PathStr = NormalizePath(Path.string());

			// 이미 처리된 파일인지 확인
			if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
			{
				ProcessedFiles.insert(PathStr);
				LoadObjStaticMesh(PathStr);
				++LoadedCount;
			}
		}
		else if (Extension == ".dds" || Extension == ".jpg" || Extension == ".png")
		{
			UResourceManager::GetInstance().Load<UTexture>(Path.string()); // 데칼 텍스쳐를 ui에서 고를 수 있게 하기 위해 임시로 만듬.
		}
	}

	// 4) 모든 StaticMeshs 가져오기
	RESOURCE.SetStaticMeshs();

	UE_LOG("FObjManager::Preload: Loaded %zu .obj files from %s", LoadedCount, DataDir.string().c_str());
}

void FObjManager::Clear()
{
	for (auto& Pair : ObjStaticMeshMap)
	{
		delete Pair.second;
	}

	ObjStaticMeshMap.Empty();
}

// PathFileName의 bin을 로드 (없으면 생성) 해서 메모리에 캐싱 후 반환
FStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	FString NormalizedPathStr = NormalizePath(PathFileName);

	// 1. 메모리 캐시 확인: 이미 로드된 에셋이 있으면 즉시 반환합니다.
	if (FStaticMesh** It = ObjStaticMeshMap.Find(NormalizedPathStr))
	{
		return *It;
	}

	std::filesystem::path Path(NormalizedPathStr);

	// 2. 파일 경로 설정
	FString Extension = Path.extension().string();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (Extension != ".obj")
	{
		UE_LOG("this file is not obj!: %s", NormalizedPathStr.c_str());
		return nullptr;
	}

#ifdef USE_OBJ_CACHE
	// 2-1. 캐시 파일 경로 설정
	FString CachePathStr = ConvertDataPathToCachePath(NormalizedPathStr);

	const FString BinPathFileName = CachePathStr + ".bin";
	const FString MatBinPathFileName = CachePathStr + ".mat.bin";

	// 캐시를 저장할 디렉토리가 없으면 생성
	fs::path CacheFileDirPath(BinPathFileName);
	if (CacheFileDirPath.has_parent_path())
	{
		fs::create_directories(CacheFileDirPath.parent_path());
	}

	// 3. 캐시 데이터 로드 시도 및 실패 시 재생성 로직
	FStaticMesh* NewFStaticMesh = new FStaticMesh();
	TArray<FMaterialInfo> MaterialInfos;
	bool bLoadedSuccessfully = false;

	// 캐시가 오래되었는지 먼저 확인
	bool bShouldRegenerate = ShouldRegenerateCache(NormalizedPathStr, BinPathFileName, MatBinPathFileName);

	if (!bShouldRegenerate)
	{
		UE_LOG("Attempting to load '%s' from cache.", NormalizedPathStr.c_str());
		try
		{
			// 캐시에서 FStaticMesh 데이터 로드
			FWindowsBinReader Reader(BinPathFileName);
			if (!Reader.IsOpen())
			{
				// Reader 생성자에서 예외를 던지지 않는 경우를 대비한 명시적 실패 처리
				throw std::runtime_error("Failed to open bin file for reading.");
			}
			Reader << *NewFStaticMesh;
			Reader.Close();

			// 캐시에서 Material 데이터 로드
			FWindowsBinReader MatReader(MatBinPathFileName);
			if (!MatReader.IsOpen())
			{
				throw std::runtime_error("Failed to open material bin file for reading.");
			}
			Serialization::ReadArray<FMaterialInfo>(MatReader, MaterialInfos);
			MatReader.Close();

			NewFStaticMesh->CacheFilePath = BinPathFileName;

			// 모든 로드가 성공적으로 완료됨
			bLoadedSuccessfully = true;
			UE_LOG("Successfully loaded '%s' from cache.", NormalizedPathStr.c_str());
		}
		catch (const std::exception& e)
		{
			UE_LOG("Error loading from cache: %s. Cache might be corrupt or incompatible.", e.what());
			UE_LOG("Deleting corrupt cache and forcing regeneration for '%s'.", NormalizedPathStr.c_str());

			// 실패 시 생성 중이던 객체 메모리 정리
			delete NewFStaticMesh;
			NewFStaticMesh = nullptr; // 포인터를 nullptr로 설정하여 이중 삭제 방지

			// 손상된 캐시 파일 삭제
			fs::remove(BinPathFileName);
			fs::remove(MatBinPathFileName);

			bLoadedSuccessfully = false;
		}
	}
#else
	FStaticMesh* NewFStaticMesh = new FStaticMesh();
	TArray<FMaterialInfo> MaterialInfos;
	bool bLoadedSuccessfully = false;
#endif // USE_OBJ_CACHE

	// 기본 머티리얼 주입 로직을 헬퍼 람다로 분리합니다.
	auto EnsureDefaultMaterial = [&](FStaticMesh* Mesh, TArray<FMaterialInfo>& Materials)
		{
			if (Mesh->GroupInfos.size() > 0 && Materials.empty())
			{
				UE_LOG("No materials found for '%s'. Assigning default 'uberlit' material.", NormalizedPathStr.c_str());

				FMaterialInfo DefaultMaterialInfo;
				UMaterial* DefaultMaterial = UResourceManager::GetInstance().GetDefaultMaterial();
				DefaultMaterialInfo.MaterialName = DefaultMaterial->GetMaterialInfo().MaterialName;
				Materials.Add(DefaultMaterialInfo);

				TArray<FGroupInfo>& GroupInfos = Mesh->GroupInfos;
				for (FGroupInfo& Group : GroupInfos)
				{
					if (Group.InitialMaterialName.empty())
					{
						Group.InitialMaterialName = DefaultMaterialInfo.MaterialName;
					}
				}
				return true; // 변경됨
			}
			return false; // 변경 없음
		};

	// 캐시 로드에 실패했거나, 처음부터 재생성이 필요했던 경우
	if (!bLoadedSuccessfully)
	{
		if (NewFStaticMesh == nullptr) { NewFStaticMesh = new FStaticMesh(); }
		UE_LOG("Regenerating cache for '%s'...", NormalizedPathStr.c_str());

		FObjInfo RawObjInfo;
		if (!FObjImporter::LoadObjModel(NormalizedPathStr, &RawObjInfo, MaterialInfos, true))
		{
			delete NewFStaticMesh;
			return nullptr;
		}

		FObjImporter::ConvertToStaticMesh(RawObjInfo, MaterialInfos, NewFStaticMesh);

		// 캐시 저장 *직전에* 기본 머티리얼 로직을 호출합니다.
		EnsureDefaultMaterial(NewFStaticMesh, MaterialInfos);

#ifdef USE_OBJ_CACHE
		// 새로운 캐시 파일(.bin) 저장 (이제 올바른 데이터가 저장됨)
		FWindowsBinWriter Writer(BinPathFileName);
		Writer << *NewFStaticMesh;
		Writer.Close();

		FWindowsBinWriter MatWriter(MatBinPathFileName);
		Serialization::WriteArray<FMaterialInfo>(MatWriter, MaterialInfos);
		MatWriter.Close();

		UE_LOG("Cache regeneration complete for '%s'.", NormalizedPathStr.c_str());
#endif // USE_OBJ_CACHE
	}
	else
	{
		// 캐시 로드에 성공한 경우(bLoadedSuccessfully == true)
		// 구버전 캐시(기본 머티리얼이 없는)일 수 있으므로, 동일한 검사를 수행합니다.
		if (EnsureDefaultMaterial(NewFStaticMesh, MaterialInfos))
		{
#ifdef USE_OBJ_CACHE
			// 변경된 경우, 캐시를 갱신합니다.
			UE_LOG("Updating outdated cache for '%s' with default material.", NormalizedPathStr.c_str());
			try
			{
				FWindowsBinWriter Writer(BinPathFileName);
				Writer << *NewFStaticMesh;
				Writer.Close();
				FWindowsBinWriter MatWriter(MatBinPathFileName);
				Serialization::WriteArray<FMaterialInfo>(MatWriter, MaterialInfos);
				MatWriter.Close();
			}
			catch (const std::exception& e)
			{
				UE_LOG("Failed to update cache for default material: %s", e.what());
			}
#endif // USE_OBJ_CACHE
		}
	}

	// 4. 머티리얼 및 텍스처 경로 처리 (공통 로직)
	// 한글 경로 지원: UTF-8 → UTF-16 변환 후 경로 처리

	// .obj 파일의 기본 디렉토리를 FString으로 미리 계산 (한글 경로 지원)
	FWideString WNormalizedPath = UTF8ToWide(NormalizedPathStr);
	fs::path BaseDirFs = fs::path(WNormalizedPath).parent_path();
	FString ObjBaseDir = NormalizePath(WideToUTF8(BaseDirFs.wstring()));

	for (auto& MaterialInfo : MaterialInfos)
	{
		// 람다 함수 대신 PathUtils 유틸리티 함수를 직접 호출
		MaterialInfo.DiffuseTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.DiffuseTextureFileName, ObjBaseDir);

		MaterialInfo.NormalTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.NormalTextureFileName, ObjBaseDir);

		MaterialInfo.TransparencyTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.TransparencyTextureFileName, ObjBaseDir);

		MaterialInfo.AmbientTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.AmbientTextureFileName, ObjBaseDir);

		MaterialInfo.SpecularTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.SpecularTextureFileName, ObjBaseDir);

		MaterialInfo.SpecularExponentTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.SpecularExponentTextureFileName, ObjBaseDir);

		MaterialInfo.EmissiveTextureFileName =
			ResolveAssetRelativePath(MaterialInfo.EmissiveTextureFileName, ObjBaseDir);
	}

	// 루프가 시작되기 전에 기본 UberLit 셰이더 포인터를 한 번만 가져옵니다.
	UShader* DefaultUberlitShader = nullptr;
	UMaterial* DefaultMaterial = UResourceManager::GetInstance().GetDefaultMaterial();
	if (DefaultMaterial)
	{
		DefaultUberlitShader = DefaultMaterial->GetShader();
	}

	if (DefaultUberlitShader == nullptr)
	{
		UE_LOG("CRITICAL: Default Uberlit Shader not found. OBJ materials may fail.");
	}

	for (const FMaterialInfo& InMaterialInfo : MaterialInfos)
	{
		if (!UResourceManager::GetInstance().Get<UMaterial>(InMaterialInfo.MaterialName))
		{
			UMaterial* Material = NewObject<UMaterial>();
			Material->SetMaterialInfo(InMaterialInfo);
			Material->SetShaderMacros(DefaultMaterial->GetShaderMacros());

			// 모든 OBJ 파생 머티리얼의 셰이더가 없으면 기본 셰이더로 강제 설정합니다.
			if (!Material->GetShader())
			{
				Material->SetShader(DefaultUberlitShader);
			}

			UResourceManager::GetInstance().Add<UMaterial>(InMaterialInfo.MaterialName, Material);
		}
	}

	// 5. 메모리 캐시에 등록하고 반환
	ObjStaticMeshMap.Add(NormalizedPathStr, NewFStaticMesh);
	return NewFStaticMesh;
}

// 여기서 BVH 정보 담아주기 작업을 해야 함 
UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName)
{
	// 0) 경로
	FString NormalizedPathStr = NormalizePath(PathFileName);

	// 1) 이미 로드된 UStaticMesh가 있는지 전체 검색 (정규화된 경로로 비교)
	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		UStaticMesh* StaticMesh = *It;

		if (StaticMesh->GetFilePath() == NormalizedPathStr)
		{
			return StaticMesh;
		}
	}

	// 2) 없으면 새로 로드 (정규화된 경로 사용)
	UStaticMesh* StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(NormalizedPathStr);

	UE_LOG("UStaticMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
	return StaticMesh;
}

// obj File to FObjInfo, FMaterialParameters
bool FObjImporter::LoadObjModel(const FString& InFileName, FObjInfo* const OutObjInfo, TArray<FMaterialInfo>& OutMaterialInfos, bool bIsRightHanded)
{
	uint32 subsetCount = 0;
	FString MtlFileName;

	bool bHasTexcoord = false;
	bool bHasNormal = false;
	FString MaterialNameTemp;

	FString Face;
	uint32 VIndex = 0;
	uint32 MeshTriangles = 0;

	size_t pos = InFileName.find_last_of("/\\");
	FString objDir = (pos == FString::npos) ? "" : InFileName.substr(0, pos + 1);

	// [안정성] .obj 파일이 존재하지 않으면 로드 실패를 반환합니다.
	// 이는 필수 데이터이므로 더 이상 진행할 수 없습니다.
	// 한글 경로 지원: UTF-8 → UTF-16 변환 후 파일 열기
	FWideString WPath = UTF8ToWide(InFileName);
	std::ifstream FileIn(WPath);
	if (!FileIn)
	{
		UE_LOG("Error: The file '%s' does not exist!", InFileName.c_str());
		return false;
	}

	OutObjInfo->ObjFileName = FString(InFileName.begin(), InFileName.end());

	FString line;
	while (std::getline(FileIn, line))
	{
		if (line.empty()) continue;

		line.erase(0, line.find_first_not_of(" \t\n\r"));

		if (line[0] == '#')
			continue;

		if (line.rfind("v ", 0) == 0) // 정점 좌표 (v x y z)
		{
			std::stringstream wss(line.substr(2));
			float vx, vy, vz;
			wss >> vx >> vy >> vz;
			if (bIsRightHanded)
			{
				OutObjInfo->Positions.push_back(FVector(vx, -vy, vz));
			}
			else
			{
				OutObjInfo->Positions.push_back(FVector(vx, vy, vz));
			}
		}
		else if (line.rfind("vt ", 0) == 0) // 텍스처 좌표 (vt u v)
		{
			std::stringstream wss(line.substr(3));
			float u, v;
			wss >> u >> v;
			// obj의 vt는 좌하단이 (0,0) -> DirectX UV는 좌상단이 (0,0) (상하 반전으로 컨버팅)
			v = 1.0f - v;
			OutObjInfo->TexCoords.push_back(FVector2D(u, v));
			bHasTexcoord = true;
		}
		else if (line.rfind("vn ", 0) == 0) // 법선 (vn x y z)
		{
			std::stringstream wss(line.substr(3));
			float nx, ny, nz;
			wss >> nx >> ny >> nz;
			if (bIsRightHanded)
			{
				OutObjInfo->Normals.push_back(FVector(nx, -ny, nz));
			}
			else
			{
				OutObjInfo->Normals.push_back(FVector(nx, ny, nz));
			}
			bHasNormal = true;
		}
		else if (line.rfind("g ", 0) == 0) // 그룹 (g groupName)
		{
			// 현재 'usemtl'을 기준으로 그룹을 나누므로 'g' 태그는 무시합니다.
		}
		else if (line.rfind("f ", 0) == 0) // 면 (f v1/vt1/vn1 v2/vt2/vn2 ...)
		{
			Face = line.substr(2);
			if (Face.length() <= 0)
			{
				continue;
			}

			// Parse face line and trim at '#' or newline
			std::stringstream wss(Face);
			FString VertexDef;

			TArray<FFaceVertex> LineFaceVertices;
			while (wss >> VertexDef)
			{
				// '#'을 만나면 주석 처리 (이후 데이터 무시)
				if (VertexDef[0] == '#')
				{
					break;
				}

				FFaceVertex FaceVertex = ParseVertexDef(VertexDef);
				LineFaceVertices.push_back(FaceVertex);
			}

			// 4각형 이상의 폴리곤도 처리하기 위해서 for문으로 처리
			for (uint32 i = 1; i < LineFaceVertices.size() - 1; ++i)
			{
				if (bIsRightHanded)
				{
					OutObjInfo->PositionIndices.push_back(LineFaceVertices[0].PositionIndex);
					OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[0].TexCoordIndex);
					OutObjInfo->NormalIndices.push_back(LineFaceVertices[0].NormalIndex);

					OutObjInfo->PositionIndices.push_back(LineFaceVertices[i + 1].PositionIndex);
					OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i + 1].TexCoordIndex);
					OutObjInfo->NormalIndices.push_back(LineFaceVertices[i + 1].NormalIndex);

					OutObjInfo->PositionIndices.push_back(LineFaceVertices[i].PositionIndex);
					OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i].TexCoordIndex);
					OutObjInfo->NormalIndices.push_back(LineFaceVertices[i].NormalIndex);
				}
				else
				{
					OutObjInfo->PositionIndices.push_back(LineFaceVertices[0].PositionIndex);
					OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[0].TexCoordIndex);
					OutObjInfo->NormalIndices.push_back(LineFaceVertices[0].NormalIndex);

					OutObjInfo->PositionIndices.push_back(LineFaceVertices[i].PositionIndex);
					OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i].TexCoordIndex);
					OutObjInfo->NormalIndices.push_back(LineFaceVertices[i].NormalIndex);

					OutObjInfo->PositionIndices.push_back(LineFaceVertices[i + 1].PositionIndex);
					OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i + 1].TexCoordIndex);
					OutObjInfo->NormalIndices.push_back(LineFaceVertices[i + 1].NormalIndex);
				}
				VIndex += 3;
				++MeshTriangles;
			}
		}
		else if (line.rfind("mtllib ", 0) == 0)
		{
			MtlFileName = objDir + line.substr(7);
		}
		else if (line.rfind("usemtl ", 0) == 0)
		{
			MaterialNameTemp = line.substr(7);
			OutObjInfo->MaterialNames.push_back(MaterialNameTemp);
			OutObjInfo->GroupIndexStartArray.push_back(VIndex);
			subsetCount++;
		}
		else
		{
			UE_LOG("While parsing the filename %s, the following unknown symbol was encountered: \'%s\'", InFileName.c_str(), line.c_str());
		}
	}

	if (subsetCount == 0)
	{
		OutObjInfo->GroupIndexStartArray.push_back(0);
		subsetCount++;
	}
	OutObjInfo->GroupIndexStartArray.push_back(VIndex);

	if (OutObjInfo->GroupIndexStartArray.size() > 1 && OutObjInfo->GroupIndexStartArray[1] == 0)
	{
		OutObjInfo->GroupIndexStartArray.erase(OutObjInfo->GroupIndexStartArray.begin() + 1);
		subsetCount--;
	}

	if (!bHasNormal)
	{
		OutObjInfo->Normals.push_back(FVector(0.0f, 0.0f, 0.0f));
	}
	if (!bHasTexcoord)
	{
		OutObjInfo->TexCoords.push_back(FVector2D(0.0f, 0.0f));
	}

	FileIn.close();

	// Material 파싱 시작
	UE_LOG("[ObjImporter::LoadObjModel] MTL file path: %s", MtlFileName.c_str());

	if (MtlFileName.empty())
	{
		UE_LOG("[ObjImporter::LoadObjModel] MTL file path is empty - loading without materials");
		OutObjInfo->bHasMtl = false;
		return true;
	}

	// 한글 경로 지원: UTF-8 → UTF-16 변환 후 파일 열기
	FWideString WMtlPath = UTF8ToWide(MtlFileName);
	FileIn.open(WMtlPath);

	// .mtl 파일이 존재하지 않더라도 로딩을 중단하지 않습니다.
	// 경고를 로깅하고, 머티리얼이 없는 모델로 처리를 계속합니다.
	if (!FileIn)
	{
		UE_LOG("[ObjImporter::LoadObjModel] ERROR: Material file '%s' not found for obj '%s'. Loading model without materials.", MtlFileName.c_str(), InFileName.c_str());
		OutObjInfo->bHasMtl = false;
		return true;
	}

	UE_LOG("[ObjImporter::LoadObjModel] MTL file opened successfully, parsing materials...");

	uint32 MatCount = 0;

	TArray<FString> TempOptions;
	FString TempTexturePath;

	while (std::getline(FileIn, line))
	{
		if (line.empty()) continue;

		line.erase(0, line.find_first_not_of(" \t\n\r"));
		if (line[0] == '#')
			continue;

		if (line.rfind("newmtl ", 0) == 0)
		{
			FMaterialInfo TempMatInfo;
			TempMatInfo.MaterialName = line.substr(7);
			OutMaterialInfos.push_back(TempMatInfo);
			++MatCount;
			UE_LOG("[ObjImporter::LoadObjModel] Found material: %s", TempMatInfo.MaterialName.c_str());
		}
		else if (MatCount > 0)
		{
			// (Kd, Ka 등 다른 속성 파싱은 변경 없음)
			if (line.rfind("Kd ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].DiffuseColor = FVector(vx, vy, vz); }
			else if (line.rfind("Ka ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].AmbientColor = FVector(vx, vy, vz); }
			else if (line.rfind("Ke ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].EmissiveColor = FVector(vx, vy, vz); }
			else if (line.rfind("Ks ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].SpecularColor = FVector(vx, vy, vz); }
			else if (line.rfind("Tf ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].TransmissionFilter = FVector(vx, vy, vz); }
			else if (line.rfind("Tr ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].Transparency = value; }
			else if (line.rfind("d ", 0) == 0) { std::stringstream wss(line.substr(2)); float value; wss >> value; OutMaterialInfos[MatCount - 1].Transparency = 1.0f - value; }
			else if (line.rfind("Ni ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].OpticalDensity = value; }
			else if (line.rfind("Ns ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].SpecularExponent = value; }
			else if (line.rfind("illum ", 0) == 0) { std::stringstream wss(line.substr(6)); float value; wss >> value; OutMaterialInfos[MatCount - 1].IlluminationModel = static_cast<int32>(value); }

			// --- 텍스처 맵 파싱 로직 ---
			else if (line.rfind("map_Kd ", 0) == 0)
			{
				ParseTextureMapLine(line, 7, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].DiffuseTextureFileName = TempTexturePath;
			}
			else if (line.rfind("map_d ", 0) == 0)
			{
				ParseTextureMapLine(line, 7, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].TransparencyTextureFileName = TempTexturePath;
			}
			else if (line.rfind("map_Ka ", 0) == 0)
			{
				ParseTextureMapLine(line, 7, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].AmbientTextureFileName = TempTexturePath;
			}
			else if (line.rfind("map_Ks ", 0) == 0)
			{
				ParseTextureMapLine(line, 7, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].SpecularTextureFileName = TempTexturePath;
			}
			else if (line.rfind("map_Ns ", 0) == 0)
			{
				ParseTextureMapLine(line, 7, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].SpecularExponentTextureFileName = TempTexturePath;
			}
			else if (line.rfind("map_Ke ", 0) == 0)
			{
				ParseTextureMapLine(line, 7, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].EmissiveTextureFileName = TempTexturePath;
			}
			else if (line.rfind("map_Bump ", 0) == 0)
			{
				ParseTextureMapLine(line, 9, TempOptions, TempTexturePath);
				OutMaterialInfos[MatCount - 1].NormalTextureFileName = TempTexturePath;
				OutMaterialInfos[MatCount - 1].BumpMultiplier = GetFloatOption(TempOptions, "-bm", 1.0f);
			}
		}
	}
	FileIn.close();

	for (uint32 i = 0; i < OutObjInfo->MaterialNames.size(); ++i)
	{
		bool bHasMat = false;
		for (uint32 j = 0; j < OutMaterialInfos.size(); ++j)
		{
			if (OutObjInfo->MaterialNames[i] == OutMaterialInfos[j].MaterialName)
			{
				OutObjInfo->GroupMaterialArray.push_back(j);
				bHasMat = true;
				break;
			}
		}

		if (!bHasMat && !OutMaterialInfos.empty())
		{
			OutObjInfo->GroupMaterialArray.push_back(0);
		}
	}

	return true;
}

// FObjInfo to FStaticMesh
void FObjImporter::ConvertToStaticMesh(const FObjInfo& InObjInfo, const TArray<FMaterialInfo>& InMaterialInfos, FStaticMesh* const OutStaticMesh)
{
	OutStaticMesh->PathFileName = InObjInfo.ObjFileName;
	uint32 NumDuplicatedVertex = static_cast<uint32>(InObjInfo.PositionIndices.size());
	TArray<FVector> TangentForVertex;
	TArray<FVector> BiTangentForVertex;
	TangentForVertex.SetNum(NumDuplicatedVertex, FVector(0.f, 0.f, 0.f));
	BiTangentForVertex.SetNum(NumDuplicatedVertex, FVector(0.f, 0.f, 0.f));

	for (uint32 Index = 0; Index < NumDuplicatedVertex; Index += 3)
	{
		FVector P0 = InObjInfo.Positions[InObjInfo.PositionIndices[Index]];
		FVector P1 = InObjInfo.Positions[InObjInfo.PositionIndices[Index + 1]];
		FVector P2 = InObjInfo.Positions[InObjInfo.PositionIndices[Index + 2]];

		FVector E1 = P1 - P0;
		FVector E2 = P2 - P0;

		FVector2D UvP0 = InObjInfo.TexCoords[InObjInfo.TexCoordIndices[Index]];
		FVector2D UvP1 = InObjInfo.TexCoords[InObjInfo.TexCoordIndices[Index + 1]];
		FVector2D UvP2 = InObjInfo.TexCoords[InObjInfo.TexCoordIndices[Index + 2]];

		float DeltaU1 = UvP1.X - UvP0.X;
		float DeltaV1 = UvP1.Y - UvP0.Y;
		float DeltaU2 = UvP2.X - UvP0.X;
		float DeltaV2 = UvP2.Y - UvP0.Y;

		float DeterminantInv = 1.0f / (DeltaU1 * DeltaV2 - DeltaV1 * DeltaU2);

		FVector Tangent = (E1 * DeltaV2 - E2 * DeltaV1) * DeterminantInv;
		FVector BiTangent = (-E1 * DeltaU2 + E2 * DeltaU1) * DeterminantInv;

		TangentForVertex[Index] += Tangent;
		TangentForVertex[Index + 1] += Tangent;
		TangentForVertex[Index + 2] += Tangent;

		BiTangentForVertex[Index] += BiTangent;
		BiTangentForVertex[Index + 1] += BiTangent;
		BiTangentForVertex[Index + 2] += BiTangent;
	}

	std::unordered_map<VertexKey, uint32, VertexKeyHash> VertexMap;

	for (uint32 CurIndex = 0; CurIndex < NumDuplicatedVertex; ++CurIndex)
	{
		VertexKey Key{ InObjInfo.PositionIndices[CurIndex], InObjInfo.TexCoordIndices[CurIndex], InObjInfo.NormalIndices[CurIndex] };
		auto It = VertexMap.find(Key);
		if (It != VertexMap.end())
		{
			OutStaticMesh->Indices.push_back(It->second);
		}
		else
		{
			FVector Tangent = TangentForVertex[CurIndex];
			FVector Normal = InObjInfo.Normals[Key.NormalIndex];
			FVector BiTangent = BiTangentForVertex[CurIndex];

			Tangent = Tangent - Normal * FVector::Dot(Tangent, Normal);
			Tangent.Normalize();
			FVector4 FinalTangent(Tangent.X, Tangent.Y, Tangent.Z);
			FinalTangent.W = FVector::Dot(FVector::Cross(Tangent, Normal), BiTangent) > 0.0f ? 1.0f : -1.0f;

			FNormalVertex NormalVertex(
				InObjInfo.Positions[Key.PosIndex],
				Normal,
				InObjInfo.TexCoords[Key.TexIndex],
				FinalTangent,
				FVector4(1, 1, 1, 1)
			);
			OutStaticMesh->Vertices.push_back(NormalVertex);
			uint32 NewIndex = static_cast<uint32>(OutStaticMesh->Vertices.size() - 1);
			OutStaticMesh->Indices.push_back(NewIndex);
			VertexMap[Key] = NewIndex;
		}
	}

	// bHasMtl 체크를 제거하거나 bHasMaterial = true로 설정 (이후 로더에서 기본값을 주입할 것이므로)
	OutStaticMesh->bHasMaterial = true;

	// 파싱된 지오메트리 그룹(GroupIndexStartArray) 기준으로 그룹 수를 결정
	uint32 NumGroup = (InObjInfo.GroupIndexStartArray.size() > 0) ? static_cast<uint32>(InObjInfo.GroupIndexStartArray.size() - 1) : 0;

	if (NumGroup == 0)
	{
		// 지오메트리가 아예 없는 경우
		return;
	}

	OutStaticMesh->GroupInfos.resize(NumGroup);

	// InMaterialInfos가 비어있다고 해서 return하지 않음
	for (uint32 i = 0; i < NumGroup; ++i)
	{
		OutStaticMesh->GroupInfos[i].StartIndex = InObjInfo.GroupIndexStartArray[i];
		OutStaticMesh->GroupInfos[i].IndexCount = InObjInfo.GroupIndexStartArray[i + 1] - InObjInfo.GroupIndexStartArray[i];

		// GroupMaterialArray가 비어있거나 인덱스가 범위를 벗어나는 경우를 대비합니다.
		if (i < InObjInfo.GroupMaterialArray.size())
		{
			uint32 matIndex = InObjInfo.GroupMaterialArray[i];
			if (matIndex < InMaterialInfos.size())
			{
				OutStaticMesh->GroupInfos[i].InitialMaterialName = InMaterialInfos[matIndex].MaterialName;
			}
			// else: matIndex가 범위를 벗어났다면 InitialMaterialName은 비어있게 됨 (정상)
		}
		// else: InitialMaterialName은 비어있게 됨 (정상)
	}
}

FObjImporter::FFaceVertex FObjImporter::ParseVertexDef(const FString& InVertexDef)
{
	FFaceVertex Result{ 0, 0, 0 };
	std::stringstream ss(InVertexDef);
	FString part;
	uint32 temp_val;

	if (std::getline(ss, part, '/')) { if (!part.empty()) { std::stringstream conv(part); if (conv >> temp_val) Result.PositionIndex = temp_val - 1; } }
	if (std::getline(ss, part, '/')) { if (!part.empty()) { std::stringstream conv(part); if (conv >> temp_val) Result.TexCoordIndex = temp_val - 1; } }
	if (std::getline(ss, part, '/')) { if (!part.empty()) { std::stringstream conv(part); if (conv >> temp_val) Result.NormalIndex = temp_val - 1; } }

	return Result;
}
