//#ifndef UE_ENUMS_H
//#define UE_ENUMS_H
#pragma once
#include "UEContainer.h"
//#include "Enums.h"
#include "Archive.h"
#include <d3d11.h>

struct FMaterialInfo
{
    int32 IlluminationModel = 2;  // illum. Default illumination model to Phong for non-Pbr materials

    FVector DiffuseColor = FVector(0.8f, 0.8f, 0.8f);   // Kd - 표준: 0.8
    FVector AmbientColor = FVector(0.2f, 0.2f, 0.2f);   // Ka - 표준: 0.2
    FVector SpecularColor = FVector::One();             // Ks - 표준: 1.0 (유지)
    FVector EmissiveColor = FVector::Zero();            // Ke - 표준: 0.0 (중요!)

    FString DiffuseTextureFileName;
    FString NormalTextureFileName;
    FString AmbientTextureFileName;
    FString SpecularTextureFileName;
    FString EmissiveTextureFileName;
    FString TransparencyTextureFileName;
    FString SpecularExponentTextureFileName;

    FVector TransmissionFilter = FVector::One(); // Tf

    float OpticalDensity = -1.0f; // Ni
    float Transparency = -1.0f; // Tr Or d
    float SpecularExponent = -1.0f; // Ns
    float BumpMultiplier = 1.0f; // map_Bump -bm

    FString MaterialName;

    friend FArchive& operator<<(FArchive& Ar, FMaterialInfo& Info)
    {
        Ar << Info.IlluminationModel;
        Ar << Info.DiffuseColor;
        Ar << Info.AmbientColor;
        Ar << Info.SpecularColor;
        Ar << Info.EmissiveColor;

        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Info.DiffuseTextureFileName);
            Serialization::WriteString(Ar, Info.NormalTextureFileName);
            Serialization::WriteString(Ar, Info.AmbientTextureFileName);
            Serialization::WriteString(Ar, Info.SpecularTextureFileName);
            Serialization::WriteString(Ar, Info.EmissiveTextureFileName);
            Serialization::WriteString(Ar, Info.TransparencyTextureFileName);
            Serialization::WriteString(Ar, Info.SpecularExponentTextureFileName);
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Info.DiffuseTextureFileName);
            Serialization::ReadString(Ar, Info.NormalTextureFileName);
            Serialization::ReadString(Ar, Info.AmbientTextureFileName);
            Serialization::ReadString(Ar, Info.SpecularTextureFileName);
            Serialization::ReadString(Ar, Info.EmissiveTextureFileName);
            Serialization::ReadString(Ar, Info.TransparencyTextureFileName);
            Serialization::ReadString(Ar, Info.SpecularExponentTextureFileName);
        }

        Ar << Info.TransmissionFilter;
        Ar << Info.OpticalDensity;
        Ar << Info.Transparency;
        Ar << Info.SpecularExponent;

        if (Ar.IsSaving())
            Serialization::WriteString(Ar, Info.MaterialName);
        else if (Ar.IsLoading())
            Serialization::ReadString(Ar, Info.MaterialName);

        return Ar;
    }
};

// ---- FObjMaterialInfo 전용 Serialization 특수화 ----
namespace Serialization {
    template<>
    inline void WriteArray<FMaterialInfo>(FArchive& Ar, const TArray<FMaterialInfo>& Arr) {
        uint32 Count = (uint32)Arr.size();
        Ar << Count;
        for (auto& Mat : Arr) Ar << const_cast<FMaterialInfo&>(Mat);
    }

    template<>
    inline void ReadArray<FMaterialInfo>(FArchive& Ar, TArray<FMaterialInfo>& Arr) {
        uint32 Count;
        Ar << Count;
        Arr.resize(Count);
        for (auto& Mat : Arr) Ar << Mat;
    }
}

struct FGroupInfo
{
    uint32 StartIndex;
    uint32 IndexCount;
    //FObjMaterialInfo MaterialInfo;
    FString InitialMaterialName; // obj 파일 자체에 맵핑된 material 이름

    friend FArchive& operator<<(FArchive& Ar, FGroupInfo& Info)
    {
        Ar << Info.StartIndex;
        Ar << Info.IndexCount;

        if (Ar.IsSaving())
            Serialization::WriteString(Ar, Info.InitialMaterialName);
        else if (Ar.IsLoading())
            Serialization::ReadString(Ar, Info.InitialMaterialName);

        return Ar;
    }
};

// Helper serialization operators for math types to ensure safe, member-wise serialization
inline FArchive& operator<<(FArchive& Ar, FVector& V) { Ar.Serialize(&V.X, sizeof(float) * 3); return Ar; }
inline FArchive& operator<<(FArchive& Ar, FVector2D& V) { Ar.Serialize(&V.X, sizeof(float) * 2); return Ar; }
inline FArchive& operator<<(FArchive& Ar, FVector4& V) { Ar.Serialize(&V.X, sizeof(float) * 4); return Ar; }

// 직렬화 포맷 (FVertexDynamic와 역할이 달라서 분리됨)
struct FNormalVertex
{
    FVector pos;
    FVector normal;
    FVector2D tex;
    FVector4 Tangent;
    FVector4 color;

    friend FArchive& operator<<(FArchive& Ar, FNormalVertex& Vtx)
    {
        // Serialize member by member to avoid issues with struct padding and alignment
        Ar << Vtx.pos;
        Ar << Vtx.normal;
        Ar << Vtx.Tangent;
        Ar << Vtx.color;
        Ar << Vtx.tex;
        return Ar;
    }

    bool operator==(const FNormalVertex& Other) const
    {
        return pos == Other.pos &&
               normal == Other.normal &&
               tex == Other.tex &&
               Tangent == Other.Tangent &&
               color == Other.color;
    }
};

// Template specialization for TArray<FNormalVertex> to force element-by-element serialization
namespace Serialization {
    template<>
    inline void WriteArray<FNormalVertex>(FArchive& Ar, const TArray<FNormalVertex>& Arr) {
        uint32 Count = (uint32)Arr.size();
        Ar << Count;
        for (const auto& Vtx : Arr) Ar << const_cast<FNormalVertex&>(Vtx);
    }

    template<>
    inline void ReadArray<FNormalVertex>(FArchive& Ar, TArray<FNormalVertex>& Arr) {
        uint32 Count;
        Ar << Count;
        Arr.resize(Count);
        for (auto& Vtx : Arr) Ar << Vtx;
    }
}

//// Cooked Data
struct FStaticMesh
{
    FString PathFileName;
    FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube.obj.bin)

    TArray<FNormalVertex> Vertices;
    TArray<uint32> Indices;
    // to do: 여러가지 추가(ex: material 관련)
    TArray<FGroupInfo> GroupInfos; // 각 group을 render 하기 위한 정보

    bool bHasMaterial;

    friend FArchive& operator<<(FArchive& Ar, FStaticMesh& Mesh)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Mesh.PathFileName);
            Serialization::WriteArray(Ar, Mesh.Vertices);
            Serialization::WriteArray(Ar, Mesh.Indices);

            uint32_t gCount = (uint32_t)Mesh.GroupInfos.size();
            Ar << gCount;
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Mesh.PathFileName);
            Serialization::ReadArray(Ar, Mesh.Vertices);
            Serialization::ReadArray(Ar, Mesh.Indices);

            uint32_t gCount;
            Ar << gCount;
            Mesh.GroupInfos.resize(gCount);
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        return Ar;
    }
};

// Bone 정보
struct FBoneInfo
{
    FString BoneName;           // 본 이름
    int32 ParentIndex;          // 부모 본 인덱스 (-1: 루트 본)

    // ========================================
    // 스키닝 행렬 (3가지 행렬의 역할)
    // ========================================

    // 1. InverseBindPoseMatrix: 월드 좌표 → 본 로컬 좌표 (Model → Bone)
    //    - 바인드 포즈에서 정점을 본의 로컬 공간으로 변환
    //    - 계산: LinkTransform.Inverse() * MeshTransform
    //    - 의미: "정점이 본 로컬 공간에서 어디에 있었나"를 기록
    FMatrix InverseBindPoseMatrix;

    // 2. GlobalTransform: 본 로컬 좌표 → 월드 좌표 (Bone → Model)
    //    - 바인드 포즈 시: 초기 본의 월드 변환 (LinkTransform)
    //    - 애니메이션 시: 현재 프레임의 본 월드 변환 (매 프레임 업데이트)
    //    - 의미: "본이 현재 월드 공간에서 어디에 있나"
    FMatrix GlobalTransform;

    // 3. SkinningMatrix: 최종 스키닝 행렬 (셰이더에 전달)
    //    - 계산: InverseBindPoseMatrix * GlobalTransform
    //    - 의미: (Model → Bone) × (Bone → Model) = 바인드 포즈 기준 본 변형
    //    - 용도: 정점 셰이더에서 v' = v * SkinningMatrix
    //    - 업데이트: 애니메이션 프레임마다 SkinningMatrix = InverseBindPoseMatrix * AnimatedGlobalTransform
    FMatrix SkinningMatrix;
};

// Skinning Vertex (Bone Weights + Indices 포함)
struct FSkinnedVertex
{
    FNormalVertex BaseVertex;  // Position, Normal, UV, Tangent, Color
    uint8 BoneIndices[4];      // 영향을 주는 Bone 인덱스 (최대 4개)
    float BoneWeights[4];      // 각 Bone의 Weight (합 = 1.0)

    FSkinnedVertex()
    {
        BoneIndices[0] = BoneIndices[1] = BoneIndices[2] = BoneIndices[3] = 0;
        BoneWeights[0] = BoneWeights[1] = BoneWeights[2] = BoneWeights[3] = 0.0f;
    }
};

//// Cooked Data
struct FSkeletalMesh
{
    FString PathFileName;
    FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube.obj.bin)

    TArray<FNormalVertex> Vertices;
    TArray<uint32> Indices;
    TArray<FGroupInfo> GroupInfos; // 각 group을 render 하기 위한 정보

    bool bHasMaterial;

    // Skeletal Mesh 전용 데이터
    TArray<FBoneInfo> Bones;           // Skeleton (Bone Hierarchy)
    TArray<FSkinnedVertex> SkinnedVertices;  // Skinning 정보가 포함된 정점들

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMesh& Mesh)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Mesh.PathFileName);
            Serialization::WriteArray(Ar, Mesh.Vertices);
            Serialization::WriteArray(Ar, Mesh.Indices);

            uint32_t gCount = (uint32_t)Mesh.GroupInfos.size();
            Ar << gCount;
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Mesh.PathFileName);
            Serialization::ReadArray(Ar, Mesh.Vertices);
            Serialization::ReadArray(Ar, Mesh.Indices);

            uint32_t gCount;
            Ar << gCount;
            Mesh.GroupInfos.resize(gCount);
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        return Ar;
    }
};

struct FMeshData
{
    // 중복 없는 정점
    TArray<FVector> Vertices;//also can be billboard world position
    // 정점 인덱스
    TArray<uint32> Indices;
    // 중복 없는 정점
    TArray<FVector4> Color;//also can be UVRect
    // UV 좌표
    TArray<FVector2D> UV;//also can be Billboard size
    // 노말 좌표
    TArray<FVector> Normal;
    //

};
enum class EPrimitiveTopology
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

struct FResourceData
{
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수 
    uint32 ByteWidth = 0;       // 전체 버텍스 데이터 크기 (sizeof(FVertexSimple) * VertexCount)
    uint32 Stride = 0;
    uint32 Offset = 0;
    EPrimitiveTopology Topology = EPrimitiveTopology::TriangleList;
    D3D11_PRIMITIVE_TOPOLOGY Topol = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};


struct FShader
{
    ID3D11InputLayout* SimpleInputLayout = nullptr;
    ID3D11VertexShader* SimpleVertexShader = nullptr;
    ID3D11PixelShader* SimplePixelShader = nullptr;
};

struct FTextureData
{
    ID3D11Resource* Texture = nullptr;
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    ID3D11BlendState* BlendState = nullptr;
};

enum class EResourceType
{
    VertexBuffer,
    IndexBuffer
};

enum class EGizmoMode : uint8
{
    Translate,
    Rotate,
    Scale,
    Select
};
enum class EGizmoSpace : uint8
{
    World,
    Local
};

enum class EKeyInput : uint8
{
    // Keyboard Keys
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space, Enter, Escape, Tab, Shift, Ctrl, Alt,
    Up, Down, Left, Right,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Mouse Buttons
    LeftMouse, RightMouse, MiddleMouse, Mouse4, Mouse5,

    // Special
    Unknown
};

//TODO EResourceType으로 재정의
enum class ResourceType : uint8
{
    None,

    StaticMesh,
    SkeletalMesh,
    Quad,
    DynamicMesh,
    Shader,
    Texture,
    Material,
    Sound,  // FMOD 사운드 리소스

    End
};

enum class EVertexLayoutType : uint8
{
    None,

    PositionColor,
    PositionColorTexturNormal,

    PositionTextBillBoard,
    PositionCollisionDebug,
    PositionBillBoard,

    End,
};

// 에디터에서 설정할 수 있는 디버깅용 뷰 모드
enum class EViewModeIndex : uint32
{
    None,

    VMI_Lit,                // 기본 Phong 조명 모드
    VMI_Lit_Gouraud,        // [Shader override] Gouraud 조명 (Per-Vertex)
    VMI_Lit_Lambert,        // [Shader override] Lambert 조명 (Per-Pixel Diffuse)
    VMI_Lit_Phong,          // [Shader override] Phong 조명 (Per-Pixel Full)
    VMI_Unlit,              // 조명 없음
    VMI_WorldNormal,        // World Normal 시각화
    VMI_Wireframe,          // 와이어프레임
    VMI_SceneDepth,         // 깊이 버퍼 시각화

    End,
};

// RHI가 사용할 래스터라이저 상태
enum class ERasterizerMode : uint32
{
    Solid,          // 면으로 채움 (기본값)
    Wireframe,      // 선으로 그림
    Solid_NoCull,   // 면으로 채우고, 뒷면 컬링 안 함(양면 그리기)
    Decal,          // 데칼 전용 상태 (Z-Fighting 방지용 DepthBias를 추가로 줌)
    Shadow          // 쉐도우 맵 렌더링 전용 (Shadow acne 방지용 DepthBias 적용)
};

// RHI가 사용할 RTV
enum class ERTVMode : uint32
{
	BackBufferWithDepth,
	BackBufferWithoutDepth,
    SceneIdTarget,
    SceneColorTarget,
    SceneColorTargetWithoutDepth,
    SceneColorTargetWithId, //모델 그릴때만 사용, 후처리는 별도
};

// RHI가 사용하는 텍스쳐들의 SRV
enum class RHI_SRV_Index : uint32
{
	SceneDepth,  // 장면 깊이
    SceneColorSource,
};

enum class RHI_Sampler_Index : uint32
{
    Default,    // 기본 샘플러 (반복, 선형 필터링)
    LinearClamp,
	PointClamp
};

enum class EPrimitiveType : uint32
{
    None,

    Default,
    Sphere,

    End,
};
/**
 * Show Flag system for toggling rendering features globally
 * Uses bit flags for efficient storage and checking
 */

enum class EEngineShowFlags : uint64
{
    None = 0,

    // Primitive rendering
    SF_Primitives = 1ull << 0,    // Show/hide all primitive geometry
    SF_StaticMeshes = 1ull << 1,  // Show/hide static mesh actors

    // Debug features
    SF_BillboardText = 1ull << 3, // Show/hide UUID text above objects
    SF_BoundingBoxes = 1ull << 4, // Show/hide collision bounds
    SF_Grid = 1ull << 5,          // Show/hide world grid
    SF_Collision = 1ull << 16,    // Show/hide collision component debug shapes
    SF_CollisionBVH = 1ull << 17, // Show/hide collision BVH debug visualization

    // Lighting
    SF_Lighting = 1ull << 6,      // Enable/disable lighting

    SF_OctreeDebug = 1ull << 7,  // Show/hide octree debug bounds
    SF_BVHDebug = 1ull << 8,  // Show/hide BVH debug bounds
    SF_Culling = 1ull << 9,          // Show/hide world grid

    SF_Decals = 1ull << 10,
    SF_Fog = 1ull << 11,

    SF_FXAA = 1ull << 12,

    SF_TileCulling = 1ull << 13,      // Enable/disable tile-based light culling
    SF_TileCullingDebug = 1ull << 14, // Show/hide tile culling debug visualization

    SF_Billboard = 1ull << 15,

    // Default enabled flags
    SF_DefaultEnabled = SF_Primitives | SF_StaticMeshes | SF_Grid | SF_Lighting | SF_Decals | SF_Fog | SF_FXAA | SF_Billboard,

    // All flags (for initialization/reset)
    SF_All = 0xFFFFFFFFFFFFFFFFull
};

enum class EViewportLayoutMode
{
    SingleMain,
    FourSplit
};


// Bit flag operators for EEngineShowFlags
inline EEngineShowFlags operator|(EEngineShowFlags a, EEngineShowFlags b)
{
    return static_cast<EEngineShowFlags>(static_cast<uint64>(a) | static_cast<uint64>(b));
}

inline EEngineShowFlags operator&(EEngineShowFlags a, EEngineShowFlags b)
{
    return static_cast<EEngineShowFlags>(static_cast<uint64>(a) & static_cast<uint64>(b));
}

inline EEngineShowFlags operator~(EEngineShowFlags a)
{
    return static_cast<EEngineShowFlags>(~static_cast<uint64>(a));
}

inline EEngineShowFlags& operator|=(EEngineShowFlags& a, EEngineShowFlags b)
{
    a = a | b;
    return a;
}

inline EEngineShowFlags& operator&=(EEngineShowFlags& a, EEngineShowFlags b)
{
    a = a & b;
    return a;
}

// Helper function to check if a flag is set
inline bool HasShowFlag(EEngineShowFlags flags, EEngineShowFlags flag)
{
    return (flags & flag) != EEngineShowFlags::None;
}

enum class EViewportType : uint8
{
    Perspective,    // 원근 뷰
    Orthographic_Top,     // 상단 직교 뷰
    Orthographic_Bottom,    // 하단 직교 뷰
    Orthographic_Front,   // 정면 직교 뷰
    Orthographic_Left,     // 왼쪽면 직교 뷰 
    Orthographic_Right,   // 오른쪽면 직교 뷰
    Orthographic_Back     // 측면 직교 뷰
};

enum class EWorldType : uint8
{
    None = 0,

    Editor,
    Game,

    End,
};

//#endif /** UE_ENUMS_H */

