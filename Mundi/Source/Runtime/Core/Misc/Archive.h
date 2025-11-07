#pragma once
#include "UEContainer.h"

class FArchive
{
public:
    FArchive(bool bLoading = false, bool bSaving = false)
        : bIsLoading(bLoading), bIsSaving(bSaving) {
    }

    virtual ~FArchive() {}

    virtual void Serialize(void* Data, int64 Length) = 0;
    /*virtual void Seek(size_t Position) = 0;
    virtual size_t Tell() const = 0;*/
    virtual bool Close() = 0;

    // 상태 확인 함수
    bool IsLoading() const { return bIsLoading; }
    bool IsSaving() const { return bIsSaving; }

    template<typename T>
    FArchive& operator<<(T& Value)
    {
        Serialize(&Value, sizeof(T));
        return *this;
    }

protected:
    bool bIsLoading;
    bool bIsSaving;
};

namespace Serialization
{
    // 최대 허용 배열 크기. 이보다 크면 캐시가 손상된 것으로 간주합니다.
    constexpr uint32_t MAX_REASONABLE_ARRAY_SIZE = 50'000'000;

    inline void WriteString(FArchive& Ar, const FString& Str)
    {
        uint32 Len = (uint32)Str.size();
        Ar << Len;
        if (Len > 0)
            Ar.Serialize((void*)Str.data(), Len);
    }

    inline void ReadString(FArchive& Ar, FString& Str)
    {
        uint32 Len;
        Ar << Len;

        // Sanity Check: 비정상적인 크기의 문자열 할당 시도 방지
        if (Len > MAX_REASONABLE_ARRAY_SIZE)
        {
            throw std::runtime_error("Cache corrupt: String length is unreasonable.");
        }

        Str.resize(Len);
        if (Len > 0)
            Ar.Serialize(&Str[0], Len);
    }

    template<typename T>
    inline void WriteArray(FArchive& Ar, const TArray<T>& Arr)
    {
        uint32 Count = (uint32)Arr.size();
        Ar << Count;
        if (Count > 0)
            Ar.Serialize((void*)Arr.data(), sizeof(T) * Count);
    }

    template<typename T>
    inline void ReadArray(FArchive& Ar, TArray<T>& Arr)
    {
        uint32_t Count;
        Ar << Count;

        // Sanity Check: 비정상적인 크기의 배열 할당 시도 방지
        if (Count > MAX_REASONABLE_ARRAY_SIZE)
        {
            throw std::runtime_error("Cache corrupt: Generic array size is unreasonable.");
        }

        Arr.resize(Count);
        if (Count > 0)
            Ar.Serialize((void*)Arr.data(), sizeof(T) * Count);
    }
}