#pragma once
#include "Vector.h" // FMatrix
#include "Enums.h"
#include "CameraComponent.h"

#include "Frustum.h"
// 전방 선언
class ACameraActor;
class UCameraComponent;
class FViewport;

/**
 * @struct FViewportRect
 * @brief 뷰포트 내의 사각 영역
 */
struct FViewportRect
{
    uint32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    uint32 Width() const { return MaxX - MinX; }
    uint32 Height() const { return MaxY - MinY; }
};

/**
 * @class FSceneView
 * @brief 단일 관점에서 씬을 렌더링하는 데 필요한 모든 데이터를 소유합니다.
 * (ViewMatrix, ProjectionMatrix, Frustum, ViewportRect 등)
 */
class FSceneView
{
public:
    // 메인 뷰(카메라)를 위한 생성자
    FSceneView(ACameraActor* InCamera, FViewport* InViewport, EViewModeIndex InViewMode);

    // TODO: 섀도우 맵(광원) 등을 위한 생성자 추가
    // FSceneView(ALightActor* InLight, FTexture* RenderTarget);

    // 렌더링 데이터
    FMatrix ViewMatrix{};
    FMatrix ProjectionMatrix{};
    FFrustum ViewFrustum{};
    FVector ViewLocation{};
    FViewportRect ViewRect{}; // 이 뷰가 그려질 뷰포트상의 영역

    // 렌더링 설정
	EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit_Phong;
    ECameraProjectionMode ProjectionMode = ECameraProjectionMode::Perspective;
    float ZNear{}, ZFar{};

    // 뷰포트 참조 (ViewportClient 접근용)
    FViewport* Viewport = nullptr;
};