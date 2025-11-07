#pragma once
#include "AABB.h"

struct FMeshBVHNode
{
	FAABB Bounds;     // 이 노드가 감싸는 AABB
	int Left = -1;     // 왼쪽 자식 인덱스
	int Right = -1;    // 오른쪽 자식 인덱스
	uint32 Start = 0;  // TriIndices 배열에서 시작 위치
	uint32 Count = 0;  // 리프 노드라면 포함된 삼각형 개수 

	bool IsLeaf() const { return Count > 0; }
};

struct FStackItem
{
	int32 NodeIndex;
	float EntryDistance;
};
class FMeshBVH
{
public:

	void Build(const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices);

	bool IntersectRay(const FRay& InLocalRay, const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices, float& OutHitDistance);


private:
	// Helper 함수들
	FAABB ComputeTriBounds(uint32 TriangleID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) const;

	FVector ComputeTriCenter(uint32 TriangleID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) const;

	FAABB ComputeBounds(uint32 Start, uint32 Count, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) const;

	int BuildRecursive(uint32 Start, uint32 Count, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices);

private:

	TArray<FMeshBVHNode> Nodes;
	//삼각형 ID(번호) 목록 , 삼각형의 인덱스를 의미한다. 
	//삼각형 순서만 재배치  , 정점 좌표와 인덱스 버퍼를 직접적으로 건들면 안되기 때문이다.
	TArray<uint32> TriIndices;
	const uint32 LeafSize = 4;
};

