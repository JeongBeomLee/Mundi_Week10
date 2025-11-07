#include "pch.h"
#include "MeshBVH.h"

void FMeshBVH::Build(const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices)
{
	TriIndices.Empty();
	Nodes.Empty();
	uint32 TriCount = Indices.Num() / 3;
	if (TriCount == 0) return;

	TriIndices.Reserve(TriCount);
	for (uint32 t = 0; t < TriCount; ++t)
		TriIndices.Add(t);

	BuildRecursive(0, TriCount, Vertices, Indices);
}

// 삼각형과 맞을 경우 , BVH를 따라 내려가면서 교차 가능성 있는 노드만 검사한다. 
// Möller–Trumbore로 교차 체크 ! 
bool FMeshBVH::IntersectRay(const FRay& InLocalRay,
	const TArray<FNormalVertex>& InVertices,
	const TArray<uint32>& InIndices,
	float& OutHitDistance)
{
	if (Nodes.Num() == 0)
	{
		return false;
	}

	float RootEntry, RootExit;
	if (!Nodes[0].Bounds.IntersectsRay(InLocalRay, RootEntry, RootExit))
	{
		return false;
	}

	struct FHeapItem
	{
		int NodeIndex;
		float EntryDistance;

		bool operator>(const FHeapItem& Other) const
		{
			return EntryDistance > Other.EntryDistance; // 최소 힙
		}
	};

	std::priority_queue<FHeapItem, TArray<FHeapItem>, std::greater<FHeapItem>> Heap;
	Heap.push({ 0, RootEntry });

	while (!Heap.empty())
	{
		FHeapItem Current = Heap.top();
		Heap.pop();

		const FMeshBVHNode& Node = Nodes[Current.NodeIndex];
		if (Node.IsLeaf())
		{
			for (uint32 TriOffset = 0; TriOffset < Node.Count; ++TriOffset)
			{
				const uint32 TriangleID = TriIndices[Node.Start + TriOffset];
				const uint32 V0 = InIndices[3 * TriangleID + 0];
				const uint32 V1 = InIndices[3 * TriangleID + 1];
				const uint32 V2 = InIndices[3 * TriangleID + 2];

				const FVector& A = InVertices[V0].pos;
				const FVector& B = InVertices[V1].pos;
				const FVector& C = InVertices[V2].pos;

				float HitT = 0.0f;
				if (IntersectRayTriangleMT(InLocalRay, A, B, C, HitT))
				{
					OutHitDistance = HitT;
					return true; // 🚀 첫 번째 히트 → 바로 종료
				}
			}
		}
		else
		{
			if (Node.Left >= 0)
			{
				float ChildEntry, ChildExit;
				if (Nodes[Node.Left].Bounds.IntersectsRay(InLocalRay, ChildEntry, ChildExit))
				{
					Heap.push({ Node.Left, ChildEntry });
				}
			}
			if (Node.Right >= 0)
			{
				float ChildEntry, ChildExit;
				if (Nodes[Node.Right].Bounds.IntersectsRay(InLocalRay, ChildEntry, ChildExit))
				{
					Heap.push({ Node.Right, ChildEntry });
				}
			}
		}
	}

	return false;
}
//bool FMeshBVH::IntersectRay(const FRay& InLocalRay, const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices, float& OutHitDistance)
//{
//	if (Nodes.Num() == 0)
//	{
//		return false;
//	}
//
//	TArray<FStackItem> NodeStack;
//
//	float RootEntryDistance, RootExitDistance;
//	// 루트 노드가 안맞으면 바로 리턴 
//	if (!Nodes[0].Bounds.RayAABB_IntersectT(InLocalRay, RootEntryDistance, RootExitDistance))
//	{
//		return false;
//	}
//	// 가장 늦게 나간 RootEntryDistance
//	// 삼각형 리스트 인덱스 , 디스턴스 값 저장 
//	NodeStack.Add({ 0, RootEntryDistance });
//	bool bHasHit = false;
//	float ClosestHitDistance = std::numeric_limits<float>::infinity();
//
//	// 깊이 우선 탐색 (DFS). 
//	// 이미 더 가까운 교차가 있으면 멀리 있는 노드는 스킵.
//	while (!NodeStack.IsEmpty())
//	{
//		const FStackItem CurrentItem = NodeStack.Pop();
//		// 이미 가까운 교차가 있으면 무시 
//		if (CurrentItem.EntryDistance > ClosestHitDistance)
//		{
//			continue;
//		}
//
//		const FMeshBVHNode& CurrentNode = Nodes[CurrentItem.NodeIndex];
//		if (CurrentNode.IsLeaf())
//		{
//			// 총 삼각형의 개수 만큼 돈다. 
//			for (uint32 TriangleOffset = 0; TriangleOffset < CurrentNode.Count; ++TriangleOffset)
//			{
//				// 삼각형 배열에서 시작 위치 Start, TriangleOffset 삼각형의 총 개수만큼 돈다. 
//				// 그쪽 영역의 삼각형이기 때문이다
//				const uint32 TriangleID = TriIndices[CurrentNode.Start + TriangleOffset];
//
//				const uint32 VertexIndex0 = InIndices[3 * TriangleID + 0];
//				const uint32 VertexIndex1 = InIndices[3 * TriangleID + 1];
//				const uint32 VertexIndex2 = InIndices[3 * TriangleID + 2];
//
//				const FVector& VertexA = InVertices[VertexIndex0].pos;
//				const FVector& VertexB = InVertices[VertexIndex1].pos;
//				const FVector& VertexC = InVertices[VertexIndex2].pos;
//
//				float HitDistance = 0.0f;
//				if (IntersectRayTriangleMT(InLocalRay, VertexA, VertexB, VertexC, HitDistance))
//				{
//					if (HitDistance < ClosestHitDistance)
//					{
//						ClosestHitDistance = HitDistance;
//						bHasHit = true;
//					}
//				}
//			}
//			continue;
//		}
//
//		// 내부 노드 → 자식 검사
//		// 삼각형에 맞으면, 검사 영역에 넣어주고 맞지 않으면 넣어주지 않는다. 
//		if (CurrentNode.Left >= 0)
//		{
//			float ChildEntry, ChildExit;
//			if (Nodes[CurrentNode.Left].Bounds.RayAABB_IntersectT(InLocalRay, ChildEntry, ChildExit))
//			{
//				NodeStack.Add({ CurrentNode.Left, ChildEntry });
//			}
//		}
//		if (CurrentNode.Right >= 0)
//		{
//			float ChildEntry, ChildExit;
//			if (Nodes[CurrentNode.Right].Bounds.RayAABB_IntersectT(InLocalRay, ChildEntry, ChildExit))
//			{
//				NodeStack.Add({ CurrentNode.Right, ChildEntry });
//			}
//		}
//	}
//
//	if (bHasHit)
//	{
//		OutHitDistance = ClosestHitDistance;
//		return true;
//	}
//	return false;
//}

FAABB FMeshBVH::ComputeTriBounds(uint32 TriangleID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) const
{
	// TriangleID : 몇 번째 삼각형인지 (0번, 1번 , 2번)
	uint32 VertexIndex0, VertexIndex1, VertexIndex2;

	// 순차적으로 꺼내온다. 
	VertexIndex0 = Indices[3 * TriangleID + 0];
	VertexIndex1 = Indices[3 * TriangleID + 1];
	VertexIndex2 = Indices[3 * TriangleID + 2];


	// 실제 정점 좌표들을 가져온다. 
	const FVector& VertexA = Vertices[VertexIndex0].pos;
	const FVector& VertexB = Vertices[VertexIndex1].pos;
	const FVector& VertexC = Vertices[VertexIndex2].pos;

	// AABB 최소/최대 좌표 계산
	FVector MinCorner(
		std::min({ VertexA.X, VertexB.X, VertexC.X }),
		std::min({ VertexA.Y, VertexB.Y, VertexC.Y }),
		std::min({ VertexA.Z, VertexB.Z, VertexC.Z })
	);

	FVector MaxCorner(
		std::max({ VertexA.X, VertexB.X, VertexC.X }),
		std::max({ VertexA.Y, VertexB.Y, VertexC.Y }),
		std::max({ VertexA.Z, VertexB.Z, VertexC.Z })
	);

	// 삼각형의 바운딩 박스 반환
	return FAABB(MinCorner, MaxCorner);
}

FVector FMeshBVH::ComputeTriCenter(uint32 TriangleID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) const
{
	// 삼각형을 구성하는 세 개의 정점 인덱스
	const uint32 VertexIndex0 = Indices[TriangleID * 3 + 0];
	const uint32 VertexIndex1 = Indices[TriangleID * 3 + 1];
	const uint32 VertexIndex2 = Indices[TriangleID * 3 + 2];

	// 실제 좌표
	const FVector& Position0 = Vertices[VertexIndex0].pos;
	const FVector& Position1 = Vertices[VertexIndex1].pos;
	const FVector& Position2 = Vertices[VertexIndex2].pos;

	// 중심점(무게중심) 계산

	const FVector TriangleCenter = (Position0 + Position1 + Position2) / 3.0f;

	return TriangleCenter;
}

// 여러 삼각형을 한 번에 감싸는 AABB를 계산 
FAABB FMeshBVH::ComputeBounds(uint32 Start, uint32 Count, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) const
{
	// 첫 번째 삼각형 ID로 AABB 초기화 
	FAABB Bounds = ComputeTriBounds(TriIndices[Start], Vertices, Indices);
	// Start+1 ~ Start+Count-1 까지 모든 삼각형에 대해 반복 
	// 가장 큰 MIN , MAX 찾아낸다. 
	for (uint32 i = 1; i < Count; i++)
	{
		FAABB TB = ComputeTriBounds(TriIndices[Start + i], Vertices, Indices);
		Bounds.Min = Bounds.Min.ComponentMin(TB.Min);
		Bounds.Max = Bounds.Max.ComponentMax(TB.Max);
	}
	return Bounds;
}

// BVH 트리 -> 재귀 구축 
int FMeshBVH::BuildRecursive(uint32 Start, uint32 Count, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices)
{
	FMeshBVHNode Node;
	Node.Start = Start;
	Node.Count = Count;

	// 이 노드가 감싸는 AABB 계산
	Node.Bounds = ComputeBounds(Start, Count, Vertices, Indices);

	// 현재 노드 인덱스 확보 & 추가 -> 자식으로 쪼갤 때 사용 
	int NodeIndex = Nodes.Num();
	Nodes.Add(Node);

	// 리프 조건: 삼각형 개수가 LeafSize 이하
	if (Count <= LeafSize)
	{
		return NodeIndex;
	}

	// -------------------------------
	// 분할 축 선택 (가장 긴 축)
	// -------------------------------
	FVector Extent = Node.Bounds.GetHalfExtent();
	EAxis Axis = EAxis::X;

	if (Extent.Y > Extent.X && Extent.Y >= Extent.Z)
		Axis = EAxis::Y;
	else if (Extent.Z > Extent.X && Extent.Z >= Extent.Y)
		Axis = EAxis::Z;

	// 중간 지점 -> 반으로 쪼갤 준비하기 
	uint32 Mid = Start + Count / 2;

	// -------------------------------
	// 삼각형 중심 좌표 기준으로 분할
	// -------------------------------
	// 오른쪽 왼쪽으로 나누기 좋은 
	// nth_element 가이드 
	// nth_element(first, nth, last, comp)
	// nth 위치에 "정렬했을 때 올 원소"를 제자리에 넣음
	// 왼쪽 구간 [first, nth) 에는 그 원소보다 "작은 것들"만
	// 오른쪽 구간 [nth+1, last) 에는 "큰 것들"만 놓음

	// 선택된 Axis를 방향으로 왼쪽과 오른쪽으로 나눈다. 
	// 전체를 정렬하지 않고, n번째 작은 원소를 찾아서 놓는다. 
	std::nth_element(
		TriIndices.begin() + Start, // 분할할 구간 시작
		TriIndices.begin() + Mid, // 중간 값의 위치 
		TriIndices.begin() + Start + Count, // 분할할 구간의 끝 
		[&](uint32 A, uint32 B)			// 비교함수 
		{
			FVector CenterA = ComputeTriCenter(A, Vertices, Indices);
			FVector CenterB = ComputeTriCenter(B, Vertices, Indices);

			switch (Axis)
			{
			case EAxis::X: return CenterA.X < CenterB.X;
			case EAxis::Y: return CenterA.Y < CenterB.Y;
			case EAxis::Z: return CenterA.Z < CenterB.Z;
			default:       return CenterA.X < CenterB.X;
			}
		});

	// -------------------------------
	// 내부 노드로 전환 & 자식 생성
	// -------------------------------
	// 자식 생성으로 본인 Count 초기화.
	// Left, Right에 자식 달기 
	Nodes[NodeIndex].Count = 0;
	Nodes[NodeIndex].Left = BuildRecursive(Start, Mid - Start, Vertices, Indices);
	Nodes[NodeIndex].Right = BuildRecursive(Mid, Start + Count - Mid, Vertices, Indices);

	return NodeIndex;
}
