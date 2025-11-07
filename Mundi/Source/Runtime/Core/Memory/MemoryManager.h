#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include "UEContainer.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#   define _CRTDBG_MAP_ALLOC
#   include <crtdbg.h>
#endif

class CMemoryManager
{
public:
    static uint32 TotalAllocationBytes;
    static uint32 TotalAllocationCount;

    static void* Allocate(size_t size);
    static void Deallocate(void* ptr);
};