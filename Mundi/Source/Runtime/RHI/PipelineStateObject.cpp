#include "pch.h"
#include "PipelineStateObject.h"

PipeLineStateObject::PipeLineStateObject(ID3D11Buffer* InVertexBuffer, ID3D11Buffer* InIndexBuffer, ID3D11ShaderResourceView* InSRV)
	: Stride(sizeof(FVertexDynamic))
	, Offset(0)
	, VertexBuffer(InVertexBuffer)
	, IndexBuffer(InIndexBuffer)
	, SRV(InSRV)
{
	//RHIDevice = UWorld::GetInstance().GetRenderer()->GetRHIDevice();
}

void PipeLineStateObject::SetRenderState()
{
    //assert(RHIDevice && "PSO dont have RHIDevice");

    //RHIDevice->GetDeviceContext()->IASetVertexBuffers(
    //    0, 1, &VertexBuffer, &Stride, &Offset
    //);

    //RHIDevice->GetDeviceContext()->IASetIndexBuffer(
    //    IndexBuffer, DXGI_FORMAT_R32_UINT, 0
    //);
}
