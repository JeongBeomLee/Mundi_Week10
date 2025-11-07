#pragma once

#include "Property.h"
#include "Object.h"

// 프로퍼티 렌더러
// 리플렉션 정보를 기반으로 ImGui UI를 자동 생성
class UPropertyRenderer
{
public:
	// 단일 프로퍼티 렌더링
	static bool RenderProperty(const FProperty& Property, void* ObjectInstance);

	// 객체의 원하는 프로퍼티를 카테고리별로 렌더링
	static void RenderProperties(const TArray<FProperty>& Properties, UObject* Object);

	// 객체의 모든 프로퍼티를 카테고리별로 렌더링
	static void RenderAllProperties(UObject* Object);

	// 객체의 모든 프로퍼티를 카테고리별로 렌더링 (부모 클래스 프로퍼티 포함)
	static void RenderAllPropertiesWithInheritance(UObject* Object);

private:
	// 타입별 렌더링 함수들
	static bool RenderBoolProperty(const FProperty& Prop, void* Instance);
	static bool RenderInt32Property(const FProperty& Prop, void* Instance);
	static bool RenderFloatProperty(const FProperty& Prop, void* Instance);
	static bool RenderVectorProperty(const FProperty& Prop, void* Instance);
	static bool RenderColorProperty(const FProperty& Prop, void* Instance);
	static bool RenderStringProperty(const FProperty& Prop, void* Instance);
	static bool RenderNameProperty(const FProperty& Prop, void* Instance);
	static bool RenderObjectPtrProperty(const FProperty& Prop, void* Instance);
	static bool RenderStructProperty(const FProperty& Prop, void* Instance);
	static bool RenderTextureProperty(const FProperty& Prop, void* Instance);
	static bool RenderStaticMeshProperty(const FProperty& Prop, void* Instance);
	static bool RenderMaterialProperty(const FProperty& Prop, void* Instance);
	static bool RenderMaterialArrayProperty(const FProperty& Prop, void* Instance);
	static bool RenderEnumProperty(const FProperty& Prop, void* Instance);
	static bool RenderSingleMaterialSlot(const char* Label, UMaterialInterface** MaterialPtr, UObject* OwningObject, uint32 MaterialIndex);	// 단일 UMaterial* 슬롯을 렌더링하는 헬퍼 함수.

	static bool RenderTextureSelectionCombo(const char* Label, UTexture* CurrentTexture, UTexture*& OutNewTexture);

	// Transform 프로퍼티 렌더링 헬퍼 함수
	static bool RenderTransformProperty(const FProperty& Prop, void* Instance);

	// 조건부 프로퍼티 표시 여부 판단 함수
	static bool ShouldRenderProperty(const FProperty& Prop, UObject* Object);

	static void CacheResources();	// 필요할 때 리소스 목록을 멤버 변수에 캐시합니다.
	static void ClearResourcesCache();	// 렌더링 패스가 끝날 때 캐시를 비웁니다.

private:
	// 렌더링 중 캐시되는 리소스 목록
	static TArray<FString> CachedStaticMeshPaths;
	static TArray<const char*> CachedStaticMeshItems;
	static TArray<FString> CachedMaterialPaths;
	static TArray<const char*> CachedMaterialItems;
	static TArray<FString> CachedShaderPaths;
	static TArray<const char*> CachedShaderItems;
	static TArray<FString> CachedTexturePaths;
	static TArray<const char*> CachedTextureItems;
};
