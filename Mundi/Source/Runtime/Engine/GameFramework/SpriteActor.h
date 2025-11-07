#pragma once
#include "Actor.h"
#include "Source/Runtime/Engine/Components/ParticleComponent.h"

class ASpriteActor : public AActor
{
public:
	DECLARE_CLASS(ASpriteActor, AActor)
	GENERATED_REFLECTION_BODY()

	ASpriteActor();
	~ASpriteActor() override = default;

	void BeginPlay() override;
	void Tick(float DeltaSeconds) override;

	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(ASpriteActor)

	void OnSerialized() override;

	// 컴포넌트 접근
	UParticleComponent* GetParticleComponent() const { return ParticleComponent; }

	// 스프라이트 애니메이션 설정
	void SetSpriteSheet(const FString& InTexturePath, int32 InRows, int32 InColumns, float InFrameRate);

	// 애니메이션이 끝나면 자동으로 삭제되는 옵션
	void SetAutoDestroy(bool bEnable, float InLifetime = 0.0f);

private:
	UParticleComponent* ParticleComponent = nullptr;

	bool bAutoDestroy = false;
	float Lifetime = 0.0f;
	float ElapsedTime = 0.0f;
};
