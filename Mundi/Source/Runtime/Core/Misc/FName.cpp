#include "pch.h"
#include "Name.h"

TArray<FNameEntry> FNamePool::Entries;
std::unordered_map<FString, uint32> FNamePool::NameMap;

uint32 FNamePool::Add(const FString& InStr)
{
    FString Lower = InStr;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(), ::tolower);

    auto it = NameMap.find(Lower);
    if (it != NameMap.end())
        return it->second;

    uint32 NewIndex = (uint32)Entries.size();
    Entries.push_back({ InStr, Lower });
    NameMap[Lower] = NewIndex;
    return NewIndex;
}

const FNameEntry& FNamePool::Get(uint32 Index)
{
    return Entries[Index];
}