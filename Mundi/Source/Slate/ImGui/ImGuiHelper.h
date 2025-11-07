#pragma once
#include <Windows.h>
#include "Object.h"

// Forward declarations
struct ID3D11Device;
struct ID3D11DeviceContext;

/**
 * @brief ImGui 초기화/렌더링/해제를 담당하는 Helper 클래스
 * UIManager에서 사용하는 유틸리티 클래스
 */
class UImGuiHelper : public UObject
{
public:
	DECLARE_CLASS(UImGuiHelper, UObject)

	UImGuiHelper();
	~UImGuiHelper() override;

	void Initialize(HWND InWindowHandle);
	void Initialize(HWND InWindowHandle, ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext);
	void Release();

	void BeginFrame() const;
	void EndFrame() const;

	static LRESULT WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

private:
	void SetupCustomTheme();

	bool bIsInitialized = false;
};
