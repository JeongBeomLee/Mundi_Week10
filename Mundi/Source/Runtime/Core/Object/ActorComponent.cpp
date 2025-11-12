#include "pch.h"
#include "ActorComponent.h"
#include "Actor.h"
#include "World.h"
#include "SelectionManager.h"

IMPLEMENT_CLASS(UActorComponent)

BEGIN_PROPERTIES(UActorComponent)
    ADD_PROPERTY(bool, bIsActive, "컴포넌트", true, "컴포넌트를 활성화합니다")
    ADD_PROPERTY(bool, bTickEnabled, "컴포넌트", true, "틱을 확성화합니다. 기본적으로 틱이 가능한 컴포넌트만 영향이 있습니다.")
END_PROPERTIES()
    
UActorComponent::UActorComponent()
    : Owner(nullptr)
    , bIsActive(true)
    , bCanEverTick(false)
    , bTickEnabled(true)
    , bRegistered(false)
    , bHasBegunPlay(false)
    , bPendingDestroy(false)
{
}

UActorComponent::~UActorComponent()
{
    // 안전장치: 등록 상태면 해제
    if (bRegistered) UnregisterComponent();
}

UWorld* UActorComponent::GetWorld() const
{
    // 초기화 중(Owner가 없거나 World가 아직 설정되지 않은 경우) GWorld로 폴백
    // NOTE: 컴포넌트가 정상적으로 등록된 후에는 항상 올바른 World를 반환함
    if (!Owner) return GWorld;

    UWorld* World = Owner->GetWorld();
    return World ? World : GWorld;
}

// ─────────────── Registration

void UActorComponent::RegisterComponent(UWorld* InWorld)
{
    if (bRegistered)
    {
        UE_LOG("ActorComponent::RegisterComponent - Already registered, skipping: %s", GetClass()->Name);
        return;
    }
    bRegistered = true;
    OnRegister(InWorld);


    // 여기서는 게임 수명 훅을 직접 부르지 않음.
    // BeginPlay/InitializeComponent는 보통 Actor/World 타이밍에서 호출.
    // 다만 에디터 유틸 컴포넌트라면 필요에 따라 여기서 InitializeComponent를 호출해도 됨.
}

void UActorComponent::UnregisterComponent()
{
    if (!bRegistered) return;

    // 게임 수명 종료가 필요하면 EndPlay 보장
    if (bHasBegunPlay)
    {
        EndPlay(EEndPlayReason::RemovedFromWorld);
        bHasBegunPlay = false;
    }

    OnUnregister();
    bRegistered = false;
}

void UActorComponent::OnRegister(UWorld* InWorld)
{
    // 리소스/핸들 생성, 메시/버퍼 생성 등(프레임 비의존)
}

void UActorComponent::OnUnregister()
{
    // 리소스/핸들 반납
}

void UActorComponent::DestroyComponent()
{
    if (bPendingDestroy) return;
    bPendingDestroy = true;

    // Owner에서 이 컴포넌트 제거 (댕글링 포인터 방지)
    if (Owner)
    {
        Owner->RemoveOwnedComponent(this);
    }

    // 등록 중이면 우선 해제(EndPlay 포함)
    if (bRegistered) UnregisterComponent();

    // Owner 참조 끊기
    Owner = nullptr;

    DeleteObject(this);
}

// ─────────────── Lifecycle (게임 수명)

void UActorComponent::InitializeComponent()
{
    // BeginPlay 이전 초기화(게임 수명 관련 초기화)
}

void UActorComponent::BeginPlay()
{
    // 월드 진입 시 1회
    bHasBegunPlay = true;
}

void UActorComponent::TickComponent(float DeltaTime)
{
    if (!IsComponentTickEnabled()) return;
    // 매 프레임 처리
}

void UActorComponent::EndPlay(EEndPlayReason Reason)
{
    // 파괴 시
    // 필요하다면 Override
        // 게임 수명 종료
    bHasBegunPlay = false;
}

void UActorComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    bCanEverTick = true; // 매 프레임 Tick 가능 여부
    Owner = nullptr; // Actor에서 이거 설정해 줌
}

void UActorComponent::PostDuplicate()
{
    bRegistered = false;
}

void UActorComponent::OnSerialized()
{
    Super::OnSerialized();

    // UActorComponent는 기본적으로 직렬화할 추가 데이터가 없음
    // 파생 클래스에서 필요한 데이터를 직렬화하도록 오버라이드
}