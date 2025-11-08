#pragma once
#include "Widgets/Widget.h"
#include "Enums.h"

class USkeletalMeshComponent;
class FOffscreenViewport;
class FOffscreenViewportClient;
class UWorld;
class AOffscreenGizmoActor;
class UBoneHierarchyWidget;
class UBoneTransformWidget;
class UViewportWidget;

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

	/** @brief 편집 내용을 원본으로 되돌림 (TargetComponent → PreviewMeshComponent) */
	void RevertChanges();

	/** @brief 리소스 정리 (UWidget에는 없지만 필요) */
	void Shutdown();

private:
	// TODO: 향후 구현
	void HandleViewportInput();
	void PerformBonePicking(float MouseX, float MouseY);

	/** @brief FBX Asset에서 Bone 데이터 로드 (TargetComponent의 SkeletalMesh에서) */
	void LoadBonesFromAsset();

	// 상태
	USkeletalMeshComponent* TargetComponent = nullptr;  // 원본 컴포넌트 (메인 에디터)
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;  // 편집 대상 (EditorWorld)
	int32 SelectedBoneIndex = -1;

	// Viewport 관련
	FOffscreenViewport* EmbeddedViewport = nullptr;
	FOffscreenViewportClient* ViewportClient = nullptr;

	/**
	 * @brief 전용 Embedded World (Singleton 패턴)
	 *
	 * 설계 근거:
	 * - UIManager에서 단일 Skeletal Mesh Editor 윈도우만 존재 (중복 열기 불가)
	 * - 여러 컴포넌트를 순차적으로 편집해도 같은 World 재사용
	 * - PreviewActor는 SetTargetComponent()에서 교체됨 (메모리 효율)
	 * - 불필요한 다중 World 생성 방지 (조명, SelectionManager 등 중복 생성 방지)
	 * - 프로그램 종료 시 자동 정리 (static 수명 주기)
	 *
	 * 대안 고려:
	 * - 인스턴스 별 World 생성 시: 메모리 낭비, Actor 누적 문제 발생
	 * - 윈도우 닫을 때 World 삭제 시: 재오픈 시 재생성 비용 발생
	 *
	 * @note InitializeEditorWorld()에서 최초 1회만 초기화됨
	 */
	static inline UWorld* EditorWorld = nullptr;

	// Editor World에 생성된 액터들
	AActor* PreviewActor = nullptr;  // SkeletalMeshComponent를 가진 미리보기 액터

	// Gizmo (Offscreen 전용 - ImGui 입력 사용)
	AOffscreenGizmoActor* BoneGizmo = nullptr;
	EGizmoMode CurrentGizmoMode = EGizmoMode::Translate;
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::Local;

	// === 복합 위젯들 (SDetailsWindow 패턴) ===
	UBoneHierarchyWidget* HierarchyWidget = nullptr;
	UBoneTransformWidget* TransformWidget = nullptr;
	UViewportWidget* ViewportWidget = nullptr;

private:
	// EditorWorld 초기화 (최초 1회만 실행)
	void InitializeEditorWorld();
};
