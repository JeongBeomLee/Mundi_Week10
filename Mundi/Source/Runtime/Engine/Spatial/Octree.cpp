#include "pch.h"
#include "Octree.h"
#include "Actor.h"

FOctree::FOctree(const FAABB& InBounds, int InDepth, int InMaxDepth, int InMaxObjects)
	: Bounds(InBounds), Depth(InDepth), MaxDepth(InMaxDepth), MaxObjects(InMaxObjects)
{
	// 안전하게 nullpter 
	for (int i = 0; i < 8; i++)
	{
		Children[i] = nullptr;
	}
}

FOctree::~FOctree()
{
	for (int i = 0; i < 8; i++)
	{
		if (Children[i])
		{
			delete Children[i];
			Children[i] = nullptr;
		}
	}
}

// 런타임 중에 옥트리 초기화 해야할 때 !
void FOctree::Clear()  
{
    // 액터 배열 정리
    Actors.Empty();
    ActorBoundsCache.Empty();

	// 자식들도 재귀적으로 정리
	for (int i = 0; i < 8; i++)
	{
		if (Children[i])
		{
			delete Children[i];
			Children[i] = nullptr;
		}
	}
}

// 벌크 삽입 최적화 - 대량 액터 처리용
void FOctree::BulkInsert(const TArray<std::pair<AActor*, FAABB>>& ActorsAndBounds)
{
    if (ActorsAndBounds.empty()) return;
    
    // 모든 액터를 대상 영역에 바로 추가
    Actors.reserve(Actors.size() + ActorsAndBounds.size());
    ActorBoundsCache.reserve(ActorBoundsCache.size() + ActorsAndBounds.size());
    for (const auto& ActorBoundPair : ActorsAndBounds)
    {
        Actors.push_back(ActorBoundPair.first);
        ActorBoundsCache.push_back(ActorBoundPair.second);
        // Actor ->  Bound 캐싱 , Update 시 빠르게 쓰기 위해서이다. 
        ActorLastBounds[ActorBoundPair.first] = ActorBoundPair.second;
    }
    
    // 임계값 초과 시 분할 시작
    if (Actors.size() > MaxObjects && Depth < MaxDepth)
    {
        if (!Children[0])
        {
            Split();
        }
        
        // 대량 데이터를 옥탄트별로 그룹화
        TArray<std::pair<AActor*, FAABB>> OctantGroups[8];
        const size_t EstimatedPerOctant = (Actors.size() / 8) + 1;
        for (int i = 0; i < 8; i++)
        {
            OctantGroups[i].reserve(EstimatedPerOctant);
        }
        
        // 인덱스 기반 역방향 순회로 swap-and-pop
        for (int idx = static_cast<int>(Actors.size()) - 1; idx >= 0; --idx)
        {
            AActor* ActorPtr = Actors[idx];
            const FAABB& Box = ActorBoundsCache[idx];
            
            int OptimalOctant = GetOctantIndex(Box);
            int TargetOctant = -1;
            
            if (CanFitInOctant(Box, OptimalOctant))
            {
                TargetOctant = OptimalOctant;
            }
            else
            {
                for (int i = 0; i < 8; i++)
                {
                    if (i != OptimalOctant && Children[i]->Contains(Box))
                    {
                        TargetOctant = i;
                        break;
                    }
                }
            }
            
            // swap-and-pop 삭제 (O(n) 삭제 배제)
            if (TargetOctant != -1)
            {
                OctantGroups[TargetOctant].emplace_back(ActorPtr, Box);
                
                if (idx != static_cast<int>(Actors.size()) - 1)
                {
                    Actors[idx] = Actors.back();
                    ActorBoundsCache[idx] = ActorBoundsCache.back();
                }
                Actors.pop_back();
                ActorBoundsCache.pop_back();
            }
        }
        
        for (int i = 0; i < 8; i++)
        {
            if (!OctantGroups[i].empty())
            {
                Children[i]->BulkInsert(OctantGroups[i]);
            }
        }
    }
}

void FOctree::Insert(AActor* InActor, const FAABB& ActorBounds)
{
    // 마지막 바운드 캐시 갱신
    ActorLastBounds[InActor] = ActorBounds;

    // 현재 노드가 자식이 있는 경우 → 최적화된 옥탄트 계산
    if (Children[0])
    {
        int OptimalOctant = GetOctantIndex(ActorBounds);
        if (CanFitInOctant(ActorBounds, OptimalOctant))
        {
            Children[OptimalOctant]->Insert(InActor, ActorBounds);
            return;
        }
        // 최적 옥탄트에 맞지 않으면 다른 옥탄트 확인 (드문 경우)
        for (int i = 0; i < 8; i++)
        {
            if (i != OptimalOctant && Children[i]->Contains(ActorBounds))
            {
                Children[i]->Insert(InActor, ActorBounds);
                return;
            }
        }
    }

    // 자식에 못 들어가면 현재 노드에 저장
    // 자식에 넣지 못하는 객체를 가질 수 도 있다. 
    Actors.push_back(InActor);
    ActorBoundsCache.push_back(ActorBounds);

    // 분할 조건 체크
    // 현재 노드에 들어있는 객체 수가 Max초과 , 최대 깊이 보다 얕은 레벨이면 스플릿 
    if (Actors.size() > MaxObjects && Depth < MaxDepth)
    {
        if (!Children[0])
        {
            Split();
        }

        // 재분배 - 최적화된 옥탄트 계산 사용
        auto It = Actors.begin();
        while (It != Actors.end())
        {
            AActor* ActorPtr = *It;
            size_t idx = static_cast<size_t>(std::distance(Actors.begin(), It));
            FAABB Box = ActorBoundsCache[idx];

            bool bMoved = false;
            // 최적 옥탄트부터 시도
            int OptimalOctant = GetOctantIndex(Box);
            if (CanFitInOctant(Box, OptimalOctant))
            {
                Children[OptimalOctant]->Insert(ActorPtr, Box);
                It = Actors.erase(It);
                ActorBoundsCache.erase(ActorBoundsCache.begin() + idx);
                bMoved = true;
            }
            else
            {
                // 최적 옥탄트에 맞지 않으면 다른 옥탄트 확인
                for (int i = 0; i < 8; i++)
                {
                    if (i != OptimalOctant && Children[i]->Contains(Box))
                    {
                        Children[i]->Insert(ActorPtr, Box);
                        It = Actors.erase(It);
                        ActorBoundsCache.erase(ActorBoundsCache.begin() + idx);
                        bMoved = true;
                        break;
                    }
                }
            }

            if (!bMoved) ++It;
        }
    }
}
bool FOctree::Contains(const FAABB& Box) const
{
    return Bounds.Contains(Box);
}

bool FOctree::Remove(AActor* InActor, const FAABB& ActorBounds)
{
    // 현재 노드에 있는 경우
    auto It = std::find(Actors.begin(), Actors.end(), InActor);
    if (It != Actors.end())
    {
        size_t idx = static_cast<size_t>(std::distance(Actors.begin(), It));
        Actors.erase(It);
        if (idx < ActorBoundsCache.size())
            ActorBoundsCache.erase(ActorBoundsCache.begin() + idx);
        // 캐시 제거
        ActorLastBounds.erase(InActor);
        return true;
    }

    // 자식 노드에 위임 - 최적화된 옥탄트 계산
    if (Children[0])
    {
        int OptimalOctant = GetOctantIndex(ActorBounds);
        if (CanFitInOctant(ActorBounds, OptimalOctant))
        {
            return Children[OptimalOctant]->Remove(InActor, ActorBounds);
        }
        // 최적 옥탄트에 없으면 다른 옥탄트 확인
        for (int i = 0; i < 8; i++)
        {
            if (i != OptimalOctant && Children[i]->Contains(ActorBounds))
            {
                return Children[i]->Remove(InActor, ActorBounds);
            }
        }
    }
    return false;
}

void FOctree::Update(AActor* InActor, const FAABB& OldBounds, const FAABB& NewBounds)
{
    Remove(InActor, OldBounds);
    Insert(InActor, NewBounds);
}

void FOctree::Remove(AActor* InActor)
{
    auto it = ActorLastBounds.find(InActor);
    if (it != ActorLastBounds.end())
    {
        Remove(InActor, it->second);
        // Remove에서 캐시도 정리됨
    }
}

void FOctree::Update(AActor* InActor)
{
    auto it = ActorLastBounds.find(InActor);
    if (it != ActorLastBounds.end())
    {
        Update(InActor, it->second, InActor->GetBounds());
    }
    else
    {
        Insert(InActor, InActor->GetBounds());
    }
}


// 최적화된 옥탄트 계산 - Contains() 대신 사용
int FOctree::GetOctantIndex(const FAABB& ActorBounds) const
{
    FVector Center = Bounds.GetCenter();
    FVector ActorCenter = ActorBounds.GetCenter();
    
    int OctantIndex = 0;
    if (ActorCenter.X >= Center.X) OctantIndex |= 1;
    if (ActorCenter.Y >= Center.Y) OctantIndex |= 2; 
    if (ActorCenter.Z >= Center.Z) OctantIndex |= 4;
    
    return OctantIndex;
}

bool FOctree::CanFitInOctant(const FAABB& ActorBounds, int OctantIndex) const
{
    if (!Children[0]) return false; // 자식이 없으면 확인 불가
    return Children[OctantIndex]->Contains(ActorBounds);
}

void FOctree::Split()
{
    FVector Center = Bounds.GetCenter();
    FVector Extent = Bounds.GetHalfExtent() * 0.5f;

    for (int i = 0; i < 8; i++)
    {
        FAABB ChildBounds = Bounds.CreateOctant(i);
        FVector ChildCenter = ChildBounds.GetCenter();
        FVector ChildExtent = ChildBounds.GetHalfExtent() * LooseFactor;
        FAABB LooseChildBounds(ChildCenter - ChildExtent, ChildCenter + ChildExtent);

        Children[i] = new FOctree(LooseChildBounds, Depth + 1, MaxDepth, MaxObjects);
        Children[i]->LooseFactor = this->LooseFactor; // propagate
    }
}

int FOctree::TotalNodeCount() const
{
    // 자신 노드 
    int Count = 1; 
    if (Children[0])
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i]) Count += Children[i]->TotalNodeCount();
        }
    }
    return Count;
}

int FOctree::TotalActorCount() const
{
    int Count = Actors.Num();
    if (Children[0])
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i]) Count += Children[i]->TotalActorCount();
        }
    }
    return Count;
}

int FOctree::MaxOccupiedDepth() const
{
    int MaxD = Depth;
    if (Children[0])
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
            {
                int ChildDepth = Children[i]->MaxOccupiedDepth();
                MaxD = (ChildDepth > MaxD) ? ChildDepth : MaxD;
            }
        }
    }
    return MaxD;
}
void FOctree::DebugDump() const
{
    UE_LOG("===== OCTREE DUMP BEGIN =====\r\n");
    // iterative DFS to access private members safely inside class method
    struct StackItem { const FOctree* Node; int D; };
    TArray<StackItem> stack;
    stack.push_back({ this, Depth });
    uint64 size = 0;
    while (!stack.empty())
    {
        StackItem it = stack.back();
        stack.pop_back();

        const FOctree* N = it.Node;
        char buf[256];
        FVector extent = N->GetBounds().GetHalfExtent();
        float length = sqrtf(extent.X * extent.X +
            extent.Y * extent.Y +
            extent.Z * extent.Z);
        std::snprintf(buf, sizeof(buf),
            "[Octree] depth=%d, actors=%zu, bounds=[(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)], extent : %f  \r\n",
            it.D,
            N->Actors.size(),
            N->Bounds.Min.X, N->Bounds.Min.Y, N->Bounds.Min.Z,
            N->Bounds.Max.X, N->Bounds.Max.Y, N->Bounds.Max.Z , length);
        UE_LOG(buf);



        size += N->Actors.size();
        if (N->Children[0])
        {
            for (int i = 7; i >= 0; --i)
            {
                if (N->Children[i])
                {
                    stack.push_back({ N->Children[i], it.D + 1 });
                }
            }
        }
    }

    UE_LOG("size : %d", size);
    UE_LOG("===== OCTREE DUMP END =====\r\n");
}

static void CreateLineDataFromAABB(
    const FVector& Min, const FVector& Max,
    OUT TArray<FVector>& Start,
    OUT TArray<FVector>& End,
    OUT TArray<FVector4>& Color,
    const FVector4& LineColor)
{
    const FVector v0(Min.X, Min.Y, Min.Z);
    const FVector v1(Max.X, Min.Y, Min.Z);
    const FVector v2(Max.X, Max.Y, Min.Z);
    const FVector v3(Min.X, Max.Y, Min.Z);
    const FVector v4(Min.X, Min.Y, Max.Z);
    const FVector v5(Max.X, Min.Y, Max.Z);
    const FVector v6(Max.X, Max.Y, Max.Z);
    const FVector v7(Min.X, Max.Y, Max.Z);

    Start.Add(v0); End.Add(v1); Color.Add(LineColor);
    Start.Add(v1); End.Add(v2); Color.Add(LineColor);
    Start.Add(v2); End.Add(v3); Color.Add(LineColor);
    Start.Add(v3); End.Add(v0); Color.Add(LineColor);

    Start.Add(v4); End.Add(v5); Color.Add(LineColor);
    Start.Add(v5); End.Add(v6); Color.Add(LineColor);
    Start.Add(v6); End.Add(v7); Color.Add(LineColor);
    Start.Add(v7); End.Add(v4); Color.Add(LineColor);

    Start.Add(v0); End.Add(v4); Color.Add(LineColor);
    Start.Add(v1); End.Add(v5); Color.Add(LineColor);
    Start.Add(v2); End.Add(v6); Color.Add(LineColor);
    Start.Add(v3); End.Add(v7); Color.Add(LineColor);
}

// 해당 함수 사용 
void FOctree::QueryRayClosest(const FRay& Ray, AActor*& OutActor, OUT float& OutBestT)
{
    OutBestT = std::numeric_limits<float>::infinity();

    float NodeTMin, NodeTMax;
    // 먼저 레이가 박스에 교차하지 않으면 함수 바로 종료 
    if (!Bounds.IntersectsRay(Ray, NodeTMin, NodeTMax))
        return;

    // BFS 사용해서 가까운 노드부터 탐색 시작 
    std::priority_queue<FNodeEntry> Heap;
    // 레이가 가장 먼저 도착한 노드 부터 탐색 
    Heap.push({ this, NodeTMin });

    const float Epsilon = 1e-3f;

    while (!Heap.empty())
    {
        FNodeEntry Entry = Heap.top();
        Heap.pop();

        // 이미 더 가까운 정확한 교차가 있으면, 더 먼 노드는 굳이 탐색하지 않고 중단 
        if (OutActor && Entry.TMin > OutBestT + Epsilon)
        {
            break;
        }

        FOctree* Node = Entry.Node;
        if (!Node) // 없으면 Stop! 
        {
            continue;
        }

        for (size_t i = 0; i < Node->Actors.size(); ++i)
        {
            AActor* Actor = Node->Actors[i];
            if (!Actor)
            {
                continue;
            }
            if (Actor->GetActorHiddenInEditor())
            {
                continue;
            }


            FAABB ActorBounds;
            //// 1. 배열 캐시에서 찾기 (빠름)
            if (i < Node->ActorBoundsCache.size())
            {
                ActorBounds = Node->ActorBoundsCache[i];
            }
            //// 2. 맵 캐시에서 찾기 (조금 느림)
            //else if (Node->ActorLastBounds.count(Actor) > 0)
            //{
            //    ActorBounds = Node->ActorLastBounds[Actor];
            //}
            //// 3. 최후 수단: Actor 객체에서 직접 얻기 (가장 느림)
            //else
            //{
                //ActorBounds = Actor->GetBounds();
            //}

            float Tmin, Tmax;
            if (!ActorBounds.IntersectsRay(Ray, Tmin, Tmax))
                continue;

            if (OutActor && Tmin > OutBestT + Epsilon)
                continue;

            float hitDistance;
            if (CPickingSystem::CheckActorPicking(Actor, Ray, hitDistance))
            {
                if (hitDistance < OutBestT)
                {
                    OutBestT = hitDistance;
                    OutActor = Actor;
                }
            }
        }
        if (Node->Children[0])
        {
            for (int i = 0; i < 8; ++i)
            {
                FOctree* Child = Node->Children[i];
                if (!Child) continue;
                float Cmin, Cmax;
                if (Child->Bounds.IntersectsRay(Ray, Cmin, Cmax))
                {
                    if (!OutActor || Cmin <= OutBestT + Epsilon)
                        Heap.push({ Child, Cmin });
                }
            }
        }
    }
}

void FOctree::DebugDraw(URenderer* InRenderer) const
{
    if (!InRenderer)
    {
        return;
    }

    struct FStackItem
    {
        const FOctree* Node;
        int32 DepthLevel;
    };

    TArray<FStackItem> Stack;
    Stack.Add({ this, Depth });

    while (Stack.Num() > 0)
    {
        FStackItem Current = Stack.Pop();
        const FOctree* CurrentNode = Current.Node;
        const int32 DepthIndex = Current.DepthLevel % 8;
        FVector4 NodeColor = LevelColors[DepthIndex];
        // AABB 박스 라인 그리기
        TArray<FVector> LineStarts;
        TArray<FVector> LineEnds;
        TArray<FVector4> LineColors;
        CreateLineDataFromAABB(CurrentNode->Bounds.Min, CurrentNode->Bounds.Max, LineStarts, LineEnds, LineColors, NodeColor);
        InRenderer->AddLines(LineStarts, LineEnds, LineColors);

        // 자식 노드 탐색
        if (CurrentNode->Children[0])
        {
            for (int32 ChildIdx = 7; ChildIdx >= 0; --ChildIdx)
            {
                if (CurrentNode->Children[ChildIdx])
                {
                    Stack.Add({ CurrentNode->Children[ChildIdx], Current.DepthLevel + 1 });
                }
            }
        }
    }
}
