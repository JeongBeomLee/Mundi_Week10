#pragma once
#include "UEContainer.h"


// ── 외부 심볼 ─────────────────────────────────────────────
class UObject;
struct UClass;
extern TArray<UObject*> GUObjectArray;

// ── ObjectFactory 네임스페이스 ─────────────────────────────
namespace ObjectFactory
{
    using ConstructFunc = std::function<UObject* ()>;

    // Registry 접근자
    TMap<UClass*, ConstructFunc>& GetRegistry();

    // 클래스 등록
    void RegisterClassType(UClass* Class, ConstructFunc Func);

    // 1) 순수 생성 (GUObjectArray 등록 X)
    UObject* ConstructObject(UClass* Class);

    // 2) 생성 + GUObjectArray 자동 등록
    UObject* NewObject(UClass* Class);

    // 3) 템플릿 버전 (타입 안전)
    template<class T>
    inline T* NewObject()
    {
        return static_cast<T*>(NewObject(T::StaticClass()));
    }

    // 4) GUObjectArray 자동 등록
    UObject* AddToGUObjectArray(UClass* Class, UObject* Obj);

    // 5) 복사생성자 호출 + GUObjectArray 자동 등록
    template<class T>
    inline T* DuplicateObject(const UObject* Source)
    {
        UObject* Dest = new T(*static_cast<const T*>(Source));
        return static_cast<T*>(AddToGUObjectArray(T::StaticClass(), Dest));
    }

    // 개별 삭제(단일 소유자: Factory)
    void DeleteObject(UObject* Obj);
    // 종료시 일괄 정리
    void DeleteAll(bool bCallBeginDestroy = true);
    // Null 슬롯 압축하여 배열 크기 축소
    void CompactNullSlots();
}

// ── 등록 매크로 ─────────────────────────────────────────────
#define IMPLEMENT_CLASS(ThisClass)                                            \
    namespace {                                                               \
        struct ThisClass##FactoryRegister                                     \
        {                                                                     \
            ThisClass##FactoryRegister()                                      \
            {                                                                 \
                ObjectFactory::RegisterClassType(                             \
                    ThisClass::StaticClass(),                                 \
                    []() -> UObject* { return new ThisClass(); }              \
                );                                                            \
            }                                                                 \
        };                                                                    \
        static ThisClass##FactoryRegister GRegister_##ThisClass;              \
        /* 즉시 실행 람다를 사용하여 정적 초기화 시점에 클래스를 자동 등록합니다. */ \
        static bool bIsRegistered_##ThisClass = [](){ ThisClass::StaticClass(); return true; }(); \
    }
using namespace ObjectFactory;
