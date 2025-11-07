#include "pch.h"
#include "ConsoleWindow.h"
#include "Widgets/ConsoleWidget.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(UConsoleWindow)

UConsoleWindow::UConsoleWindow()
    : UUIWindow()
{
    ConsoleWidget = nullptr;
    
    // 윈도우 설정
    GetMutableConfig().WindowTitle = "Console Window";
    GetMutableConfig().DefaultSize = ImVec2(800, 500);
    GetMutableConfig().MinSize = ImVec2(400, 200);

    // Create and initialize console widget
    ConsoleWidget = NewObject<UConsoleWidget>();
    if (ConsoleWidget)
    {
        UE_LOG("ConsoleWidget created successfully\n");
        ConsoleWidget->Initialize();
        AddWidget(ConsoleWidget);
        UE_LOG("ConsoleWindow: Successfully created ConsoleWidget");
    }
    else
    {
        UE_LOG("ERROR: Failed to create ConsoleWidget\n");
    }
}

UConsoleWindow::~UConsoleWindow()
{
    // ConsoleWidget은 UUIWindow의 소멸자에서 자동으로 삭제됨
    // 추가적인 정리가 필요한 경우 여기에 작성
}

void UConsoleWindow::Initialize()
{
}

void UConsoleWindow::AddLog(const char* fmt, ...)
{
    if (ConsoleWidget)
    {
        va_list args;
        va_start(args, fmt);
        ConsoleWidget->VAddLog(fmt, args);
        va_end(args);
    }
}

void UConsoleWindow::ClearLog()
{
    if (ConsoleWidget)
    {
        ConsoleWidget->ClearLog();
    }
}

UConsoleWidget* UConsoleWindow::GetConsoleWidget() const
{
    return ConsoleWidget;
}
