#pragma once

#include "Actor.h"

class UStaticMeshComponent;
class UBoxComponent;

// ────────────────────────────────────────────────────────────────────────────
// AGravityWall
// StaticMeshComponent와 BoxComponent를 가진 벽 액터
// 충돌 감지를 통해 지면/벽 구분 가능
// ────────────────────────────────────────────────────────────────────────────
class AGravityWall : public AActor
{
public:
	DECLARE_CLASS(AGravityWall, AActor)
	GENERATED_REFLECTION_BODY()

	AGravityWall();
	virtual ~AGravityWall() override;

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	// 메시 경로 설정 (Lua에서 호출)
	void SetMeshPath(const FString& MeshPath);

	// 벽 타입
	// 지면 여부 (지면=true, 벽=false)
	bool IsFloor() const { return bIsFloor; }
	void SetIsFloor(bool bInIsFloor) { bIsFloor = bInIsFloor; }

	// 벽 법선 벡터 (벽이 향하는 방향)
	FVector GetWallNormal() const { return WallNormal; }
	void SetWallNormal(const FVector& InNormal) { WallNormal = InNormal.GetNormalized(); }

	// 복제 및 직렬화
	void DuplicateSubObjects() override;
	DECLARE_ACTOR_DUPLICATE(AGravityWall)
	void OnSerialized() override;

protected:
	UStaticMeshComponent* StaticMeshComponent;

	UBoxComponent* BoxComponent;

	// 지면 여부 (true: 바닥, false: 벽)
	bool bIsFloor;

	// 벽 법선 벡터 (벽이 향하는 방향, 정규화됨)
	FVector WallNormal;
};
