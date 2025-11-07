#pragma once
#include "UIWindow.h"
#include <cstdarg>

class UConsoleWidget;

/**
 * @brief Console Window containing ConsoleWidget
 * Replaces the old ImGuiConsole with new widget system
 */
class UConsoleWindow : public UUIWindow
{
public:
    DECLARE_CLASS(UConsoleWindow, UUIWindow)

    UConsoleWindow();
    ~UConsoleWindow() override;

    void Initialize() override;
    
    // Console specific methods
    void AddLog(const char* fmt, ...);
    void ClearLog();
    
    // Accessors
    UConsoleWidget* GetConsoleWidget() const;

private:
    UConsoleWidget* ConsoleWidget;
};
