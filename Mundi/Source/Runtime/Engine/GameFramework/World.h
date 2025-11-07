#pragma once
#include "Object.h"
#include "Enums.h"
#include "RenderSettings.h"
#include "Level.h"
#include "Gizmo/GizmoActor.h"
#include "LightManager.h"
#include "Source/Runtime/LuaScripting/UScriptManager.h"

// Forward Declarations
class UResourceManager;
class UUIManager;
class UInputManager;
class USelectionManager;
class AActor;
class URenderer;
class ACameraActor;
class AGizmoActor;
class AGridActor;
class FViewport;
class USlateManager;
class URenderManager;
struct FTransform;
struct FSceneCompData;
class SViewportWindow;
class UWorldPartitionManager;
class AStaticMeshActor;
class BVHierachy;
class UStaticMesh;
class FOcclusionCullingManagerCPU;
struct Frustum;
struct FCandidateDrawable;
class FShadowManager;
class UCollisionManager;
class AGameModeBase;
class AGameStateBase;
class UDeltaTimeManager;

class UWorld final : public UObject
{
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld();
    ~UWorld() override;

    bool bPie = false;

public:
    /** 초기화 */
    void Initialize();
    void InitializeGrid();
    void InitializeGizmo();

    template<class T>
    T* SpawnActor();

    template<class T>
    T* SpawnActor(const FTransform& Transform);

    AActor* SpawnActor(UClass* Class, const FTransform& Transform);
    AActor* SpawnActor(UClass* Class);

    bool DestroyActor(AActor* Actor);
    
    // 지연 삭제 시스템
    void MarkActorForDestruction(AActor* Actor);
    void ProcessPendingActorDestruction();

    // Partial hooks
    void OnActorSpawned(AActor* Actor);
    void OnActorDestroyed(AActor* Actor);

    void CreateLevel();

    void SpawnDefaultActors();

    // Level ownership API
    void SetLevel(std::unique_ptr<ULevel> InLevel);
    ULevel* GetLevel() const { return Level.get(); }
    FLightManager* GetLightManager() const { return LightManager.get(); }
    FShadowManager* GetShadowManager() const { return ShadowManager.get(); }
    UCollisionManager* GetCollisionManager() const { return CollisionManager.get(); }

    ACameraActor* GetCameraActor() { return MainCameraActor; }
    void SetCameraActor(ACameraActor* InCamera)
    {
        MainCameraActor = InCamera;

        //기즈모 카메라 설정
        if (GizmoActor)
            GizmoActor->SetCameraActor(MainCameraActor);
    }

    /** Generate unique name for actor based on type */
    FString GenerateUniqueActorName(const FString& ActorType);

    /** === 타임 / 틱 === */
    virtual void Tick(float DeltaSeconds);

    /** === 필요한 엑터 게터 === */
    const TArray<AActor*>& GetActors() { static TArray<AActor*> Empty; return Level ? Level->GetActors() : Empty; }
    const TArray<AActor*>& GetEditorActors() { return EditorActors; }
    AGizmoActor* GetGizmoActor() { return GizmoActor; }
    AGridActor* GetGridActor() { return GridActor; }
    UWorldPartitionManager* GetPartitionManager() { return Partition.get(); }

    // Per-world render settings
    URenderSettings& GetRenderSettings() { return RenderSettings; }
    const URenderSettings& GetRenderSettings() const { return RenderSettings; }

    // Per-world SelectionManager accessor
    USelectionManager* GetSelectionManager() { return SelectionMgr.get(); }

    // GameMode/GameState accessor (PIE 전용)
    AGameModeBase* GetGameMode() const { return GameMode; }
    AGameStateBase* GetGameState() const { return GameState; }

    // PIE용 World 생성
    static UWorld* DuplicateWorldForPIE(UWorld* InEditorWorld);

    /** === PIE 설정 (에디터에서 설정, PIE 시작 시 사용) === */
    UClass* GetGameModeClass() const { return GameModeClass; }
    void SetGameModeClass(UClass* InClass) { GameModeClass = InClass; }

    UClass* GetDefaultPawnClass() const { return DefaultPawnClass; }
    void SetDefaultPawnClass(UClass* InClass) { DefaultPawnClass = InClass; }

    UClass* GetPlayerControllerClass() const { return PlayerControllerClass; }
    void SetPlayerControllerClass(UClass* InClass) { PlayerControllerClass = InClass; }

    FVector GetPlayerSpawnLocation() const { return PlayerSpawnLocation; }
    void SetPlayerSpawnLocation(const FVector& InLocation) { PlayerSpawnLocation = InLocation; }

    /** === DeltaTime 관리 === */

    /**
     * DeltaTime 매니저를 반환합니다.
     */
    UDeltaTimeManager* GetDeltaTimeManager() const { return DeltaTimeManager.get(); }

    /**
     * 조정된 DeltaTime을 반환합니다 (편의 함수)
     * @param RealDeltaTime - 실제 프레임 시간
     * @return TimeDilation이 적용된 DeltaTime
     */
    float GetScaledDeltaTime(float RealDeltaTime) const;


	float GetRealDeltaTime() const { return CurrentRealDeltaTime; }

    /** === 게임 프레임워크 (PIE 전용) === */
    AGameModeBase* GameMode = nullptr;
    AGameStateBase* GameState = nullptr;

    /** === PIE 설정 (에디터에서 편집 가능) === */
    UClass* GameModeClass = nullptr;
    UClass* DefaultPawnClass = nullptr;
    UClass* PlayerControllerClass = nullptr;
    FVector PlayerSpawnLocation = FVector(0.0f, 0.0f, 100.0f);

private:
    /** === 에디터 특수 액터 관리 === */
    TArray<AActor*> EditorActors;
    ACameraActor* MainCameraActor = nullptr;
    AGridActor* GridActor = nullptr;
    AGizmoActor* GizmoActor = nullptr;

    /** === 레벨 컨테이너 === */
    std::unique_ptr<ULevel> Level;

    /** === 라이트 매니저 ===*/
    std::unique_ptr<FLightManager> LightManager;

    /** === 섀도우 매니저 ===*/
    std::unique_ptr<FShadowManager> ShadowManager;

    /** === 충돌 매니저 ===*/
    std::unique_ptr<UCollisionManager> CollisionManager;

    // Object naming system
    TMap<FString, int32> ObjectTypeCounts;

    // Internal helper to register spawned actors into current level
    void AddActorToLevel(AActor* Actor);

    // Per-world render settings
    URenderSettings RenderSettings;

    //partition
    std::unique_ptr<UWorldPartitionManager> Partition = nullptr;

    // Per-world selection manager
    std::unique_ptr<USelectionManager> SelectionMgr;

    /** DeltaTime 및 Time Dilation 관리자 */
    std::unique_ptr<UDeltaTimeManager> DeltaTimeManager;

    // 지연 삭제 큐
    TArray<AActor*> PendingDestroyActors;

	float CurrentRealDeltaTime = 0.016f; // 기본값 60FPS
};

template<class T>
inline T* UWorld::SpawnActor()
{
    return SpawnActor<T>(FTransform());
}

template<class T>
inline T* UWorld::SpawnActor(const FTransform& Transform)
{
    static_assert(std::is_base_of<AActor, T>::value, "T must be derived from AActor");

    // 새 액터 생성
    T* NewActor = NewObject<T>();

    // 초기 트랜스폼 적용
    NewActor->SetActorTransform(Transform);

    //  월드 등록
    NewActor->SetWorld(this);
    
    // 월드에 등록
    AddActorToLevel(NewActor);

    return NewActor;
}
