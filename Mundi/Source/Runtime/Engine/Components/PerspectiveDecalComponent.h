#pragma once
#include "DecalComponent.h"
#include "AABB.h"
#include "Vector.h"

// Forward declarations
struct FOBB;
class UTexture;
struct FDecalProjectionData;

class UPerspectiveDecalComponent : public UDecalComponent
{
public:
	DECLARE_CLASS(UPerspectiveDecalComponent, UDecalComponent)
	GENERATED_REFLECTION_BODY()

	UPerspectiveDecalComponent();
	~UPerspectiveDecalComponent() override = default;

	// Render API
	void RenderDebugVolume(URenderer* Renderer) const override;

	// Projection & UV Mapping API
	FMatrix GetDecalProjectionMatrix() const override;

	float GetFovY() { return FovY; };
	void SetFovY(float InFov);

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UPerspectiveDecalComponent)

	// Serialize
	void OnSerialized() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	float FovY = 60;
};