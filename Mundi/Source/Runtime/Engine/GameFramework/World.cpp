#include "pch.h"
#include "SelectionManager.h"
#include "Picking.h"
#include "CameraActor.h"
#include "StaticMeshActor.h"
#include "CameraComponent.h"
#include "ObjectFactory.h"
#include "TextRenderComponent.h"
#include "FViewport.h"
#include "Windows/SViewportWindow.h"
#include "USlateManager.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Octree.h"
#include "BVHierarchy.h"
#include "Frustum.h"
#include "Occlusion.h"
#include "Gizmo/GizmoActor.h"
#include "Gizmo/OffscreenGizmoActor.h"
#include "Grid/GridActor.h"
#include "StaticMeshComponent.h"
#include "DirectionalLightActor.h"
#include "Frustum.h"
#include "Level.h"
#include "LightManager.h"
#include "ShadowManager.h"
#include "CollisionManager.h"
#include"Pawn.h"
#include"PlayerController.h"
#include "DeltaTimeManager.h"

IMPLEMENT_CLASS(UWorld)

UWorld::UWorld(EWorldType InWorldType)
	: WorldType(InWorldType)
	, Partition(new UWorldPartitionManager())
{
	SelectionMgr = std::make_unique<USelectionManager>();
	//PIE의 경우 Initalize 없이 빈 Level 생성만 해야함
	Level = std::make_unique<ULevel>();
	LightManager = std::make_unique<FLightManager>();
	ShadowManager = std::make_unique<FShadowManager>();
	CollisionManager = std::make_unique<UCollisionManager>();
	CollisionManager->SetWorld(this);

	DeltaTimeManager = std::make_unique<UDeltaTimeManager>();
}

UWorld::~UWorld()
{
	if (Level)
	{
		for (AActor* Actor : Level->GetActors())
		{
			ObjectFactory::DeleteObject(Actor);
		}
		Level->Clear();
	}
	for (AActor* Actor : EditorActors)
	{
		ObjectFactory::DeleteObject(Actor);
	}
	EditorActors.clear();

	GridActor = nullptr;
	GizmoActor = nullptr;
}

void UWorld::Initialize()
{
	// Embedded World는 기본 액터(라이트) 없이 빈 레벨 생성
	// Editor/Game World는 기본 라이트 포함된 레벨 생성
	if (WorldType == EWorldType::Embedded)
	{
		SetLevel(ULevelService::CreateDefaultLevel());
	}
	else
	{
		CreateLevel();
	}

	// Grid는 공간감을 위해 모든 World에 필요
	InitializeGrid();

	// Gizmo 초기화: Editor는 AGizmoActor, Embedded는 AOffscreenGizmoActor
	if (WorldType == EWorldType::Editor)
	{
		InitializeGizmo();
	}
	else if (WorldType == EWorldType::Embedded)
	{
		InitializeEmbeddedGizmo();
	}

	//// Pawn 생성
	//APawn* MyPawn = SpawnActor<APawn>();

	//// PlayerController 생성
	//APlayerController* PC = NewObject<APlayerController>();

	//// Pawn 빙의
	//PC->Possess(MyPawn);
}

void UWorld::InitializeGrid()
{
	GridActor = NewObject<AGridActor>();
	GridActor->SetWorld(this);
	GridActor->Initialize();

	EditorActors.push_back(GridActor);
}

void UWorld::InitializeGizmo()
{
	GizmoActor = NewObject<AGizmoActor>();
	GizmoActor->SetWorld(this);
	GizmoActor->SetActorTransform(FTransform(FVector{ 0, 0, 0 }, FQuat::MakeFromEulerZYX(FVector{ 0, -90, 0 }),
		FVector{ 1, 1, 1 }));

	EditorActors.push_back(GizmoActor);
}

void UWorld::InitializeEmbeddedGizmo()
{
	// Embedded World용 Gizmo (ImGui 입력 전용, SkeletalMeshEditor 등에서 사용)
	OffscreenGizmoActor = NewObject<AOffscreenGizmoActor>();
	OffscreenGizmoActor->SetWorld(this);
	OffscreenGizmoActor->PostActorCreated();
	OffscreenGizmoActor->SetTickInEditor(true);
	OffscreenGizmoActor->SetbRender(false);  // 초기에는 숨김 (본 선택 시 활성화)
	OffscreenGizmoActor->SetMode(EGizmoMode::Translate);
	OffscreenGizmoActor->SetSpace(EGizmoSpace::Local);

	EditorActors.push_back(OffscreenGizmoActor);

	// GizmoActor는 nullptr (Embedded에서는 AGizmoActor 사용 안 함)
	GizmoActor = nullptr;
}

void UWorld::Tick(float DeltaSeconds)
{
	//DeltaSeconds *= 0.1f; //시간 느리게 흐르도록 조정

	CurrentRealDeltaTime = DeltaSeconds;

	if (DeltaTimeManager)
	{
		DeltaTimeManager->Update(DeltaSeconds);
	}
	float ScaledDeltaTime = GetScaledDeltaTime(DeltaSeconds);

	Partition->Update(ScaledDeltaTime, /*budget*/256);

	//순서 바꾸면 안댐
	if (Level)
	{
		// Index-based iteration: Tick 중에 Actor가 추가/삭제되어도 안전
		const TArray<AActor*>& Actors = Level->GetActors();
		for (size_t i = 0; i < Actors.size(); ++i)
		{
			AActor* Actor = Actors[i];
			// PendingKill 상태인 Actor는 Tick하지 않음
			if (Actor && !Actor->IsPendingKill() && (Actor->CanTickInEditor() || bPie))
			{
				Actor->Tick(ScaledDeltaTime);
			}
		}
	}

	UScriptManager::GetInstance().UpdateCoroutineState(ScaledDeltaTime);

	// EditorActors도 인덱스 기반 순회로 변경
	for (size_t i = 0; i < EditorActors.size(); ++i)
	{
		AActor* EditorActor = EditorActors[i];
		if (EditorActor && !EditorActor->IsPendingKill() && !bPie)
		{
			EditorActor->Tick(DeltaSeconds); // ⭐ 실제 시간 사용
		}
	}

	// ⭐ 프레임 끝에 지연 삭제된 Actor들 처리: UpdateCollisions 이전에 호출되야 함!(Unregister를 이 함수 내에서 하므로)
	ProcessPendingActorDestruction();

	// 충돌 감지 업데이트
	if (CollisionManager)
	{
		CollisionManager->UpdateCollisions(DeltaSeconds);
	}
}

UWorld* UWorld::DuplicateWorldForPIE(UWorld* InEditorWorld)
{
	// 레벨 새로 생성
	// 월드 카피 및 월드에 이 새로운 레벨 할당
	// 월드 컨텍스트 새로 생성(월드타입, 카피한 월드)
	// 월드의 레벨에 원래 Actor들 다 복사
	// 해당 월드의 Initialize 호출?

	//ULevel* NewLevel = ULevelService::CreateNewLevel();
	UWorld* PIEWorld = NewObject<UWorld>(); // 레벨도 새로 생성됨
	PIEWorld->bPie = true;

	// EditorWorld의 PIE 설정 복사
	PIEWorld->GameModeClass = InEditorWorld->GameModeClass;
	PIEWorld->DefaultPawnClass = InEditorWorld->DefaultPawnClass;
	PIEWorld->PlayerControllerClass = InEditorWorld->PlayerControllerClass;
	PIEWorld->PlayerSpawnLocation = InEditorWorld->PlayerSpawnLocation;

	UE_LOG("[PIE] Copied settings from EditorWorld:");
	UE_LOG("  DefaultPawnClass: %s", PIEWorld->DefaultPawnClass ? PIEWorld->DefaultPawnClass->Name : "nullptr");
	UE_LOG("  PlayerControllerClass: %s", PIEWorld->PlayerControllerClass ? PIEWorld->PlayerControllerClass->Name : "nullptr");
	UE_LOG("  SpawnLocation: (%.1f, %.1f, %.1f)",
		PIEWorld->PlayerSpawnLocation.X,
		PIEWorld->PlayerSpawnLocation.Y,
		PIEWorld->PlayerSpawnLocation.Z);

	FWorldContext PIEWorldContext = FWorldContext(PIEWorld, EWorldType::Game);
	GEngine.AddWorldContext(PIEWorldContext);


	const TArray<AActor*>& SourceActors = InEditorWorld->GetLevel()->GetActors();
	for (AActor* SourceActor : SourceActors)
	{
		if (!SourceActor)
		{
			UE_LOG("Duplicate failed: SourceActor is nullptr");
			continue;
		}

		AActor* NewActor = SourceActor->Duplicate();

		if (!NewActor)
		{
			UE_LOG("Duplicate failed: NewActor is nullptr");
			continue;
		}
		PIEWorld->AddActorToLevel(NewActor);
		NewActor->SetWorld(PIEWorld);
		
	}
	PIEWorld->MainCameraActor = InEditorWorld->GetCameraActor();
	return PIEWorld;
}

FString UWorld::GenerateUniqueActorName(const FString& ActorType)
{
	// GetInstance current count for this type
	int32& CurrentCount = ObjectTypeCounts[ActorType];
	FString UniqueName = ActorType + "_" + std::to_string(CurrentCount);
	CurrentCount++;
	return UniqueName;
}

//
// 액터 제거
//
bool UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor) return false;

	// 재진입 가드
	if (Actor->IsPendingKill()) return false;

	// 지연 삭제 큐에 추가
	MarkActorForDestruction(Actor);
	return true;
}

void UWorld::OnActorSpawned(AActor* Actor)
{
	if (!Actor) return;

	// Level에 Actor 추가 (SceneManagerWidget에 표시되도록)
	AddActorToLevel(Actor);
}

void UWorld::OnActorDestroyed(AActor* Actor)
{
	if (Actor)
	{
		Partition->Unregister(Actor);
	}
}

// 지연 삭제 큐에 Actor 추가
void UWorld::MarkActorForDestruction(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	Actor->MarkPendingKill();
	PendingDestroyActors.push_back(Actor);

	UE_LOG("[World] Marked actor '%s' for destruction (deferred)", 
		Actor->GetName().ToString().c_str());
}

// 지연 삭제 큐의 Actor들을 실제로 삭제
void UWorld::ProcessPendingActorDestruction()
{
	if (PendingDestroyActors.empty())
	{
		return;
	}

	UE_LOG("[World] Processing %d pending actor destruction(s)", 
		static_cast<int>(PendingDestroyActors.size()));

	// 복사본으로 처리 (처리 중 새로운 삭제 요청이 들어올 수 있음)
	TArray<AActor*> ActorsToDestroy = PendingDestroyActors;
	PendingDestroyActors.clear();

	for (AActor* Actor : ActorsToDestroy)
	{
		if (!Actor)
		{
			continue;
		}

		// 선택/UI 해제
		if (SelectionMgr)
		{
			SelectionMgr->DeselectActor(Actor);
		}

		Actor->EndPlay(EEndPlayReason::Destroyed);

		// 컴포넌트 정리 (등록 해제 → 파괴)
		TArray<USceneComponent*> Components = Actor->GetSceneComponents();
		for (USceneComponent* Comp : Components)
		{
			if (Comp)
			{
				Comp->SetOwner(nullptr); // 소유자 해제
			}
		}

		// 옥트리에서 제거
		OnActorDestroyed(Actor);

		// 실제 파괴 수행
		Actor->DestroyImmediate();

		// 레벨에서 제거
		if (Level && Level->RemoveActor(Actor))
		{
			// 옥트리에서 제거
			OnActorDestroyed(Actor);

			// 메모리 해제
			ObjectFactory::DeleteObject(Actor);

			// 삭제된 액터 정리
			if (SelectionMgr)
			{
				SelectionMgr->CleanupInvalidActors();
				SelectionMgr->ClearSelection();
			}
		}
	}

	// 삭제된 액터 정리
	if (SelectionMgr)
	{
		SelectionMgr->CleanupInvalidActors();
	}

	UE_LOG("[World] Finished processing pending destructions");
}

void UWorld::CreateLevel()
{
	if (SelectionMgr) SelectionMgr->ClearSelection();
	
	SetLevel(ULevelService::CreateNewLevel());
	// 이름 카운터 초기화: 씬을 새로 시작할 때 각 BaseName 별 suffix를 0부터 다시 시작
	ObjectTypeCounts.clear();
}

//어느 레벨이든 기본적으로 존재하는 엑터(디렉셔널 라이트) 생성
void UWorld::SpawnDefaultActors()
{
	SpawnActor<ADirectionalLightActor>();
}
void UWorld::SetLevel(std::unique_ptr<ULevel> InLevel)
{
    // Make UI/selection safe before destroying previous actors
    if (SelectionMgr) SelectionMgr->ClearSelection();

    // Cleanup current
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            ObjectFactory::DeleteObject(Actor);
        }
        Level->Clear();
    }
    // Clear spatial indices
    Partition->Clear();

    Level = std::move(InLevel);

    // Adopt actors: set world and register
    if (Level)
    {
		Partition->BulkRegister(Level->GetActors());
        for (AActor* A : Level->GetActors())
        {
            if (!A) continue;
            A->SetWorld(this);
        }
    }

    // Clean any dangling selection references just in case
    if (SelectionMgr) SelectionMgr->CleanupInvalidActors();
}

float UWorld::GetScaledDeltaTime(float RealDeltaTime) const
{
	return DeltaTimeManager ? DeltaTimeManager->GetScaledDeltaTime(RealDeltaTime) : RealDeltaTime;
}

void UWorld::AddActorToLevel(AActor* Actor)
{
	if (Level) 
	{
		Level->AddActor(Actor);
		Partition->Register(Actor);

		// Lua Scripting 작동을 확인하기 위한 임시 코드
		// FLuaLocalValue LuaLocalValue = {Actor};
		//
		// UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, "actor_transform.lua");
	}
}

AActor* UWorld::SpawnActor(UClass* Class, const FTransform& Transform)
{
	// 유효성 검사: Class가 유효하고 AActor를 상속했는지 확인
	if (!Class || !Class->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG("SpawnActor failed: Invalid class provided.");
		return nullptr;
	}

	// ObjectFactory를 통해 UClass*로부터 객체 인스턴스 생성
	AActor* NewActor = Cast<AActor>(ObjectFactory::NewObject(Class));
	if (!NewActor)
	{
		UE_LOG("SpawnActor failed: ObjectFactory could not create an instance of");
		return nullptr;
	}

	// 초기 트랜스폼 적용
	NewActor->SetActorTransform(Transform);

	// 월드 참조 설정
	NewActor->SetWorld(this);

	// PostActorCreated 호출 (World 설정 완료 후 초기화)
	NewActor->PostActorCreated();

	// 현재 레벨에 액터 등록
	AddActorToLevel(NewActor);

	return NewActor;
}

AActor* UWorld::SpawnActor(UClass* Class)
{
	// 기본 Transform(원점)으로 스폰하는 메인 함수를 호출합니다.
	return SpawnActor(Class, FTransform());
}
