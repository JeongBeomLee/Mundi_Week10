#pragma once
#include "LightManager.h"
#include "TileCullingStats.h"
#include "D3D11RHI.h"
#include "Frustum.h"

// 타일 기반 라이트 컬링을 CPU에서 수행하는 클래스
// Conservative(near, far) frustum 방식으로 각 타일에 영향을 주는 라이트를 계산
class FTileLightCuller
{
public:
	FTileLightCuller();
	~FTileLightCuller();

	// 초기화 (Structured Buffer 생성)
	void Initialize(D3D11RHI* InRHI, UINT InTileSize = 16);

	// 타일 컬링 수행 (매 프레임 호출)
	void CullLights(
		const TArray<FPointLightInfo>& PointLights,
		const TArray<FSpotLightInfo>& SpotLights,
		const FMatrix& ViewMatrix,
		const FMatrix& ProjMatrix,
		float NearPlane,
		float FarPlane,
		UINT ViewportWidth,
		UINT ViewportHeight
	);

	// 컬링 결과를 Structured Buffer에 업데이트하고 SRV 반환
	ID3D11ShaderResourceView* GetLightIndexBufferSRV();

	// 통계 정보 반환
	const FTileCullingStats& GetStats() const { return Stats; }

	// 리소스 해제
	void Release();

private:
	// 타일 프러스텀 생성 (Conservative near/far 방식)
	FFrustum CreateTileFrustum(
		UINT TileX,
		UINT TileY,
		const FMatrix& InvViewProj,
		float NearPlane,
		float FarPlane
	);

	// 라이트가 타일 프러스텀과 교차하는지 테스트
	bool TestPointLightAgainstFrustum(const FPointLightInfo& Light, const FFrustum& Frustum, const FMatrix& ViewMatrix);
	bool TestSpotLightAgainstFrustum(const FSpotLightInfo& Light, const FFrustum& Frustum, const FMatrix& ViewMatrix);

	// 구체와 프러스텀 교차 테스트
	bool SphereIntersectsFrustum(const FVector& Center, float Radius, const FFrustum& Frustum);

	// 원뿔과 프러스텀 교차 테스트 (SpotLight용)
	bool ConeIntersectsFrustum(
		const FVector& Apex,           // 원뿔 꼭지점 (라이트 위치)
		const FVector& Direction,      // 원뿔 방향 (정규화됨)
		float Height,                  // 원뿔 높이 (AttenuationRadius)
		float CosHalfAngle,            // cos(OuterConeAngle / 2)
		const FFrustum& Frustum
	);

private:
	D3D11RHI* RHI;

	// 타일 설정
	UINT TileSize;          // 타일 크기 (픽셀, 기본값 16)
	UINT TileCountX;        // 가로 타일 개수
	UINT TileCountY;        // 세로 타일 개수
	UINT TotalTileCount;    // 전체 타일 개수

	// 타일당 최대 라이트 개수 (보수적으로 설정)
	static constexpr UINT MaxLightsPerTile = 256;

	// 타일별 라이트 인덱스 저장
	// [TileIndex * MaxLightsPerTile] 위치에 라이트 개수 저장
	// [TileIndex * MaxLightsPerTile + 1 ~ ...] 위치에 라이트 인덱스 저장
	TArray<uint32> TileLightIndices;

	// GPU 리소스
	ID3D11Buffer* LightIndexBuffer;
	ID3D11ShaderResourceView* LightIndexBufferSRV;

	// 통계
	FTileCullingStats Stats;
};
