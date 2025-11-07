-- ────────────────────────────────────────────────────────────────────────────
-- GravitySystem.lua
-- 측면 충돌 시 중력 방향 전환 및 카메라/캐릭터 회전 시스템
-- ────────────────────────────────────────────────────────────────────────────

-- 상수 정의
local ROTATION_DURATION = 0.5  -- 회전 애니메이션 시간 (초)
local GRAVITY_MAGNITUDE = 980.0  -- 중력 크기 (cm/s²)

-- 상태 변수
local CurrentGravityDirection = FVector(0, 0, -1)  -- 현재 중력 방향 (기본: 아래)
local IsRotating = false  -- 회전 중인지 여부
local ActorReference = nil  -- RunnerCharacter 액터 참조
local LastWallNormal = nil  -- 마지막으로 충돌한 벽의 Normal

-- ────────────────────────────────────────────────────────────────────────────
-- 회전 애니메이션 Coroutine
-- ────────────────────────────────────────────────────────────────────────────
local function RotateToNewGravity(newGravityDir)
    if IsRotating then
        PrintToConsole("[GravitySystem] Already rotating, ignoring new gravity change")
        return  -- 이미 회전 중이면 무시
    end

    if not ActorReference then
        PrintToConsole("[GravitySystem] ERROR: ActorReference is nil!")
        return
    end

    IsRotating = true

    -- CharacterMovementComponent에 회전 시작 알림 (중력 멈춤)
    local movement = ActorReference:GetCharacterMovement()
    if movement then
        movement:SetIsRotating(true)
    end

    SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/RotatingFx.mp3", false, SoundChannelType.SFX);

    StartCoroutine(function()
        local newGravityCopy = FVector(newGravityDir.X, newGravityDir.Y, newGravityDir.Z)
        local oldGravityDir = CurrentGravityDirection
        local newUpDir = FVector.Mul(newGravityCopy, -1)  -- 중력 반대 = Up 방향
        local oldUpDir = FVector.Mul(oldGravityDir, -1)

        -- 캐릭터와 카메라 가져오기
        local world = GEngine:GetPIEWorld()
        if not world then
            IsRotating = false
            if movement then movement:SetIsRotating(false) end
            return
        end

        local camera = world:GetCameraActor()
        if not camera then
            IsRotating = false
            if movement then movement:SetIsRotating(false) end
            return
        end

        -- 시작 회전
        local charStartRot = ActorReference:GetRotation()
        local camStartRot = camera:GetRotation()

        -- 목표 회전 계산: oldUp에서 newUp으로 회전하는 Quaternion
        local rotationQuat = FQuat.FromToRotation(oldUpDir, newUpDir)

        local charTargetRot = FQuat.Mul(rotationQuat, charStartRot)
        local camTargetRot = FQuat.Mul(rotationQuat, camStartRot)

        -- 부드러운 회전 (ROTATION_DURATION 초 동안)
        local elapsed = 0.0
        while elapsed < ROTATION_DURATION do
            local dt = coroutine.yield()  -- DeltaTime 반환

            -- dt가 nil이면 기본값 사용
            if not dt then
                dt = 0.016  -- 기본 60 FPS
            end

            elapsed = elapsed + dt

            local alpha = math.min(1.0, elapsed / ROTATION_DURATION)

            -- Slerp로 부드러운 회전
            local currentCharRot = FQuat.Slerp(charStartRot, charTargetRot, alpha)
            local currentCamRot = FQuat.Slerp(camStartRot, camTargetRot, alpha)

            ActorReference:SetRotation(currentCharRot)
            camera:SetRotation(currentCamRot)
        end

        -- 최종 위치 보정
        ActorReference:SetRotation(charTargetRot)
        camera:SetRotation(camTargetRot)

        -- 중력 방향 업데이트 (복사본 사용)
        CurrentGravityDirection = newGravityCopy

        local finalMovement = ActorReference:GetCharacterMovement()
        if finalMovement then
            finalMovement:SetGravityDirection(newGravityCopy)
            -- 회전 종료 알림 (중력 재개)
            finalMovement:SetIsRotating(false)
        end

        IsRotating = false
        LastWallNormal = nil  -- 회전 완료 후 초기화
    end)
end

-- ────────────────────────────────────────────────────────────────────────────
-- 측면 충돌 콜백
-- ────────────────────────────────────────────────────────────────────────────
function OnWallCollision(wallNormal)
    -- 이미 회전 중이면 무시
    if IsRotating then
        return
    end

    -- 같은 벽과의 충돌이면 무시 (Normal이 거의 같으면)
    if LastWallNormal then
        local dotWithLast = FVector.Dot(wallNormal, LastWallNormal)
        if dotWithLast > 0.99 then  -- 거의 같은 방향
            return  -- 중복 호출 무시
        end
    end

    PrintToConsole("[GravitySystem] Wall collision detected. Normal: " ..
        wallNormal.X .. ", " .. wallNormal.Y .. ", " .. wallNormal.Z)

    -- 중력 방향과 벽 Normal의 내적 확인
    local dotProduct = FVector.Dot(CurrentGravityDirection, wallNormal)

    PrintToConsole("[GravitySystem] Dot product with gravity: " .. dotProduct)

    -- 벽이 측면인지 확인 (중력 방향과 거의 수직)
    -- abs(dot) < 0.3 이면 측면 벽으로 판정
    if math.abs(dotProduct) < 0.3 then
        PrintToConsole("[GravitySystem] Side wall detected! Changing gravity...")

        -- 새로운 중력 방향 = 벽 Normal의 반대
        local newGravityDir = FVector.Mul(wallNormal, -1)

        -- 디버그: 계산된 새 중력 방향 출력
        PrintToConsole("[GravitySystem] WallNormal: (" .. wallNormal.X .. ", " .. wallNormal.Y .. ", " .. wallNormal.Z .. ")")
        PrintToConsole("[GravitySystem] NewGravityDir (after Mul): (" .. newGravityDir.X .. ", " .. newGravityDir.Y .. ", " .. newGravityDir.Z .. ")")

        -- 마지막 벽 Normal 저장
        LastWallNormal = wallNormal

        RotateToNewGravity(newGravityDir)
    else
        PrintToConsole("[GravitySystem] Not a side wall (floor or ceiling), ignoring")
    end
end

-- ────────────────────────────────────────────────────────────────────────────
-- 초기화
-- ────────────────────────────────────────────────────────────────────────────
function Initialize(actor)
    PrintToConsole("[GravitySystem] Initializing...")

    -- Actor 참조 저장
    if not actor then
        PrintToConsole("[GravitySystem] ERROR: Actor is nil!")
        return
    end

    ActorReference = actor
    PrintToConsole("[GravitySystem] Actor reference set")

    -- CharacterMovementComponent에 콜백 등록
    local movement = ActorReference:GetCharacterMovement()
    if movement then
        movement:SetOnWallCollisionCallback(OnWallCollision)
        PrintToConsole("[GravitySystem] Callback registered successfully")
    else
        PrintToConsole("[GravitySystem] ERROR: CharacterMovement is nil!")
    end

    -- 초기 중력 방향 설정
    CurrentGravityDirection = FVector(0, 0, -1)
    PrintToConsole("[GravitySystem] Initialization complete")
end

-- ────────────────────────────────────────────────────────────────────────────
-- 회전 상태 확인
-- ────────────────────────────────────────────────────────────────────────────
function IsCurrentlyRotating()
    return IsRotating
end

-- ────────────────────────────────────────────────────────────────────────────
-- Export (다른 Lua 스크립트에서 require 가능)
-- ────────────────────────────────────────────────────────────────────────────
return {
    Initialize = Initialize,
    OnWallCollision = OnWallCollision,
    IsCurrentlyRotating = IsCurrentlyRotating
}
