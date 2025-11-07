#pragma once
#include "SWindow.h"
#include "Enums.h"

class FViewport;
class FViewportClient;
class UTexture;

class SViewportWindow : public SWindow
{
public:
    SViewportWindow();
    virtual ~SViewportWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType);

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;

    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    FViewport* GetViewport() const { return Viewport; }
    FViewportClient* GetViewportClient() const { return ViewportClient; }
    void SetVClientWorld(UWorld* InWorld);
private:
    void RenderToolbar();
    void RenderGizmoModeButtons();
    void RenderGizmoSpaceButton();
    void RenderCameraOptionDropdownMenu();
    void RenderViewModeDropdownMenu();
    void RenderShowFlagDropdownMenu();
    void RenderViewportLayoutSwitchButton();
    void LoadToolbarIcons(ID3D11Device* Device);

private:
    FViewport* Viewport = nullptr;
    FViewportClient* ViewportClient = nullptr;

    EViewportType ViewportType;
    FName ViewportName;

    bool bIsActive;
    bool bIsMouseDown;

    // ViewMode 관련 상태 저장
    int CurrentLitSubMode = 0; // 0=default(Phong) 1=Gouraud, 2=Lambert, 3=Phong [기본값: default(Phong)]
    int CurrentBufferVisSubMode = 1; // 0=SceneDepth, 1=WorldNormal (기본값: WorldNormal)

    // 툴바 아이콘 텍스처
    UTexture* IconSelect = nullptr;
    UTexture* IconMove = nullptr;
    UTexture* IconRotate = nullptr;
    UTexture* IconScale = nullptr;
    UTexture* IconWorldSpace = nullptr;
    UTexture* IconLocalSpace = nullptr;

    // 뷰포트 모드 아이콘 텍스처
    UTexture* IconCamera = nullptr;
    UTexture* IconPerspective = nullptr;
    UTexture* IconTop = nullptr;
    UTexture* IconBottom = nullptr;
    UTexture* IconLeft = nullptr;
    UTexture* IconRight = nullptr;
    UTexture* IconFront = nullptr;
    UTexture* IconBack = nullptr;

    // 뷰포트 설정 아이콘 텍스처
    UTexture* IconSpeed = nullptr;
    UTexture* IconFOV = nullptr;
    UTexture* IconNearClip = nullptr;
    UTexture* IconFarClip = nullptr;

    // 뷰모드 아이콘 텍스처
    UTexture* IconViewMode_Lit = nullptr;
    UTexture* IconViewMode_Unlit = nullptr;
    UTexture* IconViewMode_Wireframe = nullptr;
    UTexture* IconViewMode_BufferVis = nullptr;

    // ShowFlag 아이콘 텍스처
    UTexture* IconShowFlag = nullptr;
    UTexture* IconRevert = nullptr;
    UTexture* IconStats = nullptr;
    UTexture* IconHide = nullptr;
    UTexture* IconBVH = nullptr;
    UTexture* IconGrid = nullptr;
    UTexture* IconDecal = nullptr;
    UTexture* IconStaticMesh = nullptr;
    UTexture* IconBillboard = nullptr;
    UTexture* IconFog = nullptr;
    UTexture* IconCollision = nullptr;
    UTexture* IconAntiAliasing = nullptr;
    UTexture* IconTile = nullptr;
    UTexture* IconShadowMap = nullptr;

    // 뷰포트 레이아웃 전환 아이콘
    UTexture* IconSingleToMultiViewport = nullptr;  // 단일 뷰포트 아이콘
    UTexture* IconMultiToSingleViewport = nullptr;   // 멀티 뷰포트 아이콘
};