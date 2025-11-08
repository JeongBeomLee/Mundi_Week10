#pragma once
#include "UIWindow.h"

class USkeletalMeshEditorWidget;
class USkeletalMeshComponent;

/**
 * @brief Skeletal Mesh Editor 창 (독립 팝업 윈도우)
 * @note UUIWindow 상속, UUIManager가 관리
 *
 * 아키텍처 (UCameraBlendEditorWindow 패턴 준수):
 * - UUIWindow 기반 독립 창
 * - UUIManager::FindUIWindow / RegisterUIWindow로 관리
 * - RenderContent()에서 실제 UI 렌더링
 * - SetWindowState(Visible)로 열기/닫기
 *
 * 예시:
 * - UCameraBlendEditorWindow (Camera blend curve editor)
 * - SSkeletalMeshEditorWindow (Skeletal mesh editor)
 */
class SSkeletalMeshEditorWindow : public UUIWindow
{
public:
	DECLARE_CLASS(SSkeletalMeshEditorWindow, UUIWindow)

	SSkeletalMeshEditorWindow();
	virtual ~SSkeletalMeshEditorWindow() override;

	virtual void Initialize() override;
	virtual void RenderContent() override;
	virtual bool OnWindowClose() override;

	/** @brief 편집 대상 컴포넌트 설정 (윈도우 열 때) */
	void SetTargetComponent(USkeletalMeshComponent* Component);

private:
	/** @brief 저장 안하고 닫기 확인 모달 렌더링 */
	void RenderUnsavedChangesModal();

	USkeletalMeshEditorWidget* EditorWidget = nullptr;
	bool bRequestCloseWithUnsavedChanges = false;  // 닫기 요청 플래그
};
