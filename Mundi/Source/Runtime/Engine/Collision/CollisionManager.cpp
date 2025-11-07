// ────────────────────────────────────────────────────────────────────────────
// CollisionManager.cpp
// 충돌 감지 시스템의 중앙 관리자 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CollisionManager.h"
#include "CollisionComponent/ShapeComponent.h"
#include "World.h"
#include "Renderer.h"

IMPLEMENT_CLASS(UCollisionManager)

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCollisionManager::UCollisionManager()

{
	// BVH 초기화 (월드 크기에 맞게 설정)
	FAABB WorldBounds(FVector(-100000, -100000, -100000), FVector(100000, 100000, 100000));
	BVH = std::make_unique<FCollisionBVH>(WorldBounds, 0, 12, 8);
}

UCollisionManager::~UCollisionManager()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 컴포넌트 등록/해제
// ────────────────────────────────────────────────────────────────────────────

void UCollisionManager::RegisterComponent(UShapeComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 이미 등록된 컴포넌트는 무시
	if (RegisteredComponents.Contains(Component))
	{
		return;
	}

	// Overlap 이벤트를 생성하지 않으면 등록하지 않음
	if (!Component->bGenerateOverlapEvents)
	{
		return;
	}

	// 컴포넌트 등록
	RegisteredComponents.push_back(Component);

	// BVH에 추가
	BVH->Update(Component);

	// 대량 등록 시 재구축 플래그 설정
	bNeedsFullRebuild = true;
}

void UCollisionManager::UnregisterComponent(UShapeComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 등록되지 않은 컴포넌트는 무시
	if (!RegisteredComponents.Contains(Component))
	{
		return;
	}

	// 컴포넌트 제거
	RegisteredComponents.erase(
		std::remove(RegisteredComponents.begin(), RegisteredComponents.end(), Component),
		RegisteredComponents.end()
	);

	// BVH에서 제거
	BVH->Remove(Component);

	// Dirty 목록에서도 제거
	DirtyComponents.erase(
		std::remove(DirtyComponents.begin(), DirtyComponents.end(), Component),
		DirtyComponents.end()
	);

	bNeedsFullRebuild = true;
}

void UCollisionManager::MarkComponentDirty(UShapeComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 등록된 컴포넌트만 Dirty 마킹
	if (!RegisteredComponents.Contains(Component))
	{
		return;
	}

	// 이미 Dirty 목록에 있으면 무시
	if (DirtyComponents.Contains(Component))
	{
		return;
	}

	DirtyComponents.push_back(Component);
}

// ────────────────────────────────────────────────────────────────────────────
// 충돌 업데이트
// ────────────────────────────────────────────────────────────────────────────

void UCollisionManager::UpdateCollisions(float DeltaTime)
{
	// 통계 초기화
	CollisionPairsChecked = 0;
	OverlapEventsTriggered = 0;

	// 1. BVH 업데이트
	if (bNeedsFullRebuild)
	{
		RebuildBVH();
		bNeedsFullRebuild = false;
	}
	else if (!DirtyComponents.empty())
	{
		UpdateBVHIncremental();
	}

	// 2. BVH 재구축 플러시
	BVH->FlushRebuild();

	// 3. 모든 컴포넌트에 대해 충돌 체크
	for (UShapeComponent* Comp : RegisteredComponents)
	{
		if (!Comp || !Comp->bGenerateOverlapEvents)
		{
			continue;
		}

		// BVH 쿼리: 이 컴포넌트와 겹칠 가능성이 있는 컴포넌트들만 반환 (Broad Phase)
		TArray<UShapeComponent*> PotentialOverlaps = BVH->QueryOverlappingComponents(Comp);

		// 자기 자신 제거
		PotentialOverlaps.erase(
			std::remove(PotentialOverlaps.begin(), PotentialOverlaps.end(), Comp),
			PotentialOverlaps.end()
		);

		CollisionPairsChecked += PotentialOverlaps.Num();

		// Narrow Phase: 정밀 충돌 체크 + 이벤트 발생
		// UpdateOverlaps가 내부적으로 BeginOverlap/EndOverlap 델리게이트 호출
		Comp->UpdateOverlaps(PotentialOverlaps);
	}

	// 4. Dirty 플래그 초기화
	ClearDirtyFlags();
}

void UCollisionManager::RebuildBVH()
{
	if (!BVH)
	{
		return;
	}

	//UE_LOG("CollisionManager: Rebuilding BVH with {} components", RegisteredComponents.Num());

	// BVH 완전 재구축
	BVH->BulkUpdate(RegisteredComponents);
}

// ────────────────────────────────────────────────────────────────────────────
// 디버그
// ────────────────────────────────────────────────────────────────────────────

void UCollisionManager::DebugDrawBVH(URenderer* Renderer) const
{
	if (!Renderer || !BVH)
	{
		return;
	}

	BVH->DebugDraw(Renderer);
}

void UCollisionManager::GetStats(int& OutTotalComponents, int& OutTotalNodes, int& OutMaxDepth) const
{
	if (!BVH)
	{
		OutTotalComponents = 0;
		OutTotalNodes = 0;
		OutMaxDepth = 0;
		return;
	}

	OutTotalComponents = BVH->TotalComponentCount();
	OutTotalNodes = BVH->TotalNodeCount();
	OutMaxDepth = BVH->MaxOccupiedDepth();
}

void UCollisionManager::DebugDump() const
{
	UE_LOG("===== CollisionManager Debug Info =====");
	UE_LOG("Registered Components: {}", RegisteredComponents.Num());
	UE_LOG("Dirty Components: {}", DirtyComponents.Num());
	UE_LOG("Collision Pairs Checked (Last Frame): {}", CollisionPairsChecked);
	UE_LOG("Overlap Events Triggered (Last Frame): {}", OverlapEventsTriggered);

	int TotalComponents, TotalNodes, MaxDepth;
	GetStats(TotalComponents, TotalNodes, MaxDepth);
	UE_LOG("BVH - Components: {}, Nodes: {}, Max Depth: {}", TotalComponents, TotalNodes, MaxDepth);

	if (BVH)
	{
		BVH->DebugDump();
	}

	UE_LOG("===== CollisionManager Debug End =====");
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 함수
// ────────────────────────────────────────────────────────────────────────────

void UCollisionManager::UpdateBVHIncremental()
{
	if (!BVH)
	{
		return;
	}

	// Dirty 컴포넌트만 증분 업데이트
	for (UShapeComponent* Comp : DirtyComponents)
	{
		if (Comp && Comp->bGenerateOverlapEvents)
		{
			BVH->Update(Comp);
		}
	}

	// 너무 많은 컴포넌트가 Dirty면 완전 재구축
	if (DirtyComponents.Num() > RegisteredComponents.Num() / 2)
	{
		bNeedsFullRebuild = true;
	}
}

void UCollisionManager::ClearDirtyFlags()
{
	DirtyComponents.clear();
}
