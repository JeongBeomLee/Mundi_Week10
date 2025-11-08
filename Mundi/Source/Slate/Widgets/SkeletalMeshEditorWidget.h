#pragma once
#include "Widgets/Widget.h"
#include "Enums.h"

class USkeletalMeshComponent;
class FOffscreenViewport;
class FOffscreenViewportClient;
class UWorld;
class AGizmoActor;

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

	/** @brief 리소스 정리 (UWidget에는 없지만 필요) */
	void Shutdown();

private:
	// UI 렌더링
	void RenderBoneHierarchy();
	void RenderBoneTreeNode(int32 BoneIndex);
	void RenderTransformEditor();

	// Viewport 렌더링
	void RenderViewport();
	void RenderViewportToolbar();
	void HandleViewportInput();

	// 본 피킹
	void PerformBonePicking(float MouseX, float MouseY);

	// Gizmo 업데이트
	void UpdateGizmoForSelectedBone();

	/** @brief FBX Asset에서 Bone 데이터 로드 (TargetComponent의 SkeletalMesh에서) */
	void LoadBonesFromAsset();

	/** @brief 자식 본이 있는지 확인 (Tree 렌더링용) */
	bool HasChildren(int32 BoneIndex) const;

	// 상태
	USkeletalMeshComponent* TargetComponent = nullptr;
	int32 SelectedBoneIndex = -1;

	// Viewport 관련
	FOffscreenViewport* EmbeddedViewport = nullptr;
	FOffscreenViewportClient* ViewportClient = nullptr;

	// 전용 World (모든 Skeletal Mesh Editor 인스턴스가 공유)
	static inline UWorld* EditorWorld = nullptr;

	// Editor World에 생성된 액터들
	AActor* PreviewActor = nullptr;  // SkeletalMeshComponent를 가진 미리보기 액터

	// Gizmo
	AGizmoActor* BoneGizmo = nullptr;
	EGizmoMode CurrentGizmoMode = EGizmoMode::Translate;
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::Local;

private:
	// EditorWorld 초기화 (최초 1회만 실행)
	static void InitializeEditorWorld();
};
