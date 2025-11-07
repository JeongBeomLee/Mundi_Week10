#pragma once
#include "UEContainer.h"

// 전방 선언
class UObject;
extern TArray<UObject*> GUObjectArray;

template<typename T>
class TWeakPtr
{
public:
	// 기본 생성자: 무효한 포인터
	TWeakPtr()
		: Index(UINT32_MAX)
	{
	}

	// 생성자: UObject 포인터로부터
	explicit TWeakPtr(T* InObject)
		: Index(UINT32_MAX)
	{
		if (InObject)
		{
			Index = InObject->InternalIndex;
		}
	}

	// 복사/대입
	TWeakPtr(const TWeakPtr&) = default;
	TWeakPtr& operator=(const TWeakPtr&) = default;
	TWeakPtr(TWeakPtr&&) noexcept = default;
	TWeakPtr& operator=(TWeakPtr&&) noexcept = default;

	// 대입 연산자: UObject 포인터
	TWeakPtr& operator=(T* InObject)
	{
		if (InObject)
		{
			Index = InObject->InternalIndex;
		}
		else
		{
			Index = UINT32_MAX;
		}
		return *this;
	}

	// 객체가 유효한지 확인
	bool IsValid() const
	{
		if (Index == UINT32_MAX)
			return false;

		if (Index >= static_cast<uint32>(GUObjectArray.Num()))
			return false;

		return GUObjectArray[Index] != nullptr;
	}

	// 객체 포인터 가져오기
	T* Get() const
	{
		if (!IsValid())
		{
			return nullptr;
		}

		UObject* Obj = GUObjectArray[Index];
		if (!Obj)
		{
			return nullptr;
		}

		return reinterpret_cast<T*>(Obj);
	}

	// 연산자 오버로딩
	T* operator->() const
	{
		return Get();
	}

	T& operator*() const
	{
		T* Ptr = Get();
		assert(Ptr && "TWeakPtr::operator*() - Object is invalid!");
		return *Ptr;
	}

	explicit operator bool() const
	{
		return IsValid();
	}

	bool operator==(std::nullptr_t) const
	{
		return !IsValid();
	}

	bool operator!=(std::nullptr_t) const
	{
		return IsValid();
	}

	bool operator==(const TWeakPtr& Other) const
	{
		return Index == Other.Index;
	}

	bool operator!=(const TWeakPtr& Other) const
	{
		return Index != Other.Index;
	}

	// 리셋
	void Reset()
	{
		Index = UINT32_MAX;
	}

	uint32 GetIndex() const
	{
		return Index;
	}

private:
	uint32 Index;  // GUObjectArray 인덱스
};
