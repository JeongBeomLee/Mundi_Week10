#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#include "UEContainer.h"
#include "Vector.h"
#include "Enums.h"
#include "StaticMesh.h" // FStaticMesh, FObjMaterialInfo 등이 정의된 헤더로 가정

// Raw Data
struct FObjInfo
{
	TArray<FVector> Positions;
	TArray<FVector2D> TexCoords;
	TArray<FVector> Normals;

	TArray<uint32> PositionIndices;
	TArray<uint32> TexCoordIndices;
	TArray<uint32> NormalIndices;

	TArray<FString> MaterialNames; // == 강의 글의 meshMaterials
	TArray<uint32> GroupIndexStartArray; // i번째 Group이 사용하는 indices 범위: GroupIndexStartArray[i] ~ GroupIndexStartArray[i+1]
	TArray<uint32> GroupMaterialArray; // i번쩨 Group이 사용하는 MaterialInfos 인덱스 넘버

	FString ObjFileName;

	bool bHasMtl = true;
};

struct FObjImporter
{
public:
	struct VertexKey
	{
		uint32 PosIndex, TexIndex, NormalIndex;
		bool operator==(const VertexKey& Other) const { return PosIndex == Other.PosIndex && TexIndex == Other.TexIndex && NormalIndex == Other.NormalIndex; }
	};

	struct VertexKeyHash
	{
		size_t operator()(const VertexKey& Key) const { return std::hash<uint32>()(Key.PosIndex) ^ (std::hash<uint32>()(Key.TexIndex) << 1) ^ (std::hash<uint32>()(Key.NormalIndex) << 2); }
	};

	static bool LoadObjModel(const FString& InFileName, FObjInfo* const OutObjInfo, TArray<FMaterialInfo>& OutMaterialInfos, bool bIsRightHanded = true);

	static void ConvertToStaticMesh(const FObjInfo& InObjInfo, const TArray<FMaterialInfo>& InMaterialInfos, FStaticMesh* const OutStaticMesh);

private:
	struct FFaceVertex { uint32 PositionIndex, TexCoordIndex, NormalIndex; };

	static FFaceVertex ParseVertexDef(const FString& InVertexDef);
};

class UStaticMesh;

class FObjManager
{
private:
	static TMap<FString, FStaticMesh*> ObjStaticMeshMap;
public:
	static void Preload();
	static void Clear();
	static FStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName);
	static UStaticMesh* LoadObjStaticMesh(const FString& PathFileName);
};
