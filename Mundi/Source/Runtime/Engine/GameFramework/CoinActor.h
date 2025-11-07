#pragma once

#include "Actor.h"

class UStaticMeshComponent;
class UBoxComponent;
class URotatingMovementComponent;


class ACoinActor : public AActor
{
public:
	DECLARE_CLASS(ACoinActor, AActor)
	GENERATED_REFLECTION_BODY()
	
	ACoinActor();
	virtual ~ACoinActor() override;

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	// 메시 경로 설정 (Lua에서 호출)
	void SetMeshPath(const FString& MeshPath);

	// 복제 및 직렬화
	/*void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(ACoinActor)
	void OnSerialized() override;*/

	void BeginPlay() override;

protected:
	UStaticMeshComponent* StaticMeshComponent;

	UBoxComponent* BoxComponent;

	URotatingMovementComponent* RotatingComponent;
};
