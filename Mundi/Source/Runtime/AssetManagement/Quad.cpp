#pragma once

#include "pch.h"
#include "Quad.h"
#include "MeshLoader.h"
#include "ResourceManager.h"
#include "ObjManager.h"

IMPLEMENT_CLASS(UQuad)

UQuad::~UQuad()
{
	ReleaseResources();
}

void UQuad::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	assert(InDevice);

	if (VertexBuffer)
	{
		VertexBuffer->Release();
	}
	if (IndexBuffer)
	{
		IndexBuffer->Release();
	}

	CreateVertexBuffer(StaticMeshAsset, InDevice);
	CreateIndexBuffer(StaticMeshAsset, InDevice);
	VertexCount = static_cast<uint32>(StaticMeshAsset->Vertices.size());
	IndexCount = static_cast<uint32>(StaticMeshAsset->Indices.size());
	VertexStride = sizeof(FBillboardVertex);

	/*MeshDataCPU = UMeshLoader::GetInstance().LoadMesh(InFilePath.c_str());
	CreateVertexBuffer(MeshDataCPU, InDevice, InVertexType);
	CreateIndexBuffer(MeshDataCPU, InDevice);
	VertexCount = MeshDataCPU->Vertices.size();
	IndexCount = MeshDataCPU->Indices.size();*/
}

//
void UQuad::Load(FMeshData* InData, ID3D11Device* InDevice, bool IsTextQuad)
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
	}
	if (IndexBuffer)
	{
		IndexBuffer->Release();
	}

	CreateVertexBuffer(InData, InDevice, IsTextQuad);
	CreateIndexBuffer(InData, InDevice);

	VertexCount = static_cast<uint32>(InData->Vertices.size());
	IndexCount = static_cast<uint32>(InData->Indices.size());
	VertexStride = sizeof(FBillboardVertex);
}

void UQuad::CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, bool IsTextQuad)
{
	if (IsTextQuad)
	{
		HRESULT hr = D3D11RHI::CreateVertexBuffer<FBillboardVertexInfo_GPU>(InDevice, *InMeshData, &VertexBuffer);
		assert(SUCCEEDED(hr));
	}
	else
	{
		HRESULT hr = D3D11RHI::CreateVertexBuffer<FBillboardVertex>(InDevice, *InMeshData, &VertexBuffer);
		assert(SUCCEEDED(hr));
	}
}


void UQuad::CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice)
{
	HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InMeshData, &IndexBuffer);

	assert(SUCCEEDED(hr));
}


void UQuad::ReleaseResources()
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
	}
	if (IndexBuffer)
	{
		IndexBuffer->Release();
	}
}
