#pragma once
#include <mutex>

enum class LogType : uint8
{
    Info,
    Warning,
    Error,
    Debug
};

class FLogManager
{
public:
    static FLogManager& GetInstance();

    void Initialize(const std::wstring& LogFileName);

    void WriteLog(LogType LogType, const wchar_t* Format, ...);

    void CloseCurrentFile();

private:
    FLogManager() = default;
    ~FLogManager();

    const wchar_t* GetTypeLabel(LogType Type) const;

    std::wofstream LogFile;
    std::mutex FileMutex;
   
};

#define INITIALIZE_LOG(FileName) \
    FLogManager::GetInstance().Initialize(FileName);

#define WRITE_LOG_FILE(LogTypeValue, ...) \
    FLogManager::GetInstance().WriteLog(LogTypeValue, __VA_ARGS__);

#define CLOSE_LOG_FILE() \
    FLogManager::GetInstance().CloseCurrentFile();