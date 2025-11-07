#pragma once
#include "PrimitiveComponent.h"
#include "AABB.h"
#include "Vector.h"

// Forward declarations
struct FOBB;
class UTexture;
struct FDecalProjectionData;

/**
 * UDecalComponent - Projection Decal implementation
 *
 * Decal volume은 USceneComponent의 Location, Rotation, Scale로 정의됩니다:
 * - Location: Decal의 중심 위치
 * - Rotation: Decal의 투영 방향 (Forward = -Z 방향으로 투영)
 * - Scale: Decal volume의 크기 (X=Width, Y=Height, Z=Depth)
 */
class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)
	GENERATED_REFLECTION_BODY()

	UDecalComponent();

protected:
	~UDecalComponent() override = default;

public:
	virtual void RenderDebugVolume(URenderer* Renderer) const override;

	// Decal Resource API
	void SetDecalTexture(UTexture* InTexture);
	void SetDecalTexture(const FString& TexturePath);
	UTexture* GetDecalTexture() const { return DecalTexture; }

	// Decal Property API
	void SetVisibility(bool bVisible) { bIsVisible = bVisible; }
	bool IsVisible() const { return bIsVisible; }
	void SetOpacity(float Opacity) { DecalOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f); }
	float GetOpacity() const { return DecalOpacity; }

	// Decal Volume & Bounds API
	FAABB GetWorldAABB() const;
	FOBB GetWorldOBB() const;

	// Projection & UV Mapping API
	virtual FMatrix GetDecalProjectionMatrix() const;   // NOTE: FakeSpotLight 를 위해서 가상 함수로 선언

	// Serialization API
	void OnSerialized() override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UDecalComponent)

	// Tick
	virtual void TickComponent(float DeltaTime) override;

	void OnRegister(UWorld* InWorld) override;

private:
	UTexture* DecalTexture = nullptr;
	UGizmoArrowComponent* DirectionGizmo = nullptr;

	bool bIsVisible = true;
	float DecalOpacity = 1.0f;

	// for PIE Tick
	float FadeSpeed = 0.5f;   // 초당 변화 속도 (0.5 = 2초에 완전 페이드)
	int FadeDirection = -1;   // -1 = 감소 중, +1 = 증가 중
};