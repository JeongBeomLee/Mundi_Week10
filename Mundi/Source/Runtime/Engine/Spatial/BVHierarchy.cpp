#include "pch.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <queue>
#include "BVHierarchy.h"
#include "Actor.h"
#include "Collision.h"
#include "Vector.h"
#include "OBB.h"
#include "Frustum.h"
#include "Picking.h" // FRay

#include "StaticMeshComponent.h"

namespace {
    inline bool RayAABB_IntersectT(const FRay& ray, const FAABB& box, float& outTMin, float& outTMax)
    {
        float tmin = -FLT_MAX;
        float tmax =  FLT_MAX;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float ro = ray.Origin[axis];
            const float rd = ray.Direction[axis];
            const float bmin = box.Min[axis];
            const float bmax = box.Max[axis];
            if (std::abs(rd) < 1e-6f)
            {
                if (ro < bmin || ro > bmax)
                    return false;
            }
            else
            {
                const float inv = 1.0f / rd;
                float t1 = (bmin - ro) * inv;
                float t2 = (bmax - ro) * inv;
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return false;
            }
        }
        outTMin = tmin < 0.0f ? 0.0f : tmin;
        outTMax = tmax;
        return true;
    }
}

FBVHierarchy::FBVHierarchy(const FAABB& InBounds, int InDepth, int InMaxDepth, int InMaxObjects)
    : Depth(InDepth)
    , MaxDepth(InMaxDepth)
    , MaxObjects(InMaxObjects)
    , Bounds(InBounds)
{
}

FBVHierarchy::~FBVHierarchy()
{
    Clear();
}

void FBVHierarchy::Clear()
{
    // NOTE: TMap, TArray를 clear로 비우면 capacity가 그대로이기 때문에 새 객체로 초기화
    StaticMeshComponentBounds = TMap<UStaticMeshComponent*, FAABB>();
    StaticMeshComponentArray = TArray<UStaticMeshComponent*>();
    Nodes = TArray<FLBVHNode>();
    Bounds = FAABB();
    bPendingRebuild = false;
}

void FBVHierarchy::BulkUpdate(const TArray<UStaticMeshComponent*>& Components)
{
    for (const auto& SMC : Components)
    {
        if (SMC)
        {
            StaticMeshComponentBounds.Add(SMC, SMC->GetWorldAABB());
        }
    }

    // Level 복사 등으로 다량의 컴포넌트를 한 번에 넣는 상황 전제
    // 일반적인 update에서 budget 단위로 끊어 갱신되는 로직 우회해 강제 rebuild
    BuildLBVH();
    bPendingRebuild = false;
}

void FBVHierarchy::Update(UStaticMeshComponent* InComponent)
{
    if (!InComponent)
    {
        return;
    }

    StaticMeshComponentBounds.Add(InComponent, InComponent->GetWorldAABB());
    bPendingRebuild = true;
}

void FBVHierarchy::Remove(UStaticMeshComponent* InComponent)
{
    if (!InComponent)
    {
        return;
    }

    if (StaticMeshComponentBounds.Find(InComponent))
    {
        StaticMeshComponentBounds.Remove(InComponent);
        bPendingRebuild = true;
    }
}

void FBVHierarchy::QueryFrustum(const FFrustum& InFrustum)
{
    if (Nodes.empty()) return;
    //프러스텀 외부에 바운드 존재
    if (!IsAABBVisible(InFrustum, Nodes[0].Bounds)) return;
    //프러스텀 내부에 바운드 존재 (교차 X)
    if (!IsAABBIntersects(InFrustum, Nodes[0].Bounds))
    {
        for (UStaticMeshComponent* Component : StaticMeshComponentArray)
        {
            if (!Component) continue;
            if (StaticMeshComponentBounds.find(Component) == StaticMeshComponentBounds.end())
                continue;
            if (AActor* Owner = Component->GetOwner())
            {
                Owner->SetCulled(false);
            }
        }
        return;
    }
    //프러스텀과 바운드가 교차
    TArray<int32> IdxStack;
    IdxStack.push_back({ 0 });

    while (!IdxStack.empty())
    {
        int32 Idx = IdxStack.back();
        IdxStack.pop_back();
        const FLBVHNode& node = Nodes[Idx];
        if (node.IsLeaf())
        {
            for (int32 i = 0; i < node.Count; ++i)
            {
                UStaticMeshComponent* Component = StaticMeshComponentArray[node.First + i];
                if (!Component || StaticMeshComponentBounds.find(Component) == StaticMeshComponentBounds.end())
                    continue;
                const FAABB* Cached = StaticMeshComponentBounds.Find(Component);
                const FAABB Box = Cached ? *Cached : Component->GetWorldAABB();
                if (IsAABBVisible(InFrustum, Box))
                {
                    if (AActor* Owner = Component->GetOwner())
                    {
                        Owner->SetCulled(false);
                    }
                }
            }
            continue;
        }
        if (node.Left >= 0 && IsAABBVisible(InFrustum, Nodes[node.Left].Bounds))
            IdxStack.push_back({ node.Left });
        if (node.Right >= 0 && IsAABBVisible(InFrustum, Nodes[node.Right].Bounds))
            IdxStack.push_back({ node.Right });
    }
}

void FBVHierarchy::DebugDraw(URenderer* Renderer) const
{
    if (!Renderer) return;
    if (Nodes.empty()) return;

    for (size_t i = 0; i < Nodes.size(); ++i)
    {
        const FLBVHNode& N = Nodes[i];
        const FVector Min = N.Bounds.Min;
        const FVector Max = N.Bounds.Max;
        const FVector4 LineColor(1.0f, N.IsLeaf() ? 0.2f : 0.8f, 0.0f, 1.0f);

        TArray<FVector> Start;
        TArray<FVector> End;
        TArray<FVector4> Color;

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

        Renderer->AddLines(Start, End, Color);
    }
}

int FBVHierarchy::TotalNodeCount() const
{
    return static_cast<int>(Nodes.size());
}

int FBVHierarchy::TotalActorCount() const
{
    return static_cast<int>(StaticMeshComponentArray.size());
}

int FBVHierarchy::MaxOccupiedDepth() const
{
    return (Nodes.empty()) ? 0 : (int)std::ceil(std::log2((double)Nodes.size() + 1));
}

void FBVHierarchy::DebugDump() const
{
    UE_LOG("===== BVHierachy (LBVH) DUMP BEGIN =====\r\n");
    char buf[256];
    std::snprintf(buf, sizeof(buf), "nodes=%zu, components=%zu\r\n", Nodes.size(), StaticMeshComponentArray.size());
    UE_LOG(buf);
    for (size_t i = 0; i < Nodes.size(); ++i)
    {
        const auto& n = Nodes[i];
        std::snprintf(buf, sizeof(buf),
            "[%zu] L=%d R=%d F=%d C=%d | [(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)]\r\n",
            i, n.Left, n.Right, n.First, n.Count,
            n.Bounds.Min.X, n.Bounds.Min.Y, n.Bounds.Min.Z,
            n.Bounds.Max.X, n.Bounds.Max.Y, n.Bounds.Max.Z);
        UE_LOG(buf);
    }
    UE_LOG("===== BVHierachy (LBVH) DUMP END =====\r\n");
}

// Morton helpers
namespace {
    inline uint32 ExpandBits(uint32 v)
    {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    }
    inline uint32 Morton3D(uint32 x, uint32 y, uint32 z)
    {
        return (ExpandBits(x) << 2) | (ExpandBits(y) << 1) | ExpandBits(z);
    }
}

void FBVHierarchy::BuildLBVH()
{
    StaticMeshComponentArray = StaticMeshComponentBounds.GetKeys();
    const int N = StaticMeshComponentArray.Num();
    Nodes = TArray<FLBVHNode>();

    if (N == 0)
    {
        Bounds = FAABB();
        return;
    }

    bool bHasBounds = false;
    for (const auto& Pair : StaticMeshComponentBounds)
    {
        const FAABB& Bound = Pair.second;
        if (!bHasBounds)
        {
            Bounds = Bound;
            bHasBounds = true;
        }
        else
        {
            Bounds = FAABB::Union(Bounds, Bound);
        }
    }

    if (!bHasBounds)
    {
        Bounds = FAABB();
        return;
    }

    TArray<uint32> Codes;
    Codes.resize(N);
    const FVector Min = Bounds.Min;
    const FVector Extent = Bounds.GetHalfExtent();

    for (int i = 0; i < N; ++i)
    {
        UStaticMeshComponent* Component = StaticMeshComponentArray[i];
        const FAABB* Bound = StaticMeshComponentBounds.Find(Component);
        const FVector Center = Bound ? Bound->GetCenter() : Component->GetWorldAABB().GetCenter();

        const auto Normalize = [](float Value, float MinValue, float ExtHalf)
        {
            if (ExtHalf > 0.0f)
            {
                return std::clamp((Value - MinValue) / (ExtHalf * 2.0f), 0.0f, 1.0f);
            }
            return 0.5f;
        };

        const float Nx = Normalize(Center.X, Min.X, Extent.X);
        const float Ny = Normalize(Center.Y, Min.Y, Extent.Y);
        const float Nz = Normalize(Center.Z, Min.Z, Extent.Z);

        const uint32 Ix = static_cast<uint32>(Nx * 1023.0f);
        const uint32 Iy = static_cast<uint32>(Ny * 1023.0f);
        const uint32 Iz = static_cast<uint32>(Nz * 1023.0f);

        Codes[i] = Morton3D(Ix, Iy, Iz);
    }

    TArray<std::pair<UStaticMeshComponent*, uint32>> ComponentCodePairs;
    ComponentCodePairs.resize(N);
    for (int i = 0; i < N; ++i)
    {
        ComponentCodePairs[i] = { StaticMeshComponentArray[i], Codes[i] };
    }

    std::sort(ComponentCodePairs.begin(), ComponentCodePairs.end(),
        [](const auto& LHS, const auto& RHS)
        {
            return LHS.second < RHS.second;
        });

    for (int i = 0; i < N; ++i)
    {
        StaticMeshComponentArray[i] = ComponentCodePairs[i].first;
    }

    Nodes.reserve(std::max(1, 2 * N));
    Nodes.clear();
    BuildRange(0, N);
}

int FBVHierarchy::BuildRange(int s, int e)
{
    int nodeIdx = static_cast<int>(Nodes.size());
    Nodes.push_back(FLBVHNode{});
    FLBVHNode& node = Nodes[nodeIdx];

    int count = e - s;
    if (count <= MaxObjects)
    {
        node.First = s;
        node.Count = count;
        bool bInitialized = false;
        FAABB Accumulated;
        for (int i = s; i < e; ++i)
        {
            UStaticMeshComponent* Component = StaticMeshComponentArray[i];
            if (!Component)
            {
                continue;
            }

            const FAABB* Bound = StaticMeshComponentBounds.Find(Component);
            const FAABB LocalBound = Bound ? *Bound : Component->GetWorldAABB();
            if (!bInitialized)
            {
                Accumulated = LocalBound;
                bInitialized = true;
            }
            else
            {
                Accumulated = FAABB::Union(Accumulated, LocalBound);
            }
        }
        node.Bounds = bInitialized ? Accumulated : Bounds;
        return nodeIdx;
    }

    int mid = (s + e) / 2;
    int L = BuildRange(s, mid);
    int R = BuildRange(mid, e);
    node.Left = L; node.Right = R; node.First = -1; node.Count = 0;
    node.Bounds = FAABB::Union(Nodes[L].Bounds, Nodes[R].Bounds);
    return nodeIdx;
}

void FBVHierarchy::QueryRayClosest(const FRay& Ray, AActor*& OutActor, OUT float& OutBestT) const
{
    OutActor = nullptr;
    // Respect caller-provided initial cap (e.g., far plane) if valid
    if (!(std::isfinite(OutBestT) && OutBestT > 0.0f))
    {
        OutBestT = std::numeric_limits<float>::infinity();
    }

    if (Nodes.empty()) return;

    float tminRoot, tmaxRoot;
    if (!RayAABB_IntersectT(Ray, Nodes[0].Bounds, tminRoot, tmaxRoot)) return;

    struct HeapItem {
        int Idx;
        float TMin;
        bool operator<(const HeapItem& other) const { return TMin > other.TMin; } // min-heap behavior
    };

    std::priority_queue<HeapItem> heap;
    heap.push({ 0, tminRoot });

    const float Epsilon = 1e-3f;
    bool isPick = false;
    while (!heap.empty())
    {
        HeapItem entry = heap.top();
        heap.pop();

        if (OutActor && entry.TMin > OutBestT + Epsilon)
            break;

        const FLBVHNode& node = Nodes[entry.Idx];
        if (node.IsLeaf())
        {
            for (int i = 0; i < node.Count; ++i)
            {
                UStaticMeshComponent* Component = StaticMeshComponentArray[node.First + i];
                if (!Component) continue;
                AActor* Owner = Component->GetOwner();
                if (!Owner) continue;
                if (Owner->GetActorHiddenInEditor()) continue;

                const FAABB* Cached = StaticMeshComponentBounds.Find(Component);
                const FAABB Box = Cached ? *Cached : Component->GetWorldAABB();

                float tmin, tmax;
                if (!RayAABB_IntersectT(Ray, Box, tmin, tmax))
                    continue;
                if (OutActor && tmin > OutBestT + Epsilon)
                    continue;

                float hitDistance;
                if (CPickingSystem::CheckActorPicking(Owner, Ray, hitDistance))
                {
                    if (hitDistance < OutBestT)
                    {
                        OutBestT = hitDistance;
                        OutActor = Owner;
                        isPick = true;
                    }
                }
            }
            continue;
        }
        if (isPick == true)
        {
            break;
        }
        // Internal node: push children if intersected and promising
        if (node.Left >= 0)
        {
            float tminL, tmaxL;
            if (RayAABB_IntersectT(Ray, Nodes[node.Left].Bounds, tminL, tmaxL))
            {
                if (!OutActor || tminL <= OutBestT + Epsilon)
                    heap.push({ node.Left, tminL });
            }
        }
        if (node.Right >= 0)
        {
            float tminR, tmaxR;
            if (RayAABB_IntersectT(Ray, Nodes[node.Right].Bounds, tminR, tmaxR))
            {
                if (!OutActor || tminR <= OutBestT + Epsilon)
                    heap.push({ node.Right, tminR });
            }
        }
    }
}

void FBVHierarchy::FlushRebuild()
{
    if (bPendingRebuild)
    {
        BuildLBVH();
        bPendingRebuild = false;
    }
}

template<typename BoundType, typename NodeIntersectFunc, typename ComponentIntersectFunc>
TArray<UStaticMeshComponent*> FBVHierarchy::QueryIntersectedComponentsGeneric(
    const BoundType& InBound,
    NodeIntersectFunc NodeIntersects,
    ComponentIntersectFunc ComponentIntersects) const
{
    TSet<UStaticMeshComponent*> IntersectedComponents;
    if (Nodes.empty())
        return TArray<UStaticMeshComponent*>();
    TArray<int32> IdxStack;
    IdxStack.push_back({ 0 });

    while (!IdxStack.empty())
    {
        int32 Idx = IdxStack.back();
        IdxStack.pop_back();
        const FLBVHNode& Node = Nodes[Idx];
        if (NodeIntersects(Node.Bounds, InBound))
        {
            if (Node.IsLeaf())
            {
                for (int32 i = 0; i < Node.Count; ++i)
                {
                    UStaticMeshComponent* Component = StaticMeshComponentArray[Node.First + i];
                    if (!Component || StaticMeshComponentBounds.find(Component) == StaticMeshComponentBounds.end())
                        continue;
                    const FAABB* Cached = StaticMeshComponentBounds.Find(Component);
                    const FAABB Box = Cached ? *Cached : Component->GetWorldAABB();
                    if (ComponentIntersects(Box, InBound))
                    {
                        IntersectedComponents.insert(Component);
                    }
                }
            }
            else
            {
                if (Node.Left >= 0) IdxStack.push_back({ Node.Left });
                if (Node.Right >= 0) IdxStack.push_back({ Node.Right });
            }
        }
    }
    return IntersectedComponents.Array();
}

// FAABB 오버로드
TArray<UStaticMeshComponent*> FBVHierarchy::QueryIntersectedComponents(const FAABB& InBound) const
{
    return QueryIntersectedComponentsGeneric(
        InBound,
        [](const FAABB& nodeBound, const FAABB& inBound) { return nodeBound.Intersects(inBound); },
        [](const FAABB& compBound, const FAABB& inBound) { return inBound.Intersects(compBound); }
    );
}

// FOBB 오버로드
TArray<UStaticMeshComponent*> FBVHierarchy::QueryIntersectedComponents(const FOBB& InBound) const
{
    return QueryIntersectedComponentsGeneric(
        InBound,
        [](const FAABB& nodeBound, const FOBB& inBound) { return Collision::Intersects(nodeBound, inBound); },
        [](const FAABB& compBound, const FOBB& inBound) { return Collision::Intersects(compBound, inBound); }
    );
}

// FBoundingSphere 오버로드
TArray<UStaticMeshComponent*> FBVHierarchy::QueryIntersectedComponents(const FBoundingSphere& InBound) const
{
    return QueryIntersectedComponentsGeneric(
        InBound,
        [](const FAABB& nodeBound, const FBoundingSphere& inBound) { return Collision::Intersects(nodeBound, inBound); },
        [](const FAABB& compBound, const FBoundingSphere& inBound) { return Collision::Intersects(compBound, inBound); }
    );
}
