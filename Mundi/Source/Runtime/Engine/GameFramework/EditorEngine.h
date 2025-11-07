#pragma once
#include "pch.h"

class URenderer;
class D3D11RHI;
class UWorld;

struct FWorldContext
{
    FWorldContext();
    FWorldContext(UWorld* InWorld, EWorldType InWorldType)
    {
        World = InWorld; WorldType = InWorldType;
    }
    UWorld* World;
    EWorldType WorldType;
};

class UEditorEngine
{
public:
    bool bChangedPieToEditor = false;

    UEditorEngine();
    virtual ~UEditorEngine();

    virtual bool Startup(HINSTANCE hInstance);
    void MainLoop();
    void Shutdown();

    void BuildScene();
    void StartPIE();
    void EndPIE();
    bool IsPIEActive() const { return bPIEActive; }

    HWND GetHWND() const { return HWnd; }

    URenderer* GetRenderer() const { return Renderer.get(); }
    D3D11RHI* GetRHIDevice() { return &RHIDevice; }
    UWorld* GetDefaultWorld();
    UWorld* GetPIEWorld() const;
    const TArray<FWorldContext>& GetWorldContexts() { return WorldContexts; }

    void AddWorldContext(const FWorldContext& InWorldContext)
    {
        WorldContexts.push_back(InWorldContext);
    }


    bool bStandAloneBuild = false;
protected:
    bool CreateMainWindow(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void GetViewportSize(HWND hWnd);

    virtual void Tick(float DeltaSeconds);
    void Render();

    void HandleUVInput(float DeltaSeconds);

    //윈도우 핸들
    HWND HWnd = nullptr;

    //디바이스 리소스 및 렌더러
    D3D11RHI RHIDevice;
    std::unique_ptr<URenderer> Renderer;

    //월드 핸들
    TArray<FWorldContext> WorldContexts;

    //틱 상태
    bool bRunning = false;
    bool bPIEActive = false;

    // 클라이언트 사이즈
    static float ClientWidth;
    static float ClientHeight;

private:
    bool bUVScrollPaused = true;
    float UVScrollTime = 0.0f;
    FVector2D UVScrollSpeed = FVector2D(0.5f, 0.5f);


};