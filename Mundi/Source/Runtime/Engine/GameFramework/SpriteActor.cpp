#include "pch.h"
#include "SpriteActor.h"

IMPLEMENT_CLASS(ASpriteActor)

BEGIN_PROPERTIES(ASpriteActor)
	MARK_AS_SPAWNABLE("스프라이트 액터", "스프라이트 애니메이션을 재생하는 액터입니다.")
END_PROPERTIES()

ASpriteActor::ASpriteActor() :ParticleComponent(nullptr)
{
	Name = "SpriteActor";

	// ParticleComponent 생성 및 루트에 부착
	ParticleComponent = CreateDefaultSubobject<UParticleComponent>("ParticleComponent");
	//if (ParticleComponent)
	//{
	//	ParticleComponent->SetupAttachment(RootComponent);
	//}
}

void ASpriteActor::BeginPlay()
{
	Super::BeginPlay();

	// 게임 시작 시 스프라이트 애니메이션 재생
	if (ParticleComponent)
	{
		ParticleComponent->Play();
	}
}

void ASpriteActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 자동 삭제 처리
	if (bAutoDestroy && Lifetime > 0.0f)
	{
		ElapsedTime += DeltaSeconds;
		if (ElapsedTime >= Lifetime)
		{
			Destroy();
		}
	}
}

void ASpriteActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void ASpriteActor::OnSerialized()
{
	Super::OnSerialized();
}

void ASpriteActor::SetSpriteSheet(const FString& InTexturePath, int32 InRows, int32 InColumns, float InFrameRate)
{
	if (ParticleComponent)
	{
		ParticleComponent->SetSpriteSheet(InTexturePath, InRows, InColumns);
		ParticleComponent->SetFrameRate(InFrameRate);
	}
}

void ASpriteActor::SetAutoDestroy(bool bEnable, float InLifetime)
{
	bAutoDestroy = bEnable;
	Lifetime = InLifetime;
	ElapsedTime = 0.0f;
}
