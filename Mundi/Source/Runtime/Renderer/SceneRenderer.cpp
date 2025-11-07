#include "pch.h"
#include "SceneRenderer.h"

// FSceneRenderer가 사용하는 모든 헤더 포함
#include "World.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Renderer.h"
#include "RHIDevice.h"
#include "PrimitiveComponent.h"
#include "DecalComponent.h"
#include "StaticMeshActor.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "RenderSettings.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "SelectionManager.h"
#include "StaticMeshComponent.h"
#include "DecalStatManager.h"
#include "BillboardComponent.h"
#include "TextRenderComponent.h"
#include "OBB.h"
#include "BoundingSphere.h"
#include "HeightFogComponent.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "DirectionalLightComponent.h"
#include "AmbientLightComponent.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "SwapGuard.h"
#include "MeshBatchElement.h"
#include "SceneView.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "TileLightCuller.h"
#include "LineComponent.h"
#include "ShadowManager.h"
#include "CollisionManager.h"
#include "ShadowViewProjection.h"
#include "CollisionComponent/ShapeComponent.h"
#include "GravityWall.h"
#include "GameModeBase.h"
#include "PlayerController.h"
#include "PlayerCameraManager.h"

// RenderLetterBoxPass 관련
#include "PlayerController.h"
#include "PlayerCameraManager.h"
#include "ViewTarget.h"
#include "GameModeBase.h"
#include "UCameraModifier_Vignetting.h"

FSceneRenderer::FSceneRenderer(UWorld* InWorld, FSceneView* InView, URenderer* InOwnerRenderer)
	: World(InWorld)
	, View(InView) // 전달받은 FSceneView 저장
	, OwnerRenderer(InOwnerRenderer)
	, RHIDevice(InOwnerRenderer->GetRHIDevice())
{
	//OcclusionCPU = std::make_unique<FOcclusionCullingManagerCPU>();

	// 타일 라이트 컬러 초기화
	TileLightCuller = std::make_unique<FTileLightCuller>();
	uint32 TileSize = World->GetRenderSettings().GetTileSize();
	TileLightCuller->Initialize(RHIDevice, TileSize);

	// 라인 수집 시작
	OwnerRenderer->BeginLineBatch();
}

FSceneRenderer::~FSceneRenderer()
{
}

//====================================================================================
// 메인 렌더 함수
//====================================================================================
void FSceneRenderer::Render()
{
	if (!IsValid()) return;

	// 뷰(View) 준비: 행렬, 절두체 등 프레임에 필요한 기본 데이터 계산
	PrepareView();
	// 렌더링할 대상 수집 (Cull + Gather)
	GatherVisibleProxies();

	// ViewMode에 따라 렌더링 경로 결정
	if (View->ViewMode == EViewModeIndex::VMI_Lit ||
		View->ViewMode == EViewModeIndex::VMI_Lit_Gouraud ||
		View->ViewMode == EViewModeIndex::VMI_Lit_Lambert ||
		View->ViewMode == EViewModeIndex::VMI_Lit_Phong)
	{
		// 1. 섀도우 패스: 라이트 관점에서 depth 렌더링
		RenderShadowPass();

		// 2. 라이트 버퍼 업데이트 및 섀도우 맵 바인딩
		GWorld->GetLightManager()->SetDirtyFlag();
		GWorld->GetLightManager()->UpdateLightBuffer(RHIDevice);	// 라이트 구조체 버퍼 업데이트, 바인딩
		GWorld->GetShadowManager()->BindShadowResources(RHIDevice);	// 섀도우 맵 텍스처 바인딩 (t5)
		GWorld->GetShadowManager()->UpdateShadowFilterBuffer(RHIDevice);	// 섀도우 필터링 설정 업데이트

		PerformTileLightCulling();	// 타일 기반 라이트 컬링 수행

		// 3. 메인 렌더링
		RenderLitPath();
		RenderPostProcessingPasses();	// 후처리 체인 실행
		RenderTileCullingDebug();	// 타일 컬링 디버그 시각화 draw
	}
	else if (View->ViewMode == EViewModeIndex::VMI_Unlit)
	{
		RenderLitPath();	// Unlit 모드는 조명 없이 렌더링
	}
	else if (View->ViewMode == EViewModeIndex::VMI_WorldNormal)
	{
		RenderLitPath();	// World Normal 시각화 모드
	}
	else if (View->ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderWireframePath();
	}
	else if (View->ViewMode == EViewModeIndex::VMI_SceneDepth)
	{
		RenderSceneDepthPath();
	}

	//그리드와 디버그용 Primitive는 Post Processing 적용하지 않음.
	RenderEditorPrimitivesPass();	// 빌보드, 기타 화살표 출력 (상호작용, 피킹 O)
	RenderDebugPass();	//  그리드, 선택한 물체의 경계 출력 (상호작용, 피킹 X)

	// 오버레이(Overlay) Primitive 렌더링
	RenderOverayEditorPrimitivesPass();	// 기즈모 출력

	RenderVignettingPass(); // Vignetting 처리
	// 레터박스 렌더링 (PIE 모드에서만)
	RenderLetterBoxPass();

	RenderFadeInOutPass();	// FadeInOut 처리
	
	// FXAA 등 화면에서 최종 이미지 품질을 위해 적용되는 효과를 적용
	ApplyScreenEffectsPass();

	// 최종적으로 Scene에 그려진 텍스쳐를 Back 버퍼에 그힌다
	CompositeToBackBuffer();
}

//====================================================================================
// Render Path 함수 구현
//====================================================================================

void FSceneRenderer::RenderLitPath()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// Base Pass
	RenderOpaquePass(View->ViewMode);
	RenderDecalPass();
}

void FSceneRenderer::RenderWireframePath()
{
	// 깊이 버퍼 초기화 후 ID만 그리기
	RHIDevice->RSSetState(ERasterizerMode::Solid);
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneIdTarget);
	RenderOpaquePass(EViewModeIndex::VMI_Unlit);

	// Wireframe으로 그리기
	RHIDevice->ClearDepthBuffer(1.0f, 0);
	RHIDevice->RSSetState(ERasterizerMode::Wireframe);
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTarget);
	RenderOpaquePass(EViewModeIndex::VMI_Unlit);

	// 상태 복구
	RHIDevice->RSSetState(ERasterizerMode::Solid);
}

void FSceneRenderer::RenderSceneDepthPath()
{
	// ✅ 디버그: SceneRTV 전환 전 viewport 확인
	D3D11_VIEWPORT vpBefore;
	UINT numVP = 1;
	RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpBefore);
	UE_LOG("[RenderSceneDepthPath] BEFORE OMSetRenderTargets(Scene): Viewport(%.1f x %.1f) at (%.1f, %.1f)",
		vpBefore.Width, vpBefore.Height, vpBefore.TopLeftX, vpBefore.TopLeftY);

	// 1. Scene RTV와 Depth Buffer Clear
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// ✅ 디버그: SceneRTV 전환 후 viewport 확인
	D3D11_VIEWPORT vpAfter;
	RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpAfter);
	UE_LOG("[RenderSceneDepthPath] AFTER OMSetRenderTargets(Scene): Viewport(%.1f x %.1f) at (%.1f, %.1f)",
		vpAfter.Width, vpAfter.Height, vpAfter.TopLeftX, vpAfter.TopLeftY);

	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetCurrentTargetRTV(), ClearColor);
	RHIDevice->ClearDepthBuffer(1.0f, 0);

	// 2. Base Pass - Scene에 메시 그리기
	RenderOpaquePass(EViewModeIndex::VMI_Unlit);

	// ✅ 디버그: BackBuffer 전환 전 viewport 확인
	RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpBefore);
	UE_LOG("[RenderSceneDepthPath] BEFORE OMSetRenderTargets(BackBuffer): Viewport(%.1f x %.1f)",
		vpBefore.Width, vpBefore.Height);

	// 3. BackBuffer Clear
	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetBackBufferRTV(), ClearColor);

	// ✅ 디버그: BackBuffer 전환 후 viewport 확인
	RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpAfter);
	UE_LOG("[RenderSceneDepthPath] AFTER OMSetRenderTargets(BackBuffer): Viewport(%.1f x %.1f)",
		vpAfter.Width, vpAfter.Height);

	// 4. SceneDepth Post 프로세싱 처리
	RenderSceneDepthPostProcess();

}

//====================================================================================
// Private 헬퍼 함수 구현
//====================================================================================

bool FSceneRenderer::IsValid() const
{
	return World && View && OwnerRenderer && RHIDevice;
}

void FSceneRenderer::PrepareView()
{
	OwnerRenderer->SetCurrentViewportSize(View->ViewRect.Width(), View->ViewRect.Height());

	// FSceneRenderer 멤버 변수(View->ViewMatrix, View->ProjectionMatrix)를 채우는 대신
	// FSceneView의 멤버를 직접 사용합니다. (예: RenderOpaquePass에서 View->ViewMatrix 사용)

	// 뷰포트 크기 설정
	D3D11_VIEWPORT Vp = {};
	Vp.TopLeftX = (float)View->ViewRect.MinX;
	Vp.TopLeftY = (float)View->ViewRect.MinY;
	Vp.Width = (float)View->ViewRect.Width();
	Vp.Height = (float)View->ViewRect.Height();
	Vp.MinDepth = 0.0f;
	Vp.MaxDepth = 1.0f;
	RHIDevice->GetDeviceContext()->RSSetViewports(1, &Vp);

	// 뷰포트 상수 버퍼 설정 (View->ViewRect, RHIDevice 크기 정보 사용)
	FViewportConstants ViewConstData;
	// 1. 뷰포트 정보 채우기
	ViewConstData.ViewportRect.X = Vp.TopLeftX;
	ViewConstData.ViewportRect.Y = Vp.TopLeftY;
	ViewConstData.ViewportRect.Z = Vp.Width;
	ViewConstData.ViewportRect.W = Vp.Height;
	// 2. 전체 화면(렌더 타겟) 크기 정보 채우기
	ViewConstData.ScreenSize.X = static_cast<float>(RHIDevice->GetViewportWidth());
	ViewConstData.ScreenSize.Y = static_cast<float>(RHIDevice->GetViewportHeight());
	ViewConstData.ScreenSize.Z = 1.0f / RHIDevice->GetViewportWidth();
	ViewConstData.ScreenSize.W = 1.0f / RHIDevice->GetViewportHeight();
	RHIDevice->SetAndUpdateConstantBuffer((FViewportConstants)ViewConstData);

	// 공통 상수 버퍼 설정 (View, Projection 등) - 루프 전에 한 번만
	FVector CameraPos = View->ViewLocation;

	FMatrix InvView = View->ViewMatrix.InverseAffine();
	FMatrix InvProjection;
	if (View->ProjectionMode == ECameraProjectionMode::Perspective)
	{
		InvProjection = View->ProjectionMatrix.InversePerspectiveProjection();
	}
	else
	{
		InvProjection = View->ProjectionMatrix.InverseOrthographicProjection();
	}

	ViewProjBufferType ViewProjBuffer = ViewProjBufferType(View->ViewMatrix, View->ProjectionMatrix, InvView, InvProjection);

	RHIDevice->SetAndUpdateConstantBuffer(ViewProjBufferType(ViewProjBuffer));
	RHIDevice->SetAndUpdateConstantBuffer(CameraBufferType(CameraPos, 0.0f));
}

void FSceneRenderer::GatherVisibleProxies()
{
	// NOTE: 일단 컴포넌트 단위와 데칼 관련 이슈 해결까지 컬링 무시
	//// 절두체 컬링 수행 -> 결과가 멤버 변수 PotentiallyVisibleActors에 저장됨
	//PerformFrustumCulling();

	const bool bDrawStaticMeshes = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
	const bool bDrawDecals = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Decals);
	const bool bDrawFog = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Fog);
	const bool bDrawLight = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Lighting);
	const bool bUseAntiAliasing = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);
	const bool bUseBillboard = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Billboard);

	// 현재 뷰포트에서 piloting 중인 액터 확인
	FViewportClient* ViewportClient = View->Viewport ? View->Viewport->GetViewportClient() : nullptr;
	AActor* PilotingActor = ViewportClient ? ViewportClient->GetPilotActor() : nullptr;

	// Helper lambda to collect components from an actor
	auto CollectComponentsFromActor = [&](AActor* Actor, bool bIsEditorActor)
		{
			if (!Actor || !Actor->IsActorVisible())
			{
				return;
			}

			for (USceneComponent* Component : Actor->GetSceneComponents())
			{
				if (!Component || !Component->IsVisible())
				{
					continue;
				}

				// 엔진 에디터 액터 컴포넌트
				if (bIsEditorActor)
				{
					if (UGizmoArrowComponent* GizmoComponent = Cast<UGizmoArrowComponent>(Component))
					{
						Proxies.OverlayPrimitives.Add(GizmoComponent);
					}
					else if (ULineComponent* LineComponent = Cast<ULineComponent>(Component))
					{
						Proxies.EditorLines.Add(LineComponent);
					}

					continue;
				}

				if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component); PrimitiveComponent)
				{
					// 에디터 보조 컴포넌트 (빌보드, 방향 화살표 등)
					// 현재 뷰포트에서 piloting 중인 액터의 에디터 컴포넌트는 제외
					if (!PrimitiveComponent->IsEditable())
					{
						// Piloting 중인 액터의 에디터 헬퍼는 현재 뷰포트에서 숨김
						if (PilotingActor && Actor == PilotingActor)
						{
							continue;
						}

						Proxies.EditorPrimitives.Add(PrimitiveComponent);
						continue;
					}

					// 일반 컴포넌트
					if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(PrimitiveComponent))
					{
						bool bShouldAdd = true;

						// 메시 타입이 '스태틱 메시'인 경우에만 ShowFlag를 검사하여 추가 여부를 결정
						if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
						{
							bShouldAdd = bDrawStaticMeshes;
						}
						// else if (USkeletalMeshComponent* SkeletalMeshComponent = ...)
						// {
						//     bShouldAdd = bDrawSkeletalMeshes;
						// }

						if (bShouldAdd)
						{
							Proxies.Meshes.Add(MeshComponent);
						}
					}
					else if (UBillboardComponent* BillboardComponent = Cast<UBillboardComponent>(PrimitiveComponent); BillboardComponent && bUseBillboard)
					{
						Proxies.Billboards.Add(BillboardComponent);
					}
					else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(PrimitiveComponent); DecalComponent && bDrawDecals)
					{
						Proxies.Decals.Add(DecalComponent);
					}
				}
				else
				{
					if (UHeightFogComponent* FogComponent = Cast<UHeightFogComponent>(Component); FogComponent && bDrawFog)
					{
						SceneGlobals.Fogs.Add(FogComponent);
					}

					else if (UDirectionalLightComponent* LightComponent = Cast<UDirectionalLightComponent>(Component); LightComponent && bDrawLight)
					{
						SceneGlobals.DirectionalLights.Add(LightComponent);
					}

					else if (UAmbientLightComponent* LightComponent = Cast<UAmbientLightComponent>(Component); LightComponent && bDrawLight)
					{
						SceneGlobals.AmbientLights.Add(LightComponent);
					}

					else if (UPointLightComponent* LightComponent = Cast<UPointLightComponent>(Component); LightComponent && bDrawLight)
					{
						if (USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(LightComponent); SpotLightComponent)
						{
							SceneLocals.SpotLights.Add(SpotLightComponent);
						}
						else
						{
							SceneLocals.PointLights.Add(LightComponent);
						}
					}
				}
			}
		};

	// Collect from Editor Actors (Gizmo, Grid, etc.)
	// 현재 뷰포트에서 piloting 중인 액터의 기즈모는 숨김
	AGizmoActor* GizmoActor = World->GetGizmoActor();
	USelectionManager* SelectionManager = World->GetSelectionManager();
	bool bHideGizmo = false;

	// 현재 뷰포트가 선택된 액터를 piloting 중이면 기즈모 숨김
	if (GizmoActor && SelectionManager && PilotingActor)
	{
		AActor* SelectedActor = SelectionManager->GetSelectedActor();
		if (SelectedActor && SelectedActor == PilotingActor)
		{
			bHideGizmo = true;
		}
	}

	for (AActor* EditorActor : World->GetEditorActors())
	{
		// 현재 뷰포트에서 piloting 중이면 Gizmo Actor 수집하지 않음
		if (bHideGizmo && EditorActor == GizmoActor)
		{
			continue;
		}

		CollectComponentsFromActor(EditorActor, true);
	}

	// Collect from Level Actors (including their Gizmo components)
	for (AActor* Actor : World->GetActors())
	{
		CollectComponentsFromActor(Actor, false);
	}
}

void FSceneRenderer::PerformTileLightCulling()
{
	if (!TileLightCuller)
		return;

	// ShowFlag 확인
	URenderSettings& RenderSettings = World->GetRenderSettings();
	bool bTileCullingEnabled = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCulling);

	// 뷰포트 크기 가져오기
	UINT ViewportWidth = static_cast<UINT>(View->ViewRect.Width());
	UINT ViewportHeight = static_cast<UINT>(View->ViewRect.Height());

	// 타일 컬링이 활성화된 경우에만 컬링 수행
	if (bTileCullingEnabled)
	{
		// PointLight와 SpotLight 정보 수집
		TArray<FPointLightInfo>& PointLights = GWorld->GetLightManager()->GetPointLightInfoList();
		TArray<FSpotLightInfo>& SpotLights = GWorld->GetLightManager()->GetSpotLightInfoList();

		// 타일 컬링 수행
		TileLightCuller->CullLights(
			PointLights,
			SpotLights,
			View->ViewMatrix,
			View->ProjectionMatrix,
			View->ZNear,
			View->ZFar,
			ViewportWidth,
			ViewportHeight
		);

		// 통계를 전역 매니저에 업데이트
		FTileCullingStatManager::GetInstance().UpdateStats(TileLightCuller->GetStats());
	}

	// 타일 컬링 상수 버퍼 업데이트
	uint32 TileSize = RenderSettings.GetTileSize();
	FTileCullingBufferType TileCullingBuffer;
	TileCullingBuffer.TileSize = TileSize;
	TileCullingBuffer.TileCountX = (ViewportWidth + TileSize - 1) / TileSize;
	TileCullingBuffer.TileCountY = (ViewportHeight + TileSize - 1) / TileSize;
	TileCullingBuffer.bUseTileCulling = bTileCullingEnabled ? 1 : 0;  // ShowFlag에 따라 설정

	RHIDevice->SetAndUpdateConstantBuffer(TileCullingBuffer);

	// Structured Buffer SRV를 t2 슬롯에 바인딩 (타일 컬링 활성화 시에만)
	if (bTileCullingEnabled)
	{
		ID3D11ShaderResourceView* TileLightIndexSRV = TileLightCuller->GetLightIndexBufferSRV();
		if (TileLightIndexSRV)
		{
			RHIDevice->GetDeviceContext()->PSSetShaderResources(2, 1, &TileLightIndexSRV);
		}
	}
}

void FSceneRenderer::RenderShadowPass()
{
	// Step 0: 쉐도우 맵 리소스 언바인딩 (렌더 타겟으로 사용하기 전에 필수!)
	GWorld->GetShadowManager()->UnbindShadowResources(RHIDevice);

	// Step 1: 섀도우 캐스팅 라이트에 인덱스 할당
	FShadowCastingLights ShadowLights(SceneGlobals.DirectionalLights, SceneLocals.SpotLights, SceneLocals.PointLights);
	GWorld->GetShadowManager()->AssignShadowMapIndices(RHIDevice, ShadowLights);

	// Step 2: 섀도우 뎁스 셰이더 로드 및 컴파일
	UShader* ShadowDepthShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Materials/ShadowDepth.hlsl");
	if (!ShadowDepthShader)
	{
		UE_LOG("RenderShadowPass: Failed to load ShadowDepth shader");
		return;
	}

	TArray<FShaderMacro> EmptyMacros;
	FShaderVariant* ShadowShaderVariant = ShadowDepthShader->GetOrCompileShaderVariant(RHIDevice->GetDevice(), EmptyMacros);
	if (!ShadowShaderVariant)
	{
		UE_LOG("RenderShadowPass: Failed to compile ShadowDepth shader");
		return;
	}

	// Step 3: 섀도우 필터 타입 가져오기
	EShadowFilterType FilterType = GWorld->GetShadowManager()->GetShadowConfiguration().FilterType;

	// Step 4: 렌더 상태 저장 (RAII 패턴)
	FSavedRenderState SavedState;
	SavedState.Save(RHIDevice);

	// Step 5: 라이트 타입별 섀도우 렌더링
	RenderDirectionalLightShadows(ShadowShaderVariant);
	RenderSpotLightShadows(ShadowShaderVariant);
	RenderPointLightShadows(ShadowShaderVariant);

	// Step 6: 렌더 상태 복구
	SavedState.Restore(RHIDevice);

	// Step 7: 카메라 ViewProj 버퍼 복구
	RestoreCameraViewProj();
}

//====================================================================================
// Shadow Pass Helper Functions
//====================================================================================

void FSceneRenderer::CollectShadowMeshBatches(TArray<FMeshBatchElement>& OutMeshBatches) const
{
	for (UMeshComponent* MeshComponent : Proxies.Meshes)
	{
		MeshComponent->CollectMeshBatches(OutMeshBatches, View);
	}
}

void FSceneRenderer::OverrideShadowShader(TArray<FMeshBatchElement>& MeshBatches, FShaderVariant* ShadowShaderVariant, EShadowFilterType FilterType)
{
	// VS는 항상 ShadowDepthShader의 VS 사용
	ID3D11VertexShader* VS = ShadowShaderVariant->VertexShader;
	ID3D11InputLayout* IL = ShadowShaderVariant->InputLayout;

	// PS는 필터 타입에 따라 선택
	UShader* PSShader = GWorld->GetShadowManager()->GetShadowPixelShaderForFilterType(FilterType);
	ID3D11PixelShader* PS = PSShader ? PSShader->GetPixelShader() : nullptr;  // NONE/PCF는 nullptr

	for (FMeshBatchElement& BatchElement : MeshBatches)
	{
		BatchElement.VertexShader = VS;
		BatchElement.PixelShader = PS;
		BatchElement.InputLayout = IL;
	}
}

void FSceneRenderer::UpdateViewProjBufferForShadow(const FShadowRenderContext& ShadowContext, bool bIsOrthographic)
{
	ViewProjBufferType ViewProjBuffer;
	ViewProjBuffer.View = ShadowContext.LightView;
	ViewProjBuffer.Proj = ShadowContext.LightProjection;
	ViewProjBuffer.InvView = ShadowContext.LightView.InverseAffine();

	if (bIsOrthographic)
	{
		ViewProjBuffer.InvProj = ShadowContext.LightProjection.InverseOrthographicProjection();
	}
	else
	{
		ViewProjBuffer.InvProj = ShadowContext.LightProjection.InversePerspectiveProjection();
	}

	RHIDevice->SetAndUpdateConstantBuffer(ViewProjBuffer);
}

void FSceneRenderer::RenderDirectionalLightShadows(FShaderVariant* ShadowShaderVariant)
{
	FShadowManager* ShadowManager = GWorld->GetShadowManager();
	const FShadowConfiguration& ShadowConfig = ShadowManager->GetShadowConfiguration();

	for (UDirectionalLightComponent* DirLight : SceneGlobals.DirectionalLights)
	{
		// 유효성 검사
		if (!IsLightValidForShadowCasting(DirLight))
			continue;

		// CSM이 활성화되어 있으면 Cascaded Shadow Maps 렌더링
		if (DirLight->GetShadowMapType() == EShadowMapType::CSM)
		{
			// 1. CSM Split 거리 계산 (DirectionalLight의 설정 사용)
			const int32 NumCascades = DirLight->GetNumCascades();
			const float CSMLambda = DirLight->GetCSMLambda();
			const float MaxShadowDistance = 1000.0f; // TODO: 필요시 DirectionalLight 프로퍼티로 추가

			TArray<float> CascadeSplits = ShadowManager->CalculateCSMSplits(
				View->ZNear,
				FMath::Min(View->ZFar, MaxShadowDistance),
				NumCascades,
				CSMLambda);

			// 2. 각 캐스케이드에 대해 섀도우 렌더링
			float PrevSplit = View->ZNear;
			for (int CascadeIndex = 0; CascadeIndex < NumCascades; ++CascadeIndex)
			{
				float CurrentSplit = CascadeSplits[CascadeIndex];

				// ShadowManager에게 CSM 섀도우 맵 렌더 시작 요청
				FShadowRenderContext ShadowContext;
				if (!ShadowManager->BeginShadowRenderCSM(RHIDevice, DirLight,
					View->ViewMatrix, View->ProjectionMatrix,
					CascadeIndex, PrevSplit, CurrentSplit, ShadowContext))
				{
					PrevSplit = CurrentSplit;
					continue;
				}

				// ViewProj 버퍼 업데이트 (Orthographic)
				UpdateViewProjBufferForShadow(ShadowContext, true);

				// 메시 수집
				TArray<FMeshBatchElement> ShadowMeshBatches;
				CollectShadowMeshBatches(ShadowMeshBatches);

				// 셰이더 오버라이드
				OverrideShadowShader(ShadowMeshBatches, ShadowShaderVariant, ShadowConfig.FilterType);

				// 그리기
				DrawMeshBatches(ShadowMeshBatches, true, true);

				// 섀도우 맵 렌더 종료
				ShadowManager->EndShadowRender(RHIDevice);

				// 다음 캐스케이드를 위한 준비
				PrevSplit = CurrentSplit;
			}
		}
		else
		{
			// CSM이 비활성화된 경우 기존 단일 섀도우 맵 렌더링
			FShadowRenderContext ShadowContext;
			if (!ShadowManager->BeginShadowRender(RHIDevice, DirLight,
				View->ViewMatrix, View->ProjectionMatrix, ShadowContext))
				continue;

			// ViewProj 버퍼 업데이트 (Orthographic)
			UpdateViewProjBufferForShadow(ShadowContext, true);

			// 메시 수집
			TArray<FMeshBatchElement> ShadowMeshBatches;
			CollectShadowMeshBatches(ShadowMeshBatches);

			// 셰이더 오버라이드
			OverrideShadowShader(ShadowMeshBatches, ShadowShaderVariant, ShadowConfig.FilterType);

			// 그리기
			DrawMeshBatches(ShadowMeshBatches, true, true);

			// 섀도우 맵 렌더 종료
			ShadowManager->EndShadowRender(RHIDevice);
		}
	}
}

void FSceneRenderer::RenderSpotLightShadows(FShaderVariant* ShadowShaderVariant)
{
	FShadowManager* ShadowManager = GWorld->GetShadowManager();
	const FShadowConfiguration& ShadowConfig = ShadowManager->GetShadowConfiguration();

	for (USpotLightComponent* SpotLight : SceneLocals.SpotLights)
	{
		// 유효성 검사
		if (!IsLightValidForShadowCasting(SpotLight))
			continue;

		// ShadowManager에게 섀도우 맵 렌더 시작 요청
		FShadowRenderContext ShadowContext;
		if (!ShadowManager->BeginShadowRender(RHIDevice, SpotLight, ShadowContext))
			continue;

		// ViewProj 버퍼 업데이트 (Perspective)
		UpdateViewProjBufferForShadow(ShadowContext, false);

		// 메시 수집
		TArray<FMeshBatchElement> ShadowMeshBatches;
		CollectShadowMeshBatches(ShadowMeshBatches);

		// 셰이더 오버라이드
		OverrideShadowShader(ShadowMeshBatches, ShadowShaderVariant, ShadowConfig.FilterType);

		// 그리기
		DrawMeshBatches(ShadowMeshBatches, true, true);

		// 섀도우 맵 렌더 종료
		GWorld->GetShadowManager()->EndShadowRender(RHIDevice);
	}
}

void FSceneRenderer::RenderPointLightShadows(FShaderVariant* ShadowShaderVariant)
{
	FShadowManager* ShadowManager = GWorld->GetShadowManager();
	const FShadowConfiguration& ShadowConfig = ShadowManager->GetShadowConfiguration();

	for (UPointLightComponent* PointLight : SceneLocals.PointLights)
	{
		// 유효성 검사
		if (!IsLightValidForShadowCasting(PointLight))
			continue;

		// Shadow Map이 할당되지 않은 라이트는 스킵
		if (PointLight->GetShadowMapIndex() < 0)
			continue;

		// 라이트당 한 번만 6개 VP 행렬 계산
		TArray<FShadowViewProjection> CubeShadowVPs = FShadowViewProjection::CreateForPointLightCube(
			PointLight->GetWorldLocation(),
			PointLight->GetAttenuationRadius(),
			0.01f); // Near plane

		// 6개 면 렌더링 (+X, -X, +Y, -Y, +Z, -Z)
		for (uint32 CubeFaceIdx = 0; CubeFaceIdx < 6; CubeFaceIdx++)
		{
			// ShadowManager에게 섀도우 맵 렌더 시작 요청
			FShadowRenderContext ShadowContext;
			if (!ShadowManager->BeginShadowRenderCube(RHIDevice, PointLight, CubeFaceIdx, CubeShadowVPs[CubeFaceIdx], ShadowContext))
				continue;

			// ViewProj 버퍼 업데이트 (Perspective)
			UpdateViewProjBufferForShadow(ShadowContext, false);

			// 메시 수집
			TArray<FMeshBatchElement> ShadowMeshBatches;
			CollectShadowMeshBatches(ShadowMeshBatches);

			// 셰이더 오버라이드
			OverrideShadowShader(ShadowMeshBatches, ShadowShaderVariant, ShadowConfig.FilterType);

			// 그리기
			DrawMeshBatches(ShadowMeshBatches, true, true);

			// 섀도우 맵 렌더 종료
			GWorld->GetShadowManager()->EndShadowRender(RHIDevice);
		}
	}
}

void FSceneRenderer::RestoreCameraViewProj()
{
	FMatrix InvView = View->ViewMatrix.InverseAffine();
	FMatrix InvProjection;

	if (View->ProjectionMode == ECameraProjectionMode::Perspective)
	{
		InvProjection = View->ProjectionMatrix.InversePerspectiveProjection();
	}
	else
	{
		InvProjection = View->ProjectionMatrix.InverseOrthographicProjection();
	}

	ViewProjBufferType ViewProjBuffer = ViewProjBufferType(View->ViewMatrix, View->ProjectionMatrix, InvView, InvProjection);
	RHIDevice->SetAndUpdateConstantBuffer(ViewProjBufferType(ViewProjBuffer));
}

//====================================================================================
// FSavedRenderState Implementation
//====================================================================================

void FSceneRenderer::FSavedRenderState::Save(D3D11RHI* RHI)
{
	// 현재 렌더 타겟 및 뷰포트 저장
	RHI->GetDeviceContext()->OMGetRenderTargets(1, &RTV, &DSV);

	UINT NumViewports = 1;
	RHI->GetDeviceContext()->RSGetViewports(&NumViewports, &Viewport);
}

void FSceneRenderer::FSavedRenderState::Restore(D3D11RHI* RHI)
{
	// 렌더 타겟 및 뷰포트 복구
	RHI->GetDeviceContext()->OMSetRenderTargets(1, &RTV, DSV);
	RHI->GetDeviceContext()->RSSetViewports(1, &Viewport);

	// 릴리스
	Release();
}

void FSceneRenderer::FSavedRenderState::Release()
{
	if (RTV)
	{
		RTV->Release();
		RTV = nullptr;
	}
	if (DSV)
	{
		DSV->Release();
		DSV = nullptr;
	}
}

void FSceneRenderer::PerformFrustumCulling()
{
	PotentiallyVisibleComponents.clear();	// 할 필요 없는데 명목적으로 초기화

	// Todo: 프로스텀 컬링 수행, 추후 프로스텀 컬링이 컴포넌트 단위로 변경되면 적용

	//World->GetPartitionManager()->FrustumQuery(ViewFrustum)

	//for (AActor* Actor : World->GetActors())
	//{
	//	if (!Actor || Actor->GetActorHiddenInEditor()) continue;

	//	// 절두체 컬링을 통과한 액터만 목록에 추가
	//	if (ViewFrustum.Intersects(Actor->GetBounds()))
	//	{
	//		PotentiallyVisibleActors.Add(Actor);
	//	}
	//}
}

void FSceneRenderer::RenderOpaquePass(EViewModeIndex InRenderViewMode)
{
	TArray<FShaderMacro> ShaderMacros;
	FString ShaderPath = "Shaders/Materials/UberLit.hlsl";
	bool bNeedsShaderOverride = true; // 뷰 모드가 셰이더를 강제하는지 여부

	switch (InRenderViewMode)
	{
	case EViewModeIndex::VMI_Lit_Phong:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Gouraud:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_GOURAUD", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Lambert:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_LAMBERT", "1" });
		break;
	case EViewModeIndex::VMI_Unlit:
		// 매크로 없음 (Unlit)
		break;
	case EViewModeIndex::VMI_WorldNormal:
		ShaderMacros.push_back(FShaderMacro{ "VIEWMODE_WORLD_NORMAL", "1" });
		break;
	default:
		// 기본 Lit 모드 등, 셰이더를 강제하지 않는 모드는 여기서 처리 가능
		bNeedsShaderOverride = false; // 예시: 기본 Lit 모드는 머티리얼 셰이더 사용
		break;
	}

	// ViewMode에 맞는 셰이더 로드 (셰이더 오버라이드가 필요한 경우에만)
	FShaderVariant* ShaderVariant = nullptr;
	UShader* ViewModeShader = nullptr;
	if (bNeedsShaderOverride)
	{
		ViewModeShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath);
		ShaderVariant = ViewModeShader->GetOrCompileShaderVariant(RHIDevice->GetDevice(), ShaderMacros);
		if (!ShaderVariant)
		{
			// 필요시 기본 셰이더로 대체하거나 렌더링 중단
			UE_LOG("RenderOpaquePass: Failed to load ViewMode shader: %s", ShaderPath.c_str());
			return;
		}
	}

	// --- 1. 수집 (Collect) ---
	MeshBatchElements.Empty();
	for (UMeshComponent* MeshComponent : Proxies.Meshes)
	{
		MeshComponent->CollectMeshBatches(MeshBatchElements, View);
	}

	// --- UMeshComponent 셰이더 오버라이드 ---
	if (bNeedsShaderOverride && ShaderVariant)
	{
		// 수집된 UMeshComponent 배치 요소의 셰이더를 ViewModeShader로 강제 변경
		for (FMeshBatchElement& BatchElement : MeshBatchElements)
		{
			BatchElement.VertexShader = ShaderVariant->VertexShader;
			BatchElement.PixelShader = ShaderVariant->PixelShader;
			BatchElement.InputLayout = ShaderVariant->InputLayout;
		}
	}

	for (UBillboardComponent* BillboardComponent : Proxies.Billboards)
	{
		BillboardComponent->CollectMeshBatches(MeshBatchElements, View);
	}

	for (UTextRenderComponent* TextRenderComponent : Proxies.Texts)
	{
		// TODO: UTextRenderComponent도 CollectMeshBatches를 통해 FMeshBatchElement를 생성하도록 구현
		//TextRenderComponent->CollectMeshBatches(MeshBatchElements, View);
	}

	// --- 2. 정렬 (Sort) ---
	MeshBatchElements.Sort();

	// --- 3. 그리기 (Draw) ---
	DrawMeshBatches(MeshBatchElements, true);
}

void FSceneRenderer::RenderDecalPass()
{
	if (Proxies.Decals.empty())
		return;

	// WorldNormal 모드에서는 Decal 렌더링 스킵
	if (View->ViewMode == EViewModeIndex::VMI_WorldNormal)
		return;

	UWorldPartitionManager* Partition = World->GetPartitionManager();
	if (!Partition)
		return;

	const FBVHierarchy* BVH = Partition->GetBVH();
	if (!BVH)
		return;

	FDecalStatManager::GetInstance().AddTotalDecalCount(Proxies.Decals.Num());	// TODO: 추후 월드 컴포넌트 추가/삭제 이벤트에서 데칼 컴포넌트의 개수만 추적하도록 수정 필요
	FDecalStatManager::GetInstance().AddVisibleDecalCount(Proxies.Decals.Num());	// 그릴 Decal 개수 수집

	// ViewMode에 따라 조명 모델 매크로 설정
	TArray<FShaderMacro> ShaderMacros;
	FString ShaderPath = "Shaders/Effects/Decal.hlsl";

	switch (View->ViewMode)
	{
	case EViewModeIndex::VMI_Lit_Phong:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Gouraud:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_GOURAUD", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Lambert:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_LAMBERT", "1" });
		break;
	case EViewModeIndex::VMI_Lit:
		// 기본 Lit 모드는 Phong 사용
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewModeIndex::VMI_Unlit:
		// 매크로 없음 (Unlit)
		break;
	default:
		// 기타 ViewMode는 매크로 없음
		break;
	}

	// ViewMode에 따른 Decal 셰이더 로드
	UShader* DecalShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath, ShaderMacros);
	FShaderVariant* ShaderVariant = DecalShader->GetOrCompileShaderVariant(RHIDevice->GetDevice(), ShaderMacros);
	if (!DecalShader)
	{
		UE_LOG("RenderDecalPass: Failed to load Decal shader with ViewMode macros!");
		return;
	}

	// 데칼 렌더 설정
	RHIDevice->RSSetState(ERasterizerMode::Decal);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly); // 깊이 쓰기 OFF
	RHIDevice->OMSetBlendState(false);

	for (UDecalComponent* Decal : Proxies.Decals)
	{
		if (!Decal || !Decal->GetDecalTexture())
		{
			continue;
		}

		// Decal이 그려질 Primitives
		TArray<UPrimitiveComponent*> TargetPrimitives;

		// 1. Decal의 World AABB와 충돌한 모든 StaticMeshComponent 쿼리
		const FOBB DecalOBB = Decal->GetWorldOBB();
		TArray<UStaticMeshComponent*> IntersectedStaticMeshComponents = BVH->QueryIntersectedComponents(DecalOBB);

		// 2. 충돌한 모든 visible Actor의 PrimitiveComponent를 TargetPrimitives에 추가
		// Actor에 기본으로 붙어있는 TextRenderComponent, BoundingBoxComponent는 decal 적용 안되게 하기 위해,
		// 임시로 PrimitiveComponent가 아닌 UStaticMeshComponent를 받도록 함
		for (UStaticMeshComponent* SMC : IntersectedStaticMeshComponents)
		{
			// 기즈모에 데칼 입히면 안되므로 에디팅이 안되는 Component는 데칼 그리지 않음
			if (!SMC || !SMC->IsEditable())
				continue;
			
			AActor* Owner = SMC->GetOwner();
			if (!Owner || !Owner->IsActorVisible())
				continue;

			FDecalStatManager::GetInstance().IncrementAffectedMeshCount();
			TargetPrimitives.push_back(SMC);
		}

		// --- 데칼 렌더 시간 측정 시작 ---
		auto CpuTimeStart = std::chrono::high_resolution_clock::now();

		// 데칼 전용 상수 버퍼 설정
		const FMatrix DecalMatrix = Decal->GetDecalProjectionMatrix();
		RHIDevice->SetAndUpdateConstantBuffer(DecalBufferType(DecalMatrix, Decal->GetOpacity()));

		// 3. TargetPrimitive 순회하며 수집 후 렌더링
		MeshBatchElements.Empty();
		for (UPrimitiveComponent* Target : TargetPrimitives)
		{
			Target->CollectMeshBatches(MeshBatchElements, View);
		}
		for (FMeshBatchElement& BatchElement : MeshBatchElements)
		{
			BatchElement.InstanceShaderResourceView = Decal->GetDecalTexture()->GetShaderResourceView();
			BatchElement.Material = Decal->GetMaterial(0);
			BatchElement.InputLayout = ShaderVariant->InputLayout;
			BatchElement.VertexShader = ShaderVariant->VertexShader;
			BatchElement.PixelShader = ShaderVariant->PixelShader;
			BatchElement.VertexStride = sizeof(FVertexDynamic);
		}
		DrawMeshBatches(MeshBatchElements, true);

		// --- 데칼 렌더 시간 측정 종료 및 결과 저장 ---
		auto CpuTimeEnd = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> CpuTimeMs = CpuTimeEnd - CpuTimeStart;
		FDecalStatManager::GetInstance().GetDecalPassTimeSlot() += CpuTimeMs.count(); // CPU 소요 시간 저장
	}

	// 상태 복구
	RHIDevice->RSSetState(ERasterizerMode::Solid);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	RHIDevice->OMSetBlendState(false);
}

void FSceneRenderer::RenderPostProcessingPasses()
{
	UHeightFogComponent* FogComponent = nullptr;
	if (0 < SceneGlobals.Fogs.Num())
	{
		FogComponent = SceneGlobals.Fogs[0];
	}

	if (!FogComponent)
	{
		return;
	}

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯부터 2개의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 2);

	// 렌더 타겟 설정
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 쉐이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* HeightFogPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/HeightFog_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !HeightFogPS || !HeightFogPS->GetPixelShader())
	{
		UE_LOG("HeightFog용 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, HeightFogPS);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* PointClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
	ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	if (!DepthSRV || !SceneSRV || !PointClampSamplerState || !LinearClampSamplerState)
	{
		UE_LOG("Depth SRV / Scene SRV / PointClamp Sampler / LinearClamp Sampler is null!\n");
		return;
	}

	ID3D11ShaderResourceView* srvs[2] = { DepthSRV, SceneSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, srvs);

	ID3D11SamplerState* Samplers[2] = { LinearClampSamplerState, PointClampSamplerState };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

	// 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = View->ProjectionMode;
	//RHIDevice->UpdatePostProcessCB(ZNear, ZFar, ProjectionMode == ECameraProjectionMode::Orthographic);
	RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->ZNear, View->ZFar, ProjectionMode == ECameraProjectionMode::Orthographic));
	UHeightFogComponent* F = FogComponent;
	//RHIDevice->UpdateFogCB(F->GetFogDensity(), F->GetFogHeightFalloff(), F->GetStartDistance(), F->GetFogCutoffDistance(), F->GetFogInscatteringColor()->ToFVector4(), F->GetFogMaxOpacity(), F->GetFogHeight());
	RHIDevice->SetAndUpdateConstantBuffer(FogBufferType(F->GetFogDensity(), F->GetFogHeightFalloff(), F->GetStartDistance(), F->GetFogCutoffDistance(), F->GetFogInscatteringColor().ToFVector4(), F->GetFogMaxOpacity(), F->GetFogHeight()));

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	// 이제 소멸자는 버퍼 스왑을 되돌리지 않고, SRV 해제 작업만 수행함
	SwapGuard.Commit();
}

void FSceneRenderer::RenderSceneDepthPostProcess()
{
	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯부터 1개의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 BackBuffer에만 그리기)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 쉐이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* SceneDepthPS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/SceneDepth_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !SceneDepthPS || !SceneDepthPS->GetPixelShader())
	{
		UE_LOG("HeightFog용 셰이더 없음!\n");
		return;
	}
	RHIDevice->PrepareShader(FullScreenTriangleVS, SceneDepthPS);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
	if (!DepthSRV)
	{
		UE_LOG("Depth SRV is null!\n");
		return;
	}

	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
	if (!SamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (슬롯 확인!)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &DepthSRV);  // t0
	RHIDevice->GetDeviceContext()->PSSetSamplers(1, 1, &SamplerState);

	// 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = View->ProjectionMode;
	//RHIDevice->UpdatePostProcessCB(ZNear, ZFar, ProjectionMode == ECameraProjectionMode::Orthographic);
	RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->ZNear, View->ZFar, ProjectionMode == ECameraProjectionMode::Orthographic));

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	// 이제 소멸자는 버퍼 스왑을 되돌리지 않고, SRV 해제 작업만 수행함
	SwapGuard.Commit();
}

void FSceneRenderer::RenderTileCullingDebug()
{
	// SF_TileCullingDebug가 비활성화되어 있으면 아무것도 하지 않음
	if (!World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_TileCullingDebug))
	{
		return;
	}

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 SRV를 자동 해제하도록 설정
	// t0 (SceneColorSource), t2 (TileLightIndices) 사용
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 SceneColor에 블렌딩)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 셰이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* TileDebugPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/TileDebugVisualization_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !TileDebugPS || !TileDebugPS->GetPixelShader())
	{
		UE_LOG("TileDebugVisualization 셰이더 없음!\n");
		return;
	}
	RHIDevice->PrepareShader(FullScreenTriangleVS, TileDebugPS);

	// 텍스처 관련 설정
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SceneSRV || !SamplerState)
	{
		UE_LOG("TileDebugVisualization: Scene SRV or Sampler is null!\n");
		return;
	}

	// t0: 원본 씬 텍스처
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// t2: 타일 라이트 인덱스 버퍼 (이미 PerformTileLightCulling에서 바인딩됨)
	// 별도 바인딩 불필요, 유지됨

	// b11: 타일 컬링 상수 버퍼 (이미 PerformTileLightCulling에서 설정됨)
	// 별도 업데이트 불필요, 유지됨

	// 전체 화면 쿼드 그리기
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	SwapGuard.Commit();
}

void FSceneRenderer::RenderFadeInOutPass()
{
	// World가 없거나 PIE 모드가 아니면 Early Return
	if (!World || !World->bPie)
	{
		return;
	}

	// GameMode가 없으면 Early Return
	AGameModeBase* GameMode = World->GetGameMode();
	if (!GameMode)
	{
		return;
	}

	// PlayerController가 없으면 Early Return
	APlayerController* PlayerController = GameMode->GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	// PlayerCameraManager가 없으면 Early Return
	APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager();
	if (!CameraManager)
	{
		return;
	}

	// bFading이 false면 Early Return
	if (!CameraManager->IsFading())
	{
		return;
	}

	// Fade 파라미터 가져오기
	float FadeAlpha = CameraManager->GetFadeAmount();
	FLinearColor FadeColorLinear = CameraManager->GetFadeColor();
	FVector FadeColor(FadeColorLinear.R, FadeColorLinear.G, FadeColorLinear.B);

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 SceneColor에 그리기)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 셰이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* FadeInOutPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/FadeInOut_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !FadeInOutPS || !FadeInOutPS->GetPixelShader())
	{
		UE_LOG("FadeInOut용 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, FadeInOutPS);

	// 텍스처 관련 설정
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SceneSRV || !SamplerState)
	{
		UE_LOG("FadeInOut: Scene SRV or Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (t0)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// Fade 상수 버퍼 업데이트
	FFadeBufferType FadeBuffer;
	FadeBuffer.FadeAlpha = FadeAlpha;
	FadeBuffer.FadeColor = FadeColor;
	RHIDevice->SetAndUpdateConstantBuffer(FadeBuffer);

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	SwapGuard.Commit();
}

void FSceneRenderer::RenderVignettingPass()
{
	// World가 없거나 PIE 모드가 아니면 Early Return
	if (!World || !World->bPie)
	{
		return;
	}

	// GameMode가 없으면 Early Return
	AGameModeBase* GameMode = World->GetGameMode();
	if (!GameMode)
	{
		return;
	}

	// PlayerController가 없으면 Early Return
	APlayerController* PlayerController = GameMode->GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	// PlayerCameraManager가 없으면 Early Return
	APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager();
	if (!CameraManager)
	{
		return;
	}

	TArray<UCameraModifier*>& ModifierList = CameraManager->GetModifierList();
	bool bVignettingExist = false;
	for (UCameraModifier* Modifier : ModifierList)
	{
		UCameraModifier_Vignetting* Vignetting = Cast<UCameraModifier_Vignetting>(Modifier);
		if (!Vignetting) continue;

		bVignettingExist = true;
	}
	if (!bVignettingExist) return;
	
	// ViewTarget에서 PostProcessSettings 가져오기
	const FViewTarget& ViewTarget = CameraManager->GetViewTarget_Internal();
	const FPostProcessSettings& PostProcessSettings = ViewTarget.PostProcessSettings;

	// Vignetting 파라미터 가져오기
	float VignettingRadius = PostProcessSettings.VignettingRadius;
	float VignettingSoftness = PostProcessSettings.VignettingSoftness;
	FVector4 VignettingColor = PostProcessSettings.VignettingColor;

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 SceneColor에 그리기)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 셰이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* VignettingPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/Vignetting_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !VignettingPS || !VignettingPS->GetPixelShader())
	{
		UE_LOG("FadeInOut용 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, VignettingPS);

	// 텍스처 관련 설정
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SceneSRV || !SamplerState)
	{
		UE_LOG("FadeInOut: Scene SRV or Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (t0)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// Fade 상수 버퍼 업데이트
	FVignettingBufferType VignettingBuffer;
	VignettingBuffer.VignettingColor = VignettingColor;
	VignettingBuffer.Radius = VignettingRadius;
	VignettingBuffer.Softness = VignettingSoftness;
	VignettingBuffer.AspectRatio = CameraManager->GetCameraComponentForRendering()->GetAspectRatio();

	RHIDevice->SetAndUpdateConstantBuffer(VignettingBuffer);

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	SwapGuard.Commit();
}

// 빌보드, 에디터 화살표 그리기 (상호 작용, 피킹 O)
void FSceneRenderer::RenderEditorPrimitivesPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);
	for (UPrimitiveComponent* GizmoComp : Proxies.EditorPrimitives)
	{
		GizmoComp->CollectMeshBatches(MeshBatchElements, View);
	}
	DrawMeshBatches(MeshBatchElements, true);
}

// 경계, 외곽선 등 표시 (상호 작용, 피킹 X)
void FSceneRenderer::RenderDebugPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTarget);

	// 그리드 라인 수집
	for (ULineComponent* LineComponent : Proxies.EditorLines)
	{
		if (GWorld->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Grid))
		{
			LineComponent->CollectLineBatches(OwnerRenderer);
		}
	}

	// 선택된 액터의 디버그 볼륨 렌더링
	// 현재 뷰포트에서 piloting 중인 액터는 선택 표시 제외
	// (PilotingActor는 GatherVisibleProxies에서 이미 계산됨)
	AActor* PilotingActor = View->Viewport && View->Viewport->GetViewportClient()
		? View->Viewport->GetViewportClient()->GetPilotActor()
		: nullptr;

	for (AActor* SelectedActor : World->GetSelectionManager()->GetSelectedActors())
	{
		// 현재 뷰포트에서 piloting 중인 액터는 선택 표시를 숨김
		if (PilotingActor && SelectedActor == PilotingActor)
		{
			continue;
		}

		for (USceneComponent* Component : SelectedActor->GetSceneComponents())
		{
			// 모든 컴포넌트에서 RenderDebugVolume 호출
			// 각 컴포넌트는 필요한 경우 override하여 디버그 시각화 제공
			Component->RenderDebugVolume(OwnerRenderer);
		}
	}

	// Debug draw (BVH, Octree 등)
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug) && World->GetPartitionManager())
	{
		if (FBVHierarchy* BVH = World->GetPartitionManager()->GetBVH())
		{
			BVH->DebugDraw(OwnerRenderer); // DebugDraw가 LineBatcher를 직접 받도록 수정 필요
		}
	}

	// Collision BVH Debug draw
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_CollisionBVH))
	{
		if (UCollisionManager* CollisionMgr = World->GetCollisionManager())
		{
			CollisionMgr->DebugDrawBVH(OwnerRenderer);
		}
	}

	// Collision Components Debug draw
	// SF_Collision 플래그가 켜져있거나, PIE 모드(GravityWall 전용)일 때 렌더링
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Collision) || World->bPie)
	{
		if (UCollisionManager* CollisionMgr = World->GetCollisionManager())
		{
			const TArray<UShapeComponent*>& CollisionComponents = CollisionMgr->GetRegisteredComponents();
			for (UShapeComponent* Component : CollisionComponents)
			{
				if (Component)
				{
					// SF_Collision 플래그가 켜져있으면 모든 컴포넌트 렌더링
					bool bShouldRender = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Collision);

					// PIE 모드에서는 GravityWall의 BoxComponent만 렌더링
					if (!bShouldRender && World->bPie)
					{
						AActor* Owner = Component->GetOwner();
						bShouldRender = (dynamic_cast<AGravityWall*>(Owner) != nullptr);
					}

					if (bShouldRender)
					{
						Component->RenderDebugVolume(OwnerRenderer);
					}
				}
			}
		}
	}

	// 수집된 라인을 출력하고 정리
	OwnerRenderer->EndLineBatch(FMatrix::Identity());
}

void FSceneRenderer::RenderLetterBoxPass()
{
	// PIE 모드가 아니거나 GameMode가 없으면 레터박스 렌더링 스킵
	if (!World->bPie || !World->GetGameMode())
		return;

	APlayerController* PlayerController = World->GetGameMode()->GetPlayerController();
	if (!PlayerController)
		return;

	APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager();
	if (!CameraManager)
		return;

	// ViewTarget에서 PostProcessSettings 가져오기
	const FViewTarget& ViewTarget = CameraManager->GetViewTarget_Internal();
	const FPostProcessSettings& PostProcessSettings = ViewTarget.PostProcessSettings;

	// LetterBoxSize가 0이면 렌더링 스킵
	if (PostProcessSettings.LetterBoxSize <= 0.0f)
		return;

	// Swap 가드 객체 생성 (리소스 정리용)
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 SceneColor에 덮어쓰기)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);

	// 블렌드 상태: 비활성화 (덮어쓰기)
	RHIDevice->OMSetBlendState(false);

	// 셰이더 로드
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* LetterBoxPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/LetterBox_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() ||
		!LetterBoxPS || !LetterBoxPS->GetPixelShader())
	{
		UE_LOG("LetterBox 셰이더를 로드할 수 없습니다!");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, LetterBoxPS);

	// 텍스처 설정 (현재 씬 컬러)
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SceneSRV || !SamplerState)
	{
		UE_LOG("LetterBox 렌더링에 필요한 리소스가 없습니다!");
		return;
	}

	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// LetterBox 상수 버퍼 설정
	LetterBoxBufferType LetterBoxBuffer;
	LetterBoxBuffer.LetterBoxSize = PostProcessSettings.LetterBoxSize;
	LetterBoxBuffer.LetterBoxOpacity = PostProcessSettings.LetterBoxOpacity;
	LetterBoxBuffer.Padding = FVector2D(0.0f, 0.0f);
	RHIDevice->SetAndUpdateConstantBuffer(LetterBoxBuffer);

	// 전체 화면 쿼드 그리기
	RHIDevice->DrawFullScreenQuad();

	// 블렌드 상태는 이미 false이므로 복구 불필요

	// Commit (리소스 정리)
	SwapGuard.Commit();
}

void FSceneRenderer::RenderOverayEditorPrimitivesPass()
{
	// 후처리된 최종 이미지 위에 원본 씬의 뎁스 버퍼를 사용하여 3D 오버레이를 렌더링합니다.
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// 뎁스 버퍼를 Clear하고 LessEqual로 그리기 때문에 오버레이로 표시되는데
	// 오버레이 끼리는 깊이 테스트가 가능함
	RHIDevice->ClearDepthBuffer(1.0f, 0);

	for (UPrimitiveComponent* GizmoComp : Proxies.OverlayPrimitives)
	{
		GizmoComp->CollectMeshBatches(MeshBatchElements, View);
	}

	// 수집된 배치를 그립니다.
	DrawMeshBatches(MeshBatchElements, true);
}

// 수집한 Batch 그리기
void FSceneRenderer::DrawMeshBatches(TArray<FMeshBatchElement>& InMeshBatches, bool bClearListAfterDraw, bool bIsShadowPass)
{
	if (InMeshBatches.IsEmpty()) return;

	// RHI 상태 초기 설정 (Opaque Pass 기본값)
	// Shadow Pass일 경우 FShadowMap::BeginRender()에서 이미 설정했으므로 덮어쓰지 않음
	if (!bIsShadowPass)
	{
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual); // 깊이 쓰기 ON
	}

	// PS 리소스 초기화
	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, nullSRVs);
	ID3D11SamplerState* nullSamplers[2] = { nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, nullSamplers);
	FPixelConstBufferType DefaultPixelConst{};
	RHIDevice->SetAndUpdateConstantBuffer(DefaultPixelConst);

	// 현재 GPU 상태 캐싱용 변수 (UStaticMesh* 대신 실제 GPU 리소스로 변경)
	ID3D11VertexShader* CurrentVertexShader = nullptr;
	ID3D11PixelShader* CurrentPixelShader = nullptr;
	UMaterialInterface* CurrentMaterial = nullptr;
	ID3D11ShaderResourceView* CurrentInstanceSRV = nullptr; // [추가] Instance SRV 캐시
	ID3D11Buffer* CurrentVertexBuffer = nullptr;
	ID3D11Buffer* CurrentIndexBuffer = nullptr;
	UINT CurrentVertexStride = 0;
	D3D11_PRIMITIVE_TOPOLOGY CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	// 기본 샘플러 미리 가져오기 (루프 내 반복 호출 방지)
	ID3D11SamplerState* DefaultSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::Default);

	// 정렬된 리스트 순회
	for (const FMeshBatchElement& Batch : InMeshBatches)
	{
		// --- 필수 요소 유효성 검사 ---
		// Shadow Pass에서는 Pixel Shader가 없을 수 있음 (depth-only rendering)
		bool bRequiresPixelShader = !bIsShadowPass;
		if (!Batch.VertexShader ||
			(bRequiresPixelShader && !Batch.PixelShader) ||
			!Batch.VertexBuffer ||
			!Batch.IndexBuffer ||
			Batch.VertexStride == 0)
		{
			// 셰이더나 버퍼, 스트라이드 정보가 없으면 그릴 수 없음
			continue;
		}

		// 1. 셰이더 상태 변경
		if (Batch.VertexShader != CurrentVertexShader || Batch.PixelShader != CurrentPixelShader)
		{
			RHIDevice->GetDeviceContext()->IASetInputLayout(Batch.InputLayout);
			RHIDevice->GetDeviceContext()->VSSetShader(Batch.VertexShader, nullptr, 0);

			RHIDevice->GetDeviceContext()->PSSetShader(Batch.PixelShader, nullptr, 0);

			CurrentVertexShader = Batch.VertexShader;
			CurrentPixelShader = Batch.PixelShader;
		}

		// --- 2. 픽셀 상태 (텍스처, 샘플러, 재질CBuffer) 변경 (캐싱됨) ---
		//
		// 'Material' 또는 'Instance SRV' 둘 중 하나라도 바뀌면
		// 모든 픽셀 리소스를 다시 바인딩해야 합니다.
		if (Batch.Material != CurrentMaterial || Batch.InstanceShaderResourceView != CurrentInstanceSRV)
		{
			ID3D11ShaderResourceView* DiffuseTextureSRV = nullptr; // t0
			ID3D11ShaderResourceView* NormalTextureSRV = nullptr;  // t1
			FPixelConstBufferType PixelConst{};

			if (Batch.Material)
			{
				PixelConst.Material = Batch.Material->GetMaterialInfo();
				PixelConst.bHasMaterial = true;
			}
			else
			{
				FMaterialInfo DefaultMaterialInfo;
				PixelConst.Material = DefaultMaterialInfo;
				PixelConst.bHasMaterial = false;
				PixelConst.bHasDiffuseTexture = false;
				PixelConst.bHasNormalTexture = false;
			}

			// 1순위: 인스턴스 텍스처 (빌보드)
			if (Batch.InstanceShaderResourceView)
			{
				DiffuseTextureSRV = Batch.InstanceShaderResourceView;
				PixelConst.bHasDiffuseTexture = true;
				PixelConst.bHasNormalTexture = false;
			}
			// 2순위: 머티리얼 텍스처 (스태틱 메시)
			else if (Batch.Material)
			{
				const FMaterialInfo& MaterialInfo = Batch.Material->GetMaterialInfo();
				if (!MaterialInfo.DiffuseTextureFileName.empty())
				{
					if (UTexture* TextureData = Batch.Material->GetTexture(EMaterialTextureSlot::Diffuse))
					{
						DiffuseTextureSRV = TextureData->GetShaderResourceView();
						PixelConst.bHasDiffuseTexture = (DiffuseTextureSRV != nullptr);
					}
				}
				if (!MaterialInfo.NormalTextureFileName.empty())
				{
					if (UTexture* TextureData = Batch.Material->GetTexture(EMaterialTextureSlot::Normal))
					{
						NormalTextureSRV = TextureData->GetShaderResourceView();
						PixelConst.bHasNormalTexture = (NormalTextureSRV != nullptr);
					}
				}
			}			// --- RHI 상태 업데이트 ---
			// 1. 텍스처(SRV) 바인딩
			ID3D11ShaderResourceView* Srvs[2] = { DiffuseTextureSRV, NormalTextureSRV };
			RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

			// 2. 샘플러 바인딩
			ID3D11SamplerState* Samplers[2] = { DefaultSampler, DefaultSampler };
			RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

			// 3. 재질 CBuffer 바인딩
			RHIDevice->SetAndUpdateConstantBuffer(PixelConst);

			// --- 캐시 업데이트 ---
			CurrentMaterial = Batch.Material;
			CurrentInstanceSRV = Batch.InstanceShaderResourceView;
		}

		// 3. IA (Input Assembler) 상태 변경
		if (Batch.VertexBuffer != CurrentVertexBuffer ||
			Batch.IndexBuffer != CurrentIndexBuffer ||
			Batch.VertexStride != CurrentVertexStride ||
			Batch.PrimitiveTopology != CurrentTopology)
		{
			UINT Stride = Batch.VertexStride;
			UINT Offset = 0;

			// Vertex/Index 버퍼 바인딩
			RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &Batch.VertexBuffer, &Stride, &Offset);
			RHIDevice->GetDeviceContext()->IASetIndexBuffer(Batch.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			// 토폴로지 설정 (이전 코드의 5번에서 이동하여 최적화)
			RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(Batch.PrimitiveTopology);

			// 현재 IA 상태 캐싱
			CurrentVertexBuffer = Batch.VertexBuffer;
			CurrentIndexBuffer = Batch.IndexBuffer;
			CurrentVertexStride = Batch.VertexStride;
			CurrentTopology = Batch.PrimitiveTopology;
		}

		// 4. 오브젝트별 상수 버퍼 설정 (매번 변경)
		RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(Batch.WorldMatrix, Batch.WorldMatrix.InverseAffine().Transpose()));

		// CustomData가 있으면 스프라이트 애니메이션 정보 전달
		ColorBufferType ColorBuffer;
		ColorBuffer.Color = Batch.InstanceColor;
		ColorBuffer.UUID = Batch.ObjectID;
		if (Batch.CustomData.size() >= 3)
		{
			ColorBuffer.SpriteRows = Batch.CustomData[0];
			ColorBuffer.SpriteColumns = Batch.CustomData[1];
			ColorBuffer.CurrentFrame = Batch.CustomData[2];
		}
		else
		{
			ColorBuffer.SpriteRows = 0.0f;
			ColorBuffer.SpriteColumns = 0.0f;
			ColorBuffer.CurrentFrame = 0.0f;
		}
		RHIDevice->SetAndUpdateConstantBuffer(ColorBuffer);

		// 5. 드로우 콜 실행
		RHIDevice->GetDeviceContext()->DrawIndexed(Batch.IndexCount, Batch.StartIndex, Batch.BaseVertexIndex);
	}

	// 루프 종료 후 리스트 비우기 (옵션)
	if (bClearListAfterDraw)
	{
		InMeshBatches.Empty();
	}
}

void FSceneRenderer::ApplyScreenEffectsPass()
{
	if (!World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_FXAA))
	{
		return;
	}

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯부터 1개의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* SourceSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SourceSRV || !SamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (슬롯 확인!)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SourceSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* CopyTexturePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/FXAA_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !CopyTexturePS || !CopyTexturePS->GetPixelShader())
	{
		UE_LOG("FXAA 셰이더 없음!\n");
		return;
	}

	//RHIDevice->UpdateFXAACB(
	//FVector2D(static_cast<float>(RHIDevice->GetViewportWidth()), static_cast<float>(RHIDevice->GetViewportHeight())),
	//	FVector2D(1.0f / static_cast<float>(RHIDevice->GetViewportWidth()), 1.0f / static_cast<float>(RHIDevice->GetViewportHeight())),
	//	0.0833f,
	//	0.166f,
	//	1.0f,	// 0.75 가 기본값이지만 효과 강조를 위해 1로 설정
	//	12
		//);
	// FXAA 파라미터를 RenderSettings에서 가져옴
	URenderSettings& RenderSettings = World->GetRenderSettings();

	RHIDevice->SetAndUpdateConstantBuffer(FXAABufferType(
		FVector2D(static_cast<float>(RHIDevice->GetViewportWidth()), static_cast<float>(RHIDevice->GetViewportHeight())),
		FVector2D(1.0f / static_cast<float>(RHIDevice->GetViewportWidth()), 1.0f / static_cast<float>(RHIDevice->GetViewportHeight())),
		RenderSettings.GetFXAAEdgeThresholdMin(),
		RenderSettings.GetFXAAEdgeThresholdMax(),
		RenderSettings.GetFXAAQualitySubPix(),
		RenderSettings.GetFXAAQualityIterations()));

	RHIDevice->PrepareShader(FullScreenTriangleVS, CopyTexturePS);

	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	// 이제 소멸자는 버퍼 스왑을 되돌리지 않고, SRV 해제 작업만 수행함
	SwapGuard.Commit();
}

// 최종 결과물의 실제 BackBuffer에 그리는 함수
void FSceneRenderer::CompositeToBackBuffer()
{
	// 1. 최종 결과물을 Source로 만들기 위해 스왑하고, 작업 후 SRV 슬롯 0을 자동 해제하는 가드 생성
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 2. 렌더 타겟을 백버퍼로 설정 (깊이 버퍼 없음)
	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);

	// 3. 텍스처 및 샘플러 설정
	// 이제 RHI_SRV_Index가 아닌, 현재 상태에 맞는 Source SRV를 직접 가져옴
	ID3D11ShaderResourceView* SourceSRV = RHIDevice->GetCurrentSourceSRV();
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SourceSRV || !SamplerState)
	{
		UE_LOG("CompositeToBackBuffer에 필요한 리소스 없음!\n");
		return; // 가드가 자동으로 스왑을 되돌리고 SRV를 해제해줌
	}

	// 4. 셰이더 리소스 바인딩
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SourceSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// 5. 셰이더 준비 - Gamma Correction 적용
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* GammaCorrectionPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/GammaCorrection_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !GammaCorrectionPS || !GammaCorrectionPS->GetPixelShader())
	{
		UE_LOG("GammaCorrection용 셰이더 없음!\n");
		return; // 가드가 자동으로 스왑을 되돌리고 SRV를 해제해줌
	}
	RHIDevice->PrepareShader(FullScreenTriangleVS, GammaCorrectionPS);

	// 6. Gamma Correction 상수 버퍼 설정
	URenderSettings& RenderSettings = World->GetRenderSettings();
	RHIDevice->SetAndUpdateConstantBuffer(GammaCorrectionBufferType{
		RenderSettings.GetGamma(),
		RenderSettings.GetInvGamma(),
		RenderSettings.GetBrightness(),
		RenderSettings.GetSaturation()
	});

	// 7. 그리기
	RHIDevice->DrawFullScreenQuad();

	// 8. 모든 작업이 성공했으므로 Commit
	SwapGuard.Commit();
}