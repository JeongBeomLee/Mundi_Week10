#include "pch.h"
#include "GravityWall.h"
#include "StaticMeshComponent.h"
#include "CollisionComponent/BoxComponent.h"

// ────────────────────────────────────────────────────────────────────────────
// 리플렉션 시스템 등록
// ────────────────────────────────────────────────────────────────────────────

IMPLEMENT_CLASS(AGravityWall)

BEGIN_PROPERTIES(AGravityWall)
	MARK_AS_SPAWNABLE("중력 벽", "4방향 중력을 지원하는 벽 액터입니다.")
	ADD_PROPERTY(bool, bIsFloor, "Wall", true, "지면 여부 (true: 바닥, false: 벽)")
	ADD_PROPERTY(FVector, WallNormal, "Wall", true, "벽 법선 벡터 (벽이 향하는 방향)")
END_PROPERTIES()

AGravityWall::AGravityWall()
	: StaticMeshComponent(nullptr)
	, BoxComponent(nullptr)
	, bIsFloor(false)
	, WallNormal(0.0f, 0.0f, 1.0f) // 기본값: 위를 향함
{
	Name = "Gravity Wall";

	// StaticMeshComponent 생성 (시각적 표현)
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	if (StaticMeshComponent)
	{
		SetMeshPath("Data/basic-cube.obj");
		StaticMeshComponent->SetOwner(this);
		RootComponent = StaticMeshComponent;
	}

	// BoxComponent 생성 (충돌 감지)
	BoxComponent = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	if (BoxComponent)
	{
		BoxComponent->SetOwner(this);
		BoxComponent->SetupAttachment(RootComponent);

		BoxComponent->SetBoxExtent(FVector(0.5f, 0.5f, 0.5f));

		// 오버랩 이벤트 활성화
		BoxComponent->bGenerateOverlapEvents = true;
	}
}

AGravityWall::~AGravityWall()
{
}

void AGravityWall::SetMeshPath(const FString& MeshPath)
{
	if (StaticMeshComponent)
	{
		StaticMeshComponent->SetStaticMesh(MeshPath);
	}
}

void AGravityWall::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// CreateDefaultSubobject로 생성된 컴포넌트는 자동 복제됨
	// 복제 후 포인터 복원
	StaticMeshComponent = nullptr;
	BoxComponent = nullptr;

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
		{
			StaticMeshComponent = MeshComp;
		}
		else if (UBoxComponent* BoxComp = Cast<UBoxComponent>(Component))
		{
			BoxComponent = BoxComp;
		}
	}
}

void AGravityWall::OnSerialized()
{
	Super::OnSerialized();

	// 직렬화 후 컴포넌트 포인터 복원
	StaticMeshComponent = Cast<UStaticMeshComponent>(RootComponent);

	// BoxComponent 찾기
	BoxComponent = nullptr;
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UBoxComponent* BoxComp = Cast<UBoxComponent>(Component))
		{
			BoxComponent = BoxComp;
			break;
		}
	}
}
