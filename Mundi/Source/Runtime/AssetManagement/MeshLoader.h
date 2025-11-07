#pragma once
#include <filesystem>
#include <vector>
#include <cassert>
#include <fstream>
#include "UEContainer.h"
#include "Enums.h"
#include "Object.h"

struct FPosition
{
	float x;
	float y;
	float z;
};

struct FNormal
{
	float x;
	float y;
	float z;
};

struct FTexCoord
{
	float u;
	float v;
};

struct FVertexKey
{
    float x, y, z;

    bool operator==(const FVertexKey& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

// std::hash 특수화
namespace std {
    template <>
    struct hash<FVertexKey>
    {
        size_t operator()(const FVertexKey& v) const noexcept
        {
            size_t hx = hash<float>()(v.x);
            size_t hy = hash<float>()(v.y);
            size_t hz = hash<float>()(v.z);
            return ((hx ^ (hy << 1)) >> 1) ^ (hz << 1);
        }
    };
}
class UMeshLoader : public UObject
{
public:
    DECLARE_CLASS(UMeshLoader, UObject)
    static UMeshLoader& GetInstance();

    UMeshLoader(const UMeshLoader&) = delete;
    UMeshLoader(UMeshLoader&&) = delete;

    UMeshLoader& operator=(const UMeshLoader&) = delete;
    UMeshLoader& operator=(UMeshLoader&&) = delete;

    // OBJ 파일 파싱 → 버텍스/인덱스 버퍼 리턴
    FMeshData* LoadMesh(const std::filesystem::path& FilePath);
    void AddMeshData(const FString& InName, FMeshData* InMeshData);
    const TMap<FString, FMeshData*>* GetMeshCache();

    UMeshLoader();
protected:
    ~UMeshLoader() override;

private:
    struct FFace
    {
        size_t IndexPosition;
        size_t IndexNormal;
        size_t IndexTexCoord;
    };

    static FFace ParseFaceBuffer(const FString& FaceBuffer);
    TMap<FString, FMeshData*> MeshCache;
};
