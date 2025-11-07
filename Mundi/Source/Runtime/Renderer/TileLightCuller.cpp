#include "pch.h"
#include "TileLightCuller.h"
#include <algorithm>

FTileLightCuller::FTileLightCuller()
	: RHI(nullptr)
	, TileSize(16)
	, TileCountX(0)
	, TileCountY(0)
	, TotalTileCount(0)
	, LightIndexBuffer(nullptr)
	, LightIndexBufferSRV(nullptr)
{
}

FTileLightCuller::~FTileLightCuller()
{
	Release();
}

void FTileLightCuller::Initialize(D3D11RHI* InRHI, UINT InTileSize)
{
	RHI = InRHI;
	TileSize = InTileSize;

	// 초기화는 CullLights에서 뷰포트 크기를 알게 되면 수행
}

void FTileLightCuller::CullLights(
	const TArray<FPointLightInfo>& PointLights,
	const TArray<FSpotLightInfo>& SpotLights,
	const FMatrix& ViewMatrix,
	const FMatrix& ProjMatrix,
	float NearPlane,
	float FarPlane,
	UINT ViewportWidth,
	UINT ViewportHeight)
{
	// 타일 그리드 계산
	TileCountX = (ViewportWidth + TileSize - 1) / TileSize;
	TileCountY = (ViewportHeight + TileSize - 1) / TileSize;
	TotalTileCount = TileCountX * TileCountY;

	// 통계 초기화
	Stats.Reset();
	Stats.TileCountX = TileCountX;
	Stats.TileCountY = TileCountY;
	Stats.TotalTileCount = TotalTileCount;
	Stats.TotalPointLights = PointLights.Num();
	Stats.TotalSpotLights = SpotLights.Num();
	Stats.TotalLights = PointLights.Num() + SpotLights.Num();

	// 타일 라이트 인덱스 버퍼 크기 재조정
	UINT RequiredSize = TotalTileCount * MaxLightsPerTile;
	if (TileLightIndices.Num() != RequiredSize)
	{
		TileLightIndices.SetNum(RequiredSize);
	}

	// 버퍼 초기화 (모든 타일의 라이트 개수를 0으로)
	memset(TileLightIndices.GetData(), 0, RequiredSize * sizeof(uint32));

	// Inverse View-Projection 행렬 계산
	FMatrix InvViewProj = ProjMatrix.InversePerspectiveProjection() * ViewMatrix.InverseAffine();

	// 각 타일에 대해 컬링 수행
	Stats.MinLightsPerTile = UINT_MAX;
	Stats.MaxLightsPerTile = 0;
	uint32 TotalLightsAcrossAllTiles = 0;

	for (UINT TileY = 0; TileY < TileCountY; ++TileY)
	{
		for (UINT TileX = 0; TileX < TileCountX; ++TileX)
		{
			UINT TileIndex = TileY * TileCountX + TileX;
			UINT TileDataOffset = TileIndex * MaxLightsPerTile;

			// 타일 프러스텀 생성
			FFrustum Frustum = CreateTileFrustum(TileX, TileY, InvViewProj, NearPlane, FarPlane);

			uint32 LightCount = 0;

			// Point Light 테스트
			for (int32 i = 0; i < PointLights.Num() && LightCount < MaxLightsPerTile - 1; ++i)
			{
				Stats.TotalLightTests++;

				if (TestPointLightAgainstFrustum(PointLights[i], Frustum, ViewMatrix))
				{
					// 라이트 인덱스 저장 (상위 16비트: 타입(0=Point), 하위 16비트: 인덱스)
					TileLightIndices[TileDataOffset + 1 + LightCount] = i;
					LightCount++;
					Stats.TotalLightsPassed++;
				}
			}

			// Spot Light 테스트
			for (int32 i = 0; i < SpotLights.Num() && LightCount < MaxLightsPerTile - 1; ++i)
			{
				Stats.TotalLightTests++;

				if (TestSpotLightAgainstFrustum(SpotLights[i], Frustum, ViewMatrix))
				{
					// 라이트 인덱스 저장 (상위 16비트: 타입(1=Spot), 하위 16비트: 인덱스)
					TileLightIndices[TileDataOffset + 1 + LightCount] = (1 << 16) | i;
					LightCount++;
					Stats.TotalLightsPassed++;
				}
			}

			// 첫 번째 요소에 라이트 개수 저장
			TileLightIndices[TileDataOffset] = LightCount;

			// 통계 업데이트
			Stats.MinLightsPerTile = FMath::Min(Stats.MinLightsPerTile, LightCount);
			Stats.MaxLightsPerTile = FMath::Max(Stats.MaxLightsPerTile, LightCount);
			TotalLightsAcrossAllTiles += LightCount;
		}
	}

	// 평균 계산
	if (TotalTileCount > 0)
	{
		Stats.AvgLightsPerTile = static_cast<float>(TotalLightsAcrossAllTiles) / static_cast<float>(TotalTileCount);
	}

	// 컬링 효율성 계산
	Stats.CalculateStats();

	// GPU 버퍼 생성 또는 업데이트
	if (!LightIndexBuffer)
	{
		// 버퍼 생성
		HRESULT hr = RHI->CreateStructuredBuffer(
			sizeof(uint32),
			RequiredSize,
			TileLightIndices.GetData(),
			&LightIndexBuffer
		);

		if (SUCCEEDED(hr))
		{
			// SRV 생성
			RHI->CreateStructuredBufferSRV(LightIndexBuffer, &LightIndexBufferSRV);
		}

		Stats.LightIndexBufferSizeBytes = RequiredSize * sizeof(uint32);
	}
	else
	{
		// 기존 버퍼 업데이트
		RHI->UpdateStructuredBuffer(
			LightIndexBuffer,
			TileLightIndices.GetData(),
			RequiredSize * sizeof(uint32)
		);
	}
}

FFrustum FTileLightCuller::CreateTileFrustum(
	UINT TileX,
	UINT TileY,
	const FMatrix& InvViewProj,
	float NearPlane,
	float FarPlane)
{
	FFrustum Frustum;

	// 타일의 NDC 좌표 계산
	// NDC: [-1, 1] 범위, 왼쪽 아래가 (-1, -1), 오른쪽 위가 (1, 1)
	// 주의: DirectX는 Y축이 위쪽이 양수

	float TileMinX = static_cast<float>(TileX * TileSize);
	float TileMaxX = static_cast<float>((TileX + 1) * TileSize);
	float TileMinY = static_cast<float>(TileY * TileSize);
	float TileMaxY = static_cast<float>((TileY + 1) * TileSize);

	// 뷰포트 크기
	float ViewportWidth = static_cast<float>(TileCountX * TileSize);
	float ViewportHeight = static_cast<float>(TileCountY * TileSize);

	// Screen Space -> NDC
	float NDC_MinX = (TileMinX / ViewportWidth) * 2.0f - 1.0f;
	float NDC_MaxX = (TileMaxX / ViewportWidth) * 2.0f - 1.0f;
	float NDC_MinY = 1.0f - (TileMaxY / ViewportHeight) * 2.0f; // Y축 반전
	float NDC_MaxY = 1.0f - (TileMinY / ViewportHeight) * 2.0f;

	// 8개의 프러스텀 코너 (NDC 공간)
	FVector4 FrustumCorners[8];

	// Near plane corners
	FrustumCorners[0] = FVector4(NDC_MinX, NDC_MinY, 0.0f, 1.0f);
	FrustumCorners[1] = FVector4(NDC_MaxX, NDC_MinY, 0.0f, 1.0f);
	FrustumCorners[2] = FVector4(NDC_MaxX, NDC_MaxY, 0.0f, 1.0f);
	FrustumCorners[3] = FVector4(NDC_MinX, NDC_MaxY, 0.0f, 1.0f);

	// Far plane corners
	FrustumCorners[4] = FVector4(NDC_MinX, NDC_MinY, 1.0f, 1.0f);
	FrustumCorners[5] = FVector4(NDC_MaxX, NDC_MinY, 1.0f, 1.0f);
	FrustumCorners[6] = FVector4(NDC_MaxX, NDC_MaxY, 1.0f, 1.0f);
	FrustumCorners[7] = FVector4(NDC_MinX, NDC_MaxY, 1.0f, 1.0f);

	// NDC -> World Space 변환
	FVector WorldCorners[8];
	for (int i = 0; i < 8; ++i)
	{
		FVector4 WorldPos = FrustumCorners[i] * InvViewProj;
		WorldPos /= WorldPos.W; // Perspective divide
		WorldCorners[i] = FVector(WorldPos.X, WorldPos.Y, WorldPos.Z);
	}

	// 프러스텀 평면 생성 (외향 법선)
	// 평면 방정식: Normal · P + Distance = 0

	// Left plane (점: 0, 3, 7)
	{
		FVector Edge1 = WorldCorners[3] - WorldCorners[0];
		FVector Edge2 = WorldCorners[7] - WorldCorners[0];
		FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();
		Frustum.LeftFace.Normal = FVector4(Normal.X, Normal.Y, Normal.Z, 0.0f);
		Frustum.LeftFace.Distance = -FVector::Dot(Normal, WorldCorners[0]);
	}

	// Right plane (점: 1, 5, 6)
	{
		FVector Edge1 = WorldCorners[5] - WorldCorners[1];
		FVector Edge2 = WorldCorners[6] - WorldCorners[1];
		FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();
		Frustum.RightFace.Normal = FVector4(Normal.X, Normal.Y, Normal.Z, 0.0f);
		Frustum.RightFace.Distance = -FVector::Dot(Normal, WorldCorners[1]);
	}

	// Bottom plane (점: 0, 1, 5)
	{
		FVector Edge1 = WorldCorners[1] - WorldCorners[0];
		FVector Edge2 = WorldCorners[5] - WorldCorners[0];
		FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();
		Frustum.BottomFace.Normal = FVector4(Normal.X, Normal.Y, Normal.Z, 0.0f);
		Frustum.BottomFace.Distance = -FVector::Dot(Normal, WorldCorners[0]);
	}

	// Top plane (점: 2, 3, 7)
	{
		FVector Edge1 = WorldCorners[3] - WorldCorners[2];
		FVector Edge2 = WorldCorners[7] - WorldCorners[2];
		FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();
		Frustum.TopFace.Normal = FVector4(Normal.X, Normal.Y, Normal.Z, 0.0f);
		Frustum.TopFace.Distance = -FVector::Dot(Normal, WorldCorners[2]);
	}

	// Near plane (점: 0, 1, 2)
	{
		FVector Edge1 = WorldCorners[1] - WorldCorners[0];
		FVector Edge2 = WorldCorners[2] - WorldCorners[0];
		FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();
		Frustum.NearFace.Normal = FVector4(Normal.X, Normal.Y, Normal.Z, 0.0f);
		Frustum.NearFace.Distance = -FVector::Dot(Normal, WorldCorners[0]);
	}

	// Far plane (점: 4, 6, 5)
	{
		FVector Edge1 = WorldCorners[6] - WorldCorners[4];
		FVector Edge2 = WorldCorners[5] - WorldCorners[4];
		FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();
		Frustum.FarFace.Normal = FVector4(Normal.X, Normal.Y, Normal.Z, 0.0f);
		Frustum.FarFace.Distance = -FVector::Dot(Normal, WorldCorners[4]);
	}

	return Frustum;
}

bool FTileLightCuller::TestPointLightAgainstFrustum(
	const FPointLightInfo& Light,
	const FFrustum& Frustum,
	const FMatrix& ViewMatrix)
{
	// Point Light는 구체로 근사
	FVector LightPos = Light.Position;
	float Radius = Light.AttenuationRadius;

	return SphereIntersectsFrustum(LightPos, Radius, Frustum);
}

bool FTileLightCuller::TestSpotLightAgainstFrustum(
	const FSpotLightInfo& Light,
	const FFrustum& Frustum,
	const FMatrix& ViewMatrix)
{
	FVector LightPos = Light.Position;
	float Radius = Light.AttenuationRadius;

	// Spot Light도 구체로 근사
	return SphereIntersectsFrustum(LightPos, Radius, Frustum);
}

bool FTileLightCuller::SphereIntersectsFrustum(
	const FVector& Center,
	float Radius,
	const FFrustum& Frustum)
{
	// 구체가 모든 평면의 앞쪽(양수)에 있으면 프러스텀 내부
	// 하나라도 뒤쪽(음수)에 있고 거리가 반지름보다 크면 외부

	// 6개 평면 각각에 대해 테스트
	const FPlane* Planes[6] = {
		&Frustum.LeftFace,
		&Frustum.RightFace,
		&Frustum.TopFace,
		&Frustum.BottomFace,
		&Frustum.NearFace,
		&Frustum.FarFace
	};

	for (int i = 0; i < 6; ++i)
	{
		// 평면 방정식: Normal · P + Distance = 0
		// 점 P에서 평면까지의 부호 있는 거리 = Normal · P + Distance
		FVector Normal3D(Planes[i]->Normal.X, Planes[i]->Normal.Y, Planes[i]->Normal.Z);
		float SignedDistance = FVector::Dot(Normal3D, Center) + Planes[i]->Distance;

		// 구체가 평면 뒤쪽에 완전히 있으면 교차하지 않음
		if (SignedDistance < -Radius)
		{
			return false;
		}
	}

	return true;
}

bool FTileLightCuller::ConeIntersectsFrustum(
	const FVector& Apex,
	const FVector& Direction,
	float Height,
	float CosHalfAngle,
	const FFrustum& Frustum)
{
	// 원뿔-프러스텀 교차 테스트
	float SinHalfAngle = sqrt(1.0f - CosHalfAngle * CosHalfAngle);

	// 원뿔 밑면의 반지름
	float BaseRadius = Height * SinHalfAngle;

	// 경계 구체의 중심은 원뿔 축을 따라 이동
	float K = (CosHalfAngle > 0.707f) ? 0.5f : (0.5f + 0.5f * SinHalfAngle);
	FVector SphereCenter = Apex + Direction * (Height * K);

	// 경계 구체의 반지름 (원뿔의 꼭지점과 밑면 가장자리를 모두 포함)
	float SphereRadius;
	if (CosHalfAngle > 0.707f) // 각도가 45도 이하 (좁은 원뿔)
	{
		// 좁은 원뿔: 밑면 중심에서 가장자리까지의 거리
		SphereRadius = sqrt(BaseRadius * BaseRadius + (Height * 0.5f) * (Height * 0.5f));
	}
	else // 각도가 45도 이상 (넓은 원뿔)
	{
		// 넓은 원뿔: 더 보수적인 반지름 사용
		SphereRadius = Height * 0.866f; // sqrt(3)/2 ≈ 0.866
	}

	// 2. 경계 구체가 프러스텀과 교차하는지 먼저 테스트 (빠른 거부)
	if (!SphereIntersectsFrustum(SphereCenter, SphereRadius, Frustum))
	{
		return false;
	}

	// 3. 추가 정밀 테스트: 원뿔 끝점(밑면 중심)도 체크
	FVector ConeEnd = Apex + Direction * Height;

	// 6개 평면 각각에 대해 테스트
	const FPlane* Planes[6] = {
		&Frustum.LeftFace,
		&Frustum.RightFace,
		&Frustum.TopFace,
		&Frustum.BottomFace,
		&Frustum.NearFace,
		&Frustum.FarFace
	};

	// 원뿔의 꼭지점과 끝점이 모두 같은 평면 뒤쪽에 있는지 확인
	for (int i = 0; i < 6; ++i)
	{
		FVector Normal3D(Planes[i]->Normal.X, Planes[i]->Normal.Y, Planes[i]->Normal.Z);

		float ApexDistance = FVector::Dot(Normal3D, Apex) + Planes[i]->Distance;
		float EndDistance = FVector::Dot(Normal3D, ConeEnd) + Planes[i]->Distance;

		// 두 점이 모두 평면 뒤쪽에 있고, 밑면 반지름보다 멀리 있으면 완전히 외부
		if (ApexDistance < -BaseRadius && EndDistance < -BaseRadius)
		{
			return false;
		}
	}

	// 모든 테스트를 통과하면 교차한다고 판단
	return true;
}

ID3D11ShaderResourceView* FTileLightCuller::GetLightIndexBufferSRV()
{
	return LightIndexBufferSRV;
}

void FTileLightCuller::Release()
{
	if (LightIndexBufferSRV)
	{
		LightIndexBufferSRV->Release();
		LightIndexBufferSRV = nullptr;
	}

	if (LightIndexBuffer)
	{
		LightIndexBuffer->Release();
		LightIndexBuffer = nullptr;
	}

	TileLightIndices.Empty();
}
