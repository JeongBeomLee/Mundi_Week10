#pragma once
#include <unordered_map>
#include "Renderer.h"
#include "Picking.h"

class FOctree;

struct FNodeEntry {
    FOctree* Node;
    float TMin;
    bool operator<(const FNodeEntry& Other) const { return TMin > Other.TMin; } // min-heap
};

static const FVector4 LevelColors[8] =
{
    FVector4(0.0f, 1.0f, 0.0f, 1.0f),   // 0: 초록
    FVector4(0.2f, 0.8f, 1.0f, 1.0f),   // 1: 하늘색
    FVector4(1.0f, 0.6f, 0.1f, 1.0f),   // 2: 주황
    FVector4(1.0f, 0.0f, 0.0f, 1.0f),   // 3: 빨강
    FVector4(0.6f, 0.0f, 1.0f, 1.0f),   // 4: 보라
    FVector4(1.0f, 1.0f, 0.0f, 1.0f),   // 5: 노랑
    FVector4(0.0f, 0.5f, 1.0f, 1.0f),   // 6: 파랑
    FVector4(1.0f, 0.0f, 1.0f, 1.0f),   // 7: 핑크
};


class FOctree
{
public:
    // 생성자/소멸자
    FOctree(const FAABB& InBounds, int InDepth = 0, int InMaxDepth = 5, int InMaxObjects = 15);
    ~FOctree();

	// 초기화
	void Clear();

	// 삽입 / 제거 / 갱신
	void Insert(AActor* InActor, const FAABB& ActorBounds);
	// 벌크 삽입 최적화 - 대량 액터 처리용
	void BulkInsert(const TArray<std::pair<AActor*, FAABB>>& ActorsAndBounds);
	bool Contains(const FAABB& Box) const;
	bool Remove(AActor* InActor, const FAABB& ActorBounds);
    void Update(AActor* InActor, const FAABB& OldBounds, const FAABB& NewBounds);

    //void QueryRayOrdered(const FRay& Ray, TArray<std::pair<AActor*, float>>& OutCandidates) ;
    // for Partition Manager Query
    void Remove(AActor* InActor);
    void Update(AActor* InActor);

    void QueryRayClosest(const FRay& Ray, AActor*& OutActor,OUT float& OutBestT);

    // Debug draw
    void DebugDraw(URenderer* InRenderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const;
    int MaxOccupiedDepth() const;
    void DebugDump() const;

    const FAABB& GetBounds() const { return Bounds; }

private:
	// 내부 함수
	void Split();
	// 최적화된 옥탄트 계산 - Contains() 대신 사용
	int GetOctantIndex(const FAABB& ActorBounds) const;
	bool CanFitInOctant(const FAABB& ActorBounds, int OctantIndex) const;

private:
	int Depth;
	int MaxDepth;
	int MaxObjects;
	FAABB Bounds;

	// Loose octree factor: expand child bounds to reduce parent crowding
	float LooseFactor = 1.25f;

	TArray<AActor*> Actors;
	FOctree* Children[8]; // 8분할 
    // TODO 리팩토링 -> 하나의 TMAP으로 관리하던 , 해야할 것 같다 . 
    TMap<AActor*, FAABB> ActorLastBounds;
    TArray<FAABB> ActorBoundsCache;
    TArray<AActor*> ActorArray;
    
    // 메모리 풀링을 위한 정적 스택
    static TArray<FOctree*> NodePool;
};

