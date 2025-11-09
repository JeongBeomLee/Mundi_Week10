#pragma once
#include "Widgets/Widget.h"

class USkeletalMeshComponent;

/**
 * @brief Bone Hierarchy Tree UI 위젯
 *
 * 설계:
 * - UWidget 상속 (기존 패턴 준수)
 * - 읽기 전용 UI (상태 변경은 포인터를 통해 부모에 전달)
 * - SDetailsWindow 패턴 준수
 *
 * 책임:
 * - Bone hierarchy를 트리 형태로 렌더링
 * - 본 선택 시 SelectedBoneIndex 업데이트
 * - 재귀적 트리 노드 렌더링
 */
class UBoneHierarchyWidget : public UWidget
{
public:
	DECLARE_CLASS(UBoneHierarchyWidget, UWidget)

	UBoneHierarchyWidget();

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	/**
	 * @brief 외부 데이터 주입 (SDetailsWindow 패턴)
	 * @param Component Preview mesh component (nullptr 안전)
	 * @param BoneIndex 선택된 본 인덱스 포인터 (부모 위젯의 상태)
	 */
	void SetPreviewComponent(USkeletalMeshComponent* Component) { PreviewComponent = Component; }
	void SetSelectedBoneIndex(int32* BoneIndex) { SelectedBoneIndexPtr = BoneIndex; }

private:
	/**
	 * @brief 재귀적으로 본 트리 노드 렌더링
	 * @param BoneIndex 렌더링할 본 인덱스
	 */
	void RenderBoneTreeNode(int32 BoneIndex);

	/**
	 * @brief 본이 자식을 가지고 있는지 확인
	 * @param BoneIndex 확인할 본 인덱스
	 * @return 자식이 있으면 true
	 */
	bool HasChildren(int32 BoneIndex) const;

	// 외부에서 주입된 데이터 (포인터/참조)
	USkeletalMeshComponent* PreviewComponent = nullptr;
	int32* SelectedBoneIndexPtr = nullptr;
};
