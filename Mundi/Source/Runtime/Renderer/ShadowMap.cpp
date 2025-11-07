#include "pch.h"
#include "ShadowMap.h"

FShadowMap::FShadowMap()
	: Width(0)
	, Height(0)
	, ArraySize(0)
	, bIsCubeMap(false)
	, FilterType(EShadowFilterType::NONE)
	, ShadowMapTexture(nullptr)
	, ShadowMapSRV(nullptr)
	, RemappedTexture(nullptr)
	, RemappedRTV(nullptr)
	, RemappedSRV(nullptr)
	, DepthRemapVS(nullptr)
	, DepthRemapPS(nullptr)
	, DepthRemapConstantBuffer(nullptr)
	, FullscreenQuadVB_Rotated(nullptr)
	, FullscreenQuadVB_NoRotation(nullptr)
	, FullscreenQuadVB_NoRotation_FlipU(nullptr)
	, DepthRemapInputLayout(nullptr)
	, PointSamplerState(nullptr)
{
	ZeroMemory(&ShadowViewport, sizeof(D3D11_VIEWPORT));
}

FShadowMap::~FShadowMap()
{
	Release();
}

void FShadowMap::Initialize(D3D11RHI* RHI, UINT InWidth, UINT InHeight, UINT InArraySize, bool bInIsCubeMap, EShadowFilterType InFilterType)
{
	Width = InWidth;
	Height = InHeight;
	ArraySize = bInIsCubeMap ? InArraySize * 6 : InArraySize;
	bIsCubeMap = bInIsCubeMap;
	FilterType = InFilterType;

	// 필터 타입에 따라 텍스처 포맷 및 바인드 플래그 결정
	DXGI_FORMAT textureFormat;
	UINT bindFlags;

	if (FilterType == EShadowFilterType::NONE || FilterType == EShadowFilterType::PCF)
	{
		// Depth-only 방식 (기존 방식)
		textureFormat = DXGI_FORMAT_R32_TYPELESS;
		bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	}
	else if (FilterType == EShadowFilterType::VSM || FilterType == EShadowFilterType::EVSM)
	{
		// VSM/EVSM: depth + depth^2 저장 (R32G32_FLOAT)
		textureFormat = DXGI_FORMAT_R32G32_FLOAT;
		bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	}
	else // ESM
	{
		// ESM: exponential depth 저장 (R32_FLOAT)
		textureFormat = DXGI_FORMAT_R32_FLOAT;
		bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	}

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = Width;
	texDesc.Height = Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = ArraySize;
	texDesc.Format = textureFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = bindFlags;
	texDesc.CPUAccessFlags = 0;

	// CubeMap인 경우 TEXTURECUBE 플래그 추가
	if (bIsCubeMap)
	{
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	}
	else
	{
		texDesc.MiscFlags = 0;
	}

	HRESULT hr = RHI->GetDevice()->CreateTexture2D(&texDesc, nullptr, &ShadowMapTexture);
	assert(SUCCEEDED(hr), "Failed to create shadow map texture array");

	// 각 배열 슬라이스에 대한 뷰 생성 (DSV 또는 RTV)
	ShadowMapDSVs.clear();
	ShadowMapRTVs.clear();

	if (FilterType == EShadowFilterType::NONE || FilterType == EShadowFilterType::PCF)
	{
		// Depth-only: DSV 생성
		UINT numDSVs = ArraySize;
		for (UINT i = 0; i < numDSVs; i++)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32-bit depth
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = i;
			dsvDesc.Texture2DArray.ArraySize = 1;
			dsvDesc.Flags = 0;

			ID3D11DepthStencilView* DSV = nullptr;
			hr = RHI->GetDevice()->CreateDepthStencilView(ShadowMapTexture, &dsvDesc, &DSV);
			assert(SUCCEEDED(hr), "Failed to create shadow map DSV");

			ShadowMapDSVs.Add(DSV);
		}
	}
	else
	{
		// VSM/ESM/EVSM: RTV 생성
		UINT numRTVs = ArraySize;
		for (UINT i = 0; i < numRTVs; i++)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = textureFormat; // R32G32_FLOAT 또는 R32_FLOAT
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			rtvDesc.Texture2DArray.ArraySize = 1;

			ID3D11RenderTargetView* RTV = nullptr;
			hr = RHI->GetDevice()->CreateRenderTargetView(ShadowMapTexture, &rtvDesc, &RTV);
			assert(SUCCEEDED(hr), "Failed to create shadow map RTV");

			ShadowMapRTVs.Add(RTV);
		}
	}

	// 전체 배열에 대한 셰이더 리소스 뷰 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	// 필터 타입에 따라 SRV 포맷 결정
	if (FilterType == EShadowFilterType::NONE || FilterType == EShadowFilterType::PCF)
	{
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // Depth를 float로 읽기
	}
	else if (FilterType == EShadowFilterType::VSM || FilterType == EShadowFilterType::EVSM)
	{
		srvDesc.Format = DXGI_FORMAT_R32G32_FLOAT; // depth + depth^2
	}
	else // ESM
	{
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // exponential depth
	}

	if (bIsCubeMap)
	{
		// TextureCubeArray로 바인딩
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		srvDesc.TextureCubeArray.MostDetailedMip = 0;
		srvDesc.TextureCubeArray.MipLevels = 1;
		srvDesc.TextureCubeArray.First2DArrayFace = 0;
		srvDesc.TextureCubeArray.NumCubes = ArraySize / 6; // 큐브 개수
	}
	else
	{
		// Texture2DArray로 바인딩
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = ArraySize;
	}

	hr = RHI->GetDevice()->CreateShaderResourceView(ShadowMapTexture, &srvDesc, &ShadowMapSRV);
	assert(SUCCEEDED(hr), "Failed to create shadow map SRV");

	// 개별 슬라이스 SRV 생성 (ImGui 표시용 - TEXTURE2DARRAY, ArraySize=1)
	ShadowMapSliceSRVs.clear();
	for (UINT i = 0; i < ArraySize; i++)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sliceSrvDesc = {};
		sliceSrvDesc.Format = srvDesc.Format; // 동일한 포맷 사용
		sliceSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		sliceSrvDesc.Texture2DArray.MostDetailedMip = 0;
		sliceSrvDesc.Texture2DArray.MipLevels = 1;
		sliceSrvDesc.Texture2DArray.FirstArraySlice = i;
		sliceSrvDesc.Texture2DArray.ArraySize = 1; // 단일 슬라이스만

		ID3D11ShaderResourceView* SliceSRV = nullptr;
		hr = RHI->GetDevice()->CreateShaderResourceView(ShadowMapTexture, &sliceSrvDesc, &SliceSRV);
		assert(SUCCEEDED(hr), "Failed to create shadow map slice SRV");

		ShadowMapSliceSRVs.Add(SliceSRV);
	}

	// 뷰포트 설정
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
	ShadowViewport.Width = static_cast<float>(Width);
	ShadowViewport.Height = static_cast<float>(Height);
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;
}

void FShadowMap::Release()
{
	if (ShadowMapSRV)
	{
		ShadowMapSRV->Release();
		ShadowMapSRV = nullptr;
	}

	for (ID3D11ShaderResourceView* SliceSRV : ShadowMapSliceSRVs)
	{
		if (SliceSRV)
		{
			SliceSRV->Release();
			SliceSRV = nullptr;
		}
	}
	ShadowMapSliceSRVs.clear();

	for (ID3D11DepthStencilView* DSV : ShadowMapDSVs)
	{
		if (DSV)
		{
			DSV->Release();
			DSV = nullptr;
		}
	}
	ShadowMapDSVs.clear();

	for (ID3D11RenderTargetView* RTV : ShadowMapRTVs)
	{
		if (RTV)
		{
			RTV->Release();
			RTV = nullptr;
		}
	}
	ShadowMapRTVs.clear();

	if (ShadowMapTexture)
	{
		ShadowMapTexture->Release();
		ShadowMapTexture = nullptr;
	}

	// 캐시된 RasterizerState 해제
	for (auto& Pair : CachedRasterizerStates)
	{
		if (Pair.second)
		{
			Pair.second->Release();
		}
	}
	CachedRasterizerStates.clear();

	// Release depth remap visualization resources
	ReleaseDepthRemapResources();
}

void FShadowMap::BeginRender(D3D11RHI* RHI, UINT ArrayIndex, float DepthBias, float SlopeScaledDepthBias)
{
	assert(RHI != nullptr, "RHI is null");
	assert(ArrayIndex < ArraySize, "Array index out of bounds");

	ID3D11DeviceContext* pContext = RHI->GetDeviceContext();

	// 섀도우 맵 SRV 언바인딩 (리소스 hazard 방지)
	// NONE/PCF: t5, t6, t7 / VSM/ESM/EVSM: t8, t9, t10
	// 이전 프레임의 바인딩을 모두 정리하기 위해 양쪽 범위 모두 언바인딩
	ID3D11ShaderResourceView* pNullSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	pContext->PSSetShaderResources(5, 6, pNullSRVs);


	// RasterizerState 캐싱 (DepthBias 조합별로 재사용)
	FRasterizerStateKey Key = { DepthBias, SlopeScaledDepthBias };
	ID3D11RasterizerState* RasterizerState = nullptr;

	// 캐시에서 찾기
	auto it = CachedRasterizerStates.find(Key);
	if (it != CachedRasterizerStates.end())
	{
		// 캐시에 있으면 재사용
		RasterizerState = it->second;
	}
	else
	{
		// 캐시에 없으면 새로 생성하고 캐시에 저장
		D3D11_RASTERIZER_DESC ShadowRasterizerDesc = {};
		ShadowRasterizerDesc.FillMode = D3D11_FILL_SOLID;
		ShadowRasterizerDesc.CullMode = D3D11_CULL_BACK;
		ShadowRasterizerDesc.DepthClipEnable = TRUE;
		ShadowRasterizerDesc.DepthBias = static_cast<INT>(DepthBias * 100000.0f);
		ShadowRasterizerDesc.SlopeScaledDepthBias = SlopeScaledDepthBias;
		ShadowRasterizerDesc.DepthBiasClamp = 0.0f;

		RHI->GetDevice()->CreateRasterizerState(&ShadowRasterizerDesc, &RasterizerState);
		CachedRasterizerStates[Key] = RasterizerState;
	}

	pContext->RSSetState(RasterizerState);

	// 필터 타입에 따라 다른 렌더링 경로 사용
	if (FilterType == EShadowFilterType::NONE || FilterType == EShadowFilterType::PCF)
	{
		// Depth-only 렌더링 (기존 방식)
		ID3D11DepthStencilView* DSV = ShadowMapDSVs[ArrayIndex];
		pContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

		// Pixel shader unbind (depth-only)
		pContext->PSSetShader(nullptr, nullptr, 0);

		// Depth-stencil state 설정
		RHI->OMSetDepthStencilState(EComparisonFunc::LessEqual);

		// OM: 컬러 렌더링 없이 깊이만 쓰기
		ID3D11RenderTargetView* nullRTV = nullptr;
		pContext->OMSetRenderTargets(1, &nullRTV, DSV);
	}
	else
	{
		// VSM/ESM/EVSM: RTV에 depth 정보 렌더링 (픽셀 쉐이더 필요)
		ID3D11RenderTargetView* RTV = ShadowMapRTVs[ArrayIndex];

		// RTV 유효성 검사
		if (!RTV)
		{
			UE_LOG("BeginRender: RTV is null for ArrayIndex %d (FilterType=%d)", ArrayIndex, (int)FilterType);
			return;
		}

		// RTV 클리어 (white = 무한대 depth를 의미)
		float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		pContext->ClearRenderTargetView(RTV, clearColor);

		// VSM/ESM/EVSM용 픽셀 쉐이더는 SceneRenderer::OverrideShadowShader()에서 설정됨

		// Depth-stencil state 비활성화 (depth test/write 불필요)
		RHI->OMSetDepthStencilState(EComparisonFunc::Always);

		// OM: RTV만 바인딩 (DSV 없음)
		pContext->OMSetRenderTargets(1, &RTV, nullptr);
	}

	// 뷰포트 설정
	D3D11_VIEWPORT ScaledViewport = ShadowViewport;
	ScaledViewport.Width = ShadowViewport.Width;
	ScaledViewport.Height = ShadowViewport.Height;
	pContext->RSSetViewports(1, &ScaledViewport);
}

void FShadowMap::EndRender(D3D11RHI* RHI)
{
	assert(RHI != nullptr, "RHI is null");

	ID3D11DeviceContext* pContext = RHI->GetDeviceContext();

	// Unbind render targets
	ID3D11RenderTargetView* nullRTV = nullptr;
	ID3D11DepthStencilView* nullDSV = nullptr;
	pContext->OMSetRenderTargets(1, &nullRTV, nullDSV);

	// 섀도우 맵 SRV 언바인딩 (리소스 hazard 방지)
	// 모든 가능한 슬롯을 언바인딩 (t5~t10)
	ID3D11ShaderResourceView* pNullSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	pContext->PSSetShaderResources(5, 6, pNullSRVs);
}

uint64_t FShadowMap::GetAllocatedMemoryBytes() const
{
	// 텍스처가 초기화되지 않았으면 0 반환
	if (!ShadowMapTexture)
	{
		return 0;
	}

	// 텍스처 Description을 쿼리하여 실제 포맷 확인
	D3D11_TEXTURE2D_DESC desc;
	ShadowMapTexture->GetDesc(&desc);

	// 포맷별 바이트 크기 계산
	uint64_t BytesPerPixel = GetDepthFormatSize(desc.Format);

	// 전체 할당된 메모리 = (Width * Height) * BytesPerPixel * ArraySize
	uint64_t PixelsPerMap = static_cast<uint64_t>(Width) * static_cast<uint64_t>(Height);
	uint64_t TotalMemory = PixelsPerMap * BytesPerPixel * ArraySize;

	return TotalMemory;
}

uint64_t FShadowMap::GetUsedMemoryBytes(uint32 UsedSlotCount) const
{
	// 텍스처가 초기화되지 않았으면 0 반환
	if (!ShadowMapTexture)
	{
		return 0;
	}

	// 텍스처 Description을 쿼리하여 실제 포맷 확인
	D3D11_TEXTURE2D_DESC desc;
	ShadowMapTexture->GetDesc(&desc);

	// 포맷별 바이트 크기 계산
	uint64_t BytesPerPixel = GetDepthFormatSize(desc.Format);

	// 사용 중인 메모리 = (Width * Height) * BytesPerPixel * UsedSlotCount
	uint64_t PixelsPerMap = static_cast<uint64_t>(Width) * static_cast<uint64_t>(Height);
	uint64_t UsedMemory = PixelsPerMap * BytesPerPixel * UsedSlotCount;

	return UsedMemory;
}

uint64_t FShadowMap::GetDepthFormatSize(DXGI_FORMAT format)
{
	switch (format)
	{
		// 64-bit formats (8 bytes per pixel) - VSM/EVSM
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
			return 8;

		// 32-bit formats (4 bytes per pixel)
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return 4;

		// 16-bit formats (2 bytes per pixel)
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
			return 2;

		// 기본값: 32-bit (안전한 상한선)
		default:
			return 4;
	}
}

void FShadowMap::ReleaseDepthRemapResources()
{
	if (PointSamplerState)
	{
		PointSamplerState->Release();
		PointSamplerState = nullptr;
	}

	if (DepthRemapInputLayout)
	{
		DepthRemapInputLayout->Release();
		DepthRemapInputLayout = nullptr;
	}

	if (FullscreenQuadVB_Rotated)
	{
		FullscreenQuadVB_Rotated->Release();
		FullscreenQuadVB_Rotated = nullptr;
	}

	if (FullscreenQuadVB_NoRotation)
	{
		FullscreenQuadVB_NoRotation->Release();
		FullscreenQuadVB_NoRotation = nullptr;
	}

	if (FullscreenQuadVB_NoRotation_FlipU)
	{
		FullscreenQuadVB_NoRotation_FlipU->Release();
		FullscreenQuadVB_NoRotation_FlipU = nullptr;
	}

	if (DepthRemapConstantBuffer)
	{
		DepthRemapConstantBuffer->Release();
		DepthRemapConstantBuffer = nullptr;
	}

	if (DepthRemapPS)
	{
		DepthRemapPS->Release();
		DepthRemapPS = nullptr;
	}

	if (DepthRemapVS)
	{
		DepthRemapVS->Release();
		DepthRemapVS = nullptr;
	}

	if (RemappedSRV)
	{
		RemappedSRV->Release();
		RemappedSRV = nullptr;
	}

	if (RemappedRTV)
	{
		RemappedRTV->Release();
		RemappedRTV = nullptr;
	}

	if (RemappedTexture)
	{
		RemappedTexture->Release();
		RemappedTexture = nullptr;
	}

	// RemappedSliceSRV 캐시 해제
	for (auto& Pair : RemappedSliceSRVCache)
	{
		if (Pair.second.SRV) Pair.second.SRV->Release();
		if (Pair.second.RTV) Pair.second.RTV->Release();
		if (Pair.second.Texture) Pair.second.Texture->Release();
	}
	RemappedSliceSRVCache.clear();
}

void FShadowMap::InitializeDepthRemapResources(D3D11RHI* RHI)
{
	// 이미 초기화되었으면 스킵
	if (DepthRemapVS != nullptr)
	{
		return;
	}

	ID3D11Device* device = RHI->GetDevice();
	HRESULT hr;

	// 1. Compile shaders from DepthRemap.hlsl
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// Vertex Shader
	hr = D3DCompileFromFile(
		L"Shaders/Utility/DepthRemap.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VS",
		"vs_5_0",
		compileFlags,
		0,
		&vsBlob,
		&errorBlob);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		assert(false, "Failed to compile DepthRemap vertex shader");
		return;
	}

	hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &DepthRemapVS);
	assert(SUCCEEDED(hr), "Failed to create DepthRemap vertex shader");

	// Pixel Shader
	hr = D3DCompileFromFile(
		L"Shaders/Utility/DepthRemap.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PS",
		"ps_5_0",
		compileFlags,
		0,
		&psBlob,
		&errorBlob);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		assert(false, "Failed to compile DepthRemap pixel shader");
		vsBlob->Release();
		return;
	}

	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &DepthRemapPS);
	assert(SUCCEEDED(hr), "Failed to create DepthRemap pixel shader");

	// 2. Create Input Layout
	D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	hr = device->CreateInputLayout(inputDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &DepthRemapInputLayout);
	assert(SUCCEEDED(hr), "Failed to create DepthRemap input layout");

	vsBlob->Release();
	psBlob->Release();

	// 3. Create Fullscreen Quad Vertex Buffers
	struct Vertex
	{
		float Position[3];
		float TexCoord[2];
	};

	// 3-1. 시계방향 90도 회전된 텍스처 좌표 (기본)
	Vertex quadVertices_Rotated[] = {
		{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f } }, // Top-left
		{ {  1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } }, // Top-right
		{ { -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } }, // Bottom-left
		{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } }  // Bottom-right
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(quadVertices_Rotated);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = quadVertices_Rotated;

	hr = device->CreateBuffer(&vbDesc, &vbData, &FullscreenQuadVB_Rotated);
	assert(SUCCEEDED(hr), "Failed to create fullscreen quad vertex buffer (rotated)");

	// 3-2. 회전 없는 텍스처 좌표 (TSM용)
	Vertex quadVertices_NoRotation[] = {
		{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } }, // Top-left
		{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } }, // Top-right
		{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, // Bottom-left
		{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } }  // Bottom-right
	};

	vbData.pSysMem = quadVertices_NoRotation;

	hr = device->CreateBuffer(&vbDesc, &vbData, &FullscreenQuadVB_NoRotation);
	assert(SUCCEEDED(hr), "Failed to create fullscreen quad vertex buffer (no rotation)");

	// 3-3. 회전 없음 + U 반전 텍스처 좌표 (OpenGL LiSPSM용)
	Vertex quadVertices_NoRotation_FlipU[] = {
		{ { -1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } }, // Top-left (U 반전)
		{ {  1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } }, // Top-right (U 반전)
		{ { -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } }, // Bottom-left (U 반전)
		{ {  1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }  // Bottom-right (U 반전)
	};

	vbData.pSysMem = quadVertices_NoRotation_FlipU;

	hr = device->CreateBuffer(&vbDesc, &vbData, &FullscreenQuadVB_NoRotation_FlipU);
	assert(SUCCEEDED(hr), "Failed to create fullscreen quad vertex buffer (no rotation + flip U)");

	// 4. Create Constant Buffer
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.ByteWidth = 16; // float MinDepth, float MaxDepth, float2 Padding
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = device->CreateBuffer(&cbDesc, nullptr, &DepthRemapConstantBuffer);
	assert(SUCCEEDED(hr), "Failed to create DepthRemap constant buffer");

	// 5. Create Point Sampler
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = device->CreateSamplerState(&samplerDesc, &PointSamplerState);
	assert(SUCCEEDED(hr), "Failed to create point sampler state");

	// 6. Create Remapped Render Target
	D3D11_TEXTURE2D_DESC rtDesc = {};
	rtDesc.Width = Width;
	rtDesc.Height = Height;
	rtDesc.MipLevels = 1;
	rtDesc.ArraySize = 1;
	rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDesc.SampleDesc.Count = 1;
	rtDesc.SampleDesc.Quality = 0;
	rtDesc.Usage = D3D11_USAGE_DEFAULT;
	rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	rtDesc.CPUAccessFlags = 0;
	rtDesc.MiscFlags = 0;

	hr = device->CreateTexture2D(&rtDesc, nullptr, &RemappedTexture);
	assert(SUCCEEDED(hr), "Failed to create remapped render target texture");

	hr = device->CreateRenderTargetView(RemappedTexture, nullptr, &RemappedRTV);
	assert(SUCCEEDED(hr), "Failed to create remapped render target view");

	hr = device->CreateShaderResourceView(RemappedTexture, nullptr, &RemappedSRV);
	assert(SUCCEEDED(hr), "Failed to create remapped shader resource view");
}

ID3D11ShaderResourceView* FShadowMap::GetRemappedSliceSRV(D3D11RHI* RHI, UINT ArrayIndex, float MinDepth, float MaxDepth, bool bRotate, bool bFlipU)
{
	// 초기화되지 않았거나 인덱스가 범위를 벗어나면 nullptr 반환
	if (ArrayIndex >= ArraySize || !ShadowMapTexture)
	{
		return nullptr;
	}

	// Depth remap 리소스 초기화 (최초 1회만)
	InitializeDepthRemapResources(RHI);

	// 캐시 키 생성
	FRemappedSliceKey cacheKey;
	cacheKey.ArrayIndex = ArrayIndex;
	cacheKey.MinDepth = MinDepth;
	cacheKey.MaxDepth = MaxDepth;
	cacheKey.bRotate = bRotate;
	cacheKey.bFlipU = bFlipU;

	// 캐시에 있는지 확인
	auto it = RemappedSliceSRVCache.find(cacheKey);
	if (it != RemappedSliceSRVCache.end())
	{
		// 캐시된 SRV 반환
		return it->second.SRV;
	}

	// 캐시에 없으면 새로 생성
	ID3D11Device* device = RHI->GetDevice();
	ID3D11DeviceContext* context = RHI->GetDeviceContext();

	// 새 RemappedTexture 생성
	FRemappedSliceResource newResource = {};

	D3D11_TEXTURE2D_DESC rtDesc = {};
	rtDesc.Width = Width;
	rtDesc.Height = Height;
	rtDesc.MipLevels = 1;
	rtDesc.ArraySize = 1;
	rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDesc.SampleDesc.Count = 1;
	rtDesc.SampleDesc.Quality = 0;
	rtDesc.Usage = D3D11_USAGE_DEFAULT;
	rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	rtDesc.CPUAccessFlags = 0;
	rtDesc.MiscFlags = 0;

	HRESULT hr = device->CreateTexture2D(&rtDesc, nullptr, &newResource.Texture);
	if (FAILED(hr)) return nullptr;

	hr = device->CreateRenderTargetView(newResource.Texture, nullptr, &newResource.RTV);
	if (FAILED(hr))
	{
		newResource.Texture->Release();
		return nullptr;
	}

	hr = device->CreateShaderResourceView(newResource.Texture, nullptr, &newResource.SRV);
	if (FAILED(hr))
	{
		newResource.RTV->Release();
		newResource.Texture->Release();
		return nullptr;
	}

	// 0. Save current render state
	ID3D11RenderTargetView* oldRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
	ID3D11DepthStencilView* oldDSV = nullptr;
	context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs, &oldDSV);

	UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	context->RSGetViewports(&numViewports, oldViewports);

	// 1. Update Constant Buffer
	struct DepthRemapParams
	{
		float MinDepth;
		float MaxDepth;
		float Padding[2];
	};

	DepthRemapParams params = { MinDepth, MaxDepth, { 0.0f, 0.0f } };

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = context->Map(DepthRemapConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		memcpy(mappedResource.pData, &params, sizeof(DepthRemapParams));
		context->Unmap(DepthRemapConstantBuffer, 0);
	}

	// 2. Set Render Target (use new resource for caching)
	context->OMSetRenderTargets(1, &newResource.RTV, nullptr);

	// 3. Set Viewport
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(Width);
	viewport.Height = static_cast<float>(Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// 4. Set Shaders and Resources
	context->VSSetShader(DepthRemapVS, nullptr, 0);
	context->PSSetShader(DepthRemapPS, nullptr, 0);
	context->IASetInputLayout(DepthRemapInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 5. Bind Source Depth Texture (specific slice)
	ID3D11ShaderResourceView* sourceSRV = GetSliceSRV(ArrayIndex);
	context->PSSetShaderResources(0, 1, &sourceSRV);
	context->PSSetSamplers(0, 1, &PointSamplerState);
	context->VSSetConstantBuffers(0, 1, &DepthRemapConstantBuffer);
	context->PSSetConstantBuffers(0, 1, &DepthRemapConstantBuffer);

	// 6. Draw Fullscreen Quad
	UINT stride = sizeof(float) * 5; // 3 pos + 2 uv
	UINT offset = 0;

	// Vertex buffer 선택:
	// - LVP: 회전 (bRotate = true)
	// - TSM: 회전 없음 (bRotate = false, bFlipU = false)
	// - OpenGL LiSPSM: 회전 없음 + U 반전 (bRotate = false, bFlipU = true)
	ID3D11Buffer* SelectedVB;
	if (bRotate)
	{
		SelectedVB = FullscreenQuadVB_Rotated;  // LVP
	}
	else if (bFlipU)
	{
		SelectedVB = FullscreenQuadVB_NoRotation_FlipU;  // OpenGL LiSPSM
	}
	else
	{
		SelectedVB = FullscreenQuadVB_NoRotation;  // TSM
	}

	context->IASetVertexBuffers(0, 1, &SelectedVB, &stride, &offset);
	context->Draw(4, 0);

	// 7. Unbind Resources
	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->PSSetShaderResources(0, 1, &nullSRV);

	// 8. Restore previous render state
	context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs, oldDSV);
	context->RSSetViewports(numViewports, oldViewports);

	// Release references
	for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		if (oldRTVs[i]) oldRTVs[i]->Release();
	}
	if (oldDSV) oldDSV->Release();

	// 9. 캐시에 저장
	RemappedSliceSRVCache[cacheKey] = newResource;

	// 10. Return the remapped SRV
	return newResource.SRV;
}

void FShadowMap::ClearRemappedSliceCache()
{
	// 캐시된 모든 리소스 해제
	for (auto& Pair : RemappedSliceSRVCache)
	{
		if (Pair.second.SRV) Pair.second.SRV->Release();
		if (Pair.second.RTV) Pair.second.RTV->Release();
		if (Pair.second.Texture) Pair.second.Texture->Release();
	}
	RemappedSliceSRVCache.clear();
}
