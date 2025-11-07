#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include "Vector.h"

// NOTE: 개발 편의성을 위해서 DX11를 종속적으로 사용한다 (URHIDevice를 사용하지 않음)

//class URHIDevice
//{
//public:
//    URHIDevice() {};
//    virtual ~URHIDevice() {};
//
//private:
//    // 복사생성자, 연산자 금지
//    URHIDevice(const URHIDevice& RHIDevice) = delete;
//    URHIDevice& operator=(const URHIDevice& RHIDevice) = delete;
//
//public:
//    virtual void Initialize(HWND hWindow) = 0;
//    virtual void Release() = 0;
//public:
//
//    //getter
//    virtual ID3D11Device* GetDevice() = 0;
//    virtual ID3D11DeviceContext* GetDeviceContext() = 0;
//
//    // create
//    virtual void CreateDeviceAndSwapChain(HWND hWindow) = 0;;
//    virtual void CreateFrameBuffer() = 0;
//    virtual void CreateRasterizerState() = 0;
//    virtual void CreateConstantBuffer() = 0;
//    virtual void CreateBlendState() = 0;
//	virtual void CreateDepthStencilState() = 0;
//    virtual void CreateShader(ID3D11InputLayout** OutSimpleInputLayout, ID3D11VertexShader** OutSimpleVertexShader, ID3D11PixelShader** OutSimplePixelShader) = 0;
//
//    // update
//    virtual void UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix) = 0;
//    virtual void UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp)=0;
//    virtual void UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture) = 0;
//    virtual void UpdateHighLightConstantBuffers(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo) = 0;
//    virtual void UpdateColorConstantBuffers(const FVector4& InColor) = 0;
//    virtual void UpdateUVScrollConstantBuffers(const FVector2D& Speed, float TimeSec) = 0;
//
//    // clear
//    virtual void ClearBackBuffer() = 0;
//    virtual void ClearDepthBuffer(float Depth, UINT Stenci) = 0;
//
//    virtual void IASetPrimitiveTopology() = 0;
//    virtual void RSSetViewport() = 0;
//    virtual void RSSetState(EViewModeIndex ViewModeIndex) = 0;
//    virtual void RSSetNoCullState() = 0;
//    virtual void OMSetRenderTargets() = 0;
//    virtual void OMSetBlendState(bool bIsBlendMode) = 0;
//    virtual void OMSetDepthStencilState(EComparisonFunc Func) = 0;
//    // Overlay precedence helpers (stencil-based)
//    virtual void OMSetDepthStencilState_OverlayWriteStencil() = 0;      // writes stencil=1 where overlay draws
//    virtual void OMSetDepthStencilState_StencilRejectOverlay() = 0;     // allows drawing only where stencil==0
//    virtual void Present() = 0;
//    virtual void PSSetDefaultSampler(UINT StartSlot) = 0;
//};
//
//
