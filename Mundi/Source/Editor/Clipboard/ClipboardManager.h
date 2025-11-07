#pragma once
#include "Object.h"
#include "UEContainer.h"

// Forward Declarations
class AActor;
class UActorComponent;
class UWorld;

class UClipboardManager : public UObject
{
public:
    DECLARE_CLASS(UClipboardManager, UObject)

    void CopyActor(AActor* Actor);
    void CopyComponent(UActorComponent* Component, AActor* OwnerActor);

    AActor* PasteActorToWorld(UWorld* World, const FVector& OffsetFromOriginal);
    bool PasteComponentToActor(AActor* TargetActor);

    bool HasCopiedActor() const { return CopiedActor != nullptr; }
    bool HasCopiedComponent() const { return CopiedComponent != nullptr; }
    void ClearClipboard();

public:
    UClipboardManager();
    ~UClipboardManager() override;

protected:
    // 복사 방지
    UClipboardManager(const UClipboardManager&) = delete;
    UClipboardManager& operator=(const UClipboardManager&) = delete;

    /** === 클립보드 데이터 === */
    AActor* CopiedActor = nullptr;              // 복사된 Actor (원본 참조)
    UActorComponent* CopiedComponent = nullptr; // 복사된 Component (원본 참조)
    AActor* CopiedComponentOwner = nullptr;     // 복사된 Component의 원래 Owner
};
