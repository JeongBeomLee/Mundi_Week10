#pragma once
#include "FViewport.h"

/**
 * @brief 오프스크린 렌더링 전용 Viewport 클래스
 *
 * FViewport를 상속하여 오프스크린 렌더링 기능을 제공합니다.
 * ImGui 창 내부에 3D 뷰포트를 임베딩할 때 사용됩니다.
 *
 * 특징:
 * - RenderTargetTexture에 렌더링
 * - ShaderResourceView를 ImGui::Image()로 전달
 * - Depth/Stencil 버퍼 자동 관리
 * - 크기 조정 시 텍스처 자동 재생성
 */
class FOffscreenViewport : public FViewport
{
public:
    FOffscreenViewport();
    virtual ~FOffscreenViewport();

    // FViewport 오버라이드
    virtual bool Initialize(float StartX, float StartY, float InSizeX, float InSizeY, ID3D11Device* Device) override;
    virtual void Cleanup() override;
    virtual void Resize(uint32 NewStartX, uint32 NewStartY, uint32 NewSizeX, uint32 NewSizeY) override;
    virtual void Render() override;

    // 오프스크린 전용 접근자
    ID3D11ShaderResourceView* GetShaderResourceView() const { return OffscreenSRV; }

    // FViewport 오버라이드 - 오프스크린 타겟 반환
    virtual ID3D11RenderTargetView* GetRenderTargetView() const override { return OffscreenRTV; }
    virtual ID3D11DepthStencilView* GetDepthStencilView() const override { return OffscreenDSV; }

private:
    // 렌더 타겟 텍스처 생성/재생성
    bool CreateRenderTargets();
    void ReleaseRenderTargets();

    // 오프스크린 렌더링 리소스
    ID3D11Texture2D* OffscreenRenderTarget = nullptr;
    ID3D11RenderTargetView* OffscreenRTV = nullptr;
    ID3D11ShaderResourceView* OffscreenSRV = nullptr;
    ID3D11Texture2D* OffscreenDepthStencil = nullptr;
    ID3D11DepthStencilView* OffscreenDSV = nullptr;
};
