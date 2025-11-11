#include "pch.h"
#include "LogManager.h"

FLogManager& FLogManager::GetInstance()
{
    static FLogManager Instance;
    return Instance;
}

void FLogManager::Initialize(const std::wstring& LogFileName)
{
    const std::filesystem::path LogDirectory = L"./logs";
    std::error_code ErrorCode;
    if (!std::filesystem::exists(LogDirectory) && !std::filesystem::create_directories(LogDirectory, ErrorCode))
    {
        return;
    }

    std::filesystem::path LogPath = LogDirectory / (LogFileName + L".txt");
    std::lock_guard<std::mutex> Lock(FileMutex);    
    if (LogFile.is_open())
    {
        LogFile.close();
    }
    // 기존 파일에 덧붙이길 원하면 std::ios::app 사용
    LogFile.open(LogPath, std::ios::out | std::ios::trunc);
}

void FLogManager::WriteLog(LogType LogType, const wchar_t* Format, ...)
{
    std::lock_guard<std::mutex> Lock(FileMutex);
    if (!LogFile.is_open())
    {
        return;
    }

    wchar_t MessageBuffer[2048];
    va_list Arguments;
    va_start(Arguments, Format);
    _vsnwprintf_s(MessageBuffer, _countof(MessageBuffer), _TRUNCATE, Format, Arguments);
    va_end(Arguments);

    LogFile << GetTypeLabel(LogType) << L": " << MessageBuffer << "\n";
}

void FLogManager::CloseCurrentFile()
{
    std::lock_guard<std::mutex> Lock(FileMutex);
    if (LogFile.is_open())
    {
        LogFile.close();
    }
}

FLogManager::~FLogManager()
{
    std::lock_guard<std::mutex> Lock(FileMutex);
    if (LogFile.is_open())
    {
        LogFile.close();
    }
}

const wchar_t* FLogManager::GetTypeLabel(LogType Type) const
{
    switch (Type)
    {
    case LogType::Info:
        return L"[Info] ";
    case LogType::Warning:
        return L"[Warning] ";
    case LogType::Error:
        return L"[Error] ";
    case LogType::Debug:
        return L"[Debug] ";
    default:
        return L"[Info] ";
    }
}
