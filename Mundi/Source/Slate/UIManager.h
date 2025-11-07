#pragma once
#include <windows.h>
#include "Object.h"
#include "Vector.h"

class UUIWindow;
class UImGuiHelper;
class UWidget;
class UWorld;
class UTargetActorTransformWidget;

// Forward declarations for compatibility
struct ID3D11Device;
struct ID3D11DeviceContext;
class ACameraActor;
class AGizmoActor;
class AActor;
class UCameraControlWidget;

/**
 * @brief UI 매니저 클래스
 * 모든 UI 윈도우를 관리하는 싱글톤 클래스
 * @param UIWindows 등록된 모든 UI 윈도우들
 * @param FocusedWindow 현재 포커스된 윈도우
 * @param bIsInitialized 초기화 상태
 * @param TotalTime 전체 경과 시간
 */
class UUIManager : public UObject
{
public:
	DECLARE_CLASS(UUIManager, UObject)
	static UUIManager& GetInstance();

	void Initialize();
	void Initialize(HWND hWindow, ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext);
	void Shutdown();
	void Update();
	void Update(float DeltaTime);
	void SetDeltaTime(float InDeltaTime) { CurrentDeltaTime = InDeltaTime; }
	float GetDeltaTime() const { return CurrentDeltaTime; }
	void Render();
	void EndFrame();
	bool RegisterUIWindow(UUIWindow* InWindow);
	bool UnregisterUIWindow(UUIWindow* InWindow);

	UUIWindow* FindUIWindow(const FString& InWindowName) const;
	UWidget* FindWidget(const FString& InWidgetName) const;
	void HideAllWindows() const;
	void ShowAllWindows() const;

	// Getter & Setter
	size_t GetUIWindowCount() const { return UIWindows.size(); }
	const TArray<UUIWindow*>& GetAllUIWindows() const { return UIWindows; }
	UUIWindow* GetFocusedWindow() const { return FocusedWindow; }

	void SetFocusedWindow(UUIWindow* InWindow);

	// ImGui 관련 메서드
	static LRESULT WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

	void RepositionImGuiWindows();

	// 기존 UIManager 호환성 메서드
	void Release() { Shutdown(); }
	
	// Actor management methods
	void SetCamera(ACameraActor* InCameraActor) { CameraActorRef = InCameraActor; }
	ACameraActor* GetCamera() const { return CameraActorRef; }
	AActor* GetPickedActor() const { return PickedActorRef; }
	
	// Camera rotation management methods
	FVector GetTempCameraRotation() const { return TempCameraRotation; }
	float GetStoredRoll() const { return StoredRoll; }
	void UpdateMouseRotation(float InPitch, float InYaw);

	// Transform Widget registration and management
	void RegisterTargetTransformWidget(UTargetActorTransformWidget* InWidget);
	void ClearTransformWidgetSelection(); // Transform 위젯의 선택을 즉시 해제

public:
	UUIManager();
protected:
	~UUIManager() override;

	UUIManager(const UUIManager&) = delete;
	UUIManager& operator=(const UUIManager&) = delete;

private:
	TArray<UUIWindow*> UIWindows;
	UUIWindow* FocusedWindow = nullptr;
	bool bIsInitialized = false;
	float TotalTime = 0.0f;
	float CurrentDeltaTime = 0.0f;

	// ImGui Helper
	UImGuiHelper* ImGuiHelper = nullptr;

	// Actor references
	ACameraActor* CameraActorRef = nullptr;
	AActor* PickedActorRef = nullptr;
	
	// Camera rotation state
	FVector TempCameraRotation = {0.0f, 0.0f, 0.0f};
	float StoredRoll = 0.0f;

	void SortUIWindowsByPriority();
	void UpdateFocusState();

	// Transform Widget reference
	UTargetActorTransformWidget* TargetTransformWidgetRef = nullptr;
};
