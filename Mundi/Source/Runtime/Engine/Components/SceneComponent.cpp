#include "pch.h"
#include "SceneComponent.h"
#include <algorithm>
#include "ObjectFactory.h"
#include "PrimitiveComponent.h"
#include "WorldPartitionManager.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(USceneComponent)

BEGIN_PROPERTIES(USceneComponent)
	MARK_AS_COMPONENT("씬 컴포넌트", "트랜스폼을 가진 씬 컴포넌트입니다.")
	ADD_PROPERTY(FVector, RelativeLocation, "Transform", true, "로컬 위치입니다.")
	ADD_PROPERTY(FVector, RelativeRotationEuler, "Transform", true, "로컬 회전입니다 (Degrees, ZYX Euler).")
	ADD_PROPERTY(FVector, RelativeScale, "Transform", true, "로컬 스케일입니다.")
    ADD_PROPERTY(bool, bIsVisible, "렌더링", true, "프리미티브를 씬에 표시합니다")
    ADD_PROPERTY(bool, bHiddenInGame, "렌더링", true, "게임에서 숨깁니다")
END_PROPERTIES()

// USceneComponent.cpp
TMap<uint32, USceneComponent*> USceneComponent::SceneIdMap;

USceneComponent::USceneComponent()
    : RelativeLocation(0, 0, 0)
    , RelativeRotation(0, 0, 0, 1)
    , RelativeScale(1, 1, 1)
    , RelativeRotationEuler(0, 0, 0)
    , AttachParent(nullptr)
{
    UpdateRelativeTransform();
}

USceneComponent::~USceneComponent()
{
    // OnUnregister에서 이미 부모-자식 관계를 정리했으므로
    // 여기서는 안전장치만 추가

    // 혹시 OnUnregister가 호출되지 않은 경우를 대비한 최종 정리
    if (AttachParent && !AttachParent->IsPendingDestroy())
    {
        AttachParent->AttachChildren.Remove(this);
    }
    AttachParent = nullptr;
    AttachChildren.clear();
}

// ──────────────────────────────
// Relative API 더티플래그 사용해서 업데이트 하도록 수정할 것임.
// 그리고 bWantsOnUpdateTransform이 True일때만 OnTransformUpdated호출
// ──────────────────────────────
void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
{
    RelativeLocation = NewLocation;
    UpdateRelativeTransform();
    OnTransformUpdated();
}
FVector USceneComponent::GetRelativeLocation() const { return RelativeLocation; }

void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
{
    RelativeRotation = NewRotation;
    RelativeRotationEuler = NewRotation.ToEulerZYXDeg(); // Euler 동기화
    UpdateRelativeTransform();
    OnTransformUpdated();
}
FQuat USceneComponent::GetRelativeRotation() const { return RelativeRotation; }

void USceneComponent::SetRelativeRotationEuler(const FVector& EulerDegrees)
{
    // Euler 각도를 직접 저장 (UI 입력 값 보존)
    RelativeRotationEuler = EulerDegrees;

    // Quaternion으로 변환하여 회전 행렬 계산용으로 사용
    RelativeRotation = FQuat::MakeFromEulerZYX(EulerDegrees).GetNormalized();

    // Euler 재계산 하지 않음 - UI에서 입력한 값을 그대로 유지
    UpdateRelativeTransform();
    OnTransformUpdated();
}

FVector USceneComponent::GetRelativeRotationEuler() const
{
    // 저장된 Euler 각도 반환 (UI 입력 값 보존)
    return RelativeRotationEuler;
}

void USceneComponent::SetRelativeScale(const FVector& NewScale)
{
    RelativeScale = NewScale;
    UpdateRelativeTransform();
    OnTransformUpdated();
}
FVector USceneComponent::GetRelativeScale() const { return RelativeScale; }

void USceneComponent::AddRelativeLocation(const FVector& DeltaLocation)
{
    RelativeLocation = RelativeLocation + DeltaLocation;
    UpdateRelativeTransform();
    OnTransformUpdated();
}

void USceneComponent::AddRelativeRotation(const FQuat& DeltaRotation)
{
    RelativeRotation = DeltaRotation * RelativeRotation;
    RelativeRotationEuler = RelativeRotation.ToEulerZYXDeg(); // Euler 동기화
    UpdateRelativeTransform();
    OnTransformUpdated();
}

void USceneComponent::AddRelativeScale3D(const FVector& DeltaScale)
{
    RelativeScale = FVector(RelativeScale.X * DeltaScale.X,
        RelativeScale.Y * DeltaScale.Y,
        RelativeScale.Z * DeltaScale.Z);
    UpdateRelativeTransform();
    OnTransformUpdated();
}

// ────────────────────────────── 
// World API
// ──────────────────────────────
FTransform USceneComponent::GetWorldTransform() const
{
    // Dangling pointer 방지를 위한 체크
     if (AttachParent && !AttachParent->IsPendingDestroy())
    {
        return AttachParent->GetWorldTransform().GetWorldTransform(RelativeTransform);
    }

    return RelativeTransform;
}

void USceneComponent::SetWorldTransform(const FTransform& W)
{
    // Dangling pointer 방지를 위한 체크
    if (AttachParent && !AttachParent->IsPendingDestroy())
    {
        const FTransform ParentWorld = AttachParent->GetWorldTransform();
        RelativeTransform = ParentWorld.GetRelativeTransform(W);
    }
    else
    {
        RelativeTransform = W;
    }

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeRotationEuler = RelativeRotation.ToEulerZYXDeg(); // Euler 동기화
    RelativeScale = RelativeTransform.Scale3D;
    OnTransformUpdated();
}
 
void USceneComponent::SetWorldLocation(const FVector& L)
{
    FTransform W = GetWorldTransform();
    W.Translation = L;
    SetWorldTransform(W);
}
FVector USceneComponent::GetWorldLocation() const
{
    return GetWorldTransform().Translation;
}

void USceneComponent::SetWorldRotation(const FQuat& R)
{
    FTransform W = GetWorldTransform();
    W.Rotation = R;
    SetWorldTransform(W); // SetWorldTransform에서 Euler 동기화
}
FQuat USceneComponent::GetWorldRotation() const
{
    return GetWorldTransform().Rotation;
}

void USceneComponent::SetWorldScale(const FVector& S)
{
    FTransform W = GetWorldTransform();
    W.Scale3D = S;
    SetWorldTransform(W);
}
FVector USceneComponent::GetWorldScale() const
{
    return GetWorldTransform().Scale3D;
}

void USceneComponent::AddWorldOffset(const FVector& Delta)
{
    FTransform W = GetWorldTransform();
    W.Translation = W.Translation + Delta;
    SetWorldTransform(W);
}

void USceneComponent::AddWorldRotation(const FQuat& DeltaRot)
{
    FTransform W = GetWorldTransform();
    W.Rotation = DeltaRot * W.Rotation;
    SetWorldTransform(W); // SetWorldTransform에서 Euler 동기화
}


void USceneComponent::SetWorldLocationAndRotation(const FVector& L, const FQuat& R)
{
    FTransform W = GetWorldTransform();
    W.Translation = L;
    W.Rotation = R;
    SetWorldTransform(W);
}

void USceneComponent::AddLocalOffset(const FVector& Delta)
{
    const FVector parentDelta = RelativeRotation.RotateVector(Delta);
    RelativeLocation = RelativeLocation + parentDelta;
    UpdateRelativeTransform();
    OnTransformUpdated();
}

void USceneComponent::AddLocalRotation(const FQuat& DeltaRot)
{
    RelativeRotation = (RelativeRotation * DeltaRot).GetNormalized(); // 로컬: 우측곱
    RelativeRotationEuler = RelativeRotation.ToEulerZYXDeg(); // Euler 동기화
    UpdateRelativeTransform();
    OnTransformUpdated();
}

void USceneComponent::SetLocalLocationAndRotation(const FVector& L, const FQuat& R)
{
    RelativeLocation = L;
    RelativeRotation = R.GetNormalized();
    RelativeRotationEuler = RelativeRotation.ToEulerZYXDeg(); // Euler 동기화
    UpdateRelativeTransform();
    OnTransformUpdated();
}


FMatrix USceneComponent::GetWorldMatrix() const
{
    return GetWorldTransform().ToMatrix();
}

// ──────────────────────────────
// Attach / Detach
// ──────────────────────────────
void USceneComponent::SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule)
{
    if (AttachParent == InParent) return;

    const FTransform OldWorld = GetWorldTransform();

    // 기존 부모에서 제거
    if (AttachParent)
    {
        auto& Siblings = AttachParent->AttachChildren;
        Siblings.erase(std::remove(Siblings.begin(), Siblings.end(), this), Siblings.end());

    }

    // 새 부모 설정
    AttachParent = InParent;

    if(AttachParent)
    { 
        AttachParent->AttachChildren.push_back(this);
        if (Rule == EAttachmentRule::KeepWorld)
        {
            const FTransform ParentWorld = AttachParent->GetWorldTransform();
            RelativeTransform = ParentWorld.GetRelativeTransform(OldWorld);
        }
        // KeepRelative: 기존 RelativeTransform 유지
    }
    else
    {
        if (Rule == EAttachmentRule::KeepWorld)
            RelativeTransform = OldWorld;
    }

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}

void USceneComponent::DetachFromParent(bool bKeepWorld)
{
    const FTransform OldWorld = GetWorldTransform();

    if (AttachParent)
    {
        auto& Siblings = AttachParent->AttachChildren;
        if(!Siblings.IsEmpty())
        Siblings.erase(std::remove(Siblings.begin(), Siblings.end(), this), Siblings.end());
        AttachParent = nullptr;
    }

    if (bKeepWorld)
        RelativeTransform = OldWorld;

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}

void USceneComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    AttachParent = nullptr; // 부모 컴포넌트가 이 객체의 SetupAttachment를 호출할 경우, 불필요한 로직(기존 부모에서 제거) 수행 방지
    SpriteComponent = nullptr;

    AttachChildren.clear(); // Actor에서 할당해줌
}

// ──────────────────────────────
// 내부 유틸
// ──────────────────────────────
void USceneComponent::UpdateRelativeTransform()
{
    RelativeTransform = FTransform(RelativeLocation, RelativeRotation, RelativeScale);
}

void USceneComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
        // 나중에 자식의 Serialize 호출될 때 부모인 이 객체를 찾기 위해 Map에 추가
        FJsonSerializer::ReadUint32(InOutHandle, "Id", SceneId);
        SceneIdMap.Add(SceneId, this);

        // 부모 찾기
        FJsonSerializer::ReadUint32(InOutHandle, "ParentId", ParentId);

        RelativeRotation = FQuat::MakeFromEulerZYX(RelativeRotationEuler).GetNormalized();

        // 해당 객체의 Transform을 위에서 읽은 값을 기반으로 변경 후, 자식에게 전파
        UpdateRelativeTransform();
        OnTransformUpdated();
	}
	else
	{
		InOutHandle["Id"] = UUID;

		if (AttachParent)
		{
			InOutHandle["ParentId"] = AttachParent->UUID;
		}
		else // RootComponent
		{
			InOutHandle["ParentId"] = 0;
		}
	}
}

void USceneComponent::OnRegister(UWorld* InWorld)
{
    if (!std::strcmp(this->GetClass()->Name , USceneComponent::StaticClass()->Name) && !SpriteComponent && !InWorld->bPie)
    {
        CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
        SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/EmptyActor.dds");

    }
}

void USceneComponent::OnUnregister()
{
    // 부모-자식 관계 정리 (댕글링 포인터 방지)
    // 이 시점에서 정리하면 소멸자보다 먼저 실행되어 안전함

    // 자식들의 AttachParent 참조 끊기
    TArray<USceneComponent*> ChildrenCopy = AttachChildren;
    for (USceneComponent* Child : ChildrenCopy)
    {
        if (Child && !Child->IsPendingDestroy())
        {
            Child->AttachParent = nullptr;
        }
    }
    AttachChildren.clear();

    // 부모에서 자신 제거
    if (AttachParent && !AttachParent->IsPendingDestroy())
    {
        AttachParent->AttachChildren.Remove(this);
    }
    AttachParent = nullptr;

    Super::OnUnregister();
}

void USceneComponent::OnSerialized()
{
	Super::OnSerialized();
}

void USceneComponent::OnTransformUpdated()
{
    for (USceneComponent* Child : GetAttachChildren())
    {
        Child->OnTransformUpdated();
    }
}

UWorld* USceneComponent::GetWorld() const
{
    return Owner ? Owner->GetWorld() : nullptr;
}