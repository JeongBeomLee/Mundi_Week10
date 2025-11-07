#pragma once
#include "Property.h"
#include "Color.h"
#include "StaticMesh.h"
#include "Texture.h"
#include <type_traits>

// ===== 타입 자동 감지 템플릿 =====

// 기본 타입 감지 템플릿
template<typename T>
struct TPropertyTypeTraits
{
	static constexpr EPropertyType GetType()
	{
		if constexpr (std::is_same_v<T, bool>)
			return EPropertyType::Bool;
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int>)
			return EPropertyType::Int32;
		else if constexpr (std::is_same_v<T, float>)
			return EPropertyType::Float;
		else if constexpr (std::is_same_v<T, FVector>)
			return EPropertyType::FVector;
		else if constexpr (std::is_same_v<T, FLinearColor>)
			return EPropertyType::FLinearColor;
		else if constexpr (std::is_same_v<T, FString>)
			return EPropertyType::FString;
		else if constexpr (std::is_same_v<T, FName>)
			return EPropertyType::FName;
		else if constexpr (std::is_pointer_v<T>)
			return EPropertyType::ObjectPtr;  // UObject* 및 파생 타입
		else if constexpr (std::is_same_v<T, UTexture>)
			return EPropertyType::Texture;
		else if constexpr (std::is_same_v<T, UStaticMesh>)
			return EPropertyType::StaticMesh;
		else 
			return EPropertyType::Struct;
	}
};

// ===== 리플렉션 매크로 (수동 등록 방식) =====

// 헤더 파일에서 사용: 리플렉션 등록 함수 선언 + 자동 호출
//
// Static 초기화 순서 안전성:
// - StaticRegisterProperties()는 lambda를 통해 StaticClass() 최초 호출 시 자동 실행됨
// - DECLARE_CLASS 매크로 내부의 static UClass Cls가 먼저 초기화된 후
//   lambda 내부의 bRegistered가 초기화되므로 순서가 보장됨
// - 프로퍼티 등록은 프로그램 시작 시 1회만 발생하며, 런타임 중 변경 불가
//
// 사용법:
// class UMyComponent : public USceneComponent {
//     DECLARE_CLASS(UMyComponent, USceneComponent)
//     GENERATED_REFLECTION_BODY()
// protected:
//     float Intensity = 1.0f;
//     bool bIsEnabled = true;
// };
#define GENERATED_REFLECTION_BODY() \
private: \
	static void StaticRegisterProperties(); \
	inline static const bool bPropertiesRegistered = []() { \
		StaticRegisterProperties(); \
		return true; \
	}(); \
public:

// CPP 파일에서 사용: 프로퍼티 등록 헬퍼 매크로들
// 사용법:
// BEGIN_PROPERTIES(UMyComponent)
//     MARK_AS_COMPONENT("My Component", "...")
//     ADD_PROPERTY(bool, bIsEnabled, "Light", true)
//     ADD_PROPERTY_RANGE(float, Intensity, "Light", 0.0f, 100.0f, true)
// END_PROPERTIES()

// StaticRegisterProperties 함수 시작
#define BEGIN_PROPERTIES(ClassName) \
	void ClassName::StaticRegisterProperties() \
	{ \
		UClass* Class = StaticClass(); \

// StaticRegisterProperties 함수 종료
#define END_PROPERTIES() \
	}

// 범위 제한이 있는 프로퍼티 추가
#define ADD_PROPERTY_RANGE(VarType, VarName, CategoryName, MinVal, MaxVal, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = TPropertyTypeTraits<VarType>::GetType(); \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.MinValue = MinVal; \
		Prop.MaxValue = MaxVal; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Class->AddProperty(Prop); \
	}

// 범위 제한이 없는 프로퍼티 추가
#define ADD_PROPERTY(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	ADD_PROPERTY_RANGE(VarType, VarName, CategoryName, 0.0f, 0.0f, bEditAnywhere, __VA_ARGS__)

// 리소스 타입 전용 프로퍼티 매크로
// Texture 프로퍼티 추가
#define ADD_PROPERTY_TEXTURE(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Texture; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Class->AddProperty(Prop); \
	}

// StaticMesh 프로퍼티 추가
#define ADD_PROPERTY_STATICMESH(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::StaticMesh; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Class->AddProperty(Prop); \
	}

// Material 프로퍼티 추가
#define ADD_PROPERTY_MATERIAL(VarType, VarName, CategoryName, bEditAnywhere, ...) \
	{ \
		static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
		              "CategoryName must be a string literal!"); \
		FProperty Prop; \
		Prop.Name = #VarName; \
		Prop.Type = EPropertyType::Material; \
		Prop.Offset = offsetof(ThisClass_t, VarName); \
		Prop.Category = CategoryName; \
		Prop.bIsEditAnywhere = bEditAnywhere; \
		Prop.Tooltip = "" __VA_ARGS__; \
		Class->AddProperty(Prop); \
	}

// TArray<...> 프로퍼티 추가
// 첫 번째 인자로 배열의 내부 요소 타입을 받습니다.
#define ADD_PROPERTY_ARRAY(InnerPropertyType, VarName, CategoryName, bEditAnywhere, ...) \
{ \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(CategoryName)>>, \
				  "CategoryName must be a string literal!"); \
	FProperty Prop; \
	Prop.Name = #VarName; \
	Prop.Type = EPropertyType::Array; \
	Prop.InnerType = InnerPropertyType; /* EPropertyType::Material 같은 enum 값 */ \
	Prop.Offset = offsetof(ThisClass_t, VarName); \
	Prop.Category = CategoryName; \
	Prop.bIsEditAnywhere = bEditAnywhere; \
	Prop.Tooltip = "" __VA_ARGS__; \
	Class->AddProperty(Prop); \
}

// 클래스 메타데이터 설정 매크로
// StaticRegisterProperties() 함수 내에서 사용

// 스폰 가능한 액터로 설정
// 사용법: MARK_AS_SPAWNABLE("스태틱 메시", "스태틱 메시 액터를 구현합니다.")
#define MARK_AS_SPAWNABLE(InDisplayName, InDesc) \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDisplayName)>>, \
	              "InDisplayName must be a string literal!"); \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDesc)>>, \
	              "InDesc must be a string literal!"); \
	assert(!Class->bIsSpawnable && "MARK_AS_SPAWNABLE called multiple times!"); \
	Class->bIsSpawnable = true; \
	Class->DisplayName = InDisplayName; \
	Class->Description = InDesc; \

// 컴포넌트로 설정
// 사용법: MARK_AS_COMPONENT("포인트 라이트", "포인트 라이트 컴포넌트를 추가합니다.")
#define MARK_AS_COMPONENT(InDisplayName, InDesc) \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDisplayName)>>, \
	              "InDisplayName must be a string literal"); \
	static_assert(std::is_array_v<std::remove_reference_t<decltype(InDesc)>>, \
	              "InDesc must be a string literal"); \
	assert(!Class->bIsComponent && "MARK_AS_COMPONENT called multiple times!"); \
	Class->bIsComponent = true; \
	Class->DisplayName = InDisplayName; \
	Class->Description = InDesc;


#define CREATE_EDITOR_COMPONENT(InVariableName, Type)\
	InVariableName = NewObject<Type>();\
	InVariableName->SetOwner(this->GetOwner());\
	InVariableName->SetupAttachment(this, EAttachmentRule::KeepRelative);\
	this->GetOwner()->AddOwnedComponent(InVariableName);\
	InVariableName->SetEditability(false);\
	InVariableName->SetHiddenInGame(true);