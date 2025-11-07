#pragma once
#include "Frustum.h"
#include "ShadowConfiguration.h"

// 전방 선언 (헤더 파일 의존성 최소화)
class UWorld;
class ACameraActor;
class FViewport;
class URenderer;
class D3D11RHI;
class UPrimitiveComponent;
class UDecalComponent;
class UHeightFogComponent;
class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
struct FMeshBatchElement;
class UMeshComponent;
class UBillboardComponent;
class UTextRenderComponent;
class UGizmoArrowComponent;
class FSceneView;
class FTileLightCuller;
class ULineComponent;
struct FShadowRenderContext;

struct FCandidateDrawable;

// 렌더링할 대상들의 집합을 담는 구조체
struct FVisibleRenderProxySet
{
	// --- Type 1: Main Scene (PP O, Depth-Test O) ---
	TArray<UMeshComponent*> Meshes;
	TArray<UBillboardComponent*> Billboards; // 인게임 빌보드 (파티클, 잔디 등)
	TArray<UDecalComponent*> Decals;
	TArray<UTextRenderComponent*> Texts;

	// --- Type 2: In-Scene Editor (PP X, Depth-Test O) ---
	TArray<ULineComponent*> EditorLines;	// 그리드
	TArray<UPrimitiveComponent*> EditorPrimitives; // 빛 기즈모, *에디터 아이콘 빌보드*

	// --- Type 3: Overlay (PP X, Depth-Test X) ---
	TArray<UPrimitiveComponent*> OverlayPrimitives; // 트랜스폼 기즈모
};

struct FSceneLocals
{
	TArray<UPointLightComponent*> PointLights;
	TArray<USpotLightComponent*> SpotLights;
};

// NOTE: 추후 UWorld로 이동해서 등록/해지 방식으로 변경?
// 전역 효과 및 설정을 담는 구조체
struct FSceneGlobals
{
	TArray<UDirectionalLightComponent*> DirectionalLights;
	TArray<UAmbientLightComponent*> AmbientLights;
	TArray<UHeightFogComponent*> Fogs;	// 첫 번째로 찾은 Fog를 사용함
};

/**
 * @class FSceneRenderer
 * @brief 한 프레임의 특정 뷰(View)에 대한 씬 렌더링을 총괄하는 임시(transient) 클래스.
 */
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, FSceneView* InView, URenderer* InOwnerRenderer);
	~FSceneRenderer();

	/** @brief 이 씬 렌더러의 모든 렌더링 파이프라인을 실행합니다. */
	void Render();

private:
	// Render Path
	void RenderLitPath();
	void RenderWireframePath();
	void RenderSceneDepthPath();

	/** @brief 렌더링에 필요한 포인터들이 유효한지 확인합니다. */
	bool IsValid() const;

	/** @brief 렌더링에 필요한 뷰 행렬, 절두체 등 프레임 데이터를 준비합니다. */
	void PrepareView();

	/** @brief 월드의 모든 액터를 대상으로 절두체 컬링을 수행합니다. */
	void PerformFrustumCulling();


	/** @brief 씬을 순회하며 컬링을 통과한 모든 렌더링 대상을 수집합니다. */
	void GatherVisibleProxies();

	/** @brief 수집한 라이트 정보들로부터 상수 버퍼를 업데이트합니다.*/
	void UpdateLightConstant();

	/** @brief 타일 기반 라이트 컬링을 수행하고 Structured Buffer를 업데이트합니다. */
	void PerformTileLightCulling();

	/** @brief 섀도우 맵을 렌더링하는 패스입니다. */
	void RenderShadowPass();

	/** @brief 불투명(Opaque) 객체들을 렌더링하는 패스입니다. */
	void RenderOpaquePass(EViewModeIndex InRenderViewMode);

	void DrawMeshBatches(TArray<FMeshBatchElement>& InMeshBatches, bool bClearListAfterDraw, bool bIsShadowPass = false);

	/** @brief 데칼(Decal)을 렌더링하는 패스입니다. */
	void RenderDecalPass();

	void RenderPostProcessingPasses();
	void RenderSceneDepthPostProcess();
	void RenderTileCullingDebug();
	void RenderFadeInOutPass();
	void RenderVignettingPass();

	/** @brief 그리드 등 에디터 전용 객체들을 렌더링하는 패스입니다. */
	void RenderEditorPrimitivesPass();
	void RenderOverayEditorPrimitivesPass();

	/** @brief BVH 등 디버그 시각화 요소를 렌더링하는 패스입니다. */
	void RenderDebugPass();

	/** @brief 레터박스(LetterBox)를 렌더링하는 패스입니다. */
	void RenderLetterBoxPass();

	/** @brief FXAA 등 화면에서 최종 이미지 품질을 위해 적용되는 효과를 적용하는 패스입니다. */
	void ApplyScreenEffectsPass();

	void CompositeToBackBuffer();

	/** @brief 프레임 렌더링의 마무리 작업을 수행합니다. (예: 로그 출력) */
	void FinalizeFrame();
private:
	// === Shadow Pass Helper Functions ===

	/** @brief 라이트가 섀도우 캐스팅에 유효한지 검사합니다. */
	template<typename TLightComponent>
	bool IsLightValidForShadowCasting(const TLightComponent* Light) const
	{
		return Light &&
			   Light->IsVisible() &&
			   Light->GetOwner()->IsActorVisible() &&
			   Light->GetIsCastShadows();
	}

	/** @brief 섀도우 패스용 메시 배치를 수집합니다. */
	void CollectShadowMeshBatches(TArray<FMeshBatchElement>& OutMeshBatches) const;

	/** @brief 메시 배치의 셰이더를 섀도우 뎁스 셰이더로 오버라이드합니다.
	 *  @param MeshBatches 오버라이드할 메시 배치
	 *  @param ShadowShaderVariant 섀도우 뎁스 VS 셰이더 (VS와 InputLayout 사용)
	 *  @param FilterType 섀도우 필터 타입 (PS 선택에 사용)
	 */
	void OverrideShadowShader(TArray<FMeshBatchElement>& MeshBatches, FShaderVariant* ShadowShaderVariant, EShadowFilterType FilterType);

	/** @brief 섀도우 렌더링을 위한 ViewProj 상수 버퍼를 업데이트합니다. */
	void UpdateViewProjBufferForShadow(const FShadowRenderContext& ShadowContext, bool bIsOrthographic);

	/** @brief DirectionalLight의 섀도우를 렌더링합니다. */
	void RenderDirectionalLightShadows(FShaderVariant* ShadowShaderVariant);

	/** @brief SpotLight의 섀도우를 렌더링합니다. */
	void RenderSpotLightShadows(FShaderVariant* ShadowShaderVariant);

	/** @brief PointLight의 섀도우를 렌더링합니다 (Cube Map). */
	void RenderPointLightShadows(FShaderVariant* ShadowShaderVariant);

	/** @brief 카메라의 ViewProj 상수 버퍼를 복구합니다. */
	void RestoreCameraViewProj();

	// === Shadow Pass Render State Management ===

	/** @brief 섀도우 패스의 렌더 상태를 저장하고 자동으로 복구하는 RAII 구조체 */
	struct FSavedRenderState
	{
		ID3D11RenderTargetView* RTV = nullptr;
		ID3D11DepthStencilView* DSV = nullptr;
		D3D11_VIEWPORT Viewport;

		void Save(D3D11RHI* RHI);
		void Restore(D3D11RHI* RHI);
		void Release();
	};

private:
	// --- 렌더링 컨텍스트 (외부에서 주입받음) ---
	UWorld* World;
	FSceneView* View;
	URenderer* OwnerRenderer;
	D3D11RHI* RHIDevice;

	// 수집된 렌더링 대상 목록
	FVisibleRenderProxySet Proxies;

	// 씬 지역 설정
	FSceneLocals SceneLocals;

	// 씬 전역 설정
	FSceneGlobals SceneGlobals;

	// 컬링을 거친 가시성 목록, NOTE: 추후 컴포넌트 단위로 수정
	TArray<UPrimitiveComponent*> PotentiallyVisibleComponents;

	// 각 패스에서 수집된 드로우 콜 정보 리스트
	TArray<FMeshBatchElement> MeshBatchElements;

	// 타일 기반 라이트 컬링 시스템 (매 프레임 생성되고 소멸되어서 스마트 포인터로 설정)
	std::unique_ptr<FTileLightCuller> TileLightCuller;
};