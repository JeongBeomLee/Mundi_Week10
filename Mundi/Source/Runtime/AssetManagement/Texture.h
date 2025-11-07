#pragma once
#include "ResourceBase.h"
#include <d3d11.h>

class UTexture : public UResourceBase
{
public:
	DECLARE_CLASS(UTexture, UResourceBase)

	UTexture();
	virtual ~UTexture();

	// 실제 로드된 파일 경로를 반환 (DDS 캐시 사용 시 DDS 경로 반환)
	// bSRGB: true = sRGB 포맷 사용 (Diffuse/Albedo 텍스처), false = Linear 포맷 (Normal/Data 텍스처)
	void Load(const FString& InFilePath, ID3D11Device* InDevice, bool bSRGB = true);

	ID3D11ShaderResourceView* GetShaderResourceView() const { return ShaderResourceView; }
	ID3D11Texture2D* GetTexture2D() const { return Texture2D; }

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	DXGI_FORMAT GetFormat() const { return Format; }

	// DDS 캐시 파일 경로
	const FString& GetCacheFilePath() const { return CacheFilePath; }

	void ReleaseResources();

private:
	FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube_texture.png.dds)

	ID3D11Texture2D* Texture2D;
	ID3D11ShaderResourceView* ShaderResourceView;

	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};
