#pragma once

#include "Frustum.h"  // GetFrustumCornersWorldSpace 함수 사용

// Shadow rendering에 필요한 View/Projection 행렬 집합
// 주의: 이 헤더를 사용하기 전에 pch.h가 include되어야 함 (FMatrix, FVector 정의 필요)
struct FShadowViewProjection
{
	FMatrix View;
	FMatrix Projection;
	FMatrix ViewProjection;

	// LiSPSM hybrid에서 실제로 사용된 알고리즘 (true = TSM, false = OpenGL LiSPSM)
	bool bUsedTSM = false;

	// SpotLight용 Shadow VP 행렬 생성
	// @param Position - 라이트 월드 위치
	// @param Direction - 라이트 방향
	// @param FOV - 전체 원뿔 각도 (OuterConeAngle * 2)
	// @param AttenuationRadius - 라이트 감쇠 반경 (Far plane으로 사용)
	// @param NearPlane - Near clipping plane (기본값 0.1f)
	static FShadowViewProjection CreateForSpotLight(
		const FVector& Position,
		const FVector& Direction,
		float FOV,
		float AttenuationRadius,
		float NearPlane = 0.1f)
	{
		FShadowViewProjection Result;

		// View 행렬 계산
		FVector LightUp = FVector(0, 1, 0);
		// Direction과 Up 벡터가 평행하면 다른 Up 벡터 사용
		if (FMath::Abs(FVector::Dot(Direction, LightUp)) > 0.99f)
		{
			LightUp = FVector(1, 0, 0);
		}

		Result.View = FMatrix::LookAtLH(Position, Position + Direction, LightUp);

		// Projection 행렬 계산 (정사각형 shadow map 가정)
		float AspectRatio = 1.0f;
		Result.Projection = FMatrix::PerspectiveFovLH(
			DegreesToRadians(FOV),
			AspectRatio,
			NearPlane,
			AttenuationRadius);

		// ViewProjection 행렬 계산
		Result.ViewProjection = Result.View * Result.Projection;

		return Result;
	}

private:
	// DirectionalLight용 Shadow VP 행렬 생성 (내부 공통 로직)
	// @param Direction - 라이트 방향 (정규화되지 않아도 됨)
	// @param FrustumCorners - Frustum의 8개 코너 (World Space)
	// @param ShadowExtension - 그림자 범위 확장 비율
	static FShadowViewProjection CreateForDirectionalLight_Internal(
		const FVector& Direction,
		const TArray<FVector>& FrustumCorners,
		float ShadowExtension)
	{
		FShadowViewProjection Result;

		// === 1. Light View 행렬 생성 ===
		FVector LightDir = Direction.GetNormalized();

		// Up vector 선택 (LightDir와 평행하지 않은 벡터)
		FVector Up = (FMath::Abs(LightDir.Y) < 0.99f)
			? FVector(0, 1, 0)
			: FVector(1, 0, 0);

		// Frustum 중심점 계산 (8개 코너의 평균)
		FVector FrustumCenter = FVector::Zero();
		for (int i = 0; i < 8; ++i)
		{
			FrustumCenter += FrustumCorners[i];
		}
		FrustumCenter /= 8.0f;

		// Light Position: Frustum 중심에서 라이트 방향 반대쪽
		FVector LightPos = FrustumCenter - LightDir;

		Result.View = FMatrix::LookAtLH(LightPos, LightPos + LightDir, Up);

		// === 2. Frustum 코너를 Light Space로 변환하여 AABB 계산 ===
		float MinX = FLT_MAX, MaxX = -FLT_MAX;
		float MinY = FLT_MAX, MaxY = -FLT_MAX;
		float MinZ = FLT_MAX, MaxZ = -FLT_MAX;

		for (int i = 0; i < 8; ++i)
		{
			// World Space 코너를 Light View Space로 변환
			FVector LightSpaceCorner = FrustumCorners[i] * Result.View;

			MinX = FMath::Min(MinX, LightSpaceCorner.X);
			MaxX = FMath::Max(MaxX, LightSpaceCorner.X);
			MinY = FMath::Min(MinY, LightSpaceCorner.Y);
			MaxY = FMath::Max(MaxY, LightSpaceCorner.Y);
			MinZ = FMath::Min(MinZ, LightSpaceCorner.Z);
			MaxZ = FMath::Max(MaxZ, LightSpaceCorner.Z);
		}

		// === 3. AABB 확장 및 Orthographic Projection 생성 ===
		float Width = MaxX - MinX;
		float Height = MaxY - MinY;
		float Depth = MaxZ - MinZ;

		// XY 평면 확장
		float ExtendX = Width * ShadowExtension;
		float ExtendY = Height * ShadowExtension;
		MinX -= ExtendX;
		MaxX += ExtendX;
		MinY -= ExtendY;
		MaxY += ExtendY;
		Width = MaxX - MinX;
		Height = MaxY - MinY;

		// Near Plane을 뒤로 확장하여 카메라 뒤의 오브젝트 포함
		float NearExtension = Depth * 2.0f;
		MinZ -= NearExtension;
		Depth = MaxZ - MinZ;

		// 중심을 원점으로 맞춘 Orthographic Projection
		Result.Projection = FMatrix::OrthoLH(Width, Height, 0.0f, Depth);

		// AABB 중심을 원점으로 이동시키기 위한 오프셋 적용
		float CenterX = (MaxX + MinX) * 0.5f;
		float CenterY = (MaxY + MinY) * 0.5f;

		// View 행렬에 오프셋 추가 (Translation 조정)
		Result.View.M[3][0] -= CenterX;
		Result.View.M[3][1] -= CenterY;
		Result.View.M[3][2] -= MinZ;  // Near plane을 0으로 맞춤

		// === 4. ViewProjection 계산 ===
		Result.ViewProjection = Result.View * Result.Projection;

		return Result;
	}

public:
	// DirectionalLight용 Shadow VP 행렬 생성 (View Frustum 기반)
	// @param Direction - 라이트 방향 (정규화되지 않아도 됨)
	// @param CameraView - 카메라의 View 행렬
	// @param CameraProjection - 카메라의 Projection 행렬
	// @param ShadowExtension - 그림자 범위 확장 비율 (기본값 0.2 = 20% 확장)
	static FShadowViewProjection CreateForDirectionalLight(
		const FVector& Direction,
		const FMatrix& CameraView,
		const FMatrix& CameraProjection,
		float ShadowExtension = 0.2f)
	{
		// Camera Frustum의 8개 코너를 월드 공간으로 변환
		TArray<FVector> FrustumCorners;

		FMatrix InvView = CameraView.InverseAffine();
		FMatrix InvProj = CameraProjection.InversePerspectiveProjection();
		FMatrix InvViewProj = InvProj * InvView;

		GetFrustumCornersWorldSpace(InvViewProj, FrustumCorners);

		// 공통 로직 호출
		return CreateForDirectionalLight_Internal(Direction, FrustumCorners, ShadowExtension);
	}

	// DirectionalLight용 Shadow VP 행렬 생성 (CSM용 - Frustum 코너를 직접 받음)
	// @param Direction - 라이트 방향 (정규화되지 않아도 됨)
	// @param FrustumCorners - Frustum의 8개 코너 (World Space)
	// @param ShadowExtension - 그림자 범위 확장 비율 (기본값 0.2 = 20% 확장)
	static FShadowViewProjection CreateForDirectionalLight_FromCorners(
		const FVector& Direction,
		const TArray<FVector>& FrustumCorners,
		float ShadowExtension = 0.2f)
	{
		// 공통 로직 호출
		return CreateForDirectionalLight_Internal(Direction, FrustumCorners, ShadowExtension);
	}

private:
	// Internal: Trapezoid Shadow Map (TSM) implementation
	// Best for parallel light/camera angles (수평)
	static FShadowViewProjection CreateForDirectionalLight_TSM_Internal(
		const FVector& Direction,
		const TArray<FVector>& FrustumCorners,
		const FMatrix& CameraView,
		const FMatrix& CameraProjection,
		float ShadowExtension = 0.2f)
	{
		FShadowViewProjection Result;

		// === Step 1: Setup Light Space Coordinate System ===

		FVector LightDir = Direction.GetNormalized();

		// Select up vector (avoid parallel vectors)
		FVector Up = (FMath::Abs(LightDir.Y) < 0.99f)
			? FVector(0, 1, 0)
			: FVector(1, 0, 0);

		// Calculate frustum center
		FVector FrustumCenter = FVector::Zero();
		for (int i = 0; i < 8; ++i)
		{
			FrustumCenter += FrustumCorners[i];
		}
		FrustumCenter /= 8.0f;

		// Position light far behind frustum to ensure coverage of all occluders
		// TSM Recipe: Light must be positioned to capture all potential shadow casters
		// Increased distance for dueling frusta stability
		FVector LightPos = FrustumCenter - LightDir * 100.0f;

		// Build light view matrix
		Result.View = FMatrix::LookAtLH(LightPos, LightPos + LightDir, Up);

		// === Step 2: Transform to Light Space ===

		TArray<FVector> LightSpaceCorners;
		LightSpaceCorners.Reserve(8);

		for (int i = 0; i < 8; ++i)
		{
			FVector LSCorner = FrustumCorners[i] * Result.View;
			LightSpaceCorners.Add(LSCorner);
		}

		// === Step 3: Get Camera Position in Light Space ===
		// Extract camera world position from inverse view matrix (translation component)
		FMatrix InvCameraView = CameraView.InverseAffine();
		FVector CameraWorldPos = FVector(InvCameraView.M[3][0], InvCameraView.M[3][1], InvCameraView.M[3][2]);
		FVector CameraLightSpace = CameraWorldPos * Result.View;

		// === Step 4: Split Frustum into Near/Far Halves ===
		// We'll focus more resolution on the near 4 corners (closer to camera)
		// by computing weighted AABB bounds

		// Near plane corners (indices 0-3), Far plane corners (indices 4-7)
		// Standard frustum corner ordering

		// Calculate AABB for near plane corners
		float NearMinX = FLT_MAX, NearMaxX = -FLT_MAX;
		float NearMinY = FLT_MAX, NearMaxY = -FLT_MAX;
		float NearMinZ = FLT_MAX, NearMaxZ = -FLT_MAX;

		for (int i = 0; i < 4; ++i)
		{
			NearMinX = FMath::Min(NearMinX, LightSpaceCorners[i].X);
			NearMaxX = FMath::Max(NearMaxX, LightSpaceCorners[i].X);
			NearMinY = FMath::Min(NearMinY, LightSpaceCorners[i].Y);
			NearMaxY = FMath::Max(NearMaxY, LightSpaceCorners[i].Y);
			NearMinZ = FMath::Min(NearMinZ, LightSpaceCorners[i].Z);
			NearMaxZ = FMath::Max(NearMaxZ, LightSpaceCorners[i].Z);
		}

		// Calculate AABB for far plane corners
		float FarMinX = FLT_MAX, FarMaxX = -FLT_MAX;
		float FarMinY = FLT_MAX, FarMaxY = -FLT_MAX;
		float FarMinZ = FLT_MAX, FarMaxZ = -FLT_MAX;

		for (int i = 4; i < 8; ++i)
		{
			FarMinX = FMath::Min(FarMinX, LightSpaceCorners[i].X);
			FarMaxX = FMath::Max(FarMaxX, LightSpaceCorners[i].X);
			FarMinY = FMath::Min(FarMinY, LightSpaceCorners[i].Y);
			FarMaxY = FMath::Max(FarMaxY, LightSpaceCorners[i].Y);
			FarMinZ = FMath::Min(FarMinZ, LightSpaceCorners[i].Z);
			FarMaxZ = FMath::Max(FarMaxZ, LightSpaceCorners[i].Z);
		}

		// === Step 5: Calculate camera direction and angle ===
		FVector CameraDir = FVector(InvCameraView.M[2][0], InvCameraView.M[2][1], InvCameraView.M[2][2]).GetNormalized();

		float CosGamma = FVector::Dot(CameraDir, LightDir);
		CosGamma = FMath::Clamp(CosGamma, -1.0f, 1.0f);
		float SinGamma = std::sqrt(1.0f - CosGamma * CosGamma);

		// Calculate full AABB first
		float MinZ = FMath::Min(NearMinZ, FarMinZ);
		float MaxZ = FMath::Max(NearMaxZ, FarMaxZ);

		// If angle is too small, fallback to equal weighting (LVP-like)
		float AngleFactor = FMath::Clamp(SinGamma * 3.0f, 0.0f, 1.0f);

		// Weighted blend based on viewing angle
		// High angle = more difference between near/far weighting
		// Low angle = more uniform (like LVP)
		// Increased base weight to prevent flickering in dueling frusta
		float NearWeight = 0.5f + AngleFactor * 0.4f;  // 0.6 to 0.9 (increased from 0.5-0.85)
		float FarWeight = 1.0f - NearWeight;           // 0.4 to 0.1 (decreased from 0.5-0.15)

		float MinX = NearMinX * NearWeight + FarMinX * FarWeight;
		float MaxX = NearMaxX * NearWeight + FarMaxX * FarWeight;
		float MinY = NearMinY * NearWeight + FarMinY * FarWeight;
		float MaxY = NearMaxY * NearWeight + FarMaxY * FarWeight;

		// === Step 6: Apply Shadow Extension ===

		float Width = MaxX - MinX;
		float Height = MaxY - MinY;
		float Depth = MaxZ - MinZ;

		float ExtendX = Width * ShadowExtension;
		float ExtendY = Height * ShadowExtension;
		MinX -= ExtendX;
		MaxX += ExtendX;
		MinY -= ExtendY;
		MaxY += ExtendY;
		Width = MaxX - MinX;
		Height = MaxY - MinY;

		// TSM Recipe: Extend near plane aggressively to catch occluders behind camera
		// This prevents light leaking from objects that cast shadows from behind
		// Reference: TSM paper - "Addressing the Polygon Offset Problem"
		//
		// Dueling Frusta: When view and light are nearly parallel, we need even more extension
		// to prevent flickering/shimmering on surfaces parallel to light direction
		float NearExtension = Depth * 10.0f;  // Increased from 5.0f to prevent flickering in dueling frusta
		MinZ -= NearExtension;

		// Also extend far plane slightly to avoid clipping distant occluders
		float FarExtension = Depth * 1.0f;  // Increased from 0.5f
		MaxZ += FarExtension;

		Depth = MaxZ - MinZ;

		// === Step 7: Create Orthographic Projection ===

		Result.Projection = FMatrix::OrthoLH(Width, Height, 0.0f, Depth);

		// === Step 8: Apply AABB Centering ===

		float CenterX = (MaxX + MinX) * 0.5f;
		float CenterY = (MaxY + MinY) * 0.5f;

		Result.View.M[3][0] -= CenterX;
		Result.View.M[3][1] -= CenterY;
		Result.View.M[3][2] -= MinZ;

		// === Step 9: Finalize ===

		Result.ViewProjection = Result.View * Result.Projection;

		return Result;
	}

	// Internal: OpenGL-style Light-space Perspective Shadow Map (LiSPSM) implementation
	// Best for perpendicular light/camera angles (수직)
	//
	// This is a direct port of the OpenGL LiSPSM algorithm from the shader tutorial
	// (Document/LisPSM/ShaderTutors/43_LightSpacePerspectiveSM/main.cpp:547-607).
	//
	// Key differences from the existing LiSPSM implementation:
	// - Uses Light-Space basis vectors (left, up, lightdir) instead of standard LookAt
	// - Applies perspective warping along the up-axis (Y in light space)
	// - Corrects eye position along the up-axis by (n - viewnear)
	// - More faithful to the original LiSPSM paper
	//
	// Algorithm (OpenGL version):
	// 1. Build Light-Space coordinate system using light direction and view direction
	// 2. Transform frustum corners to Light Space and calculate AABB
	// 3. Calculate LiSPSM parameters:
	//    - cosgamma: dot(viewdir, lightdir)
	//    - singamma: sqrt(1 - cosgamma^2)
	//    - znear: viewnear / singamma
	//    - d: AABB height in light space
	//    - zfar: znear + d * singamma
	//    - n: (znear + sqrt(zfar * znear)) / singamma
	//    - f: n + d
	// 4. Build perspective projection matrix (warps along Y-axis)
	// 5. Correct eye position: eyepos - up * (n - viewnear)
	// 6. Fit to unit cube using orthographic projection
	//
	// @param Direction - Light direction (normalized or not)
	// @param FrustumCorners - 8 world-space corners of the view frustum
	// @param CameraView - Camera's view matrix
	// @param CameraProjection - Camera's projection matrix
	// @return Shadow view-projection matrices
	//
	// References:
	// - "Light Space Perspective Shadow Maps" by Michael Wimmer et al. (2004)
	// - Original OpenGL implementation in shader tutorial
	static FShadowViewProjection CreateForDirectionalLight_OpenGLLiSPSM_Internal(
		const FVector& Direction,
		const TArray<FVector>& FrustumCorners,
		const FMatrix& CameraView,
		const FMatrix& CameraProjection)
	{
		FShadowViewProjection Result;

		// === Step 1: Extract view direction from camera ===
		// DirectX row-major: view matrix's 3rd row is the forward direction
		// Negate because we want the direction the camera is looking
		FVector ViewDir = FVector(-CameraView.M[0][2], -CameraView.M[1][2], -CameraView.M[2][2]);
		ViewDir.Normalize();

		FVector LightDir = Direction.GetNormalized();

		// Extract camera world position from inverse view matrix
		FMatrix InvCameraView = CameraView.InverseAffine();
		FVector EyePos = FVector(InvCameraView.M[3][0], InvCameraView.M[3][1], InvCameraView.M[3][2]);

		// === Step 2: Build Light-Space coordinate system ===
		// Light-Space uses: left, up, lightdir as basis vectors
		// This differs from standard LookAt which uses right, up, forward

		FVector LSLeft, LSUp;

		// OpenGL (Right-handed): cross(lightdir, viewdir) = left
		// DirectX (Left-handed): cross(viewdir, lightdir) = left (순서 반대)
		// 또는 negate the result to flip handedness
		LSLeft = FVector::Cross(ViewDir, LightDir);
		LSLeft.Normalize();

		// Calculate up vector: cross(left, lightdir)
		LSUp = FVector::Cross(LSLeft, LightDir);
		LSUp.Normalize();

		// Build Light-Space view matrix manually
		// DirectX Left-handed row-major format
		FMatrix LSLightView = FMatrix::Identity();

		LSLightView.M[0][0] = LSLeft.X;    LSLightView.M[0][1] = LSUp.X;    LSLightView.M[0][2] = LightDir.X;
		LSLightView.M[1][0] = LSLeft.Y;    LSLightView.M[1][1] = LSUp.Y;    LSLightView.M[1][2] = LightDir.Y;
		LSLightView.M[2][0] = LSLeft.Z;    LSLightView.M[2][1] = LSUp.Z;    LSLightView.M[2][2] = LightDir.Z;

		LSLightView.M[3][0] = -FVector::Dot(LSLeft, EyePos);
		LSLightView.M[3][1] = -FVector::Dot(LSUp, EyePos);
		LSLightView.M[3][2] = -FVector::Dot(LightDir, EyePos);

		// === Step 3: Transform frustum corners to Light Space and calculate AABB ===
		// CRITICAL: viewnear must be large enough to prevent clipping!
		// Too small = near plane too close = geometry clipped (shows as red/far in depth view)
		// Original OpenGL code uses camera's actual near plane, but that's too small for shadows
		// Use a reasonable value based on scene scale
		float viewnear = 10.0f;  // Increased from 1.0f to prevent clipping

		TArray<FVector> LSCorners;
		LSCorners.Reserve(8);

		float MinX = FLT_MAX, MaxX = -FLT_MAX;
		float MinY = FLT_MAX, MaxY = -FLT_MAX;
		float MinZ = FLT_MAX, MaxZ = -FLT_MAX;

		for (int i = 0; i < 8; ++i)
		{
			FVector LSCorner = FrustumCorners[i] * LSLightView;
			LSCorners.Add(LSCorner);

			MinX = FMath::Min(MinX, LSCorner.X);
			MaxX = FMath::Max(MaxX, LSCorner.X);
			MinY = FMath::Min(MinY, LSCorner.Y);
			MaxY = FMath::Max(MaxY, LSCorner.Y);
			MinZ = FMath::Min(MinZ, LSCorner.Z);
			MaxZ = FMath::Max(MaxZ, LSCorner.Z);
		}

		// === Step 4: Calculate LiSPSM parameters ===
		float cosgamma = FVector::Dot(ViewDir, LightDir);
		float singamma = FMath::Sqrt(1.0f - cosgamma * cosgamma);

		// Prevent division by zero
		singamma = FMath::Max(singamma, 0.01f);

		float znear = viewnear / singamma;
		float d = FMath::Abs(MaxY - MinY);  // AABB height in light space
		float zfar = znear + d * singamma;
		float n = (znear + FMath::Sqrt(zfar * znear)) / singamma;
		float f = n + d;

		// === Step 5: Build LiSPSM projection matrix ===
		// This matrix warps along the Y-axis (up direction in light space)
		FMatrix LiSPSMProj = FMatrix::Identity();

		LiSPSMProj.M[1][1] = (f + n) / (f - n);
		LiSPSMProj.M[3][1] = -2.0f * f * n / (f - n);
		LiSPSMProj.M[1][3] = 1.0f;
		LiSPSMProj.M[3][3] = 0.0f;

		// === Step 6: Correct eye position ===
		FVector CorrectedEyePos = EyePos - LSUp * (n - viewnear);

		LSLightView.M[3][0] = -FVector::Dot(LSLeft, CorrectedEyePos);
		LSLightView.M[3][1] = -FVector::Dot(LSUp, CorrectedEyePos);
		LSLightView.M[3][2] = -FVector::Dot(LightDir, CorrectedEyePos);

		// === Step 7: Apply LiSPSM projection and recalculate AABB ===
		FMatrix LSLightViewProj = LSLightView * LiSPSMProj;

		MinX = FLT_MAX; MaxX = -FLT_MAX;
		MinY = FLT_MAX; MaxY = -FLT_MAX;
		MinZ = FLT_MAX; MaxZ = -FLT_MAX;

		for (int i = 0; i < 8; ++i)
		{
			// Transform to 4D homogeneous coordinates
			FVector Corner = FrustumCorners[i];
			FVector4 Corner4D = FVector4(Corner.X, Corner.Y, Corner.Z, 1.0f);
			FVector4 WarpedCorner4D = Corner4D * LSLightViewProj;

			// CRITICAL: Perspective division (divide by w)
			// LiSPSM perspective matrix puts depth-dependent value in w
			if (FMath::Abs(WarpedCorner4D.W) > 1e-6f)
			{
				FVector WarpedCorner = FVector(
					WarpedCorner4D.X / WarpedCorner4D.W,
					WarpedCorner4D.Y / WarpedCorner4D.W,
					WarpedCorner4D.Z / WarpedCorner4D.W);

				MinX = FMath::Min(MinX, WarpedCorner.X);
				MaxX = FMath::Max(MaxX, WarpedCorner.X);
				MinY = FMath::Min(MinY, WarpedCorner.Y);
				MaxY = FMath::Max(MaxY, WarpedCorner.Y);
				MinZ = FMath::Min(MinZ, WarpedCorner.Z);
				MaxZ = FMath::Max(MaxZ, WarpedCorner.Z);
			}
		}

		// === Step 8: Fit to unit cube using orthographic projection ===
		// DirectX uses Left-handed coordinate system with Z=[0,1]
		// Create off-center orthographic projection manually
		float Width = MaxX - MinX;
		float Height = MaxY - MinY;
		float Depth = MaxZ - MinZ;

		FMatrix FitToUnitCube = FMatrix::OrthoLH(Width, Height, MinZ, MaxZ);

		// Apply center offset
		float CenterX = (MaxX + MinX) * 0.5f;
		float CenterY = (MaxY + MinY) * 0.5f;
		FitToUnitCube.M[3][0] -= CenterX * FitToUnitCube.M[0][0];
		FitToUnitCube.M[3][1] -= CenterY * FitToUnitCube.M[1][1];

		// === Step 9: Final matrices ===
		Result.View = LSLightView;
		Result.Projection = LiSPSMProj * FitToUnitCube;
		Result.ViewProjection = Result.View * Result.Projection;

		return Result;
	}

public:
	// Hybrid Light-space Perspective Shadow Map (TSM + OpenGL LiSPSM)
	// Adaptively chooses the best algorithm based on light/camera angle:
	// - Parallel angles (수평): Uses TSM (weighted AABB) - better when light and camera are aligned
	// - Perpendicular angles (수직): Uses OpenGL LiSPSM (perspective warping) - better at oblique angles
	//
	// This hybrid approach combines the strengths of both methods to provide
	// robust shadow quality across all viewing angles.
	//
	// @param Direction - Light direction (normalized or not)
	// @param FrustumCorners - 8 world-space corners of the view frustum
	// @param CameraView - Camera's view matrix
	// @param CameraProjection - Camera's projection matrix
	// @param ShadowExtension - Shadow range extension ratio (used by TSM only, default 0.2 = 20%)
	// @return Shadow view-projection matrices
	static FShadowViewProjection CreateForDirectionalLight_LiSPSM(
		const FVector& Direction,
		const FMatrix& CameraView,
		const FMatrix& CameraProjection,
		float ShadowExtension = 0.2f)
	{
		// Extract camera frustum corners
		TArray<FVector> FrustumCorners;
		FMatrix InvView = CameraView.InverseAffine();
		FMatrix InvProj = CameraProjection.InversePerspectiveProjection();
		FMatrix InvViewProj = InvProj * InvView;
		GetFrustumCornersWorldSpace(InvViewProj, FrustumCorners);

		// Calculate view direction from camera
		// DirectX row-major: view matrix's 3rd row is the forward direction
		FVector ViewDir = FVector(-CameraView.M[0][2], -CameraView.M[1][2], -CameraView.M[2][2]);
		ViewDir.Normalize();

		FVector LightDir = Direction.GetNormalized();

		// Calculate angle between view and light direction
		// AbsCosAngle close to 1 = parallel (수평), close to 0 = perpendicular (수직)
		float AbsCosAngle = FMath::Abs(FVector::Dot(ViewDir, LightDir));

		// Threshold decision:
		// AbsCosAngle < threshold: use OpenGL LiSPSM (perpendicular, 수직)
		// AbsCosAngle >= threshold: use TSM (parallel, 수평)
		// Higher threshold = use PSM more often (better quality)
		const float ParallelThreshold = 0.8f;  // ~37 degree angle

		FShadowViewProjection Result;

		if (AbsCosAngle < ParallelThreshold)
		{
			// More perpendicular (수직, AbsCosAngle → 0.0) - use OpenGL LiSPSM for better quality
			Result = CreateForDirectionalLight_OpenGLLiSPSM_Internal(Direction, FrustumCorners, CameraView, CameraProjection);
			Result.bUsedTSM = false;  // OpenGL LiSPSM 사용
		}
		else
		{
			// More parallel (수평, AbsCosAngle → 1.0) - use TSM for better stability
			// Prevents flickering in dueling frusta situations
			Result = CreateForDirectionalLight_TSM_Internal(Direction, FrustumCorners, CameraView, CameraProjection, ShadowExtension);
			Result.bUsedTSM = true;  // TSM 사용
		}

		return Result;
	}

	// Hybrid Light-space Perspective Shadow Map (TSM + OpenGL LiSPSM) - CSM variant
	// Same as above but accepts frustum corners directly (for CSM cascades)
	//
	// @param Direction - Light direction (normalized or not)
	// @param FrustumCorners - 8 world-space corners of the view frustum (or cascade)
	// @param CameraView - Camera's view matrix
	// @param CameraProjection - Camera's projection matrix
	// @param ShadowExtension - Shadow range extension ratio (used by TSM only, default 0.2 = 20%)
	// @return Shadow view-projection matrices
	static FShadowViewProjection CreateForDirectionalLight_LiSPSM_FromCorners(
		const FVector& Direction,
		const TArray<FVector>& FrustumCorners,
		const FMatrix& CameraView,
		const FMatrix& CameraProjection,
		float ShadowExtension = 0.2f)
	{
		// Calculate view direction from camera
		FVector ViewDir = FVector(-CameraView.M[0][2], -CameraView.M[1][2], -CameraView.M[2][2]);
		ViewDir.Normalize();

		FVector LightDir = Direction.GetNormalized();

		// Calculate angle between view and light direction
		float AbsCosAngle = FMath::Abs(FVector::Dot(ViewDir, LightDir));

		// Threshold decision
		// AbsCosAngle < threshold: use OpenGL LiSPSM (perpendicular, 수직)
		// AbsCosAngle >= threshold: use TSM (parallel, 수평)
		// Higher threshold = use PSM more often (better quality)
		const float ParallelThreshold = 0.85f;  // ~37 degree angle

		FShadowViewProjection Result;

		if (AbsCosAngle < ParallelThreshold)
		{
			// More perpendicular (수직, AbsCosAngle → 0.0) - use OpenGL LiSPSM for better quality
			Result = CreateForDirectionalLight_OpenGLLiSPSM_Internal(Direction, FrustumCorners, CameraView, CameraProjection);
			Result.bUsedTSM = false;  // OpenGL LiSPSM 사용
		}
		else
		{
			// More parallel (수평, AbsCosAngle → 1.0) - use TSM for better stability
			// Prevents flickering in dueling frusta situations
			Result = CreateForDirectionalLight_TSM_Internal(Direction, FrustumCorners, CameraView, CameraProjection, ShadowExtension);
			Result.bUsedTSM = true;  // TSM 사용
		}

		return Result;
	}

	// PointLight용 Cube Map Shadow VP 행렬 6개 생성
	// @param Position - 라이트 월드 위치
	// @param AttenuationRadius - 라이트 감쇠 반경 (Far plane으로 사용)
	// @param NearPlane - Near clipping plane (기본값 0.1f)
	// @return 6개의 VP 행렬 배열
	static TArray<FShadowViewProjection> CreateForPointLightCube(
		const FVector& Position,
		float AttenuationRadius,
		float NearPlane = 0.01f)
	{
		TArray<FShadowViewProjection> Results;
		Results.Reserve(6);

		// Cube Map의 6개 면에 대한 방향 및 Up 벡터
		// DirectX Cube Map Face 순서: +X, -X, +Y, -Y, +Z, -Z
		struct FCubeFace
		{
			FVector Direction;
			FVector Up;
		};

		// Cube Map Face 정의
		FCubeFace CubeFaces[6] =
		{
			{ FVector( 1,  0,  0), FVector(0,  1,  0) },  // Slice 0: +X (Right)
			{ FVector(-1,  0,  0), FVector(0,  1,  0) },  // Slice 1: -X (Left)
			{ FVector( 0,  1,  0), FVector(0,  0,  -1) }, // Slice 2: +Y (Up)
			{ FVector( 0, -1,  0), FVector(0,  0,  1) }, // Slice 3: -Y (Down)
			{ FVector( 0,  0,  1), FVector(0,  1,  0) },  // Slice 4: +Z (Forward)                       
			{ FVector( 0,  0, -1), FVector(0,  1,  0) }   // Slice 5: -Z (Back)
		};

		// 각 면에 대한 VP 행렬 생성
		for (int i = 0; i < 6; i++)
		{
			FShadowViewProjection VP;

			// View 행렬 생성
			VP.View = FMatrix::LookAtLH(
				Position,
				Position + CubeFaces[i].Direction,
				CubeFaces[i].Up);

			// Projection 행렬 생성 (90도 FOV, 정사각형 aspect ratio)
			VP.Projection = FMatrix::PerspectiveFovLH(
				DegreesToRadians(90.0f),  // 90도 FOV
				1.0f,                      // 정사각형 aspect ratio
				NearPlane,
				AttenuationRadius);

			// ViewProjection 계산
			VP.ViewProjection = VP.View * VP.Projection;

			Results.Add(VP);
		}

		return Results;
	}

};
