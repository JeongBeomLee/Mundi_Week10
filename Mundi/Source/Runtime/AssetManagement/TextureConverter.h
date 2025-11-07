/**
 * @file TextureConverter.h
 * @brief DDS 포맷 변환 및 캐싱을 지원하는 텍스처 변환 유틸리티
 *
 * DirectXTex 라이브러리를 사용하여 원본 텍스처 파일(PNG, JPG, TGA 등)을
 * DDS 포맷으로 변환하는 기능을 제공합니다. OBJ 바이너리 캐싱과 유사한
 * 캐시 시스템을 구현하여 텍스처 로딩 성능을 향상시킵니다.
 */

#pragma once
#include "UEContainer.h"
#include <d3d11.h>
#include <filesystem>

/**
 * @class FTextureConverter
 * @brief 텍스처 포맷 변환 및 캐시 관리를 위한 정적 유틸리티 클래스
 */
class FTextureConverter
{
public:
	/**
	 * @brief 원본 텍스처를 DDS 포맷으로 변환
	 * @param SourcePath 원본 텍스처 파일 경로 (.png, .jpg, .tga 등)
	 * @param OutputPath 출력 DDS 파일 경로 (비어있으면 자동 생성)
	 * @param Format 대상 DXGI 포맷 (기본값: 빠른 압축을 위한 BC3_UNORM)
	 * @return 변환 성공 시 true, 실패 시 false
	 */
	static bool ConvertToDDS(
		const FString& SourcePath,
		const FString& OutputPath = "",
		DXGI_FORMAT Format = DXGI_FORMAT_BC3_UNORM
	);

	/**
	 * @brief DDS 캐시 재생성이 필요한지 확인
	 * @param SourcePath 원본 텍스처 파일 경로
	 * @param DDSPath 캐시된 DDS 파일 경로
	 * @return 캐시가 유효하지 않거나 없으면 true
	 */
	static bool ShouldRegenerateDDS(
		const FString& SourcePath,
		const FString& DDSPath
	);

	/**
	 * @brief 주어진 원본 텍스처에 대한 DDS 캐시 경로 생성
	 * @param SourcePath 원본 텍스처 파일 경로
	 * @return Data/TextureCache/에 생성된 캐시 경로
	 */
	static FString GetDDSCachePath(const FString& SourcePath);

	/**
	 * @brief WIC 로딩을 지원하는 파일 확장자인지 확인
	 * @param Extension 파일 확장자 (점 포함, 예: ".png")
	 * @return 지원되는 포맷이면 true
	 */
	static bool IsSupportedFormat(const FString& Extension);

	/**
	 * @brief 텍스처 변환 시 밉맵 생성 설정
	 * @param bGenerateMips 밉맵 생성 여부 (기본값: true)
	 */
	static void SetGenerateMipmaps(bool bGenerateMips);

	/**
	 * @brief 이미지 특성에 따라 권장 압축 포맷 반환
	 * @param bHasAlpha 이미지에 알파 채널이 있는지 여부
	 * @param bSRGB sRGB 포맷 사용 여부 (Diffuse=true, Normal=false)
	 * @return 최적의 DXGI 포맷
	 */
	static DXGI_FORMAT GetRecommendedFormat(bool bHasAlpha, bool bSRGB = true);

private:
	// 인스턴스화 비활성화
	FTextureConverter() = delete;
	~FTextureConverter() = delete;

	/**
	 * @brief 캐시 디렉토리가 존재하는지 확인하고 생성
	 * @param CachePath 캐시 파일 경로
	 */
	static void EnsureCacheDirectoryExists(const FString& CachePath);

	// 설정
	static inline bool bShouldGenerateMipmaps = true;
};
