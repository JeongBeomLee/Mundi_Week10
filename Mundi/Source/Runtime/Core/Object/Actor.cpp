#include "pch.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"
#include "MeshComponent.h"
#include "TextRenderComponent.h"
#include "WorldPartitionManager.h"
#include "BillboardComponent.h"
#include "AABB.h"
#include "JsonSerializer.h"
#include "World.h"
#include "CollisionComponent/ShapeComponent.h"
#include "GameModeBase.h"

IMPLEMENT_CLASS(AActor)

AActor::AActor()
{
	Name = "DefaultActor";
}

AActor::~AActor()
{
	// GameMode 델리게이트 구독 해제 (이 Actor에 등록된 핸들만 제거)
	if (World)
	{
		AGameModeBase* GameMode = World->GetGameMode();
		if (GameMode)
		{
			if (GameStartedHandle != 0)
				GameMode->OnGameStarted.RemoveDynamic(GameStartedHandle);
			if (GameEndedHandle != 0)
				GameMode->OnGameEnded.RemoveDynamic(GameEndedHandle);
			if (GameRestartedHandle != 0)
				GameMode->OnGameRestarted.RemoveDynamic(GameRestartedHandle);
			if (GamePausedHandle != 0)
				GameMode->OnGamePaused.RemoveDynamic(GamePausedHandle);
			if (GameResumedHandle != 0)
				GameMode->OnGameResumed.RemoveDynamic(GameResumedHandle);
		}
	}

	// UE처럼 역순/안전 소멸: 모든 컴포넌트 DestroyComponent
	// DestroyComponent 내부에서 RemoveOwnedComponent를 호출하므로 복사본으로 순회
	TSet<UActorComponent*> ComponentsCopy = OwnedComponents;
	for (UActorComponent* Comp : ComponentsCopy)
		if (Comp && !Comp->IsPendingDestroy())
			Comp->DestroyComponent();  // 안에서 Unregister/Detach/Remove 처리

	OwnedComponents.clear();
	SceneComponents.Empty();
	RootComponent = nullptr;

	// Lua 스크립트는 이미 DestroyImmediate에서 정리되었으므로 제거
	// UScriptManager::GetInstance().DetachAllScriptFrom(const_cast<AActor*>(this));
}

void AActor::BeginPlay()
{
	// 컴포넌트들 Initialize/BeginPlay 순회
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->InitializeComponent();
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->BeginPlay();

	for (FScript* Script : UScriptManager::GetInstance().GetScriptsOfActor(this))
	{
		Script->LuaTemplateFunctions.BeginPlay();
	}

	// GameMode 델리게이트 구독 (핸들 저장)
	if (World)
	{
		AGameModeBase* GameMode = World->GetGameMode();
		if (GameMode)
		{
			GameStartedHandle = GameMode->OnGameStarted.AddDynamic(this, &AActor::OnGameStartedHandler);
			GameEndedHandle = GameMode->OnGameEnded.AddDynamic(this, &AActor::OnGameEndedHandler);
			GameRestartedHandle = GameMode->OnGameRestarted.AddDynamic(this, &AActor::OnGameRestartedHandler);
			GamePausedHandle = GameMode->OnGamePaused.AddDynamic(this, &AActor::OnGamePausedHandler);
			GameResumedHandle = GameMode->OnGameResumed.AddDynamic(this, &AActor::OnGameResumedHandler);

			//UE_LOG("[Actor] Subscribed to GameMode delegates");
		}
	}
}


void AActor::Tick(float DeltaSeconds)
{
	// 에디터에서 틱 Off면 스킵
	if (!bTickInEditor && World->bPie == false) return;

	// 게임이 시작되지 않았으면 Tick 실행하지 않음 (PIE 모드에서만)
	

	// Lua 스크립트 먼저 실행 (입력 처리를 위해)
	for (FScript* Script : UScriptManager::GetInstance().GetScriptsOfActor(this))
	{
		Script->LuaTemplateFunctions.Tick(DeltaSeconds);
	}

	// 컴포넌트 Tick (Lua에서 설정한 입력 사용)
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp && Comp->IsComponentTickEnabled())
		{
			Comp->TickComponent(DeltaSeconds /*, … 필요 인자*/);
		}
	}
}
void AActor::EndPlay(EEndPlayReason Reason)
{
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp)
		{
			//if(UShapeComponent* ShapeComp = Cast<UShapeComponent>(Comp))
			//{
			//	// 충돌 처리 컴포넌트일 경우 Script의 OnOverlap function 연결 해제
			//	TArray<FScript*> Scripts = UScriptManager::GetInstance().GetScriptsOfActor(this);
			//	ShapeComp->OnComponentBeginOverlap.RemoveAll();
			//}
			Comp->EndPlay(Reason);
		}
	}

	if (Reason == EEndPlayReason::EndPlayInEditor)
	{
		for (FScript* Script : UScriptManager::GetInstance().GetScriptsOfActor(this))
		{
			Script->LuaTemplateFunctions.EndPlay();
		}
	}
}
void AActor::Destroy()
{
	// 이미 파괴 예약 중이면 중복 호출 방지
	if (bPendingKill)
	{
		return;
	}

	// 파괴 예약 플래그 설정
	bPendingKill = true;

	// 월드가 있으면 월드의 지연 삭제 큐에 추가
	if (World) 
	{ 
		World->MarkActorForDestruction(this);
		return; 
	}

	// 월드가 없을 때만 즉시 정리 (예외 상황)
	DestroyImmediate();
}

void AActor::DestroyImmediate()
{
	// 실제 파괴 로직
	UnregisterAllComponents(true);
	DestroyAllComponents();
	ClearSceneComponentCaches();
	
	// Lua 스크립트 정리
	UScriptManager::GetInstance().DetachAllScriptFrom(this);
}



void AActor::SetRootComponent(USceneComponent* InRoot)
{
	if (RootComponent == InRoot)
	{
		return;
	}

	// 루트 교체
	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = InRoot;
	RemoveOwnedComponent(TempRootComponent);

	if (RootComponent)
	{
		RootComponent->SetOwner(this);
	}
}

void AActor::AddOwnedComponent(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (OwnedComponents.count(Component))
	{
		return;
	}

	OwnedComponents.insert(Component);
	Component->SetOwner(this);
	if (USceneComponent* SC = Cast<USceneComponent>(Component))
	{
		SceneComponents.AddUnique(SC);
		// 루트가 없으면 자동 루트 지정
		if (!RootComponent)
		{
			SetRootComponent(SC);
		}
	}

	// 충돌 처리 컴포넌트일 경우 Script의 OnOverlap function을 연결
	if (UShapeComponent* ShapeComponent = Cast<UShapeComponent>(Component))
	{
		TArray<FScript*> Scripts = UScriptManager::GetInstance().GetScriptsOfActor(this);

		for (FScript* Script : Scripts)
		{
			Script->LuaTemplateFunctions.OnOverlapDelegateHandle = ShapeComponent->OnComponentBeginOverlap.Add(
			Script->LuaTemplateFunctions.OnOverlap
			);
		}
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (!OwnedComponents.count(Component) || RootComponent == Component)
	{
		return;
	}

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		// 자식 컴포넌트들을 먼저 재귀적으로 삭제
		// (자식을 먼저 삭제하면 부모의 AttachChildren이 변경되므로 복사본으로 순회)
		TArray<USceneComponent*> ChildrenCopy = SceneComponent->GetAttachChildren();
		for (USceneComponent* Child : ChildrenCopy)
		{
			RemoveOwnedComponent(Child); // 재귀 호출로 자식들 먼저 삭제
		}

		if (SceneComponent == RootComponent)
		{
			RootComponent = nullptr;
		}

		SceneComponents.Remove(SceneComponent);
		GWorld->GetPartitionManager()->Unregister(SceneComponent);
		SceneComponent->DetachFromParent(true);
	}

	// OwnedComponents에서 제거
	OwnedComponents.erase(Component);

	Component->UnregisterComponent();
	Component->DestroyComponent();
}

void AActor::RegisterAllComponents(UWorld* InWorld)
{

	for (UActorComponent* Component : OwnedComponents)
	{
		Component->RegisterComponent(InWorld);
	}
	if (!RootComponent)
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>("DefaultSceneComponent");
		RootComponent->RegisterComponent(InWorld);
	}
}

void AActor::UnregisterAllComponents(bool bCallEndPlayOnBegun)
{
	// 파괴 경로에서 안전하게 등록 해제
	// 복사본으로 순회 (컨테이너 변형 중 안전)
	TArray<UActorComponent*> Temp;
	Temp.reserve(OwnedComponents.size());
	for (UActorComponent* C : OwnedComponents) Temp.push_back(C);

	for (UActorComponent* C : Temp)
	{
		if (!C) continue;

		// BeginPlay가 이미 호출된 컴포넌트라면 EndPlay(RemovedFromWorld) 보장
		if (bCallEndPlayOnBegun && C->HasBegunPlay())
		{
			C->EndPlay(EEndPlayReason::RemovedFromWorld);
		}
		C->UnregisterComponent(); // 내부 OnUnregister/리소스 해제
	}
}

void AActor::DestroyAllComponents()
{
	// Unregister 이후 최종 파괴
	TArray<UActorComponent*> Temp;
	Temp.reserve(OwnedComponents.size());
	for (UActorComponent* C : OwnedComponents) Temp.push_back(C);

	for (UActorComponent* C : Temp)
	{
		if (!C) continue;
		C->DestroyComponent(); // 내부에서 Owner=nullptr 등도 처리
	}
	OwnedComponents.clear();
}

void AActor::ClearSceneComponentCaches()
{
	SceneComponents.Empty();
	RootComponent = nullptr;
}

// ───────────────
// Transform API
// ───────────────
void AActor::SetActorTransform(const FTransform& NewTransform)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	FTransform OldTransform = RootComponent->GetWorldTransform();
	if (!(OldTransform == NewTransform))
	{
		RootComponent->SetWorldTransform(NewTransform);
		MarkPartitionDirty();
	}
}

FTransform AActor::GetActorTransform() const
{
	return RootComponent ? RootComponent->GetWorldTransform() : FTransform();
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	FVector OldLocation = RootComponent->GetWorldLocation();
	if (!(OldLocation == NewLocation)) // 위치가 실제로 변경되었을 때만
	{
		RootComponent->SetWorldLocation(NewLocation);
		MarkPartitionDirty();
	}
}

FVector AActor::GetActorLocation() const
{
	return RootComponent ? RootComponent->GetWorldLocation() : FVector();
}

void AActor::MarkPartitionDirty()
{
	if(GetWorld() && GetWorld()->GetPartitionManager())
		GetWorld()->GetPartitionManager()->MarkDirty(this);
}

void AActor::SetActorRotation(const FVector& EulerDegree)
{
	if (RootComponent)
	{
		FQuat NewRotation = FQuat::MakeFromEulerZYX(EulerDegree);
		FQuat OldRotation = RootComponent->GetWorldRotation();
		if (!(OldRotation == NewRotation)) // 회전이 실제로 변경되었을 때만
		{
			RootComponent->SetWorldRotation(NewRotation);
			MarkPartitionDirty();
		}
	}
}

void AActor::SetActorRotation(const FQuat& InQuat)
{
	if (RootComponent == nullptr)
	{
		return;
	}
	FQuat OldRotation = RootComponent->GetWorldRotation();
	if (!(OldRotation == InQuat)) // 회전이 실제로 변경되었을 때만
	{
		RootComponent->SetWorldRotation(InQuat);
		MarkPartitionDirty();
	}
}

FQuat AActor::GetActorRotation() const
{
	return RootComponent ? RootComponent->GetWorldRotation() : FQuat();
}

void AActor::SetActorScale(const FVector& NewScale)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	FVector OldScale = RootComponent->GetWorldScale();
	if (!(OldScale == NewScale)) // 스케일이 실제로 변경되었을 때만
	{
		RootComponent->SetWorldScale(NewScale);
		MarkPartitionDirty();
	}
}

FVector AActor::GetActorScale() const
{
	return RootComponent ? RootComponent->GetWorldScale() : FVector(1, 1, 1);
}

FMatrix AActor::GetWorldMatrix() const
{
	if (RootComponent == nullptr)
	{
		UE_LOG("RootComponent is nullptr");
	}
	return RootComponent ? RootComponent->GetWorldMatrix() : FMatrix::Identity();
}

void AActor::AddActorWorldRotation(const FQuat& DeltaRotation)
{
	if (RootComponent && !DeltaRotation.IsIdentity()) // 단위 쿼터니온이 아닐 때만
	{
		RootComponent->AddWorldRotation(DeltaRotation);
		MarkPartitionDirty();
	}
}

void AActor::AddActorWorldRotation(const FVector& DeltaEuler)
{
	/* if (RootComponent)
	 {
		 FQuat DeltaQuat = FQuat::FromEuler(DeltaEuler.X, DeltaEuler.Y, DeltaEuler.Z);
		 RootComponent->AddWorldRotation(DeltaQuat);
	 }*/
}

void AActor::AddActorWorldLocation(const FVector& DeltaLocation)
{
	if (RootComponent && !DeltaLocation.IsZero()) // 영 벡터가 아닐 때만
	{
		RootComponent->AddWorldOffset(DeltaLocation);
		MarkPartitionDirty();
	}
}

void AActor::AddActorLocalRotation(const FVector& DeltaEuler)
{
	/*  if (RootComponent)
	  {
		  FQuat DeltaQuat = FQuat::FromEuler(DeltaEuler.X, DeltaEuler.Y, DeltaEuler.Z);
		  RootComponent->AddLocalRotation(DeltaQuat);
	  }*/
}

void AActor::AddActorLocalRotation(const FQuat& DeltaRotation)
{
	if (RootComponent && !DeltaRotation.IsIdentity()) // 단위 쿼터니온이 아닐 때만
	{
		RootComponent->AddLocalRotation(DeltaRotation);
		MarkPartitionDirty();
	}
}

void AActor::AddActorLocalLocation(const FVector& DeltaLocation)
{
	if (RootComponent && !DeltaLocation.IsZero()) // 영 벡터가 아닐 때만
	{
		RootComponent->AddLocalOffset(DeltaLocation);
		MarkPartitionDirty();
	}
}

void AActor::SetActorHiddenInEditor(bool bNewHidden)
{
	bHiddenInEditor = bNewHidden; 
	GWorld->GetLightManager()->SetDirtyFlag();
}

bool AActor::IsActorVisible() const
{
	return GWorld->bPie ? !bHiddenInGame : !bHiddenInEditor;
}

void AActor::PostDuplicate()
{
	Super::PostDuplicate();

	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp)
		{
			Comp->SetRegistered(false);
		}
	}
}

void AActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 기본 프로퍼티 초기화
	bIsPicked = false;
	bCanEverTick = true;
	bHiddenInEditor = false;
	bIsCulled = false;
	World = nullptr; // PIE World는 복제 프로세스의 상위 레벨에서 설정해 주어야 합니다.

	if (OwnedComponents.empty())
	{
		return; // 복제할 컴포넌트가 없으면 종료
	}

	// ========================================================================
	// 1단계: 모든 컴포넌트 복제 및 '원본 -> 사본' 매핑 테이블 생성
	// ========================================================================
	TMap<UActorComponent*, UActorComponent*> OldToNewComponentMap;
	TSet<UActorComponent*> NewOwnedComponents;

	for (UActorComponent* OriginalComp : OwnedComponents)
	{
		if (!OriginalComp) continue;

		// NOTE: 이 코드가 없으면 Direction을 나타내는 GizmoComponent가 PIE World에 복사되는 버그가 발생
		// PIE 모드로 복사할 때, 에디터 전용 컴포넌트(bIsEditable == false)는 복사하지 않음
		// BillboardComponent, DirectionGizmo(GizmoArrowComponent) 등이 CREATE_EDITOR_COMPONENT로 생성되며 bIsEditable = false
		if (!OriginalComp->IsEditable())
		{
			continue; // 에디터 전용 컴포넌트는 PIE World로 복사하지 않음
		}
		// 컴포넌트를 깊은 복사합니다.
		UActorComponent* NewComp = OriginalComp->Duplicate();
		NewComp->SetOwner(this);
		
		// 매핑 테이블에 (원본 포인터, 새 포인터) 쌍을 기록합니다.
		OldToNewComponentMap.insert({ OriginalComp, NewComp });

		// 새로운 소유 컴포넌트 목록에 추가합니다.
		NewOwnedComponents.insert(NewComp);
	}

	// 복제된 컴포넌트 목록으로 교체합니다.
	OwnedComponents = NewOwnedComponents;

	// ========================================================================
	// 2단계: 매핑 테이블을 이용해 씬 계층 구조 재구성
	// ========================================================================

	// 2-1. 새로운 루트 컴포넌트 설정
	UActorComponent** FoundNewRootPtr = OldToNewComponentMap.Find(RootComponent);
	if (FoundNewRootPtr)
	{
		RootComponent = Cast<USceneComponent>(*FoundNewRootPtr);
	}
	else
	{
		// 원본 루트를 찾지 못하는 심각한 오류. 
		// 이 경우엔 첫 번째 씬 컴포넌트를 임시 루트로 삼거나 에러 처리.
		RootComponent = nullptr;
	}

	// 2-2. 모든 씬 컴포넌트의 부모-자식 관계 재연결
	SceneComponents.clear(); // 새 컴포넌트로 목록을 다시 채웁니다.
	if (RootComponent)
	{
		SceneComponents.push_back(RootComponent);
	}

	for (auto const& [OriginalComp, NewComp] : OldToNewComponentMap)
	{
		USceneComponent* OriginalSceneComp = Cast<USceneComponent>(OriginalComp);
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewComp);

		if (OriginalSceneComp && NewSceneComp)
		{
			// 루트가 아닌 경우에만 부모를 찾아 연결합니다.
			if (OriginalSceneComp != GetRootComponent()) // 여기서 비교는 원본 액터의 루트와 해야 합니다.
			{
				USceneComponent* OriginalParent = OriginalSceneComp->GetAttachParent();
				if (OriginalParent)
				{
					// 매핑 테이블에서 원본 부모에 해당하는 '새로운 부모'를 찾습니다.
					UActorComponent** FoundNewParentPtr = OldToNewComponentMap.Find(OriginalParent);
					if (FoundNewParentPtr)
					{
						if (USceneComponent* ParentSceneComponent = Cast<USceneComponent>(*FoundNewParentPtr))
						{
							NewSceneComp->SetupAttachment(ParentSceneComponent, EAttachmentRule::KeepRelative);
						}
					}
				}
			}

			// 루트가 아닌 씬 컴포넌트들을 캐시 목록에 추가합니다.
			if (NewSceneComp != RootComponent)
			{
				SceneComponents.push_back(NewSceneComp);
			}
		}
	}
}



void AActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		uint32 RootUUID;
		FJsonSerializer::ReadUint32(InOutHandle, "RootComponentId", RootUUID);
	
		FString NameStrTemp;
		FJsonSerializer::ReadString(InOutHandle, "Name", NameStrTemp);
		SetName(NameStrTemp);
	
		JSON ComponentsJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "OwnedComponents", ComponentsJson))
		{
			// 1) OwnedComponents와 SceneComponents에 Component들 추가
			for (uint32 i = 0; i < static_cast<uint32>(ComponentsJson.size()); ++i)
			{
				JSON ComponentJson = ComponentsJson.at(i);
				
				FString TypeString;
				FJsonSerializer::ReadString(ComponentJson, "Type", TypeString);
	
				UClass* NewClass = UClass::FindClass(TypeString);
	
				UActorComponent* NewComponent = Cast<UActorComponent>(ObjectFactory::NewObject(NewClass));
	
				NewComponent->Serialize(bInIsLoading, ComponentJson);
	
				// RootComponent 설정
				if (USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent))
				{
					if (RootUUID == NewSceneComponent->GetSceneId())
					{
						assert(NewSceneComponent);
						SetRootComponent(NewSceneComponent);
					}
				}
	
				// OwnedComponents와 SceneComponents에 Component 추가
				AddOwnedComponent(NewComponent);
			}
	
			// 2) 컴포넌트 간 부모 자식 관계 설정
			for (auto& Component : OwnedComponents)
			{
				USceneComponent* SceneComp = Cast<USceneComponent>(Component);
				if (!SceneComp)
				{
					continue;
				}
				uint32 ParentId = SceneComp->GetParentId();
				if (ParentId != 0) // RootComponent가 아니면 부모 설정
				{
					USceneComponent** ParentP = SceneComp->GetSceneIdMap().Find(ParentId);
					USceneComponent* Parent = *ParentP;

					SceneComp->SetupAttachment(Parent, EAttachmentRule::KeepRelative);
				}
			}
		}

		// Script 역직렬화
		JSON ScriptsJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "Scripts", ScriptsJson))
		{
			for (uint32 i = 0; i < static_cast<uint32>(ScriptsJson.size()); ++i)
			{
				FString ScriptName = ScriptsJson.at(i).ToString();
				if (!ScriptName.empty())
				{
					try
					{
						FLuaLocalValue LuaLocalValue;
						LuaLocalValue.MyActor = this;
						UScriptManager::GetInstance().AttachScriptTo(LuaLocalValue, ScriptName);
					}
					catch (const std::exception& e)
					{
						// Script 파일이 없거나 로드 실패 시 경고만 출력하고 계속 진행
						UE_LOG("[Actor] Failed to attach script '%s' to actor '%s': %s",
							   ScriptName.c_str(),
							   GetName().ToString().c_str(),
							   e.what());
					}
				}
			}
		}
	}
	else if (RootComponent)
	{
		InOutHandle["RootComponentId"] = RootComponent->UUID;
	
		JSON Components = JSON::Make(JSON::Class::Array);
		for (auto& Component : OwnedComponents)
		{
			// 에디터 전용 컴포넌트는 직렬화하지 않음
			// (CREATE_EDITOR_COMPONENT로 생성된 컴포넌트들은 OnRegister()에서 매번 새로 생성됨)
			if (!Component->IsEditable())
			{
				continue;
			}
	
			JSON ComponentJson;
	
			ComponentJson["Type"] = Component->GetClass()->Name;
	
			Component->Serialize(bInIsLoading, ComponentJson);
			Components.append(ComponentJson);
		}
		InOutHandle["OwnedComponents"] = Components;
		InOutHandle["Name"] = GetName().ToString();

		// Script 직렬화
		TArray<FScript*> Scripts = UScriptManager::GetInstance().GetScriptsOfActor(this);
		if (!Scripts.empty())
		{
			JSON ScriptsJson = JSON::Make(JSON::Class::Array);
			for (FScript* Script : Scripts)
			{
				if (Script && !Script->ScriptName.empty())
				{
					ScriptsJson.append(Script->ScriptName);
				}
			}
			InOutHandle["Scripts"] = ScriptsJson;
		}
	}
}

//AActor* AActor::Duplicate()
//{
//	AActor* NewActor = ObjectFactory::DuplicateObject<AActor>(this); // 모든 멤버 얕은 복사
//
//	NewActor->DuplicateSubObjects();
//
//	return nullptr;
//}

void AActor::RegisterComponentTree(USceneComponent* SceneComp, UWorld* InWorld)
{
	if (!SceneComp)
	{
		return;
	}
	// 씬 그래프 등록(월드에 등록/렌더 시스템 캐시 등)
	SceneComp->RegisterComponent(InWorld);
	// 자식들도 재귀적으로
	for (USceneComponent* Child : SceneComp->GetAttachChildren())
	{
		RegisterComponentTree(Child, InWorld);
	}
}

void AActor::UnregisterComponentTree(USceneComponent* SceneComp)
{
	if (!SceneComp)
	{
		return;
	}
	for (USceneComponent* Child : SceneComp->GetAttachChildren())
	{
		UnregisterComponentTree(Child);
	}
	SceneComp->UnregisterComponent();
}

// ────────────────────────────────────────────────────────────────────────────
// 게임 모드 델리게이트 핸들러
// ────────────────────────────────────────────────────────────────────────────

void AActor::OnGameStartedHandler()
{
	bGameStarted = true;
	//UE_LOG("[Actor] Game Started - %s", GetName().ToString());
}

void AActor::OnGameEndedHandler(bool bVictory)
{
	bGameStarted = false;
	//UE_LOG("[Actor] Game Ended - %s (Victory: %d)", GetName().ToString(), bVictory);
}

void AActor::OnGameRestartedHandler()
{
	bGameStarted = true;
	//UE_LOG("[Actor] Game Restarted - %s", GetName().ToString());
}

void AActor::OnGamePausedHandler()
{
	bGameStarted = false;
	//UE_LOG("[Actor] Game Paused - %s", GetName().ToString());
}

void AActor::OnGameResumedHandler()
{
	bGameStarted = true;
	//UE_LOG("[Actor] Game Resumed - %s", GetName().ToString());
}
