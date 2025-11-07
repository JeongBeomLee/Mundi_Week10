#include "pch.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include "StatsOverlayD2D.h"
#include "UIManager.h"
#include "MemoryManager.h"
#include "Picking.h"
#include "PlatformTime.h"
#include "DecalStatManager.h"
#include "TileCullingStats.h"
#include "ShadowStats.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

static inline void SafeRelease(IUnknown* p) { if (p) p->Release(); }

UStatsOverlayD2D& UStatsOverlayD2D::Get()
{
	static UStatsOverlayD2D Instance;
	return Instance;
}

void UStatsOverlayD2D::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain)
{
	D3DDevice = InDevice;
	D3DContext = InContext;
	SwapChain = InSwapChain;
	bInitialized = (D3DDevice && D3DContext && SwapChain);
}

void UStatsOverlayD2D::Shutdown()
{
	D3DDevice = nullptr;
	D3DContext = nullptr;
	SwapChain = nullptr;
	bInitialized = false;
}

void UStatsOverlayD2D::EnsureInitialized()
{
}

void UStatsOverlayD2D::ReleaseD2DTarget()
{
}

static void DrawTextBlock(
	ID2D1DeviceContext* InD2dCtx,
	IDWriteFactory* InDwrite,
	const wchar_t* InText,
	const D2D1_RECT_F& InRect,
	float InFontSize,
	D2D1::ColorF InBgColor,
	D2D1::ColorF InTextColor)
{
	if (!InD2dCtx || !InDwrite || !InText) return;

	ID2D1SolidColorBrush* BrushFill = nullptr;
	InD2dCtx->CreateSolidColorBrush(InBgColor, &BrushFill);

	ID2D1SolidColorBrush* BrushText = nullptr;
	InD2dCtx->CreateSolidColorBrush(InTextColor, &BrushText);

	InD2dCtx->FillRectangle(InRect, BrushFill);

	IDWriteTextFormat* Format = nullptr;
	InDwrite->CreateTextFormat(
		L"Segoe UI",
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		InFontSize,
		L"en-us",
		&Format);

	if (Format)
	{
		Format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		Format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		InD2dCtx->DrawTextW(
			InText,
			static_cast<UINT32>(wcslen(InText)),
			Format,
			InRect,
			BrushText);
		Format->Release();
	}

	SafeRelease(BrushText);
	SafeRelease(BrushFill);
}

void UStatsOverlayD2D::Draw()
{
	if (!bInitialized || (!bShowFPS && !bShowMemory && !bShowPicking && !bShowDecal && !bShowTileCulling && !bShowShadowMap) || !SwapChain)
		return;

	ID2D1Factory1* D2dFactory = nullptr;
	D2D1_FACTORY_OPTIONS Opts{};
#ifdef _DEBUG
	Opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &Opts, (void**)&D2dFactory)))
		return;

	IDXGISurface* Surface = nullptr;
	if (FAILED(SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&Surface)))
	{
		SafeRelease(D2dFactory);
		return;
	}

	IDXGIDevice* DxgiDevice = nullptr;
	if (FAILED(D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DxgiDevice)))
	{
		SafeRelease(Surface);
		SafeRelease(D2dFactory);
		return;
	}

	ID2D1Device* D2dDevice = nullptr;
	if (FAILED(D2dFactory->CreateDevice(DxgiDevice, &D2dDevice)))
	{
		SafeRelease(DxgiDevice);
		SafeRelease(Surface);
		SafeRelease(D2dFactory);
		return;
	}

	ID2D1DeviceContext* D2dCtx = nullptr;
	if (FAILED(D2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2dCtx)))
	{
		SafeRelease(D2dDevice);
		SafeRelease(DxgiDevice);
		SafeRelease(Surface);
		SafeRelease(D2dFactory);
		return;
	}

	IDWriteFactory* Dwrite = nullptr;
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&Dwrite)))
	{
		SafeRelease(D2dCtx);
		SafeRelease(D2dDevice);
		SafeRelease(DxgiDevice);
		SafeRelease(Surface);
		SafeRelease(D2dFactory);
		return;
	}

	D2D1_BITMAP_PROPERTIES1 BmpProps = {};
	BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	BmpProps.dpiX = 96.0f;
	BmpProps.dpiY = 96.0f;
	BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

	ID2D1Bitmap1* TargetBmp = nullptr;
	if (FAILED(D2dCtx->CreateBitmapFromDxgiSurface(Surface, &BmpProps, &TargetBmp)))
	{
		SafeRelease(Dwrite);
		SafeRelease(D2dCtx);
		SafeRelease(D2dDevice);
		SafeRelease(DxgiDevice);
		SafeRelease(Surface);
		SafeRelease(D2dFactory);
		return;
	}

	D2dCtx->SetTarget(TargetBmp);

	D2dCtx->BeginDraw();
	const float Margin = 12.0f;
	const float Space = 8.0f;   // 패널간의 간격
	const float PanelWidth = 200.0f;
	const float PanelHeight = 48.0f;
	float NextY = 90.0f; // 툴바와 겹치지 않도록 20픽셀 아래로

	if (bShowFPS)
	{
		float Dt = UUIManager::GetInstance().GetDeltaTime();
		float Fps = Dt > 0.0f ? (1.0f / Dt) : 0.0f;
		float Ms = Dt * 1000.0f;

		wchar_t Buf[128];
		swprintf_s(Buf, L"FPS: %.1f\nFrame time: %.2f ms", Fps, Ms);

		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(
			D2dCtx, Dwrite, Buf, rc, 16.0f,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Yellow));

		NextY += PanelHeight + Space;
	}

	if (bShowPicking)
	{
		// Build the entire block in one Buffer to avoid overwriting previous lines
		wchar_t Buf[256];
		double LastMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetLastPickTime());
		double TotalMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetTotalPickTime());
		uint32 Count = CPickingSystem::GetPickCount();
		double AvgMs = (Count > 0) ? (TotalMs / (double)Count) : 0.0;
		swprintf_s(Buf, L"Pick Count: %u\nLast: %.3f ms\nAvg: %.3f ms\nTotal: %.3f ms", Count, LastMs, AvgMs, TotalMs);

		// Increase panel height to fit multiple lines
		const float PickPanelHeight = 96.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PickPanelHeight);
		DrawTextBlock(
			D2dCtx, Dwrite, Buf, rc, 16.0f,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::SkyBlue));

		NextY += PickPanelHeight + Space;
	}

	if (bShowMemory)
	{
		double Mb = static_cast<double>(CMemoryManager::TotalAllocationBytes) / (1024.0 * 1024.0);

		wchar_t Buf[128];
		swprintf_s(Buf, L"Memory: %.1f MB\nAllocs: %u", Mb, CMemoryManager::TotalAllocationCount);

		D2D1_RECT_F Rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(
			D2dCtx, Dwrite, Buf, Rc, 16.0f,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::LightGreen));

		NextY += PanelHeight + Space;
	}

	if (bShowDecal)
	{
		// 1. FDecalStatManager로부터 통계 데이터를 가져옵니다.
		uint32_t TotalCount = FDecalStatManager::GetInstance().GetTotalDecalCount();
		//uint32_t VisibleDecalCount = FDecalStatManager::GetInstance().GetVisibleDecalCount();
		uint32_t AffectedMeshCount = FDecalStatManager::GetInstance().GetAffectedMeshCount();
		double TotalTime = FDecalStatManager::GetInstance().GetDecalPassTimeMS();
		double AverageTimePerDecal = FDecalStatManager::GetInstance().GetAverageTimePerDecalMS();
		double AverageTimePerDraw = FDecalStatManager::GetInstance().GetAverageTimePerDrawMS();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[256];
		swprintf_s(Buf, L"[Decal Stats]\nTotal: %u\nAffectedMesh: %u\n전체 소요 시간: %.3f ms\nAvg/Decal: %.3f ms\nAvg/Mesh: %.3f ms",
			TotalCount,
			AffectedMeshCount,
			TotalTime,
			AverageTimePerDecal,
			AverageTimePerDraw);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float decalPanelHeight = 140.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + decalPanelHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 주황색(Orange)으로 설정합니다.
		DrawTextBlock(
			D2dCtx, Dwrite, Buf, rc, 16.0f,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Orange));

		NextY += decalPanelHeight + Space;
	}

	if (bShowTileCulling)
	{
		// 1. FTileCullingStatManager로부터 통계 데이터를 가져옵니다.
		const FTileCullingStats& TileStats = FTileCullingStatManager::GetInstance().GetStats();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[512];
		swprintf_s(Buf, L"[Tile Culling Stats]\nTiles: %u x %u (%u)\nLights: %u (P:%u S:%u)\nMin/Avg/Max: %u / %.1f / %u\nCulling Eff: %.1f%%\nBuffer: %u KB",
			TileStats.TileCountX,
			TileStats.TileCountY,
			TileStats.TotalTileCount,
			TileStats.TotalLights,
			TileStats.TotalPointLights,
			TileStats.TotalSpotLights,
			TileStats.MinLightsPerTile,
			TileStats.AvgLightsPerTile,
			TileStats.MaxLightsPerTile,
			TileStats.CullingEfficiency,
			TileStats.LightIndexBufferSizeBytes / 1024);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float tilePanelHeight = 160.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + tilePanelHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 cyan으로 설정합니다.
		DrawTextBlock(
			D2dCtx, Dwrite, Buf, rc, 16.0f,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Cyan));

		NextY += tilePanelHeight + Space;
	}

	if (bShowShadowMap)
	{
		// 1. FShadowStatManager로부터 통계 데이터를 가져옵니다.
		const FShadowStats& ShadowStats = FShadowStatManager::GetInstance().GetStats();

		// 2. 메모리를 MB 단위로 변환
		double SpotUsedMB = static_cast<double>(ShadowStats.SpotLightUsedBytes) / (1024.0 * 1024.0);
		double PointUsedMB = static_cast<double>(ShadowStats.PointLightUsedBytes) / (1024.0 * 1024.0);
		double TotalUsedMB = static_cast<double>(ShadowStats.TotalUsedBytes) / (1024.0 * 1024.0);

		// 3. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[1024];

		if (ShadowStats.bUsingCSM)
		{
			// CSM 사용 시: Low/Medium/High 티어별로 표시
			double CSMLowUsedMB = static_cast<double>(ShadowStats.CSMTierUsedBytes[0]) / (1024.0 * 1024.0);
			double CSMMediumUsedMB = static_cast<double>(ShadowStats.CSMTierUsedBytes[1]) / (1024.0 * 1024.0);
			double CSMHighUsedMB = static_cast<double>(ShadowStats.CSMTierUsedBytes[2]) / (1024.0 * 1024.0);

			swprintf_s(Buf, L"[Shadow Map Stats]\n해상도:\n  Dir (CSM):\n    Low: %ux%u (%u개)\n    Med: %ux%u (%u개)\n    High: %ux%u (%u개)\n  Spot: %ux%u\n  Point: %ux%u\n메모리:\n  Dir CSM: %.2f MB\n    Low: %.2f MB\n    Med: %.2f MB\n    High: %.2f MB\n  Spot: %u개 (%.2f MB)\n  Point: %u개 (%.2f MB)\n전체: %u개\n전체 메모리: %.2f MB",
				ShadowStats.CSMTierResolutions[0], ShadowStats.CSMTierResolutions[0], ShadowStats.CSMTierCascadeCounts[0],
				ShadowStats.CSMTierResolutions[1], ShadowStats.CSMTierResolutions[1], ShadowStats.CSMTierCascadeCounts[1],
				ShadowStats.CSMTierResolutions[2], ShadowStats.CSMTierResolutions[2], ShadowStats.CSMTierCascadeCounts[2],
				ShadowStats.SpotLightResolution, ShadowStats.SpotLightResolution,
				ShadowStats.PointLightResolution, ShadowStats.PointLightResolution,
				CSMLowUsedMB + CSMMediumUsedMB + CSMHighUsedMB,
				CSMLowUsedMB,
				CSMMediumUsedMB,
				CSMHighUsedMB,
				ShadowStats.SpotLightCount,
				SpotUsedMB,
				ShadowStats.PointLightCount,
				PointUsedMB,
				ShadowStats.GetTotalLightCount(),
				TotalUsedMB);
		}
		else
		{
			// Non-CSM: 기존 방식 유지
			double DirUsedMB = static_cast<double>(ShadowStats.DirectionalLightUsedBytes) / (1024.0 * 1024.0);

			swprintf_s(Buf, L"[Shadow Map Stats]\n해상도:\n  Dir: %ux%u\n  Spot: %ux%u\n  Point: %ux%u\n메모리:\n  Dir: %u개 (%.2f MB)\n  Spot: %u개 (%.2f MB)\n  Point: %u개 (%.2f MB)\n전체: %u개\n전체 메모리: %.2f MB",
				ShadowStats.DirectionalLightResolution,
				ShadowStats.DirectionalLightResolution,
				ShadowStats.SpotLightResolution,
				ShadowStats.SpotLightResolution,
				ShadowStats.PointLightResolution,
				ShadowStats.PointLightResolution,
				ShadowStats.DirectionalLightCount,
				DirUsedMB,
				ShadowStats.SpotLightCount,
				SpotUsedMB,
				ShadowStats.PointLightCount,
				PointUsedMB,
				ShadowStats.GetTotalLightCount(),
				TotalUsedMB);
		}

		// 4. 텍스트를 여러 줄 표시해야 하므로 패널 크기를 늘립니다.
		const float shadowPanelWidth = 280.0f;
		const float shadowPanelHeight = ShadowStats.bUsingCSM ? 360.0f : 270.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + shadowPanelWidth, NextY + shadowPanelHeight);

		// 5. DrawTextBlock 함수를 호출하여 화면에 그립니다.
		DrawTextBlock(
			D2dCtx, Dwrite, Buf, rc, 16.0f,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Magenta));

		NextY += shadowPanelHeight + Space;
	}

	D2dCtx->EndDraw();
	D2dCtx->SetTarget(nullptr);

	SafeRelease(TargetBmp);
	SafeRelease(Dwrite);
	SafeRelease(D2dCtx);
	SafeRelease(D2dDevice);
	SafeRelease(DxgiDevice);
	SafeRelease(Surface);
	SafeRelease(D2dFactory);
}

void UStatsOverlayD2D::SetShowFPS(bool b)
{
	bShowFPS = b;
}

void UStatsOverlayD2D::SetShowMemory(bool b)
{
	bShowMemory = b;
}

void UStatsOverlayD2D::SetShowPicking(bool b)
{
	bShowPicking = b;
}

void UStatsOverlayD2D::SetShowDecal(bool b)
{
	bShowDecal = b;
}

void UStatsOverlayD2D::ToggleFPS()
{
	bShowFPS = !bShowFPS;
}

void UStatsOverlayD2D::ToggleMemory()
{
	bShowMemory = !bShowMemory;
}

void UStatsOverlayD2D::TogglePicking()
{
	bShowPicking = !bShowPicking;
}

void UStatsOverlayD2D::ToggleDecal()
{
	bShowDecal = !bShowDecal;
}

void UStatsOverlayD2D::SetShowTileCulling(bool b)
{
	bShowTileCulling = b;
}

void UStatsOverlayD2D::ToggleTileCulling()
{
	bShowTileCulling = !bShowTileCulling;
}

void UStatsOverlayD2D::SetShowShadowMap(bool b)
{
	bShowShadowMap = b;
}

void UStatsOverlayD2D::ToggleShadowMap()
{
	bShowShadowMap = !bShowShadowMap;
}
