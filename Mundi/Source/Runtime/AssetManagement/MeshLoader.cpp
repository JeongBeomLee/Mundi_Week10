#include "pch.h"
#include "MeshLoader.h"

IMPLEMENT_CLASS(UMeshLoader)

UMeshLoader& UMeshLoader::GetInstance()
{
    static UMeshLoader* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<UMeshLoader>();
    }
    return *Instance;
}

UMeshLoader::FFace UMeshLoader::ParseFaceBuffer(const FString& FaceBuffer)
{
    FFace Face = {};
    std::istringstream Tokenizer(FaceBuffer);
    FString IndexBuffer;

    // v (항상 존재)
    if (std::getline(Tokenizer, IndexBuffer, '/'))
        Face.IndexPosition = std::stoi(IndexBuffer);

    // vt (항상 존재)
    if (std::getline(Tokenizer, IndexBuffer, '/'))
    {
        if (!IndexBuffer.empty())
            Face.IndexTexCoord = std::stoi(IndexBuffer);
    }

    return Face;
}

UMeshLoader::~UMeshLoader()
{
    for (auto& it : MeshCache)
    {
        if (it.second)
        {
            delete it.second;
        }
        it.second = nullptr;
    }
    MeshCache.Empty();
}
FMeshData* UMeshLoader::LoadMesh(const std::filesystem::path& FilePath)
{
    auto it = MeshCache.find(FilePath.string());
    if (it != MeshCache.end())
    {
        return it->second;
    }

    if (!std::filesystem::exists(FilePath))
        return nullptr;

    std::ifstream File(FilePath);
    if (!File)
        return nullptr;

    TArray<FPosition> Positions;
    TArray<FNormal> Normals;
    TArray<FTexCoord> TexCoords;
    TArray<FFace> Faces;

    FString Line;
    while (std::getline(File, Line))
    {
        std::istringstream Tokenizer(Line);

        FString Prefix;
        Tokenizer >> Prefix;

        if (Prefix == "v") // position
        {
            FPosition Position;
            Tokenizer >> Position.x >> Position.y >> Position.z;
            Positions.push_back(Position);
        }
        else if (Prefix == "vn") // normal
        {
            FNormal Normal;
            Tokenizer >> Normal.x >> Normal.y >> Normal.z;
            Normals.push_back(Normal);
        }
        else if (Prefix == "vt") // uv
        {
            FTexCoord TexCoord;
            Tokenizer >> TexCoord.u >> TexCoord.v;
            TexCoords.push_back(TexCoord);
        }
        else if (Prefix == "f") // face
        {
            FString FaceBuffer;
            while (Tokenizer >> FaceBuffer)
            {
                Faces.push_back(ParseFaceBuffer(FaceBuffer));
            }
        }
        else if (Prefix == "l") // line (바운딩박스 OBJ용)
        {
            FString LineBuffer;
            while (Tokenizer >> LineBuffer)
            {
                Faces.push_back(ParseFaceBuffer(LineBuffer));
            }
        }
    }
    FMeshData* MeshData = new FMeshData();
    TMap<FVertexKey, uint32> UniqueVertexMap;

    for (const auto& Face : Faces)
    {
        const FPosition& Pos = Positions[Face.IndexPosition - 1];
        FVertexKey key{ Pos.x, Pos.y, Pos.z };

        auto it = UniqueVertexMap.find(key);
        if (it != UniqueVertexMap.end())
        {
            MeshData->Indices.push_back(it->second);
        }
        else
        {
            uint32 newIndex = static_cast<uint32>(MeshData->Vertices.size());

            // 위치 데이터 추가
            MeshData->Vertices.push_back(FVector(Pos.x, Pos.y, Pos.z));
            
            // 랜덤 색상 추가
            MeshData->Color.push_back(FVector4(
                static_cast<float>(rand()) / RAND_MAX,
                static_cast<float>(rand()) / RAND_MAX,
                static_cast<float>(rand()) / RAND_MAX,
                1.0f
            ));

            MeshData->Indices.push_back(newIndex);
            UniqueVertexMap[key] = newIndex;
        }
    }

    MeshCache[FilePath.string()] = MeshData;

    return MeshData;
}

void UMeshLoader::AddMeshData(const FString& InName, FMeshData* InMeshData)
{
    if (MeshCache.Contains(InName))
    {
        delete MeshCache[InName];
    }
    MeshCache[InName] = InMeshData;
}

const TMap<FString, FMeshData*>* UMeshLoader::GetMeshCache()
{
    return &MeshCache;
}

UMeshLoader::UMeshLoader()
{
}
