#include "pch.h"
#include "FViewport.h"
#include "FViewportClient.h"

FViewport::FViewport()
{
}

FViewport::~FViewport()
{
	Cleanup();
}

bool FViewport::Initialize(float InStartX, float InStartY, float InSizeX, float InSizeY, ID3D11Device* Device)
{
	if (!Device)
		return false;

	D3DDevice = Device;
	D3DDevice->GetImmediateContext(&D3DDeviceContext);
	StartX = static_cast<uint32>(InStartX);
	StartY = static_cast<uint32>(InStartY);
	SizeX = static_cast<uint32>(InSizeX);
	SizeY = static_cast<uint32>(InSizeY);

	return true;
}

void FViewport::Cleanup()
{
	if (D3DDeviceContext)
	{
		D3DDeviceContext->Release();
		D3DDeviceContext = nullptr;
	}

	D3DDevice = nullptr;
}

void FViewport::BeginRenderFrame()
{
	// 뷰포트 설정
 //   D3D11_VIEWPORT viewport = {};
 //   viewport.Width = static_cast<float>(SizeX);
 //   viewport.Height = static_cast<float>(SizeY);
 //   viewport.MinDepth = 0.0f;
 //   viewport.MaxDepth = 1.0f;
 //   viewport.TopLeftX = static_cast<float>(StartX);
 //   viewport.TopLeftY = static_cast<float>(StartY);
 //   D3DDeviceContext->RSSetViewports(1, &viewport);

	//// ✅ 디버그: 각 뷰포트가 설정한 viewport 출력
	//static int callCount = 0;
	//if (callCount++ % 60 == 0) // 60프레임마다 출력
	//{
	//	UE_LOG("[FViewport::BeginRenderFrame] Viewport: TopLeft(%.1f, %.1f), Size(%.1f x %.1f)", 
	//		viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height);
	//}
}

void FViewport::Render()
{
	if (!ViewportClient) return;
	BeginRenderFrame();

	ViewportClient->Draw(this);

	EndRenderFrame();
}


void FViewport::EndRenderFrame()
{
	// 렌더링 완료 후 처리할 작업이 있다면 여기에 추가
}

void FViewport::Resize(uint32 NewStartX, uint32 NewStartY, uint32 NewSizeX, uint32 NewSizeY)
{
	if (SizeX == NewSizeX && SizeY == NewSizeY)
		return;
	StartX = NewStartX;
	StartY = NewStartY;
	SizeX = NewSizeX;
	SizeY = NewSizeY;
}

void FViewport::ProcessMouseMove(int32 X, int32 Y)
{
	if (ViewportClient)
	{
		ViewportMousePosition.X = static_cast<float>(X);
		ViewportMousePosition.Y = static_cast<float>(Y);
		ViewportClient->MouseMove(this, X, Y);
	}
}

void FViewport::ProcessMouseButtonDown(int32 X, int32 Y, int32 Button)
{
	if (ViewportClient)
	{
		ViewportClient->MouseButtonDown(this, X, Y, Button);
	}
}

void FViewport::ProcessMouseButtonUp(int32 X, int32 Y, int32 Button)
{
	if (ViewportClient)
	{
		ViewportClient->MouseButtonUp(this, X, Y, Button);
	}
}

void FViewport::ProcessKeyDown(int32 KeyCode)
{
	if (ViewportClient)
	{
		ViewportClient->KeyDown(this, KeyCode);
	}
}

void FViewport::ProcessKeyUp(int32 KeyCode)
{
	if (ViewportClient)
	{
		ViewportClient->KeyUp(this, KeyCode);
	}
}