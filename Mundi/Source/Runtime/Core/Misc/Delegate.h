#pragma once

#include <functional>
#include <memory>
#include "UEContainer.h"
#include "WeakPtr.h"

// 단일 함수 바인딩을 위한 Delegate
template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	TDelegate() = default;

	// 람다, 함수 포인터, std::function 바인딩
	void Bind(HandlerType InFunction)
	{
		Function = InFunction;
	}

	// 멤버 함수 바인딩 (TWeakPtr 기반, UObject 전용)
	template<typename TObject>
	void BindDynamic(TObject* InObject, void (TObject::*InMethod)(Args...))
	{
		if (!InObject)
		{
			Function = nullptr;
			return;
		}

		TWeakPtr<TObject> WeakPtr(InObject);
		Function = [WeakPtr, InMethod](Args... InArgs)
		{
			// 객체가 여전히 유효한지 확인
			if (WeakPtr.IsValid())
			{
				TObject* Obj = WeakPtr.Get();
				if (Obj)
				{
					(Obj->*InMethod)(InArgs...);
				}
			}
		};
	}

	// 바인딩 해제
	void Unbind()
	{
		Function = nullptr;
	}

	// 바인딩 여부 확인
	bool IsBound() const
	{
		return Function != nullptr;
	}

	// 실행
	void Execute(Args... InArgs) const
	{
		if (IsBound())
		{
			Function(InArgs...);
		}
	}

	// () 연산자 오버로딩
	void operator()(Args... InArgs) const
	{
		Execute(InArgs...);
	}

private:
	HandlerType Function;
};

// 여러 함수 바인딩을 위한 Multicast Delegate
template<typename... Args>
class TMulticastDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;
	using DelegateHandle = size_t;

	TMulticastDelegate() = default;

	// 람다, 함수 포인터, std::function 추가
	DelegateHandle Add(HandlerType InFunction)
	{
			DelegateHandle Handle = NextHandle++;
		Functions.emplace_back(Handle, InFunction);
		return Handle;
	}

	// 멤버 함수 추가 (TWeakPtr 기반, UObject 전용)
	template<typename TObject>
	DelegateHandle AddDynamic(TObject* InObject, void (TObject::*InMethod)(Args...))
	{
		if (!InObject)
		{
			return static_cast<DelegateHandle>(-1);
		}

		TWeakPtr<TObject> WeakPtr(InObject);
		auto Function = [WeakPtr, InMethod](Args... InArgs)
		{
			// 객체가 여전히 유효한지 확인
			if (WeakPtr.IsValid())
			{
				TObject* Obj = WeakPtr.Get();
				if (Obj)
				{
					(Obj->*InMethod)(InArgs...);
				}
			}
		};
		return Add(Function);
	}

	// 핸들로 제거
	void RemoveDynamic(DelegateHandle Handle)
	{
		if (Functions.IsEmpty())
		{
			return;
		}

		Functions.erase(
			std::remove_if(Functions.begin(), Functions.end(),
				[Handle](const std::pair<DelegateHandle, HandlerType>& Pair)
				{
					return Pair.first == Handle;
				}),
			Functions.end()
		);
	}

	// 모두 제거
	void RemoveAll()
	{
		Functions.clear();
	}

	// 바인딩 여부 확인
	bool IsBound() const
	{
		return !Functions.empty();
	}

	// 모든 함수 실행
	void Broadcast(Args... InArgs) const
	{
		// 재진입 방지
		if (bIsBroadcasting)
		{
			return;
		}

		bIsBroadcasting = true;

		// 복사본으로 안전하게 실행
		TArray<TPair<DelegateHandle, HandlerType>> FunctionsCopy = Functions;

		for (const auto& Pair : FunctionsCopy)
		{
			if (Pair.second)
			{
				Pair.second(InArgs...);
			}
		}

		bIsBroadcasting = false;
	}

	// () 연산자 오버로딩
	void operator()(Args... InArgs) const
	{
		Broadcast(InArgs...);
	}

private:
	TArray<TPair<DelegateHandle, HandlerType>> Functions;
	DelegateHandle NextHandle = 0;
	mutable bool bIsBroadcasting = false;  // 브로드캐스트 중 플래그
};

// 매크로 정의 (언리얼 스타일)

// 파라미터가 없는 Delegate 선언
#define DECLARE_DELEGATE(DelegateName) \
	using DelegateName = TDelegate<>

// 파라미터가 1개인 Delegate 선언
#define DECLARE_DELEGATE_OneParam(DelegateName, Param1Type) \
	using DelegateName = TDelegate<Param1Type>

// 파라미터가 2개인 Delegate 선언
#define DECLARE_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type) \
	using DelegateName = TDelegate<Param1Type, Param2Type>

// 파라미터가 3개인 Delegate 선언
#define DECLARE_DELEGATE_ThreeParams(DelegateName, Param1Type, Param2Type, Param3Type) \
	using DelegateName = TDelegate<Param1Type, Param2Type, Param3Type>

// 파라미터가 4개인 Delegate 선언
#define DECLARE_DELEGATE_FourParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type) \
	using DelegateName = TDelegate<Param1Type, Param2Type, Param3Type, Param4Type>

// Multicast Delegate 매크로

// 파라미터가 없는 Multicast Delegate 선언
#define DECLARE_MULTICAST_DELEGATE(DelegateName) \
	using DelegateName = TMulticastDelegate<>

// 파라미터가 1개인 Multicast Delegate 선언
#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, Param1Type) \
	using DelegateName = TMulticastDelegate<Param1Type>

// 파라미터가 2개인 Multicast Delegate 선언
#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type) \
	using DelegateName = TMulticastDelegate<Param1Type, Param2Type>

// 파라미터가 3개인 Multicast Delegate 선언
#define DECLARE_MULTICAST_DELEGATE_ThreeParams(DelegateName, Param1Type, Param2Type, Param3Type) \
	using DelegateName = TMulticastDelegate<Param1Type, Param2Type, Param3Type>

// 파라미터가 4개인 Multicast Delegate 선언
#define DECLARE_MULTICAST_DELEGATE_FourParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type) \
	using DelegateName = TMulticastDelegate<Param1Type, Param2Type, Param3Type, Param4Type>

// 파라미터가 5개인 Multicast Delegate 선언
#define DECLARE_MULTICAST_DELEGATE_FiveParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type) \
	using DelegateName = TMulticastDelegate<Param1Type, Param2Type, Param3Type, Param4Type, Param5Type>
