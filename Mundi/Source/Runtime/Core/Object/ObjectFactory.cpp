#include "pch.h"
#include "ObjectFactory.h"
// 전역 오브젝트 배열 정의 (한 번만!)
TArray<UObject*> GUObjectArray;

namespace ObjectFactory
{
    TMap<UClass*, ConstructFunc>& GetRegistry()
    {
        static TMap<UClass*, ConstructFunc> Registry;
        return Registry;
    }

    void RegisterClassType(UClass* Class, ConstructFunc Func)
    {
        GetRegistry()[Class] = std::move(Func);
    }

    UObject* ConstructObject(UClass* Class)
    {
        auto& reg = GetRegistry();
        auto it = reg.find(Class);
        if (it == reg.end()) return nullptr;
        return it->second();
    }
    
    UObject* NewObject(UClass* Class)
    {
        UObject* Obj = ConstructObject(Class);
        if (!Obj) return nullptr;

        int32 idx = -1;

        idx = GUObjectArray.Add(Obj);

        Obj->InternalIndex = static_cast<uint32>(idx);

        static TMap<UClass*, int> NameCounters;
        int Count = ++NameCounters[Class];

        const std::string base = Class->Name; // FName -> string
        std::string unique;
        unique.reserve(base.size() + 1 + 12);            // "_" + 최대 10~12자리 여유
        unique.append(base);
        unique.push_back('_');
        unique.append(std::to_string(Count));

        Obj->ObjectName = FName(unique);

        return Obj;
    }

    UObject* AddToGUObjectArray(UClass* Class, UObject* Obj)
    {
        if (!Obj) return nullptr;

        // 배열에 등록: 빈 슬롯 재사용
        int32 idx = -1;

        idx = GUObjectArray.Add(Obj);
        //}
        Obj->InternalIndex = static_cast<uint32>(idx);

        static TMap<UClass*, int> NameCounters;
        int Count = ++NameCounters[Class];

        const std::string base = Class->Name; // FName -> string
        std::string unique;
        unique.reserve(base.size() + 1 + 12);            // "_" + 최대 10~12자리 여유
        unique.append(base);
        unique.push_back('_');
        unique.append(std::to_string(Count));

        Obj->ObjectName = FName(unique);

        return Obj;
    }

    void DeleteObject(UObject* Obj)
    {
        if (!Obj) return;

        // Important: DO NOT dereference Obj fields before verifying it is still in GUObjectArray.
        int32 foundIndex = -1;
        for (int32 i = 0; i < GUObjectArray.Num(); ++i)
        {
            if (GUObjectArray[i] == Obj)
            {
                foundIndex = i;
                break;
            }
        }
        if (foundIndex < 0)
        {
            // Not managed or already deleted.
            return;
        }

        GUObjectArray[foundIndex] = nullptr;
        // Safe to delete now; Obj still valid since we found it in GUObjectArray
        Obj->DestroyInternal();
    }

    void DeleteAll(bool bCallBeginDestroy)
    {
        // 실제 삭제 (역순 안전)
        for (int32 i = GUObjectArray.Num() - 1; i >= 0; --i)
        {
            if (UObject* Obj = GUObjectArray[i])
            {
                DeleteObject(Obj);
            }
        }
        GUObjectArray.Empty();
        GUObjectArray.Shrink();
    }

    // (선택) null 슬롯 압축
    void CompactNullSlots()
    {
        int32 write = 0;
        for (int32 read = 0; read < GUObjectArray.Num(); ++read)
        {
            if (UObject* Obj = GUObjectArray[read])
            {
                if (write != read)
                {
                    GUObjectArray[write] = Obj;
                    Obj->InternalIndex = static_cast<uint32>(write);
                    GUObjectArray[read] = nullptr;
                }
                ++write;
            }
        }
        // 크기(Num) 축소 + 불필요한 capacity도 반환
        GUObjectArray.SetNum(write);
    }
}
