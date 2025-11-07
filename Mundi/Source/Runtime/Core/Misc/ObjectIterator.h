#pragma once

#include "ObjectFactory.h"

template<typename TObject>
class TObjectIterator
{
public:
	TObjectIterator()
	{
		++(*this); // 첫 번째 유효 객체로 이동
	}

	// 다음 객체로 이동
	TObjectIterator& operator++()
	{
		++CurrentIndex;
		AdvanceToNextValidObject();
		return *this;
	}

	// 현재 객체에 접근
	TObject* operator*() const
	{
		// 이 시점의 CurrentIndex는 유효한 TObject를 가리키고 있어야 함
		return static_cast<TObject*>(GUObjectArray[CurrentIndex]);
	}

	// 현재 객체에 접근 (포인터 연산자)
	TObject* operator->() const
	{
		return **this;
	}

	// 비교 연산자
	bool operator!=(const TObjectIterator& Other) const
	{
		return CurrentIndex != Other.CurrentIndex;
	}

	// bool 변환 연산자
	explicit operator bool() const
	{
		// CurrentIndex가 배열 범위 내에 있는지 확인
		return CurrentIndex < GUObjectArray.Num();
	}

private:
	// 현재 인덱스부터 시작하여 다음 유효 객체를 찾는 헬퍼 함수
	void AdvanceToNextValidObject()
	{
		while (CurrentIndex < GUObjectArray.Num())
		{
			UObject* Object = GUObjectArray[CurrentIndex];
			// 현재 객체가 유효하고, TObject 타입이면 검색 종료
			if (Object && Object->IsA<TObject>())
			{
				break;
			}

			// 다음 인덱스로 이동
			++CurrentIndex;
		}
	}

private:
	int32 CurrentIndex = -1;
};
