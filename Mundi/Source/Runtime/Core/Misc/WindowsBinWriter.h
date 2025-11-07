#pragma once
#include "Archive.h"
#include "UEContainer.h"
#include <fstream>

class FWindowsBinWriter : public FArchive
{
public:
    FWindowsBinWriter(const FString& Filename)
        : FArchive(false, true) // Saving 모드
    {
        File.open(Filename, std::ios::binary | std::ios::out);
    }
    ~FWindowsBinWriter() { Close(); }

    void Serialize(void* Data, int64 Length) override
    {
        File.write(reinterpret_cast<char*>(Data), Length);
    }
    /*void Seek(size_t Position) override { File.seekp(Position); }
    size_t Tell() const override { return (size_t)File.tellp(); }*/
    bool Close() override
    {
        if (File.is_open()) { File.close(); return true; }
        return false;
    }

private:
    std::ofstream File;
};