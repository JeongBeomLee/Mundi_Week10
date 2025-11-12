#pragma once
#include "Widgets/Widget.h"
#include "Enums.h"

class FOffscreenViewport;
class FOffscreenViewportClient;
class AOffscreenGizmoActor;
struct FTransform;


/**
 * @brief 범용 Viewport 렌더링 및 Gizmo 상호작용 위젯
 *
 * 설계:
 * - UWidget 상속 (기존 패턴 준수)
 * - 3D Viewport 렌더링 + Toolbar + Gizmo 입력 처리
 * - Transform 포인터 주입으로 타겟과 동기화
 * - 완전히 범용적: 본, 액터, 커스텀 오브젝트 모두 지원
 *
 * 책임:
 * - Viewport 렌더링 (FOffscreenViewport 사용)
 * - Viewport Toolbar 렌더링 (Gizmo 모드 전환 버튼)
 * - Gizmo 상호작용 처리 (드래그, 키보드 단축키)
 * - Gizmo ↔ Transform 동기화 (타겟 위치로 이동, 드래그 시 Transform 수정)
 * - 카메라 입력 전달 (ViewportClient)
 *
 * 사용법:
 * - SetTargetTransform(&transform)로 타겟 주입
 * - Update()에서 Gizmo가 transform을 직접 수정
 * - 외부에서 transform을 다시 타겟에 반영
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
	 * @note Gizmo 모드/공간은 ViewportWidget이 내부에서 관리
	 * @note 타겟 Transform은 SetTargetTransform()으로 주입
	 */
	void SetViewport(FOffscreenViewport* Viewport) { EmbeddedViewport = Viewport; }
	void SetViewportClient(FOffscreenViewportClient* Client) { ViewportClient = Client; }
	void SetGizmo(AOffscreenGizmoActor* Gizmo) { BoneGizmo = Gizmo; }

	/**
	 * @brief Gizmo가 따라다닐 타겟 Transform 설정
	 * @param Transform 타겟의 Transform 포인터 (nullptr이면 Gizmo 숨김)
	 * @note 외부에서 매 프레임 업데이트된 Transform 포인터 주입
	 */
	void SetTargetTransform(FTransform* Transform) { TargetTransform = Transform; }

private:
	// Viewport 렌더링
	void RenderViewportToolbar();
	void RenderViewModeDropdown();

	// 툴바 아이콘 지연 로드 (최초 1회만 실행)
	void LoadToolbarIcons();

	// 외부에서 주입된 데이터 (포인터)
	FOffscreenViewport* EmbeddedViewport = nullptr;
	FOffscreenViewportClient* ViewportClient = nullptr;
	AOffscreenGizmoActor* BoneGizmo = nullptr;

	// Gizmo 타겟 Transform (외부에서 매 프레임 주입)
	FTransform* TargetTransform = nullptr;

	// Gizmo 상태 (ViewportWidget이 직접 소유)
	EGizmoMode CurrentGizmoMode = EGizmoMode::Translate;
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::Local;

	// 드래그 시작 시 타겟의 원래 회전 저장 (World 모드 Rotate에서 사용)
	FQuat DragStartRotation = FQuat::Identity();

	// ViewMode 관련 상태 저장
	int CurrentLitSubMode = 0; // 0=default(Phong) 1=Gouraud, 2=Lambert, 3=Phong [기본값: default(Phong)]
	int CurrentBufferVisSubMode = 1; // 0=SceneDepth, 1=WorldNormal (기본값: WorldNormal)

	// 툴바 아이콘 텍스처 (메인 뷰포트와 동일)
	class UTexture* IconSelect = nullptr;
	class UTexture* IconMove = nullptr;
	class UTexture* IconRotate = nullptr;
	class UTexture* IconScale = nullptr;
	class UTexture* IconWorldSpace = nullptr;
	class UTexture* IconLocalSpace = nullptr;

	// 뷰모드 아이콘 텍스처
	class UTexture* IconViewMode_Lit = nullptr;
	class UTexture* IconViewMode_Unlit = nullptr;
	class UTexture* IconViewMode_Wireframe = nullptr;
	class UTexture* IconViewMode_BufferVis = nullptr;
};
