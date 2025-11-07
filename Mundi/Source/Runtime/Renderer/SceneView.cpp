#include "pch.h"
#include "SceneView.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "Frustum.h"

FSceneView::FSceneView(ACameraActor* InCamera, FViewport* InViewport, EViewModeIndex InViewMode)
{
    UCameraComponent* CamComp = InCamera->GetCameraComponent();
    if (!CamComp || !InViewport)
    {
        UE_LOG("[FSceneView::FSceneView()]: CamComp 또는 InViewport 가 없습니다.");
        return;
    }

    // Viewport 저장 (ViewportClient 접근용)
    Viewport = InViewport;

    // --- 이 로직이 FSceneRenderer::PrepareView()에서 이동해 옴 ---

    float AspectRatio = 1.0f;
    if (InViewport->GetSizeY() > 0)
    {
        AspectRatio = (float)InViewport->GetSizeX() / (float)InViewport->GetSizeY();
    }

    ViewMatrix = CamComp->GetViewMatrix();
    ProjectionMatrix = CamComp->GetProjectionMatrix(AspectRatio, InViewport);
    ViewFrustum = CreateFrustumFromCamera(*CamComp, AspectRatio);
    ViewLocation = CamComp->GetWorldLocation();
    ZNear = CamComp->GetNearClip();
    ZFar = CamComp->GetFarClip();

    ViewRect.MinX = InViewport->GetStartX();
    ViewRect.MinY = InViewport->GetStartY();
    ViewRect.MaxX = ViewRect.MinX + InViewport->GetSizeX();
    ViewRect.MaxY = ViewRect.MinY + InViewport->GetSizeY();

    // ViewMode는 World나 RenderSettings에서 가져와야 함 (임시)
    ViewMode = InViewMode;
    ProjectionMode = InCamera->GetCameraComponent()->GetProjectionMode();
}