#pragma once

// 프로퍼티 타입 열거형
enum class EPropertyType : uint8
{
	Unknown,
	Bool,
	Int32,
	Float,
	FVector,
	FLinearColor,
	FString,
	FName,
	ObjectPtr,      // UObject* 및 파생 타입
	Struct,
	Texture,        // UTexture* 타입 (리소스 선택 UI)
	StaticMesh,     // UStaticMesh* 타입 (리소스 선택 UI)
	Material,		// UMaterial* 타입 (리소스 선택 UI)
	Array,			// TArray 용으로 추가
	Enum			// Enum 타입 (라디오 버튼 UI)

	// 추후 추가될 프로퍼티들은 직접 해줘야함.
};

// 프로퍼티 메타데이터
struct FProperty
{
	const char* Name = nullptr;              // 프로퍼티 이름
	EPropertyType Type = EPropertyType::Unknown;  // 프로퍼티 타입
	EPropertyType InnerType = EPropertyType::Unknown; // TArray<T>의 'T' 타입
	size_t Offset = 0;                       // 클래스 인스턴스 내 오프셋
	const char* Category = nullptr;          // UI 카테고리
	float MinValue = 0.0f;                   // 범위 최소값
	float MaxValue = 0.0f;                   // 범위 최대값
	bool bIsEditAnywhere = false;            // UI에 노출 여부
	const char* Tooltip = nullptr;           // 툴팁 설명

	// Enum 타입 전용 메타데이터
	const char** EnumNames = nullptr;        // Enum 값 이름 배열 (예: {"Low", "Medium", "High"})
	uint32 EnumCount = 0;                    // Enum 값 개수

	// 객체 인스턴스에서 프로퍼티 값의 포인터를 가져옴
	template<typename T>
	T* GetValuePtr(void* ObjectInstance) const
	{
		return reinterpret_cast<T*>(reinterpret_cast<char*>(ObjectInstance) + Offset);
	}

	// 객체 인스턴스에서 프로퍼티 값의 const 포인터를 가져옴
	template<typename T>
	const T* GetValuePtr(const void* ObjectInstance) const
	{
		return reinterpret_cast<const T*>(reinterpret_cast<const char*>(ObjectInstance) + Offset);
	}
};
