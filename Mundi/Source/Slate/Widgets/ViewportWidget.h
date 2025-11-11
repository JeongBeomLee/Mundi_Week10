#pragma once
#include "Widgets/Widget.h"
#include "Enums.h"
#include <functional>

class FOffscreenViewport;
class FOffscreenViewportClient;
class AOffscreenGizmoActor;
class USkeletalMeshComponent;
class UWorld;
class AActor;

/**
 * @brief Viewport 렌더링 및 Gizmo 상호작용 위젯
 *
 * 설계:
 * - UWidget 상속 (기존 패턴 준수)
 * - 3D Viewport 렌더링 + Toolbar + Gizmo 입력 처리
 * - 포인터 주입으로 상태 공유
 * - SDetailsWindow 패턴 준수
 *
 * 책임:
 * - Viewport 렌더링 (FOffscreenViewport 사용)
 * - Viewport Toolbar 렌더링 (Gizmo 모드 전환 버튼)
 * - Gizmo 상호작용 처리 (드래그, 키보드 단축키)
 * - Gizmo 위치 업데이트 (선택된 본 위치로 이동)
 * - 카메라 입력 전달 (ViewportClient)
 */
class UViewportWidget : public UWidget
{
public:
	DECLARE_CLASS(UViewportWidget, UWidget)

	UViewportWidget();

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	/**
	 * @brief 외부 데이터 주입 (SDetailsWindow 패턴)
	 * @param Viewport Offscreen viewport (nullptr 안전)
	 * @param Client Viewport client (nullptr 안전)
	 * @param Gizmo Gizmo actor (nullptr 안전)
	 * @param Component Preview mesh component (nullptr 안전)
	 * @param World Editor world (nullptr 안전)
	 * @param Actor Preview actor (nullptr 안전)
	 * @param BoneIndex 선택된 본 인덱스 포인터
	 * @param GizmoMode Gizmo 모드 포인터
	 * @param GizmoSpace Gizmo 공간 포인터
	 */
	void SetViewport(FOffscreenViewport* Viewport) { EmbeddedViewport = Viewport; }
	void SetViewportClient(FOffscreenViewportClient* Client) { ViewportClient = Client; }
	void SetGizmo(AOffscreenGizmoActor* Gizmo) { BoneGizmo = Gizmo; }
	void SetPreviewComponent(USkeletalMeshComponent* Component) { PreviewComponent = Component; }
	void SetEditorWorld(UWorld* World) { EditorWorld = World; }
	void SetPreviewActor(AActor* Actor) { PreviewActor = Actor; }
	void SetSelectedBoneIndex(int32* BoneIndex) { SelectedBoneIndexPtr = BoneIndex; }
	void SetGizmoMode(EGizmoMode* Mode) { GizmoModePtr = Mode; }
	void SetGizmoSpace(EGizmoSpace* Space) { GizmoSpacePtr = Space; }

	/**
	 * @brief Gizmo 위치 업데이트 (선택된 본 위치로 이동)
	 * @note Update()에서 매 프레임 호출됨
	 */
	void UpdateGizmoForSelectedBone();

private:
	// Viewport 렌더링
	void RenderViewportToolbar();

	// 툴바 아이콘 지연 로드 (최초 1회만 실행)
	void LoadToolbarIcons();

	// 외부에서 주입된 데이터 (포인터)
	FOffscreenViewport* EmbeddedViewport = nullptr;
	FOffscreenViewportClient* ViewportClient = nullptr;
	AOffscreenGizmoActor* BoneGizmo = nullptr;
	USkeletalMeshComponent* PreviewComponent = nullptr;
	UWorld* EditorWorld = nullptr;
	AActor* PreviewActor = nullptr;
	int32* SelectedBoneIndexPtr = nullptr;
	EGizmoMode* GizmoModePtr = nullptr;
	EGizmoSpace* GizmoSpacePtr = nullptr;

	// 드래그 시작 시 본의 원래 회전 저장 (World 모드 Rotate에서 사용)
	FQuat DragStartBoneRotation = FQuat::Identity();

	// 툴바 아이콘 텍스처 (메인 뷰포트와 동일)
	class UTexture* IconSelect = nullptr;
	class UTexture* IconMove = nullptr;
	class UTexture* IconRotate = nullptr;
	class UTexture* IconScale = nullptr;
	class UTexture* IconWorldSpace = nullptr;
	class UTexture* IconLocalSpace = nullptr;
};
