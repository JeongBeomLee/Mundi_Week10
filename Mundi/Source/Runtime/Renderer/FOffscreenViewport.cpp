#include "pch.h"
#include "FOffscreenViewport.h"
#include "FViewportClient.h"

FOffscreenViewport::FOffscreenViewport()
{
}

FOffscreenViewport::~FOffscreenViewport()
{
    Cleanup();
}

bool FOffscreenViewport::Initialize(float InStartX, float InStartY, float InSizeX, float InSizeY, ID3D11Device* Device)
{
    // 기본 초기화 (부모 클래스)
    if (!FViewport::Initialize(InStartX, InStartY, InSizeX, InSizeY, Device))
        return false;

    // 오프스크린 렌더 타겟 생성
    if (!CreateRenderTargets())
    {
        UE_LOG("FOffscreenViewport::Initialize - Failed to create render targets");
        return false;
    }

    return true;
}

void FOffscreenViewport::Cleanup()
{
    ReleaseRenderTargets();
    FViewport::Cleanup();
}

void FOffscreenViewport::Resize(uint32 NewStartX, uint32 NewStartY, uint32 NewSizeX, uint32 NewSizeY)
{
    // 크기가 변경되지 않았으면 무시
    if (SizeX == NewSizeX && SizeY == NewSizeY && StartX == NewStartX && StartY == NewStartY)
        return;

    // 부모 클래스 Resize 호출
    FViewport::Resize(NewStartX, NewStartY, NewSizeX, NewSizeY);

    // 렌더 타겟 재생성
    ReleaseRenderTargets();

    if (SizeX > 0 && SizeY > 0)
    {
        if (!CreateRenderTargets())
        {
            UE_LOG("FOffscreenViewport::Resize - Failed to recreate render targets");
        }
    }
}

bool FOffscreenViewport::CreateRenderTargets()
{
    if (!D3DDevice || SizeX == 0 || SizeY == 0)
        return false;

    HRESULT hr;

    // ========================================
    // 1. Render Target Texture 생성
    // ========================================
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = SizeX;
    texDesc.Height = SizeY;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    hr = D3DDevice->CreateTexture2D(&texDesc, nullptr, &OffscreenRenderTarget);
    if (FAILED(hr))
    {
        UE_LOG("FOffscreenViewport::CreateRenderTargets - Failed to create render target texture");
        return false;
    }

    // ========================================
    // 2. Render Target View 생성
    // ========================================
    hr = D3DDevice->CreateRenderTargetView(OffscreenRenderTarget, nullptr, &OffscreenRTV);
    if (FAILED(hr))
    {
        UE_LOG("FOffscreenViewport::CreateRenderTargets - Failed to create RTV");
        OffscreenRenderTarget->Release();
        OffscreenRenderTarget = nullptr;
        return false;
    }

    // ========================================
    // 3. Shader Resource View 생성 (ImGui 전달용)
    // ========================================
    hr = D3DDevice->CreateShaderResourceView(OffscreenRenderTarget, nullptr, &OffscreenSRV);
    if (FAILED(hr))
    {
        UE_LOG("FOffscreenViewport::CreateRenderTargets - Failed to create SRV");
        OffscreenRTV->Release();
        OffscreenRTV = nullptr;
        OffscreenRenderTarget->Release();
        OffscreenRenderTarget = nullptr;
        return false;
    }

    // ========================================
    // 4. Depth/Stencil Texture 생성
    // ========================================
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = SizeX;
    depthDesc.Height = SizeY;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = D3DDevice->CreateTexture2D(&depthDesc, nullptr, &OffscreenDepthStencil);
    if (FAILED(hr))
    {
        UE_LOG("FOffscreenViewport::CreateRenderTargets - Failed to create depth/stencil texture");
        return false;
    }

    // ========================================
    // 5. Depth Stencil View 생성
    // ========================================
    hr = D3DDevice->CreateDepthStencilView(OffscreenDepthStencil, nullptr, &OffscreenDSV);
    if (FAILED(hr))
    {
        UE_LOG("FOffscreenViewport::CreateRenderTargets - Failed to create DSV");
        OffscreenDepthStencil->Release();
        OffscreenDepthStencil = nullptr;
        return false;
    }

    return true;
}

void FOffscreenViewport::ReleaseRenderTargets()
{
    if (OffscreenDSV)
    {
        OffscreenDSV->Release();
        OffscreenDSV = nullptr;
    }
    if (OffscreenDepthStencil)
    {
        OffscreenDepthStencil->Release();
        OffscreenDepthStencil = nullptr;
    }
    if (OffscreenSRV)
    {
        OffscreenSRV->Release();
        OffscreenSRV = nullptr;
    }
    if (OffscreenRTV)
    {
        OffscreenRTV->Release();
        OffscreenRTV = nullptr;
    }
    if (OffscreenRenderTarget)
    {
        OffscreenRenderTarget->Release();
        OffscreenRenderTarget = nullptr;
    }
}

void FOffscreenViewport::Render()
{
    static int renderCount = 0;
    if (renderCount++ % 60 == 0)
    {
        UE_LOG("FOffscreenViewport::Render [BEFORE] - this=%p, ViewportClient=%p, OffscreenRTV=%p",
            this, ViewportClient, OffscreenRTV);
    }

    if (!ViewportClient || !OffscreenRTV || !OffscreenDSV)
    {
        UE_LOG("FOffscreenViewport::Render - Skipping render (missing resources)");
        return;
    }

    // ViewportClient 렌더링
    // FSceneRenderer가 GetRenderTargetView()를 통해 OffscreenRTV를 가져가서
    // 자동으로 오프스크린 텍스처에 렌더링함
    BeginRenderFrame();

    if (renderCount % 60 == 0)
    {
        UE_LOG("FOffscreenViewport::Render [CALLING Draw] - ViewportClient=%p", ViewportClient);
    }

    ViewportClient->Draw(this);

    if (renderCount % 60 == 0)
    {
        UE_LOG("FOffscreenViewport::Render [AFTER Draw] - ViewportClient=%p", ViewportClient);
    }

    EndRenderFrame();
}
