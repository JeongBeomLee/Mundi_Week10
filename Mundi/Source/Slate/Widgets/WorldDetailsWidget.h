#pragma once
#include "Widget.h"
#include "UEContainer.h"

// Forward Declarations
class UWorld;
class AGameModeBase;
class UClass;

/**
 * @brief World 설정을 표시하고 편집하는 위젯
 * GameMode의 DefaultPawnClass, PlayerControllerClass 등을 설정할 수 있습니다.
 */
class UWorldDetailsWidget : public UWidget
{
public:
	DECLARE_CLASS(UWorldDetailsWidget, UWidget)

	UWorldDetailsWidget();
	virtual ~UWorldDetailsWidget() override = default;

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	/** World 참조 설정 */
	void SetWorld(UWorld* InWorld);

private:
	/** World 참조 */
	UWorld* World = nullptr;

	/** UI 렌더링 */
	void RenderGameModeSettings();
	void RenderGameModeClassSelector();
	void RenderPawnClassSelector();
	void RenderControllerClassSelector();
	void RenderSpawnLocationEditor();

	/** 사용 가능한 클래스 목록 업데이트 */
	void UpdateAvailableClasses();

	/** 클래스 이름 리스트 (ImGui Combo용) */
	TArray<const char*> GameModeClassNames;
	TArray<const char*> PawnClassNames;
	TArray<const char*> ControllerClassNames;

	/** 사용 가능한 클래스 목록 */
	TArray<UClass*> AvailableGameModeClasses;
	TArray<UClass*> AvailablePawnClasses;
	TArray<UClass*> AvailableControllerClasses;

	/** 현재 선택된 인덱스 */
	int SelectedGameModeClassIndex = 0;
	int SelectedPawnClassIndex = 0;
	int SelectedControllerClassIndex = 0;

	/** SpawnLocation 편집용 */
	FVector EditSpawnLocation = FVector(0.0f, 0.0f, 0.0f);
};
