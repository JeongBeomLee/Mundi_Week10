// ────────────────────────────────────────────────────────────────────────────
// CollisionManager.h
// 충돌 감지 시스템의 중앙 관리자
// ────────────────────────────────────────────────────────────────────────────
#pragma once
#include "Object.h"
#include "CollisionBVH.h"
#include <memory>

// Forward Declarations
class UShapeComponent;
class UWorld;
class URenderer;

/**
 * UCollisionManager
 *
 * 월드의 모든 ShapeComponent를 관리하고 충돌 감지를 수행하는 중앙 관리자입니다.
 * BVH(Bounding Volume Hierarchy)를 사용하여 O(log N) 성능의 충돌 감지를 제공합니다.
 *
 * 주요 기능:
 * - ShapeComponent 등록/해제
 * - 매 프레임 충돌 감지 업데이트
 * - BVH 기반 공간 분할 최적화
 * - Overlap 이벤트 발생
 *
 * 사용법:
 * - World::Initialize()에서 생성
 * - World::Tick()에서 UpdateCollisions() 호출
 * - ShapeComponent가 BeginPlay/EndPlay에서 자동 등록/해제
 */
class UCollisionManager : public UObject
{
public:
	DECLARE_CLASS(UCollisionManager, UObject)

	/**
	 * CollisionManager를 생성합니다.
	 *
	 * @param InWorld - 소유하는 월드
	 */
	UCollisionManager();
	~UCollisionManager() override;

	// ────────────────────────────────────────────────
	// 컴포넌트 등록/해제
	// ────────────────────────────────────────────────

	/**
	 * ShapeComponent를 충돌 시스템에 등록합니다.
	 * BeginPlay 시 자동으로 호출됩니다.
	 *
	 * @param Component - 등록할 컴포넌트
	 */
	void RegisterComponent(UShapeComponent* Component);

	/**
	 * ShapeComponent를 충돌 시스템에서 해제합니다.
	 * EndPlay 시 자동으로 호출됩니다.
	 *
	 * @param Component - 해제할 컴포넌트
	 */
	void UnregisterComponent(UShapeComponent* Component);

	/**
	 * 컴포넌트가 이동했음을 알립니다.
	 * Transform 변경 시 호출하여 BVH 업데이트를 예약합니다.
	 *
	 * @param Component - 이동한 컴포넌트
	 */
	void MarkComponentDirty(UShapeComponent* Component);

	// ────────────────────────────────────────────────
	// 충돌 업데이트
	// ────────────────────────────────────────────────

	/**
	 * 모든 충돌을 감지하고 Overlap 이벤트를 발생시킵니다.
	 * World::Tick()에서 매 프레임 호출됩니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void UpdateCollisions(float DeltaTime);

	/**
	 * BVH를 강제로 재구축합니다.
	 * 대량의 컴포넌트가 추가/제거/이동한 경우 호출합니다.
	 */
	void RebuildBVH();

	// ────────────────────────────────────────────────
	// 디버그
	// ────────────────────────────────────────────────

	/**
	 * 디버그 BVH를 렌더링합니다.
	 *
	 * @param Renderer - 렌더러 객체
	 */
	void DebugDrawBVH(URenderer* Renderer) const;

	/**
	 * 충돌 통계를 반환합니다.
	 *
	 * @param OutTotalComponents - 등록된 컴포넌트 수
	 * @param OutTotalNodes - BVH 노드 수
	 * @param OutMaxDepth - BVH 최대 깊이
	 */
	void GetStats(int& OutTotalComponents, int& OutTotalNodes, int& OutMaxDepth) const;

	/**
	 * 디버그 정보를 콘솔에 출력합니다.
	 */
	void DebugDump() const;

	// ────────────────────────────────────────────────
	// Getter
	// ────────────────────────────────────────────────

	/**
	 * 소유하는 월드를 반환합니다.
	 *
	 * @return World 객체
	 */
	UWorld* GetWorld() const { return World; }


	void SetWorld(UWorld* InWorld) { World = InWorld; }	
	/**
	 * 등록된 컴포넌트 배열을 반환합니다.
	 *
	 * @return 컴포넌트 배열
	 */
	const TArray<UShapeComponent*>& GetRegisteredComponents() const { return RegisteredComponents; }

	/**
	 * BVH 디버그 렌더링 활성화 여부
	 */
	bool bDebugDrawBVH = true;

private:
	// ────────────────────────────────────────────────
	// 내부 함수
	// ────────────────────────────────────────────────

	/**
	 * 증분 BVH 업데이트를 수행합니다.
	 * DirtyComponents만 업데이트합니다.
	 */
	void UpdateBVHIncremental();

	/**
	 * Dirty 플래그를 초기화합니다.
	 */
	void ClearDirtyFlags();

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** 소유하는 월드 */
	UWorld* World;

	/** BVH 구조 */
	std::unique_ptr<FCollisionBVH> BVH;

	/** 등록된 모든 컴포넌트 */
	TArray<UShapeComponent*> RegisteredComponents;

	/** 이동한 컴포넌트 (증분 업데이트용) */
	TArray<UShapeComponent*> DirtyComponents;

	/** 완전 재구축 필요 여부 */
	bool bNeedsFullRebuild = false;

	/** 이번 프레임에 처리된 충돌 쌍 수 (통계용) */
	int32 CollisionPairsChecked = 0;

	/** 이번 프레임에 발생한 Overlap 이벤트 수 (통계용) */
	int32 OverlapEventsTriggered = 0;
};
