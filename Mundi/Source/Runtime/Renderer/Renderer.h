#pragma once
#include "RHIDevice.h"
#include "LineDynamicMesh.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMeshComponent;
class URHIDevice;
class UShader;
class UStaticMesh;
class UBillboardComponent;
class UPrimitiveComponent;
struct FMaterialSlot;

class URenderer
{
public:
	URenderer(D3D11RHI* InDevice);

	~URenderer();

public:
	void RenderSceneForView(UWorld* InWorld, ACameraActor* InCamera, FViewport* InViewport);

	void BeginFrame();
	void EndFrame();

	// Viewport size for current draw context (used by overlay/gizmo scaling)
	void SetCurrentViewportSize(uint32 InWidth, uint32 InHeight) { CurrentViewportWidth = InWidth; CurrentViewportHeight = InHeight; }
	uint32 GetCurrentViewportWidth() const { return CurrentViewportWidth; }
	uint32 GetCurrentViewportHeight() const { return CurrentViewportHeight; }
	UPrimitiveComponent* GetPrimitiveCollided(int MouseX, int MouseY) const;

	// Batch Line Rendering System
	void BeginLineBatch();
	void AddLine(const FVector& Start, const FVector& End, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	void AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors);
	void EndLineBatch(const FMatrix& ModelMatrix);
	void ClearLineBatch();

	D3D11RHI* GetRHIDevice() { return RHIDevice; }

	void SetCurrentCamera(ACameraActor* InCamera) { CurrentCamera = InCamera; }
	ACameraActor* GetCurrentCamera() const { return CurrentCamera; }

private:
	D3D11RHI* RHIDevice;    // NOTE: 개발 편의성을 위해서 DX11를 종속적으로 사용한다 (URHIDevice를 사용하지 않음)

	// Current viewport size (per FViewport draw); 0 if unset

	uint32 CurrentViewportWidth = 0;
	uint32 CurrentViewportHeight = 0;

	// Batch Line Rendering System using UDynamicMesh for efficiency
	ULineDynamicMesh* DynamicLineMesh = nullptr;
	FMeshData* LineBatchData = nullptr;
	UShader* LineShader = nullptr;
	bool bLineBatchActive = false;
	static const uint32 MAX_LINES = 200000;  // Maximum lines per batch (safety headroom)

	void InitializeLineBatch();

	// 이전 drawCall에서 이미 썼던 RnderState면, 다시 Set 하지 않기 위해 만든 변수들
	EViewModeIndex PreViewModeIndex = EViewModeIndex::VMI_Wireframe; // RSSetState, UpdateColorConstantBuffers
	//UMaterial* PreUMaterial = nullptr; // SRV, UpdatePixelConstantBuffers
	//UStaticMesh* PreStaticMesh = nullptr; // VertexBuffer, IndexBuffer
	/*ID3D11Buffer* PreVertexBuffer = nullptr;
	ID3D11ShaderResourceView* PreSRV = nullptr;*/

	ACameraActor* CurrentCamera = nullptr;
};

