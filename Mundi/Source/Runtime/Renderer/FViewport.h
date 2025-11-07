#pragma once
#include "Object.h"
#include "Vector.h"
#include <d3d11.h>

class FViewportClient;

/**
 * @brief 뷰포트 클래스 - UE의 FViewport를 모방
 */
class FViewport
{
public:
    FViewport();
    virtual ~FViewport();

    // 초기화 및 정리
    bool Initialize(float StartX, float StartY, float InSizeX, float InSizeY, ID3D11Device* Device);
    void Cleanup();

    // 렌더링
    void BeginRenderFrame();
    void Render();
    void EndRenderFrame();

    // 크기 조정
    void Resize(uint32 NewStartX, uint32 NewStartY,uint32 NewSizeX, uint32 NewSizeY);

    // ViewportClient 설정
    void SetViewportClient(FViewportClient* InClient) { ViewportClient = InClient; }
    FViewportClient* GetViewportClient() const { return ViewportClient; }
    
    // 접근자
    uint32 GetSizeX() const { return SizeX; }
    uint32 GetSizeY() const { return SizeY; }
    FVector2D GetSize() const { return FVector2D(static_cast<float>(SizeX), static_cast<float>(SizeY)); }

    uint32 GetStartX() const { return StartX; }
    uint32 GetStartY() const { return StartY; }
    
    FVector2D GetViewportMousePosition() { return ViewportMousePosition; }

    // 마우스/키보드 입력 처리
    void ProcessMouseMove(int32 X, int32 Y);
    void ProcessMouseButtonDown(int32 X, int32 Y, int32 Button);

    void ProcessMouseButtonUp(int32 X, int32 Y, int32 Button);
    void ProcessKeyDown(int32 KeyCode);
    void ProcessKeyUp(int32 KeyCode);

private:
    // 뷰포트 속성
    uint32 SizeX = 0;
    uint32 SizeY = 0;
    uint32 StartX = 0;
    uint32 StartY = 0;
    // D3D 리소스들
    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DDeviceContext = nullptr;

    // ViewportClient
    FViewportClient* ViewportClient = nullptr;

    FVector2D ViewportMousePosition{};
};

