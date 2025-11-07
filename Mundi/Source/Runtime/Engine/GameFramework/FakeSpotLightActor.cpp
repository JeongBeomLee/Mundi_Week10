#include "pch.h"
#include "FakeSpotLightActor.h"
#include "PerspectiveDecalComponent.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(AFakeSpotLightActor)

BEGIN_PROPERTIES(AFakeSpotLightActor)
	MARK_AS_SPAWNABLE("가짜 스포트 라이트", "데칼을 사용한 가짜 스포트 라이트 액터입니다.")
END_PROPERTIES()

AFakeSpotLightActor::AFakeSpotLightActor()
{
	Name = "Fake Spot Light Actor";
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	BillboardComponent->SetEditability(false);
	DecalComponent = CreateDefaultSubobject<UPerspectiveDecalComponent>("DecalComponent");

	BillboardComponent->SetTextureName("Editor/SpotLight_64x.png");
	DecalComponent->SetRelativeScale((FVector(10, 5, 5)));
	DecalComponent->SetRelativeLocation((FVector(0, 0, -5)));
	DecalComponent->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 90, 0)));
	DecalComponent->SetDecalTexture(GDataDir + "/Textures/GreenLight.png");
	DecalComponent->SetFovY(60);
	
	RootComponent = DecalComponent;
	BillboardComponent->SetupAttachment(RootComponent);
}

AFakeSpotLightActor::~AFakeSpotLightActor()
{
}

void AFakeSpotLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	// 두 컴포넌트를 모두 찾을 때까지 순회
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UPerspectiveDecalComponent* Decal = Cast<UPerspectiveDecalComponent>(Component))
		{
			DecalComponent = Decal;
		}
		else if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			BillboardComponent = Billboard;
		}

		// 두 컴포넌트를 모두 찾았으면 조기 종료
		if (DecalComponent && BillboardComponent)
		{
			break;
		}
	}
}

void AFakeSpotLightActor::OnSerialized()
{
	Super::OnSerialized();

	
	// 로딩 시 컴포넌트 중복 방지 로직:
	// 
	// 1. 생성자에서 생성된 native 컴포넌트(DecalComponent, BillboardComponent)가 있음
	// 2. 씬 파일에서 직렬화된 컴포넌트가 로드되면 SceneComponents에 추가됨
	// 3. 이 로직은 native 컴포넌트를 제거하고 직렬화된 컴포넌트로 교체함
	// 4. 이를 통해 PIE나 씬 로드 시 중복 컴포넌트를 방지함
	
	UPerspectiveDecalComponent* DecalComponentPre = nullptr;
	UBillboardComponent* BillboardCompPre = nullptr;

	// 직렬화된 컴포넌트를 찾아서 포인터 교체
	for (auto& Component : SceneComponents)
	{
		if (UPerspectiveDecalComponent* PerpectiveDecalComp = Cast<UPerspectiveDecalComponent>(Component))
		{
			DecalComponentPre = DecalComponent;  // 기존 native 컴포넌트 백업
			DecalComponent->DetachFromParent();  // 부모에서 분리
			DecalComponent = PerpectiveDecalComp;  // 새로 로드된 컴포넌트로 교체
		}
		else if (UBillboardComponent* BillboardCompTemp = Cast<UBillboardComponent>(Component))
		{
			BillboardCompPre = BillboardComponent;  // 기존 native 컴포넌트 백업
			BillboardComponent->DetachFromParent();  // 부모에서 분리
			BillboardComponent = BillboardCompTemp;  // 새로 로드된 컴포넌트로 교체
		}
	}

	// 기존 native 컴포넌트를 OwnedComponents에서 제거 (메모리는 나중에 정리됨)
	if (DecalComponentPre)
	{
		DecalComponentPre->GetOwner()->RemoveOwnedComponent(DecalComponentPre);
	}
	if (BillboardCompPre)
	{
		BillboardCompPre->GetOwner()->RemoveOwnedComponent(BillboardCompPre);
	}
	
}
