#pragma once
//#include <d3d11.h>
#include "RHIDevice.h"

// StaticMeshComponent만 사용한다고 가정
class PipeLineStateObject
{
public:
	PipeLineStateObject(ID3D11Buffer* InVertexBuffer, ID3D11Buffer* InIndexBuffer, ID3D11ShaderResourceView* InSRV);
	void SetRenderState();
private:
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11ShaderResourceView* SRV = nullptr;

	URHIDevice* RHIDevice = nullptr;
	UINT Stride;
	UINT Offset;
};