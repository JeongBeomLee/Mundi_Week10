// ────────────────────────────────────────────────────────────────────────────
// CollisionBVH.cpp
// ShapeComponent 기반 충돌 감지용 BVH 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CollisionBVH.h"
#include "CollisionComponent/ShapeComponent.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>

// ────────────────────────────────────────────────────────────────────────────
// Morton Code Helpers (LBVH용)
// ────────────────────────────────────────────────────────────────────────────

namespace
{
	/**
	 * 10비트 정수를 30비트로 확장 (각 비트 사이에 2개의 0 삽입)
	 */
	inline uint32 ExpandBits(uint32 v)
	{
		v = (v * 0x00010001u) & 0xFF0000FFu;
		v = (v * 0x00000101u) & 0x0F00F00Fu;
		v = (v * 0x00000011u) & 0xC30C30C3u;
		v = (v * 0x00000005u) & 0x49249249u;
		return v;
	}

	/**
	 * 3D Morton Code 계산 (Z-order curve)
	 */
	inline uint32 Morton3D(uint32 x, uint32 y, uint32 z)
	{
		return (ExpandBits(x) << 2) | (ExpandBits(y) << 1) | ExpandBits(z);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

FCollisionBVH::FCollisionBVH(const FAABB& InBounds, int InDepth, int InMaxDepth, int InMaxObjects)
	: Depth(InDepth)
	, MaxDepth(InMaxDepth)
	, MaxObjects(InMaxObjects)
	, Bounds(InBounds)
{
}

FCollisionBVH::~FCollisionBVH()
{
	Clear();
}

// ────────────────────────────────────────────────────────────────────────────
// 관리 API
// ────────────────────────────────────────────────────────────────────────────

void FCollisionBVH::Clear()
{
	// NOTE: TMap, TArray를 clear로 비우면 capacity가 그대로이기 때문에 새 객체로 초기화
	ShapeComponentBounds = TMap<UShapeComponent*, FAABB>();
	ShapeComponentArray = TArray<UShapeComponent*>();
	Nodes = TArray<FLBVHNode>();
	Bounds = FAABB();
	bPendingRebuild = false;
}

void FCollisionBVH::BulkUpdate(const TArray<UShapeComponent*>& Components)
{
	for (UShapeComponent* Comp : Components)
	{
		if (Comp && Comp->bGenerateOverlapEvents)
		{
			// Bounds 강제 업데이트 (World Transform이 설정된 후)
			Comp->UpdateBounds();
			ShapeComponentBounds[Comp] = Comp->GetScaledBounds().GetBox();
		}
	}

	// Level 복사 등으로 다량의 컴포넌트를 한 번에 넣는 상황 전제
	// 일반적인 update에서 budget 단위로 끊어 갱신되는 로직 우회해 강제 rebuild
	BuildLBVH();
	bPendingRebuild = false;
}

void FCollisionBVH::Update(UShapeComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (!InComponent->bGenerateOverlapEvents)
	{
		// Overlap 이벤트를 생성하지 않으면 제거
		Remove(InComponent);
		return;
	}

	// Bounds 강제 업데이트 (World Transform이 설정된 후)
	InComponent->UpdateBounds();
	ShapeComponentBounds[InComponent] = InComponent->GetScaledBounds().GetBox();
	bPendingRebuild = true;
}

void FCollisionBVH::Remove(UShapeComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (ShapeComponentBounds.Find(InComponent))
	{
		ShapeComponentBounds.Remove(InComponent);
		bPendingRebuild = true;
	}
}

void FCollisionBVH::FlushRebuild()
{
	if (bPendingRebuild)
	{
		BuildLBVH();
		bPendingRebuild = false;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 쿼리 API
// ────────────────────────────────────────────────────────────────────────────

TArray<UShapeComponent*> FCollisionBVH::QueryOverlappingComponents(UShapeComponent* Component) const
{
	if (!Component)
	{
		return TArray<UShapeComponent*>();
	}

	// 컴포넌트의 Bounds를 FAABB로 변환하여 쿼리
	FAABB QueryBound = Component->GetScaledBounds().GetBox();
	return QueryIntersectedComponents(QueryBound);
}

TArray<UShapeComponent*> FCollisionBVH::QueryIntersectedComponents(const FAABB& InBound) const
{
	TArray<UShapeComponent*> Result;

	if (Nodes.empty())
	{
		return Result;
	}

	// 루트 노드와 교차하지 않으면 종료
	if (!Nodes[0].Bounds.Intersects(InBound))
	{
		return Result;
	}

	// DFS 스택 기반 순회
	TArray<int32> IdxStack;
	IdxStack.push_back(0);

	while (!IdxStack.empty())
	{
		int32 Idx = IdxStack.back();
		IdxStack.pop_back();

		const FLBVHNode& Node = Nodes[Idx];

		// 리프 노드: 컴포넌트 체크
		if (Node.IsLeaf())
		{
			for (int32 i = 0; i < Node.Count; ++i)
			{
				UShapeComponent* Comp = ShapeComponentArray[Node.First + i];
				if (!Comp)
					continue;

				const FAABB* Cached = ShapeComponentBounds.Find(Comp);
				const FAABB Box = Cached ? *Cached : Comp->GetScaledBounds().GetBox();

				if (Box.Intersects(InBound))
				{
					Result.push_back(Comp);
				}
			}
			continue;
		}

		// 내부 노드: 자식 노드와 교차 체크
		if (Node.Left >= 0 && Nodes[Node.Left].Bounds.Intersects(InBound))
		{
			IdxStack.push_back(Node.Left);
		}

		if (Node.Right >= 0 && Nodes[Node.Right].Bounds.Intersects(InBound))
		{
			IdxStack.push_back(Node.Right);
		}
	}

	return Result;
}

// ────────────────────────────────────────────────────────────────────────────
// 디버그 / 통계
// ────────────────────────────────────────────────────────────────────────────

void FCollisionBVH::DebugDraw(URenderer* Renderer) const
{
	if (!Renderer)
		return;

	if (Nodes.empty())
		return;

	for (size_t i = 0; i < Nodes.size(); ++i)
	{
		const FLBVHNode& N = Nodes[i];
		const FVector Min = N.Bounds.Min;
		const FVector Max = N.Bounds.Max;

		// 리프 노드는 초록색, 내부 노드는 노란색
		const FVector4 LineColor(1.0f, N.IsLeaf() ? 0.2f : 0.8f, 0.0f, 1.0f);

		TArray<FVector> Start;
		TArray<FVector> End;
		TArray<FVector4> Color;

		// AABB 8개 꼭짓점
		const FVector v0(Min.X, Min.Y, Min.Z);
		const FVector v1(Max.X, Min.Y, Min.Z);
		const FVector v2(Max.X, Max.Y, Min.Z);
		const FVector v3(Min.X, Max.Y, Min.Z);
		const FVector v4(Min.X, Min.Y, Max.Z);
		const FVector v5(Max.X, Min.Y, Max.Z);
		const FVector v6(Max.X, Max.Y, Max.Z);
		const FVector v7(Min.X, Max.Y, Max.Z);

		// 뒤쪽 면 (4개 선분)
		Start.Add(v0); End.Add(v1); Color.Add(LineColor);
		Start.Add(v1); End.Add(v2); Color.Add(LineColor);
		Start.Add(v2); End.Add(v3); Color.Add(LineColor);
		Start.Add(v3); End.Add(v0); Color.Add(LineColor);

		// 앞쪽 면 (4개 선분)
		Start.Add(v4); End.Add(v5); Color.Add(LineColor);
		Start.Add(v5); End.Add(v6); Color.Add(LineColor);
		Start.Add(v6); End.Add(v7); Color.Add(LineColor);
		Start.Add(v7); End.Add(v4); Color.Add(LineColor);

		// 앞뒤 연결 (4개 선분)
		Start.Add(v0); End.Add(v4); Color.Add(LineColor);
		Start.Add(v1); End.Add(v5); Color.Add(LineColor);
		Start.Add(v2); End.Add(v6); Color.Add(LineColor);
		Start.Add(v3); End.Add(v7); Color.Add(LineColor);

		Renderer->AddLines(Start, End, Color);
	}
}

int FCollisionBVH::TotalNodeCount() const
{
	return static_cast<int>(Nodes.size());
}

int FCollisionBVH::TotalComponentCount() const
{
	return static_cast<int>(ShapeComponentArray.size());
}

int FCollisionBVH::MaxOccupiedDepth() const
{
	return (Nodes.empty()) ? 0 : static_cast<int>(std::ceil(std::log2(static_cast<double>(Nodes.size()) + 1)));
}

void FCollisionBVH::DebugDump() const
{
	UE_LOG("===== CollisionBVH (LBVH) DUMP BEGIN =====\r\n");

	char buf[256];
	std::snprintf(buf, sizeof(buf), "nodes=%zu, components=%zu\r\n", Nodes.size(), ShapeComponentArray.size());
	UE_LOG(buf);

	for (size_t i = 0; i < Nodes.size(); ++i)
	{
		const FLBVHNode& n = Nodes[i];
		std::snprintf(buf, sizeof(buf),
			"[%zu] L=%d R=%d F=%d C=%d | [(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)]\r\n",
			i, n.Left, n.Right, n.First, n.Count,
			n.Bounds.Min.X, n.Bounds.Min.Y, n.Bounds.Min.Z,
			n.Bounds.Max.X, n.Bounds.Max.Y, n.Bounds.Max.Z);
		UE_LOG(buf);
	}

	UE_LOG("===== CollisionBVH (LBVH) DUMP END =====\r\n");
}

// ────────────────────────────────────────────────────────────────────────────
// LBVH 구축
// ────────────────────────────────────────────────────────────────────────────

void FCollisionBVH::BuildLBVH()
{
	// 1. 컴포넌트 배열 생성
	ShapeComponentArray = ShapeComponentBounds.GetKeys();
	const int N = ShapeComponentArray.Num();
	Nodes = TArray<FLBVHNode>();

	if (N == 0)
	{
		Bounds = FAABB();
		return;
	}

	// 2. 전체 Bounds 계산
	bool bHasBounds = false;
	for (const auto& Pair : ShapeComponentBounds)
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

	// 3. Morton Code 계산
	TArray<uint32> Codes;
	Codes.resize(N);
	const FVector Min = Bounds.Min;
	const FVector Extent = Bounds.GetHalfExtent();

	for (int i = 0; i < N; ++i)
	{
		UShapeComponent* Comp = ShapeComponentArray[i];
		const FAABB* Bound = ShapeComponentBounds.Find(Comp);
		const FVector Center = Bound ? Bound->GetCenter() : Comp->GetScaledBounds().GetBox().GetCenter();

		// 정규화 (0.0 ~ 1.0)
		auto Normalize = [](float Value, float MinValue, float ExtHalf)
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

		// 10비트 정수로 변환 (0 ~ 1023)
		const uint32 Ix = static_cast<uint32>(Nx * 1023.0f);
		const uint32 Iy = static_cast<uint32>(Ny * 1023.0f);
		const uint32 Iz = static_cast<uint32>(Nz * 1023.0f);

		Codes[i] = Morton3D(Ix, Iy, Iz);
	}

	// 4. Morton Code 기준 정렬
	TArray<std::pair<UShapeComponent*, uint32>> ComponentCodePairs;
	ComponentCodePairs.resize(N);
	for (int i = 0; i < N; ++i)
	{
		ComponentCodePairs[i] = { ShapeComponentArray[i], Codes[i] };
	}

	std::sort(ComponentCodePairs.begin(), ComponentCodePairs.end(),
		[](const auto& LHS, const auto& RHS)
		{
			return LHS.second < RHS.second;
		});

	for (int i = 0; i < N; ++i)
	{
		ShapeComponentArray[i] = ComponentCodePairs[i].first;
	}

	// 5. BVH 트리 구축
	Nodes.reserve(std::max(1, 2 * N));
	Nodes.clear();
	BuildRange(0, N);
}

int FCollisionBVH::BuildRange(int s, int e)
{
	int nodeIdx = static_cast<int>(Nodes.size());
	Nodes.push_back(FLBVHNode{});
	FLBVHNode& node = Nodes[nodeIdx];

	int count = e - s;

	// 리프 노드 조건: 컴포넌트 개수가 MaxObjects 이하
	if (count <= MaxObjects)
	{
		node.First = s;
		node.Count = count;

		bool bInitialized = false;
		FAABB Accumulated;

		for (int i = s; i < e; ++i)
		{
			UShapeComponent* Comp = ShapeComponentArray[i];
			if (!Comp)
			{
				continue;
			}

			const FAABB* Bound = ShapeComponentBounds.Find(Comp);
			const FAABB LocalBound = Bound ? *Bound : Comp->GetScaledBounds().GetBox();

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

	// 내부 노드: 재귀적으로 분할
	int mid = (s + e) / 2;
	int L = BuildRange(s, mid);
	int R = BuildRange(mid, e);

	node.Left = L;
	node.Right = R;
	node.First = -1;
	node.Count = 0;
	node.Bounds = FAABB::Union(Nodes[L].Bounds, Nodes[R].Bounds);

	return nodeIdx;
}
