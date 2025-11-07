// ────────────────────────────────────────────────────────────────────────────
// InputComponent.h
// 입력 바인딩을 관리하는 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "ActorComponent.h"
#include "Delegate.h"
#include <functional>

/**
 * FInputActionBinding
 *
 * 액션 입력 바인딩 정보를 저장합니다.
 * 특정 키가 눌렸을 때/떼었을 때 호출될 콜백을 관리합니다.
 */
struct FInputActionBinding
{
	/** 액션 이름 */
	FString ActionName;

	/** 바인딩된 키 코드 (Virtual Key Code) */
	int32 KeyCode;

	/** 눌렸을 때 호출될 콜백 */
	std::function<void()> PressedCallback;

	/** 떼었을 때 호출될 콜백 */
	std::function<void()> ReleasedCallback;

	FInputActionBinding()
		: ActionName("")
		, KeyCode(0)
		, PressedCallback(nullptr)
		, ReleasedCallback(nullptr)
	{
	}

	FInputActionBinding(const FString& InActionName, int32 InKeyCode)
		: ActionName(InActionName)
		, KeyCode(InKeyCode)
		, PressedCallback(nullptr)
		, ReleasedCallback(nullptr)
	{
	}
};

/**
 * FInputAxisBinding
 *
 * 축 입력 바인딩 정보를 저장합니다.
 * 매 프레임 호출되며 입력 값(-1.0 ~ 1.0)을 전달합니다.
 */
struct FInputAxisBinding
{
	/** 축 이름 */
	FString AxisName;

	/** 바인딩된 키 코드 (Virtual Key Code) */
	int32 KeyCode;

	/** 입력 스케일 (예: W = 1.0, S = -1.0) */
	float Scale;

	/** 매 프레임 호출될 콜백 (float는 축 값) */
	std::function<void(float)> Callback;

	FInputAxisBinding()
		: AxisName("")
		, KeyCode(0)
		, Scale(1.0f)
		, Callback(nullptr)
	{
	}

	FInputAxisBinding(const FString& InAxisName, int32 InKeyCode, float InScale)
		: AxisName(InAxisName)
		, KeyCode(InKeyCode)
		, Scale(InScale)
		, Callback(nullptr)
	{
	}
};

/**
 * UInputComponent
 *
 * Pawn의 입력 바인딩을 관리하는 컴포넌트입니다.
 * 키 입력을 액션/축 이벤트에 바인딩하고 처리합니다.
 *
 * 주요 기능:
 * - 액션 바인딩 (눌림/떼어짐 이벤트)
 * - 축 바인딩 (연속 입력 값)
 * - 입력 우선순위 관리
 */
class UInputComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UInputComponent, UActorComponent)

	UInputComponent();
	virtual ~UInputComponent() override;

	// ────────────────────────────────────────────────
	// 액션 바인딩
	// ────────────────────────────────────────────────

	/**
	 * 액션을 바인딩합니다.
	 *
	 * @param ActionName - 액션 이름
	 * @param KeyCode - 바인딩할 키 코드 (VK_*)
	 * @param PressedCallback - 눌렸을 때 호출될 함수
	 * @param ReleasedCallback - 떼었을 때 호출될 함수 (옵션)
	 */
	void BindAction(const FString& ActionName, int32 KeyCode,
		std::function<void()> PressedCallback,
		std::function<void()> ReleasedCallback = nullptr);

	/**
	 * 템플릿 버전: 멤버 함수를 바인딩합니다.
	 *
	 * @param ActionName - 액션 이름
	 * @param KeyCode - 바인딩할 키 코드
	 * @param Object - 호출할 객체 포인터
	 * @param PressedFunc - 눌렸을 때 호출될 멤버 함수
	 * @param ReleasedFunc - 떼었을 때 호출될 멤버 함수 (옵션)
	 */
	template<typename UserClass>
	void BindAction(const FString& ActionName, int32 KeyCode,
		UserClass* Object,
		void (UserClass::*PressedFunc)(),
		void (UserClass::*ReleasedFunc)() = nullptr)
	{
		auto PressedCallback = PressedFunc ? std::bind(PressedFunc, Object) : std::function<void()>();
		auto ReleasedCallback = ReleasedFunc ? std::bind(ReleasedFunc, Object) : std::function<void()>();
		BindAction(ActionName, KeyCode, PressedCallback, ReleasedCallback);
	}

	// ────────────────────────────────────────────────
	// 축 바인딩
	// ────────────────────────────────────────────────

	/**
	 * 축을 바인딩합니다.
	 *
	 * @param AxisName - 축 이름
	 * @param KeyCode - 바인딩할 키 코드
	 * @param Scale - 입력 스케일 (보통 1.0 또는 -1.0)
	 * @param Callback - 매 프레임 호출될 함수
	 */
	void BindAxis(const FString& AxisName, int32 KeyCode, float Scale, std::function<void(float)> Callback);

	/**
	 * 템플릿 버전: 멤버 함수를 바인딩합니다.
	 *
	 * @param AxisName - 축 이름
	 * @param KeyCode - 바인딩할 키 코드
	 * @param Scale - 입력 스케일
	 * @param Object - 호출할 객체 포인터
	 * @param Func - 매 프레임 호출될 멤버 함수
	 */
	template<typename UserClass>
	void BindAxis(const FString& AxisName, int32 KeyCode, float Scale,
		UserClass* Object,
		void (UserClass::*Func)(float))
	{
		auto Callback = std::bind(Func, Object, std::placeholders::_1);
		BindAxis(AxisName, KeyCode, Scale, Callback);
	}

	// ────────────────────────────────────────────────
	// 입력 처리
	// ────────────────────────────────────────────────

	/**
	 * 매 프레임 호출되어 입력을 처리합니다.
	 * PlayerController에서 호출됩니다.
	 */
	void ProcessInput();

	/**
	 * 모든 바인딩을 제거합니다.
	 */
	void ClearBindings();

private:
	/** 액션 바인딩 목록 */
	TArray<FInputActionBinding> ActionBindings;

	/** 축 바인딩 목록 */
	TArray<FInputAxisBinding> AxisBindings;
};
