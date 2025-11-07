#pragma once
#include "Actor.h"
#include "LineComponent.h"

class AGridActor : public AActor
{
public:
    DECLARE_CLASS(AGridActor, AActor);
    AGridActor();
    void Initialize();

protected:
    ~AGridActor() override;

public:
    // Grid and Axis creation methods
    void CreateGridLines(int32 GridSize = 50, float CellSize = 1.0f, const FVector& Center = FVector());
    void CreateAxisLines(float Length = 50.0f, const FVector& Origin = FVector());
    void ClearLines();
    
    // Grid settings
    float GetLineSize() { return LineSize; }
    void SetLineSize(float NewLineSize) { LineSize = NewLineSize; SetActorScale({ NewLineSize, NewLineSize, NewLineSize }); }
    
    // Component access
    ULineComponent* GetLineComponent() const { return LineComponent; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(AGridActor)
private:
    void RegenerateGrid();
    
    ULineComponent* LineComponent;
    
    // Grid settings
    int32 GridSize = 100;
    float CellSize = 1.0f;
    float AxisLength = 100.0f;

    float LineSize = 1.0f;   // 기본값으로 사용됨
};

