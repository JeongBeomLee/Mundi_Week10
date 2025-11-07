#include "pch.h"
#include "MemoryManager.h"
#include <cstddef>

uint32 CMemoryManager::TotalAllocationBytes = 0;
uint32 CMemoryManager::TotalAllocationCount = 0;

void* CMemoryManager::Allocate(size_t size)
{
    size_t totalSize = size + sizeof(size_t);
#if defined(_MSC_VER) && defined(_DEBUG)
    void* raw = _malloc_dbg(totalSize, _NORMAL_BLOCK, nullptr, 0);
#else
    void* raw = std::malloc(totalSize);
#endif
    if (!raw)
        return nullptr;

    *reinterpret_cast<size_t*>(raw) = size;
    TotalAllocationBytes += static_cast<uint32>(size);
    TotalAllocationCount++;

    return static_cast<void*>(static_cast<unsigned char*>(raw) + sizeof(size_t));
}

void CMemoryManager::Deallocate(void* ptr)
{
    if (!ptr)
        return;

    unsigned char* userPtr = static_cast<unsigned char*>(ptr);
    unsigned char* raw = userPtr - sizeof(size_t);
    size_t size = *reinterpret_cast<size_t*>(raw);

    TotalAllocationBytes -= static_cast<uint32>(size);
    TotalAllocationCount--;
#if defined(_MSC_VER) && defined(_DEBUG)
    _free_dbg(raw, _NORMAL_BLOCK);
#else
    std::free(raw);
#endif
}

// Global operators removed. Allocation is scoped to UObject via class-specific operators.
