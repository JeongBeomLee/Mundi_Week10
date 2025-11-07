-- CameraUtility 싱글톤 모듈: PlayerCameraManager와 CameraModifier 관련 유틸리티
-- require로 로드하면 항상 동일한 인스턴스가 반환됩니다.

local CameraUtility = {};

-- Private 멤버 (모듈 내부에서만 접근 가능)
local PlayerController = nil;
local PlayerCameraManager = nil;

-- PlayerCameraManager 초기화
-- @param World: PIE World 객체
-- @return boolean: 초기화 성공 여부
function CameraUtility.Initialize(World)
    -- 이미 초기화되었는지 확인 (싱글톤 보장)
    if PlayerCameraManager ~= nil then
        PrintToConsole("[CameraUtility] Already initialized (singleton)");
        return true;
    end

    local GameMode = GetRunnerGameMode(World);
    if not GameMode then
        PrintToConsole("[CameraUtility] WARNING: GameMode is nil");
        return false;
    end

    PlayerController = GameMode:GetPlayerController();
    if not PlayerController then
        PrintToConsole("[CameraUtility] WARNING: PlayerController is nil");
        return false;
    end

    PlayerCameraManager = PlayerController:GetPlayerCameraManager();
    if not PlayerCameraManager then
        PrintToConsole("[CameraUtility] WARNING: PlayerCameraManager is nil");
        return false;
    end

    PrintToConsole("[CameraUtility] PlayerCameraManager initialized (singleton)");
    return true;
end

-- 카메라 셰이크 추가
-- @param RotationAmplitude: 흔들림 각도 크기 (기본값: 10.0)
-- @param AlphaInTime: 흔들림 지속 시간 (기본값: 2.0)
-- @param NumSamples: 흔들림 곡선의 주기 개수 (기본값: 6)
function CameraUtility.AddCameraShake(RotationAmplitude, AlphaInTime, NumSamples)
    if not PlayerCameraManager then
        return;
    end

    -- 기본값 설정
    RotationAmplitude = RotationAmplitude or 10.0;
    AlphaInTime = AlphaInTime or 2.0;
    NumSamples = NumSamples or 6;

    -- Camera Shake
    local CameraShakeModifier = UCameraModifier_CameraShake();
    -- 흔들리는 정도 설정
    CameraShakeModifier:SetRotationAmplitude(RotationAmplitude);
    -- 흔들리는 시간 설정
    CameraShakeModifier:SetAlphaInTime(AlphaInTime);
    -- 흔들림 곡선의 주기 설정
    CameraShakeModifier:SetNumSamples(NumSamples);
    -- 앞선 설정으로 새로운 흔들림 생성
    CameraShakeModifier:GetNewShake();

    PlayerCameraManager:AddCameraModifier(CameraShakeModifier);
end

local CurLetterBoxSize = 0.0;
local letterBox = nil;

function CameraUtility.AddLetterBox(FadeSize, Opacity, FadeTime, bFadeIn)
    if not PlayerCameraManager then
        return;
    end

    letterBox = UCameraModifier_LetterBox()
    -- 레터박스 시작 (크기, 불투명도, 페이드인 시간)
    if bFadeIn == true then
        CurLetterBoxSize = 0.0
        letterBox:SetFadeIn(FadeSize, Opacity, FadeTime, CurLetterBoxSize)
        CurLetterBoxSize = FadeSize
    else
        letterBox:SetFadeOut(FadeTime, CurLetterBoxSize)
    end
    
    PlayerCameraManager:AddCameraModifier(letterBox)
end

function CameraUtility.AddVignetting(Color, Radius, Softness, FadeInTime)
    if not PlayerCameraManager then
        return;
    end

    -- Vignetting 모디파이어 생성
    local Vignetting = UCameraModifier_Vignetting();

    -- 기본값 설정
    Color = Color or FVector4(1.0, 207.0 / 255.0, 64.0 / 255.0, 1.0);
    Radius = Radius or 0.99;
    Softness = Softness or 0.2;
    FadeInTime = FadeInTime or 1.0;

    PrintToConsole(string.format("[AddVignetting] Color: (%.2f, %.2f, %.2f)", Color.X, Color.Y, Color.Z));
    PrintToConsole(string.format("[AddVignetting] Radius: %.2f, Softness: %.2f", Radius, Softness));

    -- Vignetting 파라미터 설정
    Vignetting:SetRadius(Radius);
    Vignetting:SetSoftness(Softness);
    Vignetting:SetVignettingColor(Color);

    -- FadeIn 시작 (AlphaInTime 설정 + 활성화 + FadeIn 플래그)
    Vignetting:SetAlphaInTime(FadeInTime);
    Vignetting:EnableModifier();
    Vignetting:SetIsFadingIn(true);

    -- PlayerCameraManager에 추가
    PlayerCameraManager:AddCameraModifier(Vignetting);
end

function CameraUtility.AddScoreAndShowGoldVignetting()
    _G.GetRunnerGameMode(GlobalObjectManager.GetPIEWorld()):OnCoinCollected(1);
    CameraUtility.AddVignetting(
            FVector4(1.0, 207.0 / 255.0, 64.0 / 255.0, 1.0),
            1.2,
            0.2,
            0.5
    );
end

    -- 비활성화된 카메라 셰이크 제거
function CameraUtility.RemoveDisabledCameraModifiers()
    if not PlayerCameraManager then
        return;
    end

    -- C++의 PlayerCameraManager가 비활성화된 모디파이어를 자동으로 제거
    PlayerCameraManager:RemoveDisabledCameraModifiers();
end

-- 카메라 업데이트 (Tick에서 호출)
-- @param dt: Delta Time
function CameraUtility.UpdateCamera(dt)
    if PlayerCameraManager then
        PlayerCameraManager:UpdateCamera(dt);
    end
end

-- 모든 활성 카메라 셰이크 제거 (리셋 시 사용)
function CameraUtility.ClearAllCameraShakes()
    if not PlayerCameraManager then
        return;
    end

    -- 모든 비활성화된 모디파이어 제거 (활성 모디파이어는 유지)
    PlayerCameraManager:RemoveDisabledCameraModifiers();
end

-- Public Getter: 초기화 여부 확인
-- @return boolean: 초기화되었으면 true, 아니면 false
function CameraUtility.IsInitialized()
    return PlayerCameraManager ~= nil;
end

-- Public Getter: 활성 셰이크 개수 반환
-- @return number: 현재 활성화된 카메라 셰이크 개수
-- 참고: C++에서 관리하는 ModifierList의 크기를 직접 가져올 수 없으므로
-- 이 함수는 정확한 개수를 반환하지 못할 수 있습니다.
function CameraUtility.GetActiveCameraShakeCount()
    -- C++에서 ModifierList 크기를 가져오는 메서드가 없으므로 0 반환
    return 0;
end

-- 카메라 페이드 시작
-- @param FromAlpha: 시작 알파 값 (0 = 완전 투명, 1 = 완전 불투명)
-- @param ToAlpha: 종료 알파 값 (0 = 완전 투명, 1 = 완전 불투명)
-- @param Duration: 페이드 지속 시간 (초)
-- @param FadeColor: 페이드 색상 (FLinearColor, 기본값: 검은색)
function CameraUtility.StartCameraFade(FromAlpha, ToAlpha, Duration, FadeColor)
    if not PlayerCameraManager then
        return;
    end

    -- 기본값 설정
    FadeColor = FadeColor or FLinearColor(0.0, 0.0, 0.0, 1.0); -- 검은색

    PlayerCameraManager:StartCameraFade(FromAlpha, ToAlpha, Duration, FadeColor);
end

-- 카메라 페이드 중지
function CameraUtility.StopCameraFade()
    if not PlayerCameraManager then
        return;
    end

    PlayerCameraManager:StopCameraFade();
end

-- 모듈 반환 (require가 이 값을 받음)
return CameraUtility;
