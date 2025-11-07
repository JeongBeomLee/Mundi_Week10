#pragma once
#include "Widget.h"
#include "Vector.h"
#include "UEContainer.h"

class UUIManager;
class UWorld;
class AActor;
class USelectionManager;
class UTexture;

/**
 * SceneManagerWidget
 * - Unreal Engine style hierarchical object browser
 * - Shows all actors in the world in a tree view
 * - Supports selection, visibility toggle, hierarchy management
 * - Syncs with 3D viewport selection
 */
class USceneManagerWidget : public UWidget
{
public:
    DECLARE_CLASS(USceneManagerWidget, UWidget)
    
    void Initialize() override;
    void Update() override;
    void RenderWidget() override;

    // Special Member Functions
    USceneManagerWidget();
    ~USceneManagerWidget() override;

    // Public API for external refresh requests
    void RequestImmediateRefresh() { RefreshActorTree(); }
    void RequestDelayedRefreshPublic() { bNeedRefreshNextFrame = true; }

private:
    UUIManager* UIManager = nullptr;
    USelectionManager* SelectionManager = nullptr;
    
    // UI State
    bool bShowOnlySelectedObjects = false;
    bool bShowHiddenObjects = true;
    FString SearchFilter = "";
    
    // Node types for tree hierarchy
    enum class ETreeNodeType
    {
        Category,
        Actor
    };
    
    // Actor Management
    struct FActorTreeNode
    {
        ETreeNodeType NodeType = ETreeNodeType::Actor;
        AActor* Actor = nullptr;
        FString CategoryName = "";
        TArray<FActorTreeNode*> Children;
        FActorTreeNode* Parent = nullptr;
        bool bIsExpanded = true;
        bool bIsVisible = true;
        
        // Constructor for Actor node
        FActorTreeNode(AActor* InActor) : NodeType(ETreeNodeType::Actor), Actor(InActor) {}
        
        // Constructor for Category node
        FActorTreeNode(const FString& InCategoryName) : NodeType(ETreeNodeType::Category), CategoryName(InCategoryName) {}
        
        ~FActorTreeNode() 
        {
            for (auto* Child : Children)
            {
                delete Child;
            }
        }
        
        // Helper methods
        bool IsCategory() const { return NodeType == ETreeNodeType::Category; }
        bool IsActor() const { return NodeType == ETreeNodeType::Actor; }
        FString GetDisplayName() const;
    };
    
    TArray<FActorTreeNode*> RootNodes;
    
    // Helper Methods
    void RefreshActorTree();
    void BuildActorHierarchy();
    void RenderActorNode(FActorTreeNode* Node, int32 Depth = 0);
    void RenderCategoryNode(FActorTreeNode* CategoryNode, int32 Depth = 0);
    bool ShouldShowActor(AActor* Actor) const;
    void HandleActorSelection(AActor* Actor);
    void HandleActorVisibilityToggle(AActor* Actor);
    void HandleActorRename(AActor* Actor);
    void HandleActorDelete(AActor* Actor);
    void HandleActorDuplicate(AActor* Actor);

    // Icon loading
    void LoadIcons();
    
    // Context Menu
    AActor* ContextMenuTarget = nullptr;
    bool bShowContextMenu = false;
    void RenderContextMenu();
    
    // Drag & Drop (for hierarchy management)
    AActor* DragSource = nullptr;
    AActor* DropTarget = nullptr;
    void HandleDragDrop();
    
    
    // Toolbar
    void RenderToolbar();
    
    // Tree management
    void ClearActorTree();
    FActorTreeNode* FindNodeByActor(AActor* Actor);
    void ExpandParentsOfSelected();
    
    // Category management
    FActorTreeNode* FindOrCreateCategoryNode(const FString& CategoryName);
    FString GetActorCategory(AActor* Actor) const;
    void BuildCategorizedHierarchy();
    void HandleCategorySelection(FActorTreeNode* CategoryNode);
    void HandleCategoryVisibilityToggle(FActorTreeNode* CategoryNode);
    void ExpandAllCategories();
    void CollapseAllCategories();
    
    // Selection synchronization
    void SyncSelectionFromViewport();
    void SyncSelectionToViewport(AActor* Actor);
    
    // Delayed refresh (to avoid iterator invalidation during rendering)
    bool bNeedRefreshNextFrame = false;
    void RequestDelayedRefresh() { bNeedRefreshNextFrame = true; }

    // Visibility icons
    UTexture* IconVisible = nullptr;
    UTexture* IconHidden = nullptr;
    float IconSize = 16.0f;
};