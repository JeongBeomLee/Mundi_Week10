#pragma once
#include <string>
#include <algorithm>
#include <windows.h>
#include <filesystem>

#include "UEContainer.h"

namespace fs = std::filesystem;

extern const FString GDataDir;
extern const FString GCacheDir;

// ============================================================================
// 경로 정규화 유틸리티 함수
// ============================================================================

/**
 * @brief 경로 문자열의 디렉토리 구분자를 모두 '/'로 통일합니다.
 * @details Windows/Unix 간 경로 구분자 차이로 인한 중복 리소스 로드를 방지합니다.
 *          예: "Data\\Textures\\image.png" -> "Data/Textures/image.png"
 * @param InPath 정규화할 경로 문자열
 * @return 정규화된 경로 문자열 ('/' 구분자 사용)
 */
inline FString NormalizePath(const FString& InPath)
{
	FString Result = InPath;
	std::replace(Result.begin(), Result.end(), '\\', '/');
	return Result;
}

// ============================================================================
// 문자열 인코딩 변환 유틸리티 함수
// ============================================================================

/**
 * @brief UTF-8 문자열을 UTF-16 와이드 문자열로 변환합니다.
 * @details 한글 경로 등 비ASCII 문자를 포함한 경로를 Windows API에서 사용할 수 있도록 변환합니다.
 * @param InUtf8Str UTF-8 인코딩된 입력 문자열
 * @return UTF-16 인코딩된 와이드 문자열 (변환 실패 시 빈 문자열)
 */
inline FWideString UTF8ToWide(const FString& InUtf8Str)
{
	if (InUtf8Str.empty()) return FWideString();

	int needed = ::MultiByteToWideChar(CP_UTF8, 0, InUtf8Str.c_str(), -1, nullptr, 0);
	if (needed <= 0)
	{
		// UTF-8 변환 실패 시 ANSI 시도
		needed = ::MultiByteToWideChar(CP_ACP, 0, InUtf8Str.c_str(), -1, nullptr, 0);
		if (needed <= 0) return FWideString();

		FWideString result(needed - 1, L'\0');
		::MultiByteToWideChar(CP_ACP, 0, InUtf8Str.c_str(), -1, result.data(), needed);
		return result;
	}

	FWideString result(needed - 1, L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, InUtf8Str.c_str(), -1, result.data(), needed);
	return result;
}

/**
 * @brief UTF-16 와이드 문자열을 UTF-8 문자열로 변환합니다.
 * @details Windows API에서 반환된 와이드 문자열을 UTF-8로 변환하여 프로젝트 전역에서 사용합니다.
 * @param InWideStr UTF-16 인코딩된 입력 와이드 문자열
 * @return UTF-8 인코딩된 문자열 (변환 실패 시 빈 문자열)
 */
inline FString WideToUTF8(const FWideString& InWideStr)
{
	if (InWideStr.empty()) return FString();

	int size_needed = ::WideCharToMultiByte(
		CP_UTF8,                            // UTF-8 코드 페이지
		0,                                  // 플래그
		InWideStr.c_str(),                  // 입력 wstring
		static_cast<int>(InWideStr.size()), // 입력 문자 수
		nullptr,                            // 출력 버퍼 (nullptr이면 크기만 계산)
		0,                                  // 출력 버퍼 크기
		nullptr,                            // 기본 문자
		nullptr                             // 기본 문자 사용 여부
	);

	if (size_needed <= 0)
	{
		return FString(); // 변환 실패
	}

	// 실제 변환 수행
	FString result(size_needed, 0);
	::WideCharToMultiByte(
		CP_UTF8,
		0,
		InWideStr.c_str(),
		static_cast<int>(InWideStr.size()),
		&result[0],                         // 출력 버퍼
		size_needed,
		nullptr,
		nullptr
	);

	return result;
}

inline FString ConvertDataPathToCachePath(const FString& InAssetPath)
{
	FString DataDirPrefix = GDataDir + "/";

	// GDataDir("Data")로 시작하는지 (대소문자 무관) 확인
	// _strnicmp는 C-스타일 문자열을 받으며, n 글자수만큼 대소문자 무관 비교
	if (InAssetPath.length() >= DataDirPrefix.length() &&
		_strnicmp(InAssetPath.c_str(), DataDirPrefix.c_str(), DataDirPrefix.length()) == 0)
	{
		// "GCacheDir/" 접두사를 제거하고 GCacheDir/"... " 접두사를 붙임
		return GCacheDir + "/" + InAssetPath.substr(DataDirPrefix.length());
	}

	// Data/로 시작하지 않는 경로 (예: 절대 경로, Data 외부의 상대 경로)
	// GetDDSCachePath의 기존 정책을 따라 파일명만 사용
	FWideString WPath = UTF8ToWide(InAssetPath);
	fs::path FileName = fs::path(WPath).filename();

	return GCacheDir + "/" + WideToUTF8(FileName.wstring());
}

/**
 * 에셋(예: mtl) 내부에서 참조된 경로를 해석하여
 * 엔진이 사용하는 (현재 작업 디렉토리 기준) 상대 경로로 변환합니다.
 */
inline FString ResolveAssetRelativePath(const FString& InAssetPath, const FString& InAssetBaseDir)
{
	if (InAssetPath.empty())
	{
		return "";
	}

	try
	{
		FString NormalizedAssetPath = NormalizePath(InAssetPath);

		FString DataDirPrefix = GDataDir + "/";
		if (NormalizedAssetPath.length() >= DataDirPrefix.length() &&
			_strnicmp(NormalizedAssetPath.c_str(), DataDirPrefix.c_str(), DataDirPrefix.length()) == 0)
		{
			return NormalizedAssetPath;
		}

		FWideString WTexPath = UTF8ToWide(NormalizedAssetPath);
		FWideString WBaseDir = UTF8ToWide(InAssetBaseDir);
		fs::path TexPath(WTexPath);
		fs::path BaseDir(WBaseDir);
		fs::path FinalPath;

		if (TexPath.is_absolute())
		{
			FinalPath = TexPath.lexically_normal();
		}
		else
		{
			FinalPath = (BaseDir / TexPath).lexically_normal();
		}

		// 현재 작업 디렉토리 기준 상대 경로로 변환 시도
		fs::path CurrentDir = fs::current_path();
		std::error_code ec;
		fs::path RelativePath = fs::relative(FinalPath, CurrentDir, ec);

		fs::path PathToUse = (ec || RelativePath.empty()) ? FinalPath : RelativePath;

		return NormalizePath(WideToUTF8(PathToUse.wstring()));
	}
	catch (const std::exception&)
	{
		// 예외 발생 시 원본 경로 유지
		return NormalizePath(InAssetPath);
	}
}