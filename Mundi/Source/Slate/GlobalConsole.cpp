#include "pch.h"
#include "Widgets/ConsoleWidget.h"

IMPLEMENT_CLASS(UGlobalConsole)

UConsoleWidget* UGlobalConsole::ConsoleWidget = nullptr;

void UGlobalConsole::Initialize()
{
    // Nothing special to initialize
}

void UGlobalConsole::Shutdown()
{
    ConsoleWidget = nullptr;
}

void UGlobalConsole::SetConsoleWidget(UConsoleWidget* InConsoleWidget)
{
    ConsoleWidget = InConsoleWidget;
    if (InConsoleWidget)
    {
        UE_LOG("GlobalConsole: ConsoleWidget set successfully\n");
    }
    else
    {
        UE_LOG("GlobalConsole: ConsoleWidget set to null\n");
    }
}

UConsoleWidget* UGlobalConsole::GetConsoleWidget()
{
    return ConsoleWidget;
}

void UGlobalConsole::Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogV(fmt, args);
    va_end(args);
}

void UGlobalConsole::LogV(const char* fmt, va_list args)
{
    // Format the message once
    char tmp[4096];
    vsnprintf_s(tmp, _countof(tmp), fmt, args);

    // Always output to Visual Studio debug console
    OutputDebugStringA(tmp);
    OutputDebugStringA("\n");

    // Also output to in-game console widget if available
    if (ConsoleWidget)
    {
        // Need to use a copy of va_list for second call
        va_list args_copy;
        va_copy(args_copy, args);
        ConsoleWidget->VAddLog(fmt, args_copy);
        va_end(args_copy);
    }
}

// Global C functions for compatibility
extern "C" void ConsoleLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UGlobalConsole::LogV(fmt, args);
    va_end(args);
}

extern "C" void ConsoleLogV(const char* fmt, va_list args)
{
    UGlobalConsole::LogV(fmt, args);
}
