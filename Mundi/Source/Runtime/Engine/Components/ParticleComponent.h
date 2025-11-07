#pragma once
#include "BillboardComponent.h"
#include "Object.h"

// 스프라이트 애니메이션 빌보드 컴포넌트
class UParticleComponent : public UBillboardComponent
{
public:
    DECLARE_CLASS(UParticleComponent, UBillboardComponent)
    GENERATED_REFLECTION_BODY()
    DECLARE_DUPLICATE(UParticleComponent)
    UParticleComponent();
    ~UParticleComponent() override = default;

    // 렌더링 (UV 좌표 변경하여 스프라이트 애니메이션)
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    // 업데이트
    void TickComponent(float DeltaSeconds);

    // 스프라이트 시트 설정
    void SetSpriteSheet(const FString& InTexturePath, int32 InRows, int32 InColumns);
    void SetFrameRate(float InFrameRate) { FrameRate = InFrameRate; }
    void SetLooping(bool bInLoop) { bLooping = bInLoop; }

    // 재생 제어
    void Play();
    void Stop();
    void Reset();

private:
    // 스프라이트 시트 정보
    int32 SpriteRows = 1;
    int32 SpriteColumns = 1;
    int32 TotalFrames = 1;
    float FrameRate = 10.0f;

    // 애니메이션 상태
    float CurrentFrame = 0.0f;
    bool bIsPlaying = false;
    bool bLooping = true;
};
