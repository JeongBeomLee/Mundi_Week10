#pragma once
#include <d3d11.h>
#include <filesystem>
#include "Object.h"

class UResourceBase : public UObject
{
public:
	DECLARE_CLASS(UResourceBase, UObject)

	UResourceBase() = default;
	virtual ~UResourceBase() {}

	const FString& GetFilePath() const { return FilePath; }
	void SetFilePath(const FString& InFilePath) { FilePath = InFilePath; }

	// 리소스가 유효하게 로드되었는지 확인 (파생 클래스에서 오버라이드)
	virtual bool IsValidResource() const { return true; }

	// Hot Reload Support
	std::filesystem::file_time_type GetLastModifiedTime() const { return LastModifiedTime; }
	void SetLastModifiedTime(std::filesystem::file_time_type InTime) { LastModifiedTime = InTime; }

protected:
	FString FilePath;	// 원본 파일의 경로이자, UResourceManager에 등록된 Key 
	std::filesystem::file_time_type LastModifiedTime;
};