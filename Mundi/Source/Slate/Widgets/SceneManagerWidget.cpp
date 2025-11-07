#include "pch.h"
#include "SceneManagerWidget.h"
#include "UIManager.h"
#include "ImGui/imgui.h"
#include "World.h"
#include "RenderSettings.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "Actor.h"
#include "StaticMeshActor.h"
#include "SelectionManager.h"
#include "Gizmo/GizmoActor.h"
#include "ResourceManager.h"
#include "Texture.h"
#include <algorithm>
#include <string>
#include <EditorEngine.h>
#include "USlateManager.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(USceneManagerWidget)

USceneManagerWidget::USceneManagerWidget()
	: UWidget("Scene Manager")
	, UIManager(&UUIManager::GetInstance())
	, SelectionManager(nullptr)
{
	LoadIcons();
}

USceneManagerWidget::~USceneManagerWidget()
{
	ClearActorTree();
}

void USceneManagerWidget::Initialize()
{
}

void USceneManagerWidget::LoadIcons()
{
	IconVisible = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Eye_Visible.png");
	IconHidden = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Eye_Hidden.png");
}

void USceneManagerWidget::Update()
{
	// 지연된 새로고침 처리 (렌더링 중 iterator invalidation 방지)
	if (bNeedRefreshNextFrame)
	{
		RefreshActorTree();
		bNeedRefreshNextFrame = false;
		return; // 이번 프레임은 새로고침만 하고 끝
	}

	// 액터 수 변화 감지만 수행 (최소한의 검사)
	static size_t LastActorCount = 0;

	UWorld* World = GWorld;
	static ULevel* LastLevel = nullptr; // 레벨이 새로 로드됐는지 감지하기 위함.

	if (World)
	{
		size_t CurrentActorCount = World->GetActors().size();
		ULevel* CurrentLevel = World->GetLevel();
		//if (CurrentActorCount != LastActorCount || CurrentLevel != LastLevel)
		//{
			RefreshActorTree();
			LastActorCount = CurrentActorCount;
			LastLevel = CurrentLevel;
			return; // 새로고침 후 이번 프레임은 더 이상 처리하지 않음
		//}

		//// 비용이 많이 드는 유효성 검사를 5초마다만 수행
		//static int32 ValidationCounter = 0;
		//ValidationCounter++;
		//if (ValidationCounter % 300 == 0) // 5초마다 (60FPS 기준)
		//{
		//	// 느린 유효성 검사 - 몇 개만 샘플링
		//	bool bNeedRefresh = false;
		//	const TArray<AActor*>& WorldActors = World->GetActors();

		//	// 전체 검사 대신 몇 개만 샘플링
		//	int32 CheckCount = 0;
		//	for (auto* RootNode : RootNodes)
		//	{
		//		if (++CheckCount > 10) break; // 최대 10개만 검사

		//		if (RootNode && RootNode->IsActor())
		//		{
		//			if (!RootNode->Actor ||
		//				std::find(WorldActors.begin(), WorldActors.end(), RootNode->Actor) == WorldActors.end())
		//			{
		//				bNeedRefresh = true;
		//				break;
		//			}
		//		}
		//	}

		//	if (bNeedRefresh)
		//	{
		//		RequestDelayedRefresh();
		//		return;
		//	}
		//}
	}
	else if (LastActorCount != 0)
	{
		RefreshActorTree();
		LastActorCount = 0;
	}

	// Sync selection from viewport
	SyncSelectionFromViewport();

	// 정기적으로 SelectionManager 정리 (매 프레임마다 하지 않고 가끔씩)
	static int32 CleanupCounter = 0;
	CleanupCounter++;
	if (CleanupCounter % 60 == 0) // 약 1초마다
	{
		if (UWorld* W = GWorld)
		{
			W->GetSelectionManager()->CleanupInvalidActors();
		}
	}
}

void USceneManagerWidget::RenderWidget()
{
	ImGui::Text("Scene Manager");
	// World status
	UWorld* World = GWorld;
	if (!World)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No World Available");
		return;
	}

	// Actor tree view
	// 사용 가능한 영역에서 하단 상태 표시줄을 위한 공간을 제외하고 모두 사용
	float availableHeight = ImGui::GetContentRegionAvail().y - 30.0f; // 하단 액터 카운트 공간 확보
	ImGui::BeginChild("ActorTreeView", ImVec2(0, availableHeight), true);

	// 유효성 검사는 Update()에서 처리했으므로 여기서는 바로 렌더링
	if (bNeedRefreshNextFrame)
	{
		ImGui::Text("로딩 중...");
	}
	else
	{
		// 빠른 렌더링 - 중복 검사 없이 바로 출력
		for (int32 i = 0; i < RootNodes.size(); ++i)
		{
			FActorTreeNode* RootNode = RootNodes[i];
			if (RootNode)
			{
				// Categories are always shown, individual actors are filtered
				if (RootNode->IsCategory() || ShouldShowActor(RootNode->Actor))
				{
					RenderActorNode(RootNode);
				}
			}
		}
	}

	ImGui::EndChild();

	ImGui::Dummy(ImVec2(0, 1.0f));
	ImGui::Text("%zu개 액터", World->GetActors().size());

	// Context menu - OpenPopup 호출 후 바로 BeginPopup 호출해야 함
	RenderContextMenu();

	// Status bar
	ImGui::SameLine();
	AActor* SelectedActor = nullptr;
	if (UWorld* W = GWorld)
	{
		SelectedActor = W->GetSelectionManager()->GetSelectedActor();
	}
	if (SelectedActor)
	{
		// 액터 이름을 가져오기 전에 안전성 확인
		try
		{
			ImGui::Text("(%s 선택됨)", SelectedActor->GetName().ToString().c_str());
		}
		catch (...)
		{
			ImGui::Text("([Invalid Actor] 선택됨)");
			// 유효하지 않은 액터를 정리
			if (SelectionManager)
			{
				SelectionManager->CleanupInvalidActors();
			}
		}
	}
	else
	{
		ImGui::Text("(선택된 액터 없음)");
	}
}

void USceneManagerWidget::RefreshActorTree()
{
	UWorld* World = GWorld;
	if (!World)
	{
		ClearActorTree();
		return;
	}

	// Clear existing tree
	ClearActorTree();

	// Build new hierarchy
	BuildActorHierarchy();
}

void USceneManagerWidget::BuildActorHierarchy()
{
	UWorld* World = GWorld;
	if (!World)
		return;

	// Build categorized hierarchy instead of flat
	BuildCategorizedHierarchy();
}

void USceneManagerWidget::RenderActorNode(FActorTreeNode* Node, int32 Depth)
{
	if (!Node)
		return;

	// Handle category nodes
	if (Node->IsCategory())
	{
		RenderCategoryNode(Node, Depth);
		return;
	}

	// Handle actor nodes
	if (!Node->Actor)
		return; // 이미 사전 검사에서 걸러졌어야 하지만 방어 코드

	AActor* Actor = Node->Actor;

	// 중요: 액터가 이미 삭제되었는지 확인 (IsPendingDestroy 체크)
	if (Actor->IsPendingDestroy())
	{
		RequestDelayedRefresh(); // 다음 프레임에 트리 새로고침 요청
		return;
	}

	// Skip if doesn't pass filters
	if (!ShouldShowActor(Actor))
		return;

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	// Check if selected
	bool bIsSelected = false;
	if (UWorld* W = GWorld)
	{
		bIsSelected = W->GetSelectionManager()->IsActorSelected(Actor);
	}
	if (bIsSelected)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	// Leaf node if no children
	if (Node->Children.empty())
	{
		NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// Create unique ID for ImGui
	ImGui::PushID(Actor);

	// Sync node visibility with actual actor state each frame
	Node->bIsVisible = Actor->IsActorVisible();

	// Visibility toggle button (only for actors)
	UTexture* CurrentIcon = Node->bIsVisible ? IconVisible : IconHidden;
	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

		if (ImGui::ImageButton("##VisibilityBtn", (void*)CurrentIcon->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
		{
			HandleActorVisibilityToggle(Actor);
		}

		ImGui::PopStyleColor(3);
	}
	else
	{
		// Fallback to text if icon not loaded
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		const char* VisibilityIcon = Node->bIsVisible ? "O" : "X";
		if (ImGui::SmallButton(VisibilityIcon))
		{
			HandleActorVisibilityToggle(Actor);
		}
		ImGui::PopStyleColor();
	}

	ImGui::SameLine(0, 1.0f);

	// Actor name and tree node
	bool bNodeOpen = ImGui::TreeNodeEx(Actor->GetName().ToString().c_str(), NodeFlags);

	// Handle selection
	if (ImGui::IsItemClicked())
	{
		HandleActorSelection(Actor);
	}
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
	{
		if (GEngine.GetDefaultWorld() && GEngine.GetDefaultWorld()->GetCameraActor())
		{
			ACameraActor* Cam = GEngine.GetDefaultWorld()->GetCameraActor();
			FVector Center = Actor->GetActorLocation();
			float kDist = 8.0f;
			FVector Dir = Cam->GetForward();
			if (Dir.SizeSquared() < KINDA_SMALL_NUMBER) Dir = FVector(1, 0, 0);
			FVector Pos = Center - Dir * kDist;
			Cam->SetActorLocation(Pos);
		}
		ImGui::ClearActiveID();
	}

	// Handle right-click context menu
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		ContextMenuTarget = Actor;
		ImGui::OpenPopup("ActorContextMenu");
	}

	// Handle drag source
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("ACTOR_DRAG", &Actor, sizeof(AActor*));
		ImGui::Text("Move %s", Actor->GetName().ToString().c_str());
		DragSource = Actor;
		ImGui::EndDragDropSource();
	}

	// Handle drop target
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ACTOR_DRAG"))
		{
			AActor* DroppedActor = *(AActor**)Payload->Data;
			if (DroppedActor != Actor)
			{
				// TODO: Implement hierarchy reparenting
				UE_LOG("Would reparent %s to %s", DroppedActor->GetName().ToString().c_str(), Actor->GetName().ToString().c_str());
			}
		}
		ImGui::EndDragDropTarget();
	}

	// Render children if node is open
	if (bNodeOpen && !Node->Children.empty())
	{
		for (FActorTreeNode* Child : Node->Children)
		{
			RenderActorNode(Child, Depth + 1);
		}
	}

	if (bNodeOpen && !Node->Children.empty())
	{
		ImGui::TreePop();
	}

	ImGui::PopID();
}

bool USceneManagerWidget::ShouldShowActor(AActor* Actor) const
{
	if (!Actor)
		return false;

	// Filter by selection
	if (bShowOnlySelectedObjects && !SelectionManager->IsActorSelected(Actor))
		return false;

	return true;
}

void USceneManagerWidget::HandleActorSelection(AActor* Actor)
{
	if (!Actor)
		return;

	if (UWorld* W = GWorld)
	{
		// Clear previous selection and select this actor
		W->GetSelectionManager()->ClearSelection();
		W->GetSelectionManager()->SelectActor(Actor);
	}


	UE_LOG("SceneManager: Selected actor %s", Actor->GetName().ToString().c_str());
}

void USceneManagerWidget::HandleActorVisibilityToggle(AActor* Actor)
{
	if (!Actor)
		return;

	// Toggle the actor's actual visibility state
	bool bNewVisible = Actor->GetActorHiddenInEditor(); // If hidden, make visible
	Actor->SetActorHiddenInEditor(!bNewVisible);

	// Update the node to match the actor's state
	FActorTreeNode* Node = FindNodeByActor(Actor);
	if (Node)
	{
		Node->bIsVisible = Actor->IsActorVisible();
		UE_LOG("SceneManager: Toggled visibility for %s: %s",
			Actor->GetName().ToString().c_str(), Node->bIsVisible ? "Visible" : "Hidden");
	}
}

void USceneManagerWidget::HandleActorRename(AActor* Actor)
{
	// TODO: Implement inline renaming
	UE_LOG("SceneManager: Rename not implemented yet");
}

void USceneManagerWidget::HandleActorDelete(AActor* Actor)
{
	if (!Actor)
		return;

	UWorld* World = GWorld;
	if (World)
	{
		World->DestroyActor(Actor);
		UE_LOG("SceneManager: Deleted actor %s", Actor->GetName().ToString().c_str());
	}
}

void USceneManagerWidget::HandleActorDuplicate(AActor* Actor)
{
	// TODO: Implement actor duplication
	UE_LOG("SceneManager: Duplicate not implemented yet");
}

void USceneManagerWidget::RenderContextMenu()
{
	// OpenPopup이 호출된 후 매 프레임 BeginPopup을 시도함
	if (ImGui::BeginPopup("ActorContextMenu"))
	{
		if (ContextMenuTarget)
		{
			ImGui::Text("%s개 액터", ContextMenuTarget->GetName().ToString().c_str());
			const char* ClassName = "Unknown";
			if (ContextMenuTarget->GetClass())
			{
				ClassName = ContextMenuTarget->GetClass()->Name;
			}
			ImGui::Text("Type: %s", ClassName);
			ImGui::Separator();

			// Selection actions
			if (ImGui::MenuItem("Focus in Viewport"))
			{
				HandleActorSelection(ContextMenuTarget);
			}

			ImGui::Separator();

			// Transform actions
			if (ImGui::MenuItem("Reset Transform"))
			{
				if (ContextMenuTarget)
				{
					ContextMenuTarget->SetActorLocation(FVector(0, 0, 0));
					ContextMenuTarget->SetActorRotation(FQuat::Identity());
					ContextMenuTarget->SetActorScale(FVector(1, 1, 1));
				}
			}

			if (ImGui::MenuItem("Reset Position"))
			{
				if (ContextMenuTarget)
				{
					ContextMenuTarget->SetActorLocation(FVector(0, 0, 0));
				}
			}

			ImGui::Separator();

			// Edit actions
			if (ImGui::MenuItem("Edit Blueprint"))
			{
				// 블루프린트 에디터 열기
				USlateManager::GetInstance().OpenActorBlueprintEditor(ContextMenuTarget);
			}

			if (ImGui::MenuItem("Rename"))
			{
				HandleActorRename(ContextMenuTarget);
			}

			if (ImGui::MenuItem("Duplicate"))
			{
				HandleActorDuplicate(ContextMenuTarget);
			}

			ImGui::Separator();

			// Danger zone
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			if (ImGui::MenuItem("Delete"))
			{
				HandleActorDelete(ContextMenuTarget);
			}
			ImGui::PopStyleColor();
		}

		ImGui::EndPopup();
	}
	// BeginPopup이 false를 반환하면 팝업이 닫힌 것
}

void USceneManagerWidget::RenderToolbar()
{
}

void USceneManagerWidget::ClearActorTree()
{
	for (auto* Node : RootNodes)
	{
		delete Node;
	}
	RootNodes.clear();
}

USceneManagerWidget::FActorTreeNode* USceneManagerWidget::FindNodeByActor(AActor* Actor)
{
	if (!Actor)
		return nullptr;

	// Search through categories and their children
	for (FActorTreeNode* RootNode : RootNodes)
	{
		if (!RootNode)
			continue;

		// Check if this root node is the actor (for backward compatibility)
		if (RootNode->IsActor() && RootNode->Actor == Actor)
			return RootNode;

		// Search within category children
		if (RootNode->IsCategory())
		{
			for (FActorTreeNode* Child : RootNode->Children)
			{
				if (Child && Child->IsActor() && Child->Actor == Actor)
					return Child;
			}
		}
	}

	return nullptr;
}

void USceneManagerWidget::SyncSelectionFromViewport()
{
	// SelectionManager에서 null 액터들을 정리
	if (UWorld* W = GWorld)
	{
		W->GetSelectionManager()->CleanupInvalidActors();
	}

	// 선택된 액터가 null인지 확인
	AActor* SelectedActor = nullptr;
	if (UWorld* W = GWorld)
	{
		SelectedActor = W->GetSelectionManager()->GetSelectedActor();
	}
	if (!SelectedActor)
	{
	}
}

void USceneManagerWidget::SyncSelectionToViewport(AActor* Actor)
{
}

// Category Management Implementation
FString USceneManagerWidget::GetActorCategory(AActor* Actor) const
{
	if (!Actor)
		return "Unknown";

	FString ActorName = Actor->GetName().ToString();

	// Extract category from actor name (assumes format: "Type_Number")
	size_t UnderscorePos = ActorName.find('_');
	if (UnderscorePos != std::string::npos)
	{
		return ActorName.substr(0, UnderscorePos);
	}

	// Fallback: use the full name as category if no underscore found
	return ActorName;
}

USceneManagerWidget::FActorTreeNode* USceneManagerWidget::FindOrCreateCategoryNode(const FString& CategoryName)
{
	// Look for existing category node in root nodes
	for (FActorTreeNode* Node : RootNodes)
	{
		if (Node && Node->IsCategory() && Node->CategoryName == CategoryName)
		{
			return Node;
		}
	}

	// Create new category node if not found
	FActorTreeNode* CategoryNode = new FActorTreeNode(CategoryName);
	RootNodes.push_back(CategoryNode);
	return CategoryNode;
}

void USceneManagerWidget::BuildCategorizedHierarchy()
{
	UWorld* World = GWorld;
	if (!World)
		return;

	const TArray<AActor*>& Actors = World->GetActors();

	// Group actors by category
	for (AActor* Actor : Actors)
	{
		if (!Actor)
			continue;

		// Create actor node and add to category
		FActorTreeNode* ActorNode = new FActorTreeNode(Actor);
		ActorNode->Parent = nullptr;
		// Initialize node visibility from actor's actual visibility state
		ActorNode->bIsVisible = Actor->IsActorVisible();
		RootNodes.Add(ActorNode);
	}

	// Initialize category visibility based on child actors
	for (FActorTreeNode* CategoryNode : RootNodes)
	{
		if (CategoryNode && CategoryNode->IsCategory())
		{
			// Category is visible if any child is visible
			bool bAnyCategoryChildVisible = false;
			for (FActorTreeNode* Child : CategoryNode->Children)
			{
				if (Child && Child->bIsVisible)
				{
					bAnyCategoryChildVisible = true;
					break;
				}
			}
			CategoryNode->bIsVisible = bAnyCategoryChildVisible;
		}
	}
}

void USceneManagerWidget::HandleCategorySelection(FActorTreeNode* CategoryNode)
{
	if (!CategoryNode || !CategoryNode->IsCategory())
		return;

	// Toggle category expansion
	CategoryNode->bIsExpanded = !CategoryNode->bIsExpanded;

	UE_LOG("SceneManager: Toggled category %s: %s",
		CategoryNode->CategoryName.c_str(),
		CategoryNode->bIsExpanded ? "Expanded" : "Collapsed");
}

void USceneManagerWidget::HandleCategoryVisibilityToggle(FActorTreeNode* CategoryNode)
{
	if (!CategoryNode || !CategoryNode->IsCategory())
		return;

	// Toggle category visibility
	CategoryNode->bIsVisible = !CategoryNode->bIsVisible;

	// Apply visibility to all child actors
	for (FActorTreeNode* Child : CategoryNode->Children)
	{
		if (Child && Child->IsActor() && Child->Actor)
		{
			Child->Actor->SetActorHiddenInEditor(!CategoryNode->bIsVisible);
			Child->bIsVisible = CategoryNode->bIsVisible;
		}
	}

	UE_LOG("SceneManager: Toggled category visibility %s: %s",
		CategoryNode->CategoryName.c_str(),
		CategoryNode->bIsVisible ? "Visible" : "Hidden");
}

void USceneManagerWidget::RenderCategoryNode(FActorTreeNode* CategoryNode, int32 Depth)
{
	if (!CategoryNode || !CategoryNode->IsCategory())
		return;

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	// Categories are always expandable
	if (!CategoryNode->Children.empty())
	{
		if (CategoryNode->bIsExpanded)
		{
			NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
	}
	else
	{
		// Empty category - show as leaf
		NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// Create unique ID for ImGui using category name
	ImGui::PushID(CategoryNode->CategoryName.c_str());

	// Category visibility toggle button
	UTexture* CurrentIcon = CategoryNode->bIsVisible ? IconVisible : IconHidden;
	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

		if (ImGui::ImageButton("##CategoryVisibilityBtn", (void*)CurrentIcon->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
		{
			HandleCategoryVisibilityToggle(CategoryNode);
		}

		ImGui::PopStyleColor(3);
	}
	else
	{
		// Fallback to text if icon not loaded
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		const char* VisibilityIcon = CategoryNode->bIsVisible ? "O" : "X";
		if (ImGui::SmallButton(VisibilityIcon))
		{
			HandleCategoryVisibilityToggle(CategoryNode);
		}
		ImGui::PopStyleColor();
	}

	ImGui::SameLine(0, 2.0f); // 간격 2픽셀로 설정

	// Category name with object count
	FString DisplayText = CategoryNode->CategoryName + " (" + std::to_string(CategoryNode->Children.size()) + ")";
	bool bNodeOpen = ImGui::TreeNodeEx(DisplayText.c_str(), NodeFlags);

	// Handle category click
	if (ImGui::IsItemClicked())
	{
		HandleCategorySelection(CategoryNode);
	}

	// Update expansion state based on ImGui tree state
	CategoryNode->bIsExpanded = bNodeOpen;

	// Render child actors if category is expanded
	if (bNodeOpen && !CategoryNode->Children.empty())
	{
		for (FActorTreeNode* Child : CategoryNode->Children)
		{
			RenderActorNode(Child, Depth + 1);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void USceneManagerWidget::ExpandAllCategories()
{
	for (FActorTreeNode* RootNode : RootNodes)
	{
		if (RootNode && RootNode->IsCategory())
		{
			RootNode->bIsExpanded = true;
		}
	}
	UE_LOG("SceneManager: Expanded all categories");
}

void USceneManagerWidget::CollapseAllCategories()
{
	for (FActorTreeNode* RootNode : RootNodes)
	{
		if (RootNode && RootNode->IsCategory())
		{
			RootNode->bIsExpanded = false;
		}
	}
	UE_LOG("SceneManager: Collapsed all categories");
}

FString USceneManagerWidget::FActorTreeNode::GetDisplayName() const
{
	if (IsCategory()) return CategoryName;
	if (IsActor() && Actor) return Actor->GetName().ToString();
	return "Unknown";
}
