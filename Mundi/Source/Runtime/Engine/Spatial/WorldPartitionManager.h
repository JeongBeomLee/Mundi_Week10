#pragma once
#include "Object.h"
#include "Vector.h"

class UPrimitiveComponent;
class AStaticMeshActor;
class UStaticMeshComponent;

class FOctree;
class FBVHierarchy;

struct FRay;
struct FAABB;
struct FFrustum;

class UWorldPartitionManager : public UObject
{
public:
	DECLARE_CLASS(UWorldPartitionManager, UObject)

	UWorldPartitionManager();
	~UWorldPartitionManager();

	void Clear();

	// 신규 등록 API
	void Register(UStaticMeshComponent* Smc);         // StaticMeshComponent 하나 추가
	void Register(AActor* Actor);			          // 액터에 부착된 StaticMeshComponent 전부 추가
	void BulkRegister(const TArray<AActor*>& Actors); // 여러 액터 한 번에 추가 (+즉시 리빌드)
	
	void Unregister(AActor* Actor);
	void Unregister(USceneComponent* Component);

	// 업데이트 큐 등록 API
	void MarkDirty(AActor* Actor);
	void MarkDirty(UStaticMeshComponent* Smc);

	void Update(float DeltaTime, const uint32 BudgetCount = 256);

    //void RayQueryOrdered(FRay InRay, OUT TArray<std::pair<AActor*, float>>& Candidates);
    void RayQueryClosest(FRay InRay, OUT AActor*& OutActor, OUT float& OutBestT);
	void FrustumQuery(FFrustum InFrustum);

	/** 옥트리 게터 */
	FOctree* GetSceneOctree() const { return SceneOctree; }
	/** BVH 게터 */
	FBVHierarchy* GetBVH() const { return BVH; }

private:

	// 싱글톤 
	UWorldPartitionManager(const UWorldPartitionManager&) = delete;
	UWorldPartitionManager& operator=(const UWorldPartitionManager&) = delete;

	//재시작시 필요 
	void ClearSceneOctree();
	void ClearBVHierarchy();
	
	TQueue<UStaticMeshComponent*> ComponentDirtyQueue; // 추가 혹은 갱신이 필요한 요소의 대기 큐
	TSet<UStaticMeshComponent*> ComponentDirtySet;     // 더티 큐 중복 추가를 막기 위한 Set
	FOctree* SceneOctree = nullptr;
	FBVHierarchy* BVH = nullptr;
};
