// ────────────────────────────────────────────────────────────────────────────
// CollisionBVH.h
// ShapeComponent 기반 충돌 감지용 BVH (Bounding Volume Hierarchy)
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "AABB.h"


// Forward Declarations
class UShapeComponent;
class URenderer;

/**
 * FCollisionBVH
 *
 * ShapeComponent 기반 충돌 감지를 위한 BVH 구조입니다.
 * LBVH (Linear BVH) 알고리즘을 사용하여 O(log N) 쿼리 성능을 제공합니다.
 *
 * 주요 기능:
 * - ShapeComponent 등록/해제/업데이트
 * - 특정 컴포넌트와 겹칠 가능성이 있는 컴포넌트 쿼리
 * - AABB 기반 공간 쿼리
 * - 디버그 렌더링
 */
class FCollisionBVH
{
public:
	// ────────────────────────────────────────────────
	// 생성자 / 소멸자
	// ────────────────────────────────────────────────

	/**
	 * BVH를 생성합니다.
	 *
	 * @param InBounds - 월드 전체 경계 (초기 BVH 루트 노드 범위)
	 * @param InDepth - 현재 깊이 (재귀용, 기본값 0)
	 * @param InMaxDepth - 최대 깊이 (기본값 12)
	 * @param InMaxObjects - 리프 노드의 최대 오브젝트 수 (기본값 8)
	 */
	FCollisionBVH(const FAABB& InBounds, int InDepth = 0, int InMaxDepth = 12, int InMaxObjects = 8);
	~FCollisionBVH();

	// ────────────────────────────────────────────────
	// 관리 API
	// ────────────────────────────────────────────────

	/**
	 * BVH를 완전히 비웁니다.
	 */
	void Clear();

	/**
	 * 여러 컴포넌트를 한 번에 등록하고 BVH를 재구축합니다.
	 * Level 로드 등 대량 등록 시 사용합니다.
	 *
	 * @param Components - 등록할 컴포넌트 배열
	 */
	void BulkUpdate(const TArray<UShapeComponent*>& Components);

	/**
	 * 단일 컴포넌트를 등록하거나 업데이트합니다.
	 * 컴포넌트가 이동했거나 새로 추가된 경우 호출합니다.
	 *
	 * @param InComponent - 등록/업데이트할 컴포넌트
	 */
	void Update(UShapeComponent* InComponent);

	/**
	 * 컴포넌트를 BVH에서 제거합니다.
	 *
	 * @param InComponent - 제거할 컴포넌트
	 */
	void Remove(UShapeComponent* InComponent);

	/**
	 * 보류 중인 BVH 재구축을 즉시 실행합니다.
	 * Update 호출 후 쿼리 전에 호출해야 합니다.
	 */
	void FlushRebuild();

	// ────────────────────────────────────────────────
	// 쿼리 API
	// ────────────────────────────────────────────────

	/**
	 * 특정 컴포넌트와 겹칠 가능성이 있는 컴포넌트들을 반환합니다.
	 * Broad Phase 역할을 수행합니다.
	 *
	 * @param Component - 쿼리할 컴포넌트
	 * @return 겹칠 가능성이 있는 컴포넌트 배열
	 */
	TArray<UShapeComponent*> QueryOverlappingComponents(UShapeComponent* Component) const;

	/**
	 * 특정 AABB와 겹치는 컴포넌트들을 반환합니다.
	 *
	 * @param InBound - 쿼리할 AABB
	 * @return 겹치는 컴포넌트 배열
	 */
	TArray<UShapeComponent*> QueryIntersectedComponents(const FAABB& InBound) const;

	// ────────────────────────────────────────────────
	// 디버그 / 통계
	// ────────────────────────────────────────────────

	/**
	 * BVH 노드를 디버그 렌더링합니다.
	 *
	 * @param Renderer - 렌더러 객체
	 */
	void DebugDraw(URenderer* Renderer) const;

	/**
	 * 전체 노드 개수를 반환합니다.
	 *
	 * @return 노드 개수
	 */
	int TotalNodeCount() const;

	/**
	 * 등록된 컴포넌트 개수를 반환합니다.
	 *
	 * @return 컴포넌트 개수
	 */
	int TotalComponentCount() const;

	/**
	 * 실제로 사용 중인 최대 깊이를 반환합니다.
	 *
	 * @return 최대 깊이
	 */
	int MaxOccupiedDepth() const;

	/**
	 * BVH 구조를 콘솔에 출력합니다 (디버그용).
	 */
	void DebugDump() const;

	/**
	 * BVH 루트 노드의 경계를 반환합니다.
	 *
	 * @return AABB 경계
	 */
	const FAABB& GetBounds() const { return Bounds; }

private:
	// ────────────────────────────────────────────────
	// LBVH 노드 구조
	// ────────────────────────────────────────────────

	/**
	 * LBVH 노드
	 * 이진 트리 구조로 구성됩니다.
	 */
	struct FLBVHNode
	{
		/** 노드가 감싸는 AABB */
		FAABB Bounds;

		/** 왼쪽 자식 노드 인덱스 (-1이면 없음) */
		int32 Left = -1;

		/** 오른쪽 자식 노드 인덱스 (-1이면 없음) */
		int32 Right = -1;

		/** 리프 노드: 첫 번째 컴포넌트 인덱스 */
		int32 First = -1;

		/** 리프 노드: 컴포넌트 개수 */
		int32 Count = 0;

		/**
		 * 리프 노드 여부를 반환합니다.
		 *
		 * @return 리프 노드면 true
		 */
		bool IsLeaf() const { return Count > 0; }
	};

	// ────────────────────────────────────────────────
	// 내부 함수
	// ────────────────────────────────────────────────

	/**
	 * LBVH를 구축합니다.
	 * Morton Code 기반 정렬을 사용합니다.
	 */
	void BuildLBVH();

	/**
	 * 재귀적으로 BVH 노드를 구축합니다.
	 *
	 * @param s - 시작 인덱스
	 * @param e - 끝 인덱스
	 * @return 생성된 노드 인덱스
	 */
	int BuildRange(int s, int e);

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** 현재 깊이 */
	int Depth;

	/** 최대 깊이 */
	int MaxDepth;

	/** 리프 노드의 최대 오브젝트 수 */
	int MaxObjects;

	/** 루트 노드 경계 */
	FAABB Bounds;

	/** 컴포넌트 -> AABB 매핑 */
	TMap<UShapeComponent*, FAABB> ShapeComponentBounds;

	/** 컴포넌트 배열 (BuildLBVH에서 정렬됨) */
	TArray<UShapeComponent*> ShapeComponentArray;

	/** LBVH 노드 배열 */
	TArray<FLBVHNode> Nodes;

	/** 재구축 대기 플래그 */
	bool bPendingRebuild = false;
};
