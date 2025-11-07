#include "pch.h"
#include "Shader.h"

IMPLEMENT_CLASS(UShader)

// 컴파일 로직을 처리하는 비공개 헬퍼 함수
static bool CompileShaderInternal(
	const FWideString& InFilePath,
	const char* InEntryPoint,
	const char* InTarget,
	UINT InCompileFlags,
	const D3D_SHADER_MACRO* InDefines,
	ID3DBlob** OutBlob
)
{
	ID3DBlob* ErrorBlob = nullptr;
	HRESULT Hr = D3DCompileFromFile(
		InFilePath.c_str(),
		InDefines,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		InEntryPoint,
		InTarget,
		InCompileFlags,
		0,
		OutBlob,
		&ErrorBlob
	);

	if (FAILED(Hr))
	{
		if (ErrorBlob)
		{
			char* Msg = (char*)ErrorBlob->GetBufferPointer();

			// wstring을 UTF-8로 안전하게 변환 (한글 경로 지원)
			std::string NarrowPath = WideToUTF8(InFilePath);

			// Msg에 %가 포함될 수 있으므로 직접 출력 (포맷 문자열로 해석되지 않도록)
			std::string ErrorMessage = "Shader '" + NarrowPath + "' compile error: " + std::string(Msg);
			UE_LOG("%s", ErrorMessage.c_str());
			ErrorBlob->Release();
		}
		if (*OutBlob) { (*OutBlob)->Release(); }
		return false;
	}

	if (ErrorBlob) { ErrorBlob->Release(); }
	return true;
}

UShader::~UShader()
{
	ReleaseResources();
}

FString UShader::GenerateShaderKey(const TArray<FShaderMacro>& InMacros)
{
	// 매크로 순서가 달라도 동일한 키를 생성하기 위해 정렬합니다.
	TArray<FShaderMacro> SortedMacros = InMacros;
	SortedMacros.Sort([](const FShaderMacro& A, const FShaderMacro& B)
		{
			return A.Name < B.Name;
		});

	FString Key;
	for (int32 Index = 0; Index < SortedMacros.Num(); ++Index)
	{
		// 첫 번째 요소가 아닌 경우에만 구분자를 추가합니다.
		if (Index > 0)
		{
			Key += ", ";
		}

		const FShaderMacro& Macro = SortedMacros[Index];
		Key += Macro.Name + "=" + Macro.Definition;
	}
	return Key;
}

/**
 * @brief UResourceManager가 셰이더 리소스를 로드/가져오기 위해 호출하는 메인 함수.
 */
void UShader::Load(const FString& InShaderPath, ID3D11Device* InDevice, const TArray<FShaderMacro>& InMacros)
{
	assert(InDevice);

	// 1. 최초 로드 시에만 파일 경로 및 타임스탬프 처리
	if (FilePath.empty())
	{
		FilePath = InShaderPath;
		try
		{
			auto FileTime = std::filesystem::last_write_time(FilePath);
			SetLastModifiedTime(FileTime);
		}
		catch (...)
		{
			SetLastModifiedTime(std::filesystem::file_time_type::clock::now());
		}
		// Include 파일 파싱 (최초 1회)
		ParseIncludeFiles(FilePath);
	}

	// 2. 실제 컴파일/가져오기 로직은 GetOrCompileShaderVariant에 위임
	// (이 함수는 InMacros에 대한 Variant가 맵에 없으면 컴파일하고 추가함)
	GetOrCompileShaderVariant(InDevice, InMacros);
}

/**
 * @brief 외부(예: UMaterial)에서 특정 매크로 조합의 Variant를 요청할 때 사용합니다.
 * 1. 이 셰이더 객체(ActualFilePath)에 대해 해당 매크로 Variant가 이미 컴파일되었는지 확인합니다.
 * 2. 컴파일되지 않았다면, 즉시 컴파일하고 맵에 추가합니다.
 * 3. FShaderVariant*를 반환합니다. (컴파일 실패 시 nullptr)
 *
 * @param InDevice D3D 디바이스
 * @param InMacros 컴파일(또는 검색)할 매크로 배열
 * @return FShaderVariant 포인터 (성공 시) 또는 nullptr (실패 시)
 */
FShaderVariant* UShader::GetOrCompileShaderVariant(ID3D11Device* InDevice, const TArray<FShaderMacro>& InMacros)
{
	assert(InDevice);

	// 이 UShader 객체가 어떤 파일인지 알아야 컴파일 가능
	if (FilePath.empty())
	{
		UE_LOG("GetOrCompileShaderVariant Error: ActualFilePath is empty. Load() must be called at least once.");
		return nullptr;
	}

	// 1. 매크로 배열을 기반으로 고유 키 생성
	FString Key = GenerateShaderKey(InMacros);

	// 2. 맵에 이미 컴파일된 Variant가 있는지 확인
	if (FShaderVariant* Found = ShaderVariantMap.Find(Key))
	{
		return Found; // 찾았으면 즉시 반환
	}

	// 3. 맵에 없음 -> 새로 컴파일
	FShaderVariant NewShaderVariant;
	bool bSuccess = CompileVariantInternal(InDevice, FilePath, InMacros, NewShaderVariant);

	if (bSuccess)
	{
		// 4. 맵에 추가하고, 새로 추가된 항목의 포인터(주소)를 반환
		// TMap::Add()는 추가된 FShaderVariant의 레퍼런스를 포함하는 TPair를 반환합니다.
		// .Value의 주소를 가져옵니다.
		ShaderVariantMap.Add(Key, NewShaderVariant);
		return &ShaderVariantMap[Key];
	}

	// 5. 컴파일 실패
	UE_LOG("GetOrCompileShaderVariant: Failed to compile variant for key '%s'", Key.c_str());

	// 컴파일에 실패했더라도, 향후 동일한 요청이 왔을 때
	// 다시 컴파일을 시도하지 않도록 '비어있는' Variant를 맵에 추가할 수 있습니다.
	// ShaderVariantMap.Add(Key, NewShaderVariant); // (선택적)

	return nullptr;
}

/**
 * @brief [신규] 실제 컴파일 로직을 수행하는 private 헬퍼 함수입니다.
 * @param InDevice D3D 디바이스
 * @param InShaderPath 컴파일할 .hlsl 파일 경로 (ActualFilePath)
 * @param InMacros 컴파일에 사용할 매크로
 * @param OutVariant [출력] 컴파일된 리소스(Blob, Shader, Layout)가 채워질 구조체
 * @return 컴파일 성공 시 true, 실패 시 false
 */
bool UShader::CompileVariantInternal(ID3D11Device* InDevice, const FString& InShaderPath, const TArray<FShaderMacro>& InMacros, FShaderVariant& OutVariant)
{
	FWideString WFilePath = UTF8ToWide(InShaderPath);

	// --- 1. D3D_SHADER_MACRO* 형태로 변환 ---
	TArray<D3D_SHADER_MACRO> Defines;
	TArray<TPair<FString, FString>> MacroStrings; // 포인터 유효성 유지를 위한 저장소
	MacroStrings.reserve(InMacros.Num());
	for (const FShaderMacro& Macro : InMacros)
	{
		MacroStrings.emplace_back(
			FString(Macro.Name.begin(), Macro.Name.end()),
			FString(Macro.Definition.begin(), Macro.Definition.end())
		);
		Defines.push_back({ MacroStrings.back().first.c_str(), MacroStrings.back().second.c_str() });
	}
	Defines.push_back({ NULL, NULL }); // 배열의 끝을 알리는 NULL 터미네이터

	// --- 2. 컴파일 플래그 설정 ---
	UINT CompileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	auto EndsWith = [](const FString& str, const FString& suffix)
		{
			if (str.size() < suffix.size()) return false;
			return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
				[](char a, char b) { return static_cast<char>(::tolower(a)) == static_cast<char>(::tolower(b)); });
		};

	HRESULT Hr;
	bool bVsCompiled = false;
	bool bPsCompiled = false;

	// --- 3. 컴파일 결과를 OutVariant에 저장 ---
	if (EndsWith(InShaderPath, "_VS.hlsl"))
	{
		bVsCompiled = CompileShaderInternal(WFilePath, "mainVS", "vs_5_0", CompileFlags, Defines.data(), &OutVariant.VSBlob);
		if (bVsCompiled)
		{
			Hr = InDevice->CreateVertexShader(OutVariant.VSBlob->GetBufferPointer(), OutVariant.VSBlob->GetBufferSize(), nullptr, &OutVariant.VertexShader);
			assert(SUCCEEDED(Hr));
			CreateInputLayout(InDevice, InShaderPath, OutVariant); // OutVariant 전달
		}
	}
	else if (EndsWith(InShaderPath, "_PS.hlsl"))
	{
		bPsCompiled = CompileShaderInternal(WFilePath, "mainPS", "ps_5_0", CompileFlags, Defines.data(), &OutVariant.PSBlob);
		if (bPsCompiled)
		{
			Hr = InDevice->CreatePixelShader(OutVariant.PSBlob->GetBufferPointer(), OutVariant.PSBlob->GetBufferSize(), nullptr, &OutVariant.PixelShader);
			assert(SUCCEEDED(Hr));
		}
	}
	else // (VS + PS)
	{
		bVsCompiled = CompileShaderInternal(WFilePath, "mainVS", "vs_5_0", CompileFlags, Defines.data(), &OutVariant.VSBlob);
		bPsCompiled = CompileShaderInternal(WFilePath, "mainPS", "ps_5_0", CompileFlags, Defines.data(), &OutVariant.PSBlob);

		if (bVsCompiled)
		{
			Hr = InDevice->CreateVertexShader(OutVariant.VSBlob->GetBufferPointer(), OutVariant.VSBlob->GetBufferSize(), nullptr, &OutVariant.VertexShader);
			assert(SUCCEEDED(Hr));
			CreateInputLayout(InDevice, InShaderPath, OutVariant);
		}
		if (bPsCompiled)
		{
			Hr = InDevice->CreatePixelShader(OutVariant.PSBlob->GetBufferPointer(), OutVariant.PSBlob->GetBufferSize(), nullptr, &OutVariant.PixelShader);
			assert(SUCCEEDED(Hr));
		}
	}

	// 4. 핫 리로드용 매크로 저장
	OutVariant.SourceMacros = InMacros;

	// 5. 컴파일 성공 여부 반환 (VS 또는 PS 둘 중 하나라도 성공 시)
	return bVsCompiled || bPsCompiled;
}

FShaderVariant* UShader::GetShaderVariant(const TArray<FShaderMacro>& InMacros)
{
	FString Key = GenerateShaderKey(InMacros);
	return ShaderVariantMap.Find(Key);
}

ID3D11InputLayout* UShader::GetInputLayout(const TArray<FShaderMacro>& InMacros)
{
	FShaderVariant* Variant = GetShaderVariant(InMacros);
	if (Variant)
	{
		return Variant->InputLayout;
	}
	return nullptr;
}

ID3D11VertexShader* UShader::GetVertexShader(const TArray<FShaderMacro>& InMacros)
{
	FShaderVariant* Variant = GetShaderVariant(InMacros);
	if (Variant)
	{
		return Variant->VertexShader;
	}
	return nullptr;
}

ID3D11PixelShader* UShader::GetPixelShader(const TArray<FShaderMacro>& InMacros)
{
	FShaderVariant* Variant = GetShaderVariant(InMacros);
	if (Variant)
	{
		return Variant->PixelShader;
	}
	return nullptr;
}

void UShader::CreateInputLayout(ID3D11Device* Device, const FString& InShaderPath, FShaderVariant& InOutVariant)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> descArray = UResourceManager::GetInstance().GetProperInputLayout(InShaderPath);
	const D3D11_INPUT_ELEMENT_DESC* layout = descArray.data();
	uint32 layoutCount = static_cast<uint32>(descArray.size());

	// InputLayout을 사용하지 않는 VS를 위한 처리
	if (0 < layoutCount)
	{
		HRESULT hr = Device->CreateInputLayout(
			layout,
			layoutCount,
			InOutVariant.VSBlob->GetBufferPointer(),
			InOutVariant.VSBlob->GetBufferSize(),
			&InOutVariant.InputLayout);
		assert(SUCCEEDED(hr));
	}
}

void UShader::ReleaseResources()
{
	// 맵의 모든 Variant를 순회하며 각각의 리소스를 해제합니다.
	for (auto& Pair : ShaderVariantMap)
	{
		Pair.second.Release(); // FShaderVariant::Release() 호출
	}
	ShaderVariantMap.Empty();
}

bool UShader::IsOutdated() const
{
	// Use the stored actual file path (not the unique key with macros)
	if (FilePath.empty())
	{
		return false; // No file path stored
	}

	try
	{
		if (!std::filesystem::exists(FilePath))
		{
			return false; // File doesn't exist, not outdated
		}

		// 메인 shader 파일 timestamp 체크
		auto CurrentFileTime = std::filesystem::last_write_time(FilePath);
		if (CurrentFileTime > GetLastModifiedTime())
		{
			return true;
		}

		// Include 파일들의 timestamp 체크
		for (const auto& Pair : IncludedFileTimestamps)
		{
			const FString& IncludedFile = Pair.first;
			const auto& StoredTime = Pair.second;

			// Include 파일이 존재하는지 확인
			if (!std::filesystem::exists(IncludedFile))
			{
				continue; // 파일이 없으면 건너뛰기
			}

			// Include 파일의 현재 timestamp 확인
			auto CurrentIncludeTime = std::filesystem::last_write_time(IncludedFile);
			if (CurrentIncludeTime > StoredTime)
			{
				return true; // Include 파일이 변경됨
			}
		}

		return false; // 모든 파일이 최신 상태
	}
	catch (...)
	{
		return false; // Error checking file, assume not outdated
	}
}

 // 셰이더 파일(.hlsl) 또는 그 #include 파일이 변경되었을 때, 이 셰이더가 관리하는 모든 Variant를 다시 컴파일합니다.
bool UShader::Reload(ID3D11Device* InDevice)
{
	// 1. 유효성 검사 및 핫 리로드 필요 여부 확인
	if (!InDevice)
	{
		return false;
	}

	if (FilePath.empty())
	{
		UE_LOG("Hot Reload Failed: No actual file path stored for shader.");
		return false;
	}

	// IsOutdated()는 메인 파일과 Include 파일들을 모두 검사합니다.
	if (!IsOutdated())
	{
		return false; // 변경 사항 없음
	}

	UE_LOG("Hot Reloading Shader File: %s (%d variants)", FilePath.c_str(), ShaderVariantMap.Num());

	// 2. [백업] 현재 맵을 Old 맵으로 이동시킵니다.
	// (ShaderVariantMap은 이제 비어있습니다)
	TMap<FString, FShaderVariant> OldShaderVariantMap = std::move(ShaderVariantMap);

	bool bAllReloadsSuccessful = true;

	// 3. [재시도] Old 맵에 있던 모든 Variant에 대해 Load를 다시 호출합니다.
	for (auto& Pair : OldShaderVariantMap)
	{
		const FString& Key = Pair.first;
		const TArray<FShaderMacro>& MacrosToReload = Pair.second.SourceMacros;

		// Load() 함수는 (이제 비어있는) ShaderVariantMap에
		// Variant가 없으므로 새로 컴파일을 시도합니다.
		Load(FilePath, InDevice, MacrosToReload);

		// 4. [검증] 새로 컴파일된 Variant가 유효한지 확인
		FShaderVariant* NewVariant = ShaderVariantMap.Find(Key); // 새로 로드된 맵에서 찾기

		if (!NewVariant || (!NewVariant->VertexShader && !NewVariant->PixelShader))
		{
			// 하나라도 컴파일에 실패하면 전체 핫 리로드는 실패로 간주
			bAllReloadsSuccessful = false;
			UE_LOG("Hot Reload Failed for variant: %s", Key.c_str());
		}
	}

	// 5. [처리] GPU 파이프라인에서 리소스 언바인딩 (필수)
	// 릴리즈하기 전에 GPU가 리소스를 잡고 있지 않도록 합니다.
	ID3D11DeviceContext* Context = nullptr;
	InDevice->GetImmediateContext(&Context);
	if (Context)
	{
		// 파이프라인에서 현재 바인딩된 셰이더를 모두 해제합니다.
		// (Old 맵의 어떤 셰이더가 바인딩되었을지 모르므로, 그냥 null로 설정)
		Context->VSSetShader(nullptr, nullptr, 0);
		Context->PSSetShader(nullptr, nullptr, 0);
		Context->IASetInputLayout(nullptr);
		Context->Flush(); // GPU 작업 완료 대기
		Context->Release();
	}

	// 6. [최종 처리] 성공/실패에 따라 맵 처리
	if (bAllReloadsSuccessful)
	{
		UE_LOG("Hot Reload Succeeded for %s", FilePath.c_str());

		// 성공: Old 맵의 모든 리소스를 해제합니다.
		for (auto& Pair : OldShaderVariantMap)
		{
			Pair.second.Release();
		}
		OldShaderVariantMap.Empty();

		// 갱신된 타임스탬프를 설정합니다.
		try
		{
			SetLastModifiedTime(std::filesystem::last_write_time(FilePath));
			UpdateIncludeTimestamps(); // Include 파일 타임스탬프도 갱신
		}
		catch (...) { /* 무시 */ }

		return true;
	}
	else
	{
		UE_LOG("Hot Reload Failed: Restoring old variants for %s", FilePath.c_str());

		// 실패: 새로 컴파일한 맵(ShaderVariantMap)의 리소스를 모두 해제합니다.
		for (auto& Pair : ShaderVariantMap)
		{
			Pair.second.Release();
		}

		// Old 맵(정상 작동하던)을 현재 맵으로 복원합니다.
		ShaderVariantMap = std::move(OldShaderVariantMap);

		return false;
	}
}

// Include 파일 파싱 (재귀적으로 처리)
void UShader::ParseIncludeFiles(const FString& ShaderPath)
{
	// 이미 파싱된 파일 목록 초기화
	IncludedFiles.clear();

	// 파싱할 파일 큐
	TArray<FString> FilesToParse;
	FilesToParse.push_back(ShaderPath);

	// 이미 파싱한 파일 추적 (중복 방지 및 순환 참조 방지)
	TSet<FString> ParsedFiles;

	while (!FilesToParse.empty())
	{
		FString CurrentFile = FilesToParse.back();
		FilesToParse.pop_back();

		// 이미 파싱한 파일은 건너뛰기
		if (ParsedFiles.find(CurrentFile) != ParsedFiles.end())
		{
			continue;
		}
		ParsedFiles.insert(CurrentFile);

		// 파일 존재 여부 확인
		if (!std::filesystem::exists(CurrentFile))
		{
			continue;
		}

		// 파일 열기
		std::ifstream File(CurrentFile);
		if (!File.is_open())
		{
			continue;
		}

		// 현재 파일의 디렉토리 경로
		std::filesystem::path CurrentDir = std::filesystem::path(CurrentFile).parent_path();

		// 한 줄씩 읽으며 #include 찾기
		FString Line;
		while (std::getline(File, Line))
		{
			// 공백 제거
			size_t FirstNonSpace = Line.find_first_not_of(" \t\r\n");
			if (FirstNonSpace == FString::npos)
			{
				continue;
			}
			Line = Line.substr(FirstNonSpace);

			// #include 지시문 찾기
			if (Line.compare(0, 8, "#include") == 0)
			{
				// #include "filename" 패턴 파싱
				size_t QuoteStart = Line.find('"');
				size_t QuoteEnd = Line.find('"', QuoteStart + 1);

				if (QuoteStart != FString::npos && QuoteEnd != FString::npos)
				{
					FString IncludedFile = Line.substr(QuoteStart + 1, QuoteEnd - QuoteStart - 1);

					// 상대 경로 해석
					std::filesystem::path IncludePath;
					if (std::filesystem::path(IncludedFile).is_absolute())
					{
						IncludePath = IncludedFile;
					}
					else
					{
						IncludePath = CurrentDir / IncludedFile;
					}

					// 경로 정규화
					try
					{
						IncludePath = std::filesystem::canonical(IncludePath);
						FString NormalizedPath = IncludePath.string();

						// 포함된 파일 목록에 추가
						if (std::find(IncludedFiles.begin(), IncludedFiles.end(), NormalizedPath) == IncludedFiles.end())
						{
							IncludedFiles.push_back(NormalizedPath);
							// 재귀적으로 파싱하기 위해 큐에 추가
							FilesToParse.push_back(NormalizedPath);
						}
					}
					catch (...)
					{
						// 파일이 존재하지 않거나 경로 오류 - 무시
					}
				}
			}
		}

		File.close();
	}

	// Include 파일들의 timestamp 업데이트
	UpdateIncludeTimestamps();
}

// Include 파일들의 timestamp 업데이트
void UShader::UpdateIncludeTimestamps()
{
	IncludedFileTimestamps.clear();

	for (const FString& IncludedFile : IncludedFiles)
	{
		try
		{
			if (std::filesystem::exists(IncludedFile))
			{
				auto FileTime = std::filesystem::last_write_time(IncludedFile);
				IncludedFileTimestamps[IncludedFile] = FileTime;
			}
		}
		catch (...)
		{
			// 파일 타임스탬프 읽기 실패 - 무시
		}
	}
}
