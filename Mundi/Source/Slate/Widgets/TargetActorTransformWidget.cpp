#include "pch.h"
#include <string>
#include "TargetActorTransformWidget.h"
#include "UIManager.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "World.h"
#include "ResourceManager.h"
#include "SelectionManager.h"
#include "WorldPartitionManager.h"
#include "PropertyRenderer.h"
#include "USlateManager.h"

#include "Actor.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "StaticMeshActor.h"
#include "FakeSpotLightActor.h"

#include "StaticMeshComponent.h"
#include "TextRenderComponent.h"
#include "CameraComponent.h"
#include "BillboardComponent.h"
#include "DecalComponent.h"
#include "PerspectiveDecalComponent.h"
#include "HeightFogComponent.h"
#include "DirectionalLightComponent.h"
#include "AmbientLightComponent.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "SceneComponent.h"
#include "Color.h"
#include "ShadowManager.h"
#include "ShadowMap.h"

using namespace std;

IMPLEMENT_CLASS(UTargetActorTransformWidget)

namespace
{
	struct FAddableComponentDescriptor
	{
		const char* Label;
		UClass* Class;
		const char* Description;
	};

	const TArray<FAddableComponentDescriptor>& GetAddableComponentDescriptors()
	{
		static TArray<FAddableComponentDescriptor> Options = []()
			{
				TArray<FAddableComponentDescriptor> Result;

				// 리플렉션 시스템을 통해 자동으로 컴포넌트 목록 가져오기
				TArray<UClass*> ComponentClasses = UClass::GetAllComponents();

				for (UClass* Class : ComponentClasses)
				{
					if (Class && Class->bIsComponent && Class->DisplayName)
					{
						Result.push_back({
							Class->DisplayName,
							Class,
							Class->Description ? Class->Description : ""
						});
					}
				}

				return Result;
			}();
		return Options;
	}

	bool TryAttachComponentToActor(AActor& Actor, UClass* ComponentClass, UActorComponent*& SelectedComponent)
	{
		if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			return false;
		USceneComponent* SelectedSceneComponent = static_cast<USceneComponent*>(SelectedComponent);

		UObject* RawObject = ObjectFactory::NewObject(ComponentClass);
		if (!RawObject)
		{
			return false;
		}

		UActorComponent* NewComp = Cast<UActorComponent>(RawObject);
		if (!NewComp)
		{
			ObjectFactory::DeleteObject(RawObject);
			return false;
		}

		NewComp->SetOwner(&Actor);

		// 씬 컴포넌트라면 SelectedComponent에 붙임
		if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
		{
			if (SelectedSceneComponent)
			{
				SceneComp->SetupAttachment(SelectedSceneComponent, EAttachmentRule::KeepRelative);
			}
			// SelectedComponent가 없으면 루트에 붙이
			else if (USceneComponent* Root = Actor.GetRootComponent())
			{
				SceneComp->SetupAttachment(Root, EAttachmentRule::KeepRelative);
			}

			Actor.RegisterComponentTree(SceneComp, GWorld);
			SelectedComponent = SceneComp;
			GWorld->GetSelectionManager()->SelectComponent(SelectedComponent);
		}

		// AddOwnedComponent 경유 (Register/Initialize 포함)
		Actor.AddOwnedComponent(NewComp);
		
		// UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
		if (UWorld* ActorWorld = Actor.GetWorld())
		{
			if (UWorldPartitionManager* Partition = ActorWorld->GetPartitionManager())
			{
				Partition->Register(Cast<UStaticMeshComponent>(NewComp));
			}
		}
		return true;
	}

	void MarkComponentSubtreeVisited(USceneComponent* Component, TSet<USceneComponent*>& Visited)
	{
		if (!Component || Visited.count(Component))
		{
			return;
		}

		Visited.insert(Component);
		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			MarkComponentSubtreeVisited(Child, Visited);
		}
	}

	void RenderActorComponent(
		AActor* SelectedActor,
		UActorComponent* SelectedComponent,
		UActorComponent* ComponentPendingRemoval
	)
	{
		for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
		{
			if (Cast<USceneComponent>(Component))
			{
				continue;
			}

			ImGuiTreeNodeFlags NodeFlags =
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_Leaf | 
				ImGuiTreeNodeFlags_NoTreePushOnOpen;

			// 선택 하이라이트: 현재 선택된 컴포넌트와 같으면 Selected 플래그
			if (Component == SelectedComponent && !GWorld->GetSelectionManager()->IsActorMode())
			{
				NodeFlags |= ImGuiTreeNodeFlags_Selected;
			}

			FString Label = Component->GetClass() ? Component->GetName() : "Unknown Component";

			ImGui::PushID(Component);
			const bool bNodeOpen = ImGui::TreeNodeEx(Component, NodeFlags, "%s", Label.c_str());
			// 좌클릭 시 컴포넌트 선택으로 전환(액터 Row 선택 해제)
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				GWorld->GetSelectionManager()->SelectComponent(Component);
			}

			if (ImGui::BeginPopupContextItem("ComponentContext"))
			{
				const bool bCanRemove = !Component->IsNative();
				if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
				{
					ComponentPendingRemoval = Component;
				}
				ImGui::EndPopup();
			}
			
			ImGui::PopID();
		}
	}
	void RenderSceneComponentTree(
		USceneComponent* Component,
		AActor& Actor,
		UActorComponent*& SelectedComponent,
		UActorComponent*& ComponentPendingRemoval,
		TSet<USceneComponent*>& Visited)
	{
		if (!Component)
			return;

		// 에디터 전용 컴포넌트는 계층구조에 표시하지 않음
		// (CREATE_EDITOR_COMPONENT로 생성된 DirectionGizmo, SpriteComponent 등)
		if (!Component->IsEditable())
		{
			return;
		}

		Visited.insert(Component);
		const TArray<USceneComponent*>& Children = Component->GetAttachChildren();
		const bool bHasChildren = !Children.IsEmpty();

		ImGuiTreeNodeFlags NodeFlags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen;

		if (!bHasChildren)
		{
			NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		// 선택 하이라이트: 현재 선택된 컴포넌트와 같으면 Selected 플래그
		if (Component == SelectedComponent && !GWorld->GetSelectionManager()->IsActorMode())
		{
			NodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		FString Label = Component->GetClass() ? Component->GetName() : "Unknown Component";
		if (Component == Actor.GetRootComponent())
		{
			Label += " (Root)";
		}

		// 트리 노드 그리기 직전에 ID 푸시
		ImGui::PushID(Component);
		const bool bNodeOpen = ImGui::TreeNodeEx(Component, NodeFlags, "%s", Label.c_str());
		// 좌클릭 시 컴포넌트 선택으로 전환(액터 Row 선택 해제)
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			GWorld->GetSelectionManager()->SelectComponent(Component);
		}

		if (ImGui::BeginPopupContextItem("ComponentContext"))
		{
			const bool bCanRemove = !Component->IsNative();
			if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
			{
				ComponentPendingRemoval = Component;
			}
			ImGui::EndPopup();
		}

		if (bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				RenderSceneComponentTree(Child, Actor, SelectedComponent, ComponentPendingRemoval, Visited);
			}
			ImGui::TreePop();
		}
		else if (!bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				MarkComponentSubtreeVisited(Child, Visited);
			}
		}
		// 항목 처리가 끝나면 반드시 PopID
		ImGui::PopID();
	}
}

// 파일명 스템(Cube 등) 추출 + .obj 확장자 제거
static inline FString GetBaseNameNoExt(const FString& Path)
{
	const size_t sep = Path.find_last_of("/\\");
	const size_t start = (sep == FString::npos) ? 0 : sep + 1;

	const FString ext = ".obj";
	size_t end = Path.size();

	if (end >= ext.size())
	{
		FString PathExt = Path.substr(end - ext.size());

		std::transform(PathExt.begin(), PathExt.end(), PathExt.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (PathExt == ext)
		{
			end -= ext.size();
		}
	}

	if (start <= end)
	{
		return Path.substr(start, end - start);
	}

	return Path.substr(start);
}

UTargetActorTransformWidget::UTargetActorTransformWidget()
	: UWidget("Target Actor Transform Widget")
	, UIManager(&UUIManager::GetInstance())
{

}

UTargetActorTransformWidget::~UTargetActorTransformWidget() = default;

void UTargetActorTransformWidget::OnSelectedActorCleared()
{
	ResetChangeFlags();
}

void UTargetActorTransformWidget::Initialize()
{
	// UIManager 참조 확보
	UIManager = &UUIManager::GetInstance();

	// Transform 위젯을 UIManager에 등록하여 선택 해제 브로드캐스트를 받을 수 있게 함
	if (UIManager)
	{
		UIManager->RegisterTargetTransformWidget(this);
	}
}


void UTargetActorTransformWidget::Update()
{
	USceneComponent* SelectedComponent = GWorld->GetSelectionManager()->GetSelectedComponent();
	if (SelectedComponent)
	{
		if (!bRotationEditing)
		{
			UpdateTransformFromComponent(SelectedComponent);
		}
	}
}

void UTargetActorTransformWidget::RenderWidget()
{
	AActor* SelectedActor = GWorld->GetSelectionManager()->GetSelectedActor();
	UActorComponent* SelectedComponent = GWorld->GetSelectionManager()->GetSelectedActorComponent();
	if (!SelectedActor)
	{
		return;
	}

	// 1. 헤더 (액터 이름, "+추가" 버튼) 렌더링
	RenderHeader(SelectedActor, SelectedComponent);

	// 2. 컴포넌트 계층 구조 렌더링
	RenderComponentHierarchy(SelectedActor, SelectedComponent);
	// 위 함수에서 SelectedComponent를 Delete하는데 아래 함수에서 SelectedComponent를 그대로 인자로 사용하고 있었음.
	// 댕글링 포인터 참조를 막기 위해 다시 한번 SelectionManager에서 Component를 얻어옴
	// 기존에는 DestroyComponent에서 DeleteObject를 호출하지도 않았음. Delete를 실제로 진행하면서 발견된 버그.
	
	// 3. 선택된 컴포넌트, 엑터의 상세 정보 렌더링 (Transform 포함)
	if (GWorld->GetSelectionManager()->IsActorMode())
	{
		RenderSelectedActorDetails(GWorld->GetSelectionManager()->GetSelectedActor());
	}
	else
	{
		RenderSelectedComponentDetails(GWorld->GetSelectionManager()->GetSelectedActorComponent());
	}
}

void UTargetActorTransformWidget::RenderHeader(AActor* SelectedActor, UActorComponent* SelectedComponent)
{
	ImGui::Text(SelectedActor->GetName().ToString().c_str());
	ImGui::SameLine();

	const float ButtonWidth = 60.0f;
	const float ButtonHeight = 25.0f;
	const float Spacing = 5.0f;
	float Avail = ImGui::GetContentRegionAvail().x;

	// Edit Blueprint 버튼과 + 추가 버튼을 나란히 배치
	float TotalButtonWidth = ButtonWidth * 2 + Spacing;
	float NewX = ImGui::GetCursorPosX() + std::max(0.0f, Avail - TotalButtonWidth);
	ImGui::SetCursorPosX(NewX);

	// Edit Blueprint 버튼
	if (ImGui::Button("Edit", ImVec2(ButtonWidth, ButtonHeight)))
	{
		USlateManager::GetInstance().OpenActorBlueprintEditor(SelectedActor);
	}

	ImGui::SameLine(0, Spacing);

	if (ImGui::Button("+ 추가", ImVec2(ButtonWidth, ButtonHeight)))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::TextUnformatted("Add Component");
		ImGui::Separator();

		for (const FAddableComponentDescriptor& Descriptor : GetAddableComponentDescriptors())
		{
			ImGui::PushID(Descriptor.Label);
			if (ImGui::Selectable(Descriptor.Label))
			{
				if (TryAttachComponentToActor(*SelectedActor, Descriptor.Class, SelectedComponent))
				{
					ImGui::CloseCurrentPopup();
				}
			}
			if (Descriptor.Description && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Descriptor.Description);
			}
			ImGui::PopID();
		}
		ImGui::EndPopup();
	}
	ImGui::Spacing();
}

void UTargetActorTransformWidget::RenderComponentHierarchy(AActor* SelectedActor, UActorComponent* SelectedComponent)
{
	AActor* ActorPendingRemoval = nullptr;
	UActorComponent* ComponentPendingRemoval = nullptr;

	// 컴포넌트 트리 박스 크기 관련
	static float PaneHeight = 120.0f;
	const float SplitterThickness = 4.0f;
	const float MinTop = 1.0f;
	const float MinBottom = 0.0f;
	const float WindowAvailY = ImGui::GetContentRegionAvail().y;
	PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, WindowAvailY - (MinBottom + SplitterThickness)));

	ImGui::BeginChild("ComponentBox", ImVec2(0, PaneHeight), true);

	// 액터 행 렌더링
	ImGui::PushID("ActorDisplay");
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));

	const bool bActorSelected = GWorld->GetSelectionManager()->IsActorMode();
	if (ImGui::Selectable(SelectedActor->GetName().ToString().c_str(), bActorSelected, ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_SpanAvailWidth))
	{
		GWorld->GetSelectionManager()->SelectActor(SelectedActor);
	}
	ImGui::PopStyleColor();
	if (ImGui::BeginPopupContextItem("ActorContextMenu"))
	{
		if (ImGui::MenuItem("삭제", "Delete", false, true))
		{
			ActorPendingRemoval = SelectedActor;
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();

	// 컴포넌트 트리 렌더링
	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	if (!RootComponent)
	{
		ImGui::BulletText("Root component not found.");
	}
	else
	{
		TSet<USceneComponent*> VisitedComponents;
		RenderSceneComponentTree(RootComponent, *SelectedActor, SelectedComponent, ComponentPendingRemoval, VisitedComponents);
		// ... (루트에 붙지 않은 씬 컴포넌트 렌더링 로직은 생략 가능성 있음, 엔진 설계에 따라)
		ImGui::Separator();
		RenderActorComponent(SelectedActor, SelectedComponent, ComponentPendingRemoval);
	}

	// 삭제 입력 처리
	const bool bDeletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete);
	if (bDeletePressed)
	{
		if (bActorSelected) ActorPendingRemoval = SelectedActor;
		else if (SelectedComponent && !SelectedComponent->IsNative()) ComponentPendingRemoval = SelectedComponent;
	}

	// 컴포넌트 삭제 실행
	if (ComponentPendingRemoval)
	{

		USceneComponent* NewSelection = SelectedActor->GetRootComponent();
		// SelectionManager를 통해 선택 해제
		GWorld->GetSelectionManager()->ClearSelection();

		// 컴포넌트 삭제
		SelectedActor->RemoveOwnedComponent(ComponentPendingRemoval);
		ComponentPendingRemoval = nullptr;

		// 삭제 후 새로운 컴포넌트 선택
		if (NewSelection)
		{
			GWorld->GetSelectionManager()->SelectComponent(NewSelection);
		}
	}

	// 액터 삭제 실행
	if (ActorPendingRemoval)
	{
		// 삭제 전에 선택 해제 (dangling pointer 방지)
		GWorld->GetSelectionManager()->ClearSelection();

		// 액터 삭제
		if (UWorld* World = ActorPendingRemoval->GetWorld()) World->DestroyActor(ActorPendingRemoval);
		else ActorPendingRemoval->Destroy();

		OnSelectedActorCleared();
	}

	ImGui::EndChild();

	// 스플리터 렌더링
	ImGui::InvisibleButton("VerticalSplitter", ImVec2(-FLT_MIN, SplitterThickness));
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	if (ImGui::IsItemActive()) PaneHeight += ImGui::GetIO().MouseDelta.y;
	ImVec2 Min = ImGui::GetItemRectMin(), Max = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddLine(ImVec2(Min.x, (Min.y + Max.y) * 0.5f), ImVec2(Max.x, (Min.y + Max.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
	ImGui::Spacing();
}

void UTargetActorTransformWidget::RenderSelectedActorDetails(AActor* SelectedActor)
{
	if (!SelectedActor)
	{
		return;
	}
	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	const TArray<FProperty>& Properties = USceneComponent::StaticClass()->GetProperties();
	

	UPropertyRenderer::RenderProperties(Properties, RootComponent);

	bool bActorHiddenInGame = SelectedActor->GetActorHiddenInGame();
	if (ImGui::Checkbox("bActorHiddendInGame", &bActorHiddenInGame))
	{
		SelectedActor->SetActorHiddenInGame(bActorHiddenInGame);
	}

	// --------------------------------------------------------------------
	// Lua Script 설정 영역
	// --------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Lua Scripts");

	static char NewScriptName[128] = "";
	static FString LastErrorMessage = "";

	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	const TArray<FScript*>& Scripts = ScriptMgr.GetScriptsOfActor(SelectedActor);

	// 현재 부착된 스크립트 목록 표시
	for (int i = 0; i < Scripts.size(); ++i)
	{
		FScript* Script = Scripts[i];
		if (!Script)
			continue;

		ImGui::PushID(i);
		ImGui::Text("%s", Script->ScriptName.c_str());
		ImGui::SameLine();

		// Edit 버튼 추가
		if (ImGui::Button("Edit"))
		{
			ScriptMgr.OpenScriptInEditor(Script->ScriptName);
		}

		ImGui::SameLine();
		if (ImGui::Button("Remove"))
		{
			ScriptMgr.DetachScriptFrom(SelectedActor, Script->ScriptName);
			ImGui::PopID();
			break;
		}

		ImGui::SameLine();
		if (ImGui::Button("Reload"))
		{
			ScriptMgr.CheckAndHotReloadLuaScript();
		}

		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::InputText("Script File", NewScriptName, IM_ARRAYSIZE(NewScriptName));

	// Create Script 버튼
	if (ImGui::Button("Create Script"))
	{
		if (strlen(NewScriptName) > 0)
		{
			FString ScriptName = FString(NewScriptName);

			if (ScriptMgr.CreateScriptFile(ScriptName))
			{
				LastErrorMessage = FString("Script created: ") + ScriptName;
				memset(NewScriptName, 0, sizeof(NewScriptName));
			}
			else
			{
				LastErrorMessage = FString("Failed to create script: ") + ScriptName +
				                   ". File may already exist.";
			}
		}
		else
		{
			LastErrorMessage = FString("Please enter a script name.");
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Add Script"))
	{
		if (strlen(NewScriptName) > 0)
		{
			FString ScriptName = FString(NewScriptName);

			try
			{
				// LuaLocalValue 설정
				FLuaLocalValue LocalVal;
				LocalVal.MyActor = SelectedActor;

				// 스크립트 추가 시도
				ScriptMgr.AttachScriptTo(LocalVal, ScriptName);
				ScriptMgr.PrintDebugLog();

				LastErrorMessage = FString("Script attached: ") + ScriptName;
				memset(NewScriptName, 0, sizeof(NewScriptName));
			}
			catch (const std::exception& e)
			{
				LastErrorMessage = FString("Error: ") + e.what();
			}
		}
		else
		{
			LastErrorMessage = FString("Please enter a script name.");
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Remove All"))
	{
		ScriptMgr.DetachAllScriptFrom(SelectedActor);
		LastErrorMessage = FString("All scripts removed.");
	}

	// 메시지 표시
	if (!LastErrorMessage.empty())
	{
		ImGui::Separator();
		// 성공 메시지는 초록색, 에러 메시지는 빨간색
		ImVec4 Color = (LastErrorMessage.find("Error:") != std::string::npos ||
		                LastErrorMessage.find("Failed") != std::string::npos)
		               ? ImVec4(1, 0.3f, 0.3f, 1)
		               : ImVec4(0.3f, 1, 0.3f, 1);
		ImGui::TextColored(Color, "%s", LastErrorMessage.c_str());
	}
}

void UTargetActorTransformWidget::RenderSelectedComponentDetails(UActorComponent* SelectedComponent)
{

	ImGui::Spacing();
	ImGui::Separator();

	if (!SelectedComponent) return;

	// 리플렉션이 적용된 컴포넌트는 자동으로 UI 생성
	if (SelectedComponent)
	{
		ImGui::Separator();
		ImGui::Text("[Reflected Properties]");
		UPropertyRenderer::RenderAllPropertiesWithInheritance(SelectedComponent);
	}

	// SpotLightComponent인 경우 ShadowMap 표시
	if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(SelectedComponent))
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("[Shadow Map]");

		bool bCastShadows = SpotLight->GetIsCastShadows();
		ImGui::Text("Cast Shadows: %s", bCastShadows ? "True" : "False");

		if (bCastShadows)
		{
			FShadowManager* ShadowManager = GWorld->GetShadowManager();

			if (ShadowManager)
			{
				FShadowMap& SpotLightShadowMap = ShadowManager->GetSpotLightShadowMap();
				int ShadowMapIndex = SpotLight->GetShadowMapIndex();

				ImGui::Text("Shadow Map Index: %d", ShadowMapIndex);
				ImGui::Text("Resolution: %u x %u", SpotLightShadowMap.GetWidth(), SpotLightShadowMap.GetHeight());

				// Depth 범위 조절 슬라이더
				static float SpotLightDepthBegin = 0.0f;
				static float SpotLightDepthEnd = 1.0f;

				ImGui::Spacing();
				ImGui::Text("Depth Visualization Range:");
				ImGui::SetNextItemWidth(200.0f);
				ImGui::DragFloat("Begin##SpotDepth", &SpotLightDepthBegin, 0.001f, 0.0f, 1.0f, "%.4f");
				ImGui::SetNextItemWidth(200.0f);
				ImGui::DragFloat("End##SpotDepth", &SpotLightDepthEnd, 0.001f, 0.0f, 1.0f, "%.4f");

				// Begin이 End보다 크면 자동 조정
				if (SpotLightDepthBegin > SpotLightDepthEnd)
					SpotLightDepthEnd = SpotLightDepthBegin;

				ID3D11ShaderResourceView* ShadowSRV = SpotLightShadowMap.GetRemappedSliceSRV(
					GEngine.GetRenderer()->GetRHIDevice(),
					ShadowMapIndex,
					SpotLightDepthBegin,
					SpotLightDepthEnd);
				
				if (ShadowSRV)
				{
					ImGui::Spacing();

					// ShadowMap 크기 조절 (입력 가능한 슬라이더)
					static float ShadowMapDisplaySize = 256.0f;
					ImGui::SetNextItemWidth(200.0f);
					ImGui::DragFloat("Display Size", &ShadowMapDisplaySize, 1.0f, 64.0f, 512.0f, "%.0f");
					ImGui::Spacing();

					// ShadowMap 이미지 표시
					ImGui::Image(
						(ImTextureID)ShadowSRV,
						ImVec2(ShadowMapDisplaySize, ShadowMapDisplaySize),
						ImVec2(0, 0),  // uv0
						ImVec2(1, 1),  // uv1
						ImVec4(1, 1, 1, 1),  // tint color
						ImVec4(0.5f, 0.5f, 0.5f, 1.0f)  // border color
					);
					ImGui::TextWrapped("Depth remapped to [%.4f, %.4f] range for better visibility.", SpotLightDepthBegin, SpotLightDepthEnd);

				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "SRV is NULL!");
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ShadowManager is NULL!");
			}
		}
	}
	// DirectionalLightComponent인 경우 ShadowMap 표시
	else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(SelectedComponent))
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("[Shadow Map]");

		bool bCastShadows = DirectionalLight->GetIsCastShadows();
		ImGui::Text("Cast Shadows: %s", bCastShadows ? "True" : "False");

		if (bCastShadows)
		{
			FShadowManager* ShadowManager = GWorld->GetShadowManager();

			if (ShadowManager)
			{
				int ShadowMapIndex = DirectionalLight->GetShadowMapIndex();
				EShadowMapType ShadowMapType = DirectionalLight->GetShadowMapType();

				ImGui::Text("Shadow Map Index: %d", ShadowMapIndex);
				ImGui::Text("Shadow Map Type: %s", ShadowMapType == EShadowMapType::CSM ? "CSM" : "Default");

				// Depth 범위 조절 슬라이더
				static float DirectionalLightDepthBegin = 0.0f;
				static float DirectionalLightDepthEnd = 1.0f;

				ImGui::Spacing();
				ImGui::Text("Depth Visualization Range:");
				ImGui::SetNextItemWidth(200.0f);
				ImGui::DragFloat("Begin##DirectionalDepth", &DirectionalLightDepthBegin, 0.001f, 0.0f, 1.0f, "%.4f");
				ImGui::SetNextItemWidth(200.0f);
				ImGui::DragFloat("End##DirectionalDepth", &DirectionalLightDepthEnd, 0.001f, 0.0f, 1.0f, "%.4f");

				// Begin이 End보다 크면 자동 조정
				DirectionalLightDepthEnd = std::max(DirectionalLightDepthBegin, DirectionalLightDepthEnd);

				// Projection type 확인 (회전/반전 로직)
				EShadowProjectionType ProjectionType = DirectionalLight->GetShadowProjectionType();
				bool bRotate = (ProjectionType == EShadowProjectionType::LVP);

				// LiSPSM hybrid인 경우, 실제 사용된 알고리즘 확인
				// TSM: U 반전 없음, OpenGL LiSPSM: U 반전 필요
				bool bFlipU = false;
				if (ProjectionType == EShadowProjectionType::LiSPSM)
				{
					bFlipU = !DirectionalLight->IsActuallyUsingTSM();  // TSM이 아니면 OpenGL LiSPSM이므로 U 반전
				}

				ImGui::Spacing();
				ImGui::Text("Shadow Projection: %s", ProjectionType == EShadowProjectionType::LVP ? "LVP" : "LiSPSM");
				if (ProjectionType == EShadowProjectionType::LiSPSM)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "(Using: %s)", bFlipU ? "LiSPSM" : "LiSPSM");
				}
				// CSM 모드인 경우 Cascade별로 표시
				if (ShadowMapType == EShadowMapType::CSM)
				{
					int32 NumCascades = DirectionalLight->GetNumCascades();
					ImGui::Text("Cascades: %d", NumCascades);

					// ShadowMap 크기 조절
					static float CSMDisplaySize = 200.0f;
					ImGui::SetNextItemWidth(200.0f);
					ImGui::DragFloat("Display Size##CSM", &CSMDisplaySize, 1.0f, 64.0f, 512.0f, "%.0f");
					ImGui::Spacing();

					// Cascade별로 표시 (2열 그리드)
					for (int CascadeIndex = 0; CascadeIndex < NumCascades; ++CascadeIndex)
					{
						// 2열 그리드 레이아웃
						if (CascadeIndex > 0 && CascadeIndex % 2 != 0)
						{
							ImGui::SameLine();
						}

						ImGui::BeginGroup();

						// Cascade Tier 정보 가져오기
						const UDirectionalLightComponent::FCascadeTierInfoCache& TierInfo = DirectionalLight->GetCascadeTierInfo(CascadeIndex);
						float SplitDistance = DirectionalLight->GetCascadeSplitDistance(CascadeIndex);

						// Cascade 정보 표시
						ImGui::Text("Cascade %d", CascadeIndex);
						ImGui::Text("Tier %d, Slice %d", TierInfo.TierIndex, TierInfo.SliceIndex);
						ImGui::Text("Res: %.0fx%.0f", TierInfo.Resolution, TierInfo.Resolution);
						ImGui::Text("Split: %.1fm", SplitDistance);

						// 해당 Tier의 Shadow Map 가져오기
						FShadowMap& TierShadowMap = ShadowManager->GetDirectionalLightShadowMapTier(TierInfo.TierIndex);

						// Remapped SRV 가져오기 (SliceIndex 사용, Projection Type 적용)
						ID3D11ShaderResourceView* CascadeSRV = TierShadowMap.GetRemappedSliceSRV(
							GEngine.GetRenderer()->GetRHIDevice(),
							TierInfo.SliceIndex,
							DirectionalLightDepthBegin,
							DirectionalLightDepthEnd,
							bRotate,
							bFlipU);

						if (CascadeSRV)
						{
							ImGui::Image(
								(ImTextureID)CascadeSRV,
								ImVec2(CSMDisplaySize, CSMDisplaySize),
								ImVec2(0, 0),  // uv0
								ImVec2(1, 1),  // uv1
								ImVec4(1, 1, 1, 1),  // tint color
								ImVec4(0.5f, 0.5f, 0.5f, 1.0f)  // border color
							);
						}
						else
						{
							ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "SRV NULL!");
						}

						ImGui::EndGroup();
					}

					ImGui::Spacing();
					ImGui::TextWrapped("Depth remapped to [%.4f, %.4f] range for better visibility.", DirectionalLightDepthBegin, DirectionalLightDepthEnd);
				}
				// Default 모드인 경우 단일 Shadow Map 표시
				else
				{
					FShadowMap& DirectionalLightShadowMap = ShadowManager->GetDirectionalLightShadowMap();

					ImGui::Text("Resolution: %u x %u", DirectionalLightShadowMap.GetWidth(), DirectionalLightShadowMap.GetHeight());

					ID3D11ShaderResourceView* ShadowSRV = DirectionalLightShadowMap.GetRemappedSliceSRV(
						GEngine.GetRenderer()->GetRHIDevice(),
						ShadowMapIndex,
						DirectionalLightDepthBegin,
						DirectionalLightDepthEnd,
					bRotate,
					bFlipU);

					if (ShadowSRV)
					{
						ImGui::Spacing();

						// ShadowMap 크기 조절
						static float DirectionalShadowMapDisplaySize = 256.0f;
						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat("Display Size##Directional", &DirectionalShadowMapDisplaySize, 1.0f, 64.0f, 512.0f, "%.0f");
						ImGui::Spacing();

						// ShadowMap 이미지 표시
						ImGui::Image(
							(ImTextureID)ShadowSRV,
							ImVec2(DirectionalShadowMapDisplaySize, DirectionalShadowMapDisplaySize),
							ImVec2(0, 0),  // uv0
							ImVec2(1, 1),  // uv1
							ImVec4(1, 1, 1, 1),  // tint color
							ImVec4(0.5f, 0.5f, 0.5f, 1.0f)  // border color
						);

						ImGui::TextWrapped("Depth remapped to [%.4f, %.4f] range for better visibility.", DirectionalLightDepthBegin, DirectionalLightDepthEnd);
					}
					else
					{
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "SRV is NULL!");
					}
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ShadowManager is NULL!");
			}
		}
	}
	// PointLightComponent인 경우 CubeShadowMap 표시 (6개 면) - SpotLight는 제외
	else if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(SelectedComponent))
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("[Cube Shadow Map]");

		bool bCastShadows = PointLight->GetIsCastShadows();
		ImGui::Text("Cast Shadows: %s", bCastShadows ? "True" : "False");

		if (bCastShadows)
		{
			FShadowManager* ShadowManager = GWorld->GetShadowManager();

			if (ShadowManager)
			{
				FShadowMap& PointLightCubeShadowMap = ShadowManager->GetPointLightCubeShadowMap();
				int ShadowMapIndex = PointLight->GetShadowMapIndex();

				ImGui::Text("Shadow Map Index: %d", ShadowMapIndex);
				ImGui::Text("Resolution: %u x %u", PointLightCubeShadowMap.GetWidth(), PointLightCubeShadowMap.GetHeight());
				ImGui::Text("Cube Map Array Size: %u", PointLightCubeShadowMap.GetArraySize());

				if (ShadowMapIndex >= 0 && ShadowMapIndex < (int)PointLightCubeShadowMap.GetArraySize() / 6)
				{
					// Depth 범위 조절 슬라이더
					static float PointLightDepthBegin = 0.0f;
					static float PointLightDepthEnd = 1.0f;

					ImGui::Spacing();
					ImGui::Text("Depth Visualization Range:");
					ImGui::SetNextItemWidth(200.0f);
					ImGui::DragFloat("Begin##PointDepth", &PointLightDepthBegin, 0.001f, 0.0f, 1.0f, "%.4f");
					ImGui::SetNextItemWidth(200.0f);
					ImGui::DragFloat("End##PointDepth", &PointLightDepthEnd, 0.001f, 0.0f, 1.0f, "%.4f");

					// Begin이 End보다 크면 자동 조정
					PointLightDepthEnd = std::max(PointLightDepthBegin, PointLightDepthEnd);
					
					// CubeShadowMap 크기 조절
					static float CubeShadowMapDisplaySize = 128.0f;
					ImGui::SetNextItemWidth(200.0f);
					ImGui::DragFloat("Display Size##Cube", &CubeShadowMapDisplaySize, 1.0f, 64.0f, 256.0f, "%.0f");
					ImGui::Spacing();

					// 6개 면 이름 (Unreal 축: X=Forward, Y=Right, Z=Up)
					const char* FaceNames[6] = {
						"+X (Forward)",
						"-X (Back)",
						"+Y (Right)",
						"-Y (Left)",
						"+Z (Up)",
						"-Z (Down)"
					};

					// 2x3 그리드로 6개 면 표시
					for (int row = 0; row < 2; row++)
					{
						for (int col = 0; col < 3; col++)
						{
							int faceIndex = row * 3 + col;
							uint32 ArrayIndex = (ShadowMapIndex * 6) + faceIndex;

							ID3D11ShaderResourceView* FaceSRV = PointLightCubeShadowMap.GetRemappedSliceSRV(
								GEngine.GetRenderer()->GetRHIDevice(),
								ArrayIndex,
								PointLightDepthBegin,
								PointLightDepthEnd);

							if (col > 0) ImGui::SameLine();

							ImGui::BeginGroup();
							ImGui::Text("%s", FaceNames[faceIndex]);

							if (FaceSRV)
							{
								ImGui::Image(
									(ImTextureID)FaceSRV,
									ImVec2(CubeShadowMapDisplaySize, CubeShadowMapDisplaySize),
									ImVec2(0, 0),  // uv0
									ImVec2(1, 1),  // uv1
									ImVec4(1, 1, 1, 1),  // tint color
									ImVec4(0.5f, 0.5f, 0.5f, 1.0f)  // border color
								);
							}
							else
							{
								ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NULL");
							}

							ImGui::EndGroup();
						}
					}
					ImGui::TextWrapped("Depth remapped to [%.4f, %.4f] range for better visibility.", PointLightDepthBegin, PointLightDepthEnd);
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid ShadowMap Index!");
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ShadowManager is NULL!");
			}
		}
	}
}

void UTargetActorTransformWidget::UpdateTransformFromComponent(USceneComponent* SelectedComponent)
{
	if (SelectedComponent)
	{
		EditLocation = SelectedComponent->GetRelativeLocation();
		EditRotation = SelectedComponent->GetRelativeRotation().ToEulerZYXDeg();
		EditScale = SelectedComponent->GetRelativeScale();
	}

	ResetChangeFlags();
}

void UTargetActorTransformWidget::ResetChangeFlags()
{
	bPositionChanged = false;
	bRotationChanged = false;
	bScaleChanged = false;
}
