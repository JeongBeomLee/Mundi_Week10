#pragma once
#include "Widgets/Widget.h"

class USkeletalMeshComponent;

/**
 * @brief Skeletal Mesh Editor 위젯 (실제 UI 구현)
 * @note 왼쪽: Bone hierarchy tree, 오른쪽: Transform editor
 *
 * 아키텍처:
 * - UWidget 상속 (기존 패턴 준수)
 * - RenderWidget()에서 ImGui 렌더링
 * - SSkeletalMeshEditorWindow가 이 위젯을 포함
 */
class USkeletalMeshEditorWidget : public UWidget
{
public:
	DECLARE_CLASS(USkeletalMeshEditorWidget, UWidget)

	USkeletalMeshEditorWidget();

	/** @brief 편집 대상 컴포넌트 설정 */
	void SetTargetComponent(USkeletalMeshComponent* Component);

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

private:
	// UI 렌더링
	void RenderBoneHierarchy();
	void RenderBoneTreeNode(int32 BoneIndex);
	void RenderTransformEditor();

	// 상태
	USkeletalMeshComponent* TargetComponent = nullptr;
	int32 SelectedBoneIndex = -1;
};
