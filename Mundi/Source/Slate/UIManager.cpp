#include "pch.h"
#include "Actor.h"
#include "Windows/UIWindow.h"
#include "Windows/PropertyWindow.h"
#include "ImGui/ImGuiHelper.h"
#include "Widgets/Widget.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"
#include "Widgets/TargetActorTransformWidget.h"

IMPLEMENT_CLASS(UUIManager)

UUIManager::UUIManager()
{
	ImGuiHelper = NewObject<UImGuiHelper>();
	Initialize();
}

UUIManager::~UUIManager()
{
}

UUIManager& UUIManager::GetInstance()
{
	static UUIManager* Instance = nullptr;
	if (Instance == nullptr)
	{
		Instance = NewObject<UUIManager>();
	}
	return *Instance;
}

/**
 * @brief UI 매니저 초기화
 */
void UUIManager::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}

	UE_LOG("UIManager: UI System 초기화 진행 중...");

	UIWindows.clear();
	FocusedWindow = nullptr;
	TotalTime = 0.0f;

	bIsInitialized = true;

	UE_LOG("UIManager: UI System 초기화 성공");
}

/**
 * @brief 기존 UIManager 호환 초기화 메서드
 * main.cpp에서 호출되는 인터페이스
 */
void UUIManager::Initialize(HWND hWindow, ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	// 기본 초기화
	Initialize();
	
	// ImGui 초기화 (device와 context 포함)
	if (ImGuiHelper)
	{
		ImGuiHelper->Initialize(hWindow, InDevice, InDeviceContext);
	}
}

/**
 * @brief UI 매니저 종료 및 정리
 */
void UUIManager::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG("UIManager: Shutting Down UI system...");

	// ImGui 정리
	if (ImGuiHelper)
	{
		DeleteObject(ImGuiHelper);
	}

	// 모든 UI 윈도우 정리
	for (auto* Window : UIWindows)
	{
		if (Window && !Window->IsSingleton())
		{
			Window->Cleanup();
			delete Window;
		}
	}

	UIWindows.clear();
	FocusedWindow = nullptr;
	bIsInitialized = false;

	UE_LOG("UIManager: UI System Shut Down Successfully.");
}

/**
 * @brief 모든 UI 윈도우 업데이트
 */
void UUIManager::Update()
{
	Update(0.0f); // 기본 Update는 DeltaTime 0으로 호출
}

/**
 * @brief 모든 UI 윈도우 업데이트 (DeltaTime 포함)
 */
void UUIManager::Update(float DeltaTime)
{
	if (!bIsInitialized)
	{
		return;
	}

	// DeltaTime 저장 및 총 시간 누적
	CurrentDeltaTime = DeltaTime;
	TotalTime += DeltaTime;

	// 모든 UI 윈도우 업데이트
	for (auto* Window : UIWindows)
	{
		if (Window && Window->IsVisible())
		{
			Window->Update();
		}
	}

	// 포커스 상태 업데이트
	UpdateFocusState();
}

/**
 * @brief 모든 UI 윈도우 렌더링
 */
void UUIManager::Render()
{
	if (!bIsInitialized)
	{
		return;
	}

	if (!ImGuiHelper)
	{
		return;
	}

	// ImGui 프레임 시작
	ImGuiHelper->BeginFrame();

	// 우선순위에 따라 정렬 (필요한 경우에만 진행)
	// SortUIWindowsByPriority();

	// 모든 UI 윈도우 렌더링
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			Window->RenderWindow();
		}
	}
}
void UUIManager::EndFrame() 
{
	// ImGui 프레임 종료
	ImGuiHelper->EndFrame();
}

bool UUIManager::RegisterUIWindow(UUIWindow* InWindow)
{
	if (!InWindow)
	{
		UE_LOG("UIManager: Error: Attempted To Register Null Window!");
		return false;
	}

	// 이미 등록된 윈도우인지 확인
	auto Iter = std::find(UIWindows.begin(), UIWindows.end(), InWindow);
	if (Iter != UIWindows.end())
	{
		UE_LOG("UIManager: Warning: Window Already Registered: %d", InWindow->GetWindowID());
		return false;
	}

	// 윈도우 초기화
	try
	{
		InWindow->Initialize();
	}
	catch (const std::exception& e)
	{
		UE_LOG("UIManager: Error: Failed To Initialize Window %d: %s", InWindow->GetWindowID(), e.what());
		return false;
	}

	UIWindows.push_back(InWindow);

	UE_LOG("UIManager: Registered UI Window: %d (%s)", InWindow->GetWindowID(), InWindow->GetWindowTitle().c_str());
	UE_LOG("UIManager: Total Registered Windows: %zu", UIWindows.size());

	return true;
}

bool UUIManager::UnregisterUIWindow(UUIWindow* InWindow)
{
	if (!InWindow)
	{
		return false;
	}

	auto It = std::find(UIWindows.begin(), UIWindows.end(), InWindow);
	if (It == UIWindows.end())
	{
		UE_LOG("UIManager: Warning: Attempted to unregister non-existent window: %d", InWindow->GetWindowID());
		return false;
	}

	// 포커스된 윈도우였다면 포커스 해제
	if (FocusedWindow == InWindow)
	{
		FocusedWindow = nullptr;
	}

	// 윈도우 정리
	InWindow->Cleanup();

	UIWindows.erase(It);

	UE_LOG("UIManager: Unregistered UI Window: %d", InWindow->GetWindowID());
	UE_LOG("UIManager: Total Registered Windows: %zu", UIWindows.size());

	return true;
}

UUIWindow* UUIManager::FindUIWindow(const FString& InWindowName) const
{
	for (auto* Window : UIWindows)
	{
		if (Window && Window->GetWindowTitle() == InWindowName)
		{
			return Window;
		}
	}
	return nullptr;
}

UWidget* UUIManager::FindWidget(const FString& InWidgetName) const
{
	for (auto* Window : UIWindows)
	{
		for (auto* Widget : Window->GetWidgets())
		{
			if (Widget->GetName() == InWidgetName)
			{
				return Widget;
			}
		}
	}
	return nullptr;
}

void UUIManager::HideAllWindows() const
{
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			Window->SetWindowState(EUIWindowState::Hidden);
		}
	}
	UE_LOG("UIManager: All Windows Hidden.");
}

void UUIManager::ShowAllWindows() const
{
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			Window->SetWindowState(EUIWindowState::Visible);
		}
	}
	UE_LOG("UIManager: All Windows Shown.");
}

void UUIManager::SetFocusedWindow(UUIWindow* InWindow)
{
	if (FocusedWindow != InWindow)
	{
		if (FocusedWindow)
		{
			FocusedWindow->OnFocusLost();
		}

		FocusedWindow = InWindow;

		if (FocusedWindow)
		{
			FocusedWindow->OnFocusGained();
		}
	}
}

void UUIManager::SortUIWindowsByPriority()
{
	std::sort(UIWindows.begin(), UIWindows.end(), [](const UUIWindow* A, const UUIWindow* B)
	{
		if (!A) return false;
		if (!B) return true;
		return A->GetPriority() < B->GetPriority();
	});
}

void UUIManager::UpdateFocusState()
{
	UUIWindow* NewFocusedWindow = nullptr;

	for (auto* Window : UIWindows)
	{
		if (Window && Window->IsVisible() && Window->IsFocused())
		{
			NewFocusedWindow = Window;
			break;
		}
	}

	if (FocusedWindow != NewFocusedWindow)
	{
		SetFocusedWindow(NewFocusedWindow);
	}
}

LRESULT UUIManager::WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)
{
	return UImGuiHelper::WndProcHandler(hwnd, msg, wParam, lParam);
}

void UUIManager::RepositionImGuiWindows()
{
	for (auto& window : UIWindows)
	{
		window->SetIsResized(true);
	}
}

void UUIManager::UpdateMouseRotation(float InPitch, float InYaw)
{
	TempCameraRotation.X = InPitch;
	TempCameraRotation.Y = InYaw;
}

void UUIManager::RegisterTargetTransformWidget(UTargetActorTransformWidget* InWidget)
{
	TargetTransformWidgetRef = InWidget;
}

void UUIManager::ClearTransformWidgetSelection()
{
	if (TargetTransformWidgetRef)
	{
		TargetTransformWidgetRef->OnSelectedActorCleared();
	}
}

