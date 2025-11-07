local _ENV = ...
-- RunnerCharacter.lua
-- Runner 게임 캐릭터 제어 (Lua 중심 설계)
-- 키 바인딩: A/D (좌우 이동), Space (점프)

-- ════════════════════════════════════════════════════════════════════════════
-- 외부 모듈
-- ════════════════════════════════════════════════════════════════════════════

local GravitySystem = require("GravitySystem");
local CameraUtility = require("CameraUtility");
local CollisionUtility = require("CollisionUtility");

-- ════════════════════════════════════════════════════════════════════════════
-- 설정 (에디터에서 조정 가능)
-- ════════════════════════════════════════════════════════════════════════════

local Config = {
    -- 이동 설정
    MaxWalkSpeed = 15.0,        -- 최대 이동 속도 (cm/s) - 실제 속도 결정!
    AutoForwardSpeed = 1.0,     -- 자동 전진 입력 크기 (보통 1.0 유지)
    StrafeSpeed = 1.0,          -- 좌우 이동 입력 크기 (보통 1.0 유지)
    bAutoForward = true,        -- 자동 전진 활성화

    -- 점프 및 중력 설정
    JumpZVelocity = 35.0,      -- 점프 초기 속도 (cm/s, 위로)
    GravityScale = 0.1,         -- 중력 스케일 (1.0 = 기본 중력)
    AirControl = 0.3,           -- 공중 제어력 (0.0 ~ 1.0)

    -- 입력 설정
    HorizontalInput = 0.0,      -- 현재 좌우 입력 (-1.0 ~ 1.0)
    LeftInput = 0.0,            -- A 키 입력
    RightInput = 0.0,           -- D 키 입력

    -- 프로젝타일 설정
    ProjectileSpeed = 800.0,    -- 발사체 속도 (cm/s)
    ProjectileSpawnOffset = FVector(0, 0, 0), -- 발사 위치 오프셋 (캐릭터 위치에서 시작)
    ProjectileGravityScale = -0.5, -- 발사체 중력 스케일
    ProjectileLifespan = 5.0,   -- 발사체 생명 시간 (초)

    -- SpringArm 카메라 설정
    -- TargetArmLength: 카메라와 캐릭터 사이의 거리 (값이 클수록 카메라가 멀어짐)
    TargetArmLength = 5.0,

    -- SocketOffset: 스프링암이 시작하는 위치의 오프셋 (캐릭터 중심 기준)
    --   X: 전진 방향 (양수 = 앞, 음수 = 뒤)
    --   Y: 우측 방향 (양수 = 오른쪽, 음수 = 왼쪽)
    --   Z: 위쪽 방향 (양수 = 위, 음수 = 아래)
    SocketOffset = FVector(0, 0, 0),

    -- TargetOffset: 카메라가 바라보는 중심점의 오프셋 (캐릭터 중심 기준)
    --   X: 전진 방향 (양수 = 앞쪽을 더 바라봄)
    --   Y: 우측 방향 (양수 = 오른쪽을 더 바라봄)
    --   Z: 위쪽 방향 (양수 = 위쪽을 더 바라봄)
    TargetOffset = FVector(0, 0, 3),

    -- EnableCameraLag: 카메라 지연 활성화 (true면 부드럽게 따라감)
    EnableCameraLag = true,

    -- CameraLagSpeed: 카메라 지연 속도 (값이 클수록 빠르게 따라감, 0~1 사이 권장)
    CameraLagSpeed = 1.0,

    -- 디버그
    bDebugLog = true,

    Health = 3,                -- 캐릭터 체력
}

function GetHealth()
    return Config.Health
end

function DecreaseHealth(amount)
    Config.Health = Config.Health - amount
    if Config.Health < 0 then
        Config.Health = 0
    end
    PrintToConsole("[RunnerCharacter] Health decreased by " .. amount .. ". Current Health: " .. Config.Health)
end

-- ════════════════════════════════════════════════════════════════════════════
-- 내부 변수
-- ════════════════════════════════════════════════════════════════════════════

local InputComponent = nil
local CharacterMovement = nil

-- ════════════════════════════════════════════════════════════════════════════
-- 생명주기 함수
-- ════════════════════════════════════════════════════════════════════════════

function BeginPlay()
    -- CameraUtility 초기화
    CameraUtility.Initialize(GlobalObjectManager.GetPIEWorld());
    
    -- 캐릭터 메시 변경 및 숨김 처리
    if MyActor.GetStaticMesh then
        local StaticMeshComponent = MyActor:GetStaticMesh()
        if StaticMeshComponent then
            -- 원하는 메시 파일 경로로 변경
            StaticMeshComponent:SetStaticMesh("Data/runner_cube.obj")
            -- 메시를 게임에서 안보이게 설정
            StaticMeshComponent:SetHiddenInGame(false)
          --  PrintToConsole("[RunnerCharacter] Character mesh changed to smokegrenade.obj")
        end
    end

    -- ParticleComponent 설정 및 재생
    if MyActor.GetParticleComponent then
        local ParticleComp = MyActor:GetParticleComponent()
        PrintToConsole("[RunnerCharacter] ParticleComponent found: " .. tostring(ParticleComp))
        if ParticleComp then
            ParticleComp:SetSpriteSheet("Data/UI/Sprite/FireExplosion_6x6.dds", 6, 6, 15.0)
            ParticleComp:SetLooping(true)
            ParticleComp:Play()
            if Config.bDebugLog then
                PrintToConsole("[RunnerCharacter] ParticleComponent configured and playing")
            end
        end
    end

    -- CharacterMovement 컴포넌트 가져오기 및 설정 적용
    if MyActor.GetCharacterMovement then
        CharacterMovement = MyActor:GetCharacterMovement()
        if CharacterMovement then
            CharacterMovement.MaxWalkSpeed = Config.MaxWalkSpeed
            CharacterMovement.JumpZVelocity = Config.JumpZVelocity
            CharacterMovement.GravityScale = Config.GravityScale
            CharacterMovement.AirControl = Config.AirControl
        end
    end

    -- InputComponent 가져오기 및 바인딩
    if MyActor.GetInputComponent then
        InputComponent = MyActor:GetInputComponent()
        if InputComponent then
            SetupInputBindings()
        end
    end

    -- GravitySystem 초기화 (측면 충돌 시 중력 전환)
    if GravitySystem and GravitySystem.Initialize then
        GravitySystem.Initialize(MyActor)
        PrintToConsole("[RunnerCharacter] GravitySystem initialized")
    end

    -- SpringArm 설정 (카메라 위치 제어)
    if MyActor.GetSpringArm then
        local SpringArm = MyActor:GetSpringArm()
        if SpringArm then
            -- 카메라 거리 설정 (값이 클수록 카메라가 멀어짐)
            SpringArm:SetTargetArmLength(Config.TargetArmLength)

            -- 스프링암 시작 위치 오프셋 (캐릭터 중심 기준)
            -- Z 값을 조정하면 카메라가 캐릭터 위쪽/아래쪽에서 시작
            SpringArm:SetSocketOffset(Config.SocketOffset)

            -- 카메라가 바라보는 중심점 오프셋
            -- X 값을 조정하면 캐릭터 앞/뒤를 더 바라봄
            -- Z 값을 조정하면 캐릭터 위/아래를 더 바라봄
            SpringArm:SetTargetOffset(Config.TargetOffset)

            -- 카메라 지연 효과 (부드러운 따라가기)
            SpringArm:SetEnableCameraLag(Config.EnableCameraLag)
            SpringArm:SetCameraLagSpeed(Config.CameraLagSpeed)

            PrintToConsole("[RunnerCharacter] SpringArm configured:")
            PrintToConsole("  - ArmLength: " .. Config.TargetArmLength)
            PrintToConsole(string.format("  - SocketOffset: (%.1f, %.1f, %.1f)",
                Config.SocketOffset.X, Config.SocketOffset.Y, Config.SocketOffset.Z))
            PrintToConsole(string.format("  - TargetOffset: (%.1f, %.1f, %.1f)",
                Config.TargetOffset.X, Config.TargetOffset.Y, Config.TargetOffset.Z))
            PrintToConsole("  - CameraLag: " .. tostring(Config.EnableCameraLag))
            PrintToConsole("  - LagSpeed: " .. Config.CameraLagSpeed)
        end
    end

    -- -- World 참조 가져오기
    -- local dtManager = World:GetDeltaTimeManager()
    -- -- Slomo (2초간 30% 속도)
    -- dtManager:ApplySlomoEffect(10.0, 0.05)
    -- PrintToConsole("[RunnerCharacter] Slomo effect applied")
    SoundManager:PlayBGM("Data/Sounds/InfinityRunner/Run2BGM.wav");

    -- StartCoroutine(function() CameraIntroSequence() end)
end

-- function CameraIntroSequence()
--     PrintToConsole("[Camera Intro] Starting cinematic sequence...")
    
--     -- 시네마틱 느낌을 위한 레터박스 추가 (선택사항)
--     -- CameraUtility.AddLetterBox(0.1, 0.8, 1.5)  -- FadeSize=0.1, Opacity=0.8, FadeInTime=1.5초
--     -- PrintToConsole("[Camera Intro] Letter box added")
    
--     -- -- 카메라 페이드 인 효과
--     -- CameraUtility.StartCameraFade(1.0, 0.0, 2.0)  -- 완전 불투명 -> 투명 (2초)
--     -- PrintToConsole("[Camera Intro] Fade in started")
    
--     -- 7초 동안 카메라가 궤도를 그리며 플레이어에게 접근
--     -- (실제 궤도 이동은 SpringArm 설정이나 별도 로직으로 구현)
--     coroutine.yield(7.0)
    
--     -- 7초 시점: 첫 번째 카메라 셰이크 (중간 강도)
--     PrintToConsole("[Camera Intro] First shake at 7s")
--     CameraUtility.AddCameraShake(10.0, 2.0, 6)
--     -- RotationAmplitude=10도, Duration=2초, NumSamples=6
    
--     -- 추가 7초 대기 (총 14초)
--     coroutine.yield(7.0)
    
--     -- 14초 시점: 더 큰 카메라 셰이크
--     PrintToConsole("[Camera Intro] Bigger shake at 14s")
--     CameraUtility.AddCameraShake(15.0, 3.0, 8)
--     -- RotationAmplitude=15도, Duration=3초, NumSamples=8
    
--     -- 이후 14초마다 큰 셰이크 반복
--     while true do
--         coroutine.yield(14.0)
--         PrintToConsole("[Camera Intro] Repeating shake")
--         CameraUtility.AddCameraShake(15.0, 3.0, 8)
        
--         -- 비활성화된 모디파이어 정리 (메모리 관리)
   --         CameraUtility.RemoveDisabledCameraShakes()
--     end
-- end

function Tick(deltaTime)
    -- 카메라 업데이트는 PlayerController::Tick에서 자동으로 수행됨
    -- (중복 호출 방지를 위해 주석 처리)
    -- CameraUtility.UpdateCamera(deltaTime);

    -- 모든 비활성 Camera Modifier 제거
    --CameraUtility.RemoveDisabledCameraModifiers();
    
    -- 회전 중에는 모든 입력 무시
    if GravitySystem and GravitySystem.IsCurrentlyRotating and GravitySystem.IsCurrentlyRotating() then
        return
    end

    -- 자동 전진 처리 (Lua에서!)
    if Config.bAutoForward then
        -- GameMode:OnCoinCollected(3)
        ProcessAutoForward(deltaTime)
        -- PrintToConsole("[RunnerCharacter] bAutoForward in lua Tick")
    end

    -- 좌우 이동 처리
    if Config.HorizontalInput ~= 0.0 then
        ProcessHorizontalMovement(deltaTime)
    end
end

function EndPlay()
    -- 스크립트 종료 시 정리 작업
end

-- 부활 작업
function Restart()
    Config.Health = 3
    MyActor:SetLocation(FVector(0.0, 0.0, 3.0));
end

-- ════════════════════════════════════════════════════════════════════════════
-- 입력 바인딩
-- ════════════════════════════════════════════════════════════════════════════

function SetupInputBindings()
    if not InputComponent then
        return
    end

    -- Axis 바인딩: 좌우 방향키 (좌우 이동)
    -- VK_LEFT = 0x25 (37), VK_RIGHT = 0x27 (39)
    if InputComponent.BindAxis then
        InputComponent:BindAxis("MoveRight", 0x27, 1.0, OnMoveRight)
        InputComponent:BindAxis("MoveLeft", 0x25, 1.0, OnMoveLeft)
    end

    -- Action 바인딩: Space 키 (점프)
    if InputComponent.BindAction then
        InputComponent:BindAction("Jump", 0x20, OnJumpPressed, OnJumpReleased)
    end

    -- Action 바인딩: C 키 (프로젝타일 던지기)
    if InputComponent.BindAction then
        InputComponent:BindAction("ThrowProjectile", string.byte('C'), OnThrowProjectile, nil)
    end

    -- Action 바인딩: V 키 (슬로우모션)
    if InputComponent.BindAction then
        InputComponent:BindAction("ToggleSlomo", string.byte('V'), OnSlomoPressed, OnSlomoReleased)
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 입력 콜백
-- ════════════════════════════════════════════════════════════════════════════

local slomoEffectId = nil

function OnSlomoPressed()
    SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/Slomo.mp3", false, SoundChannelType.SFX);
    local dtManager = World:GetDeltaTimeManager()
    CameraUtility.AddLetterBox(0.2, 1.0, 1.0, true)  -- Height, Duration
    slomoEffectId = dtManager:ApplySlomoEffect(1000.0, 0.2)
    PrintToConsole("[RunnerCharacter] Slomo effect applied")
end

function OnSlomoReleased()
    local dtManager = World:GetDeltaTimeManager()
    dtManager:CancelEffect(slomoEffectId)
    slomoEffectId = nil
    CameraUtility.AddLetterBox(0.2, 1.0, 1.0, false)  -- 1초간 원래대로 페이드
    PrintToConsole("[RunnerCharacter] Slomo effect removed")
end

function OnMoveLeft(value)
    
    Config.LeftInput = value
    Config.HorizontalInput = Config.RightInput - Config.LeftInput
end

function OnMoveRight(value)
    Config.RightInput = value
    Config.HorizontalInput = Config.RightInput - Config.LeftInput
end

function OnJumpPressed()
    if MyActor.Jump then
        SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/JumpFx.mp3", false, SoundChannelType.SFX);
        MyActor:Jump()
    end
end

function OnJumpReleased()
    if MyActor.StopJumping then
        MyActor:StopJumping()
    end
end

function OnThrowProjectile()
  PrintToConsole("[RunnerCharacter] function OnThrowProjectile()!")
    SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/LaserShotFx.mp3", false, SoundChannelType.SFX);
    ThrowProjectile()
end

-- ════════════════════════════════════════════════════════════════════════════
-- 프로젝타일 발사
-- ════════════════════════════════════════════════════════════════════════════

function ThrowProjectile()
    -- World가 없으면 리턴
    if not World then
        PrintToConsole("[RunnerCharacter] ERROR: World is nil!")
        return
    end

    -- 캐릭터 위치 가져오기
    local actorLocation = MyActor:GetLocation()

    -- 캐릭터가 바라보는 방향으로 발사 (전진 방향)
    local fireDirection = MyActor:GetForwardDirection()

    -- 발사 위치 계산 (캐릭터 위치 + 오프셋)
    local offset = Config.ProjectileSpawnOffset
    local spawnLocation = FVector(
        actorLocation.X + offset.X,
        actorLocation.Y + offset.Y,
        actorLocation.Z + offset.Z
    )

    -- Transform 생성
    local spawnTransform = FTransform(spawnLocation, FQuat(), FVector(1, 1, 1))

    -- ProjectileActor 생성
    local projectile = World:SpawnProjectileActor(spawnTransform)
    if not projectile then
        PrintToConsole("[RunnerCharacter] ERROR: Failed to spawn projectile!")
        return
    end

    -- 발사체에 캐릭터의 중력 방향 적용
    if MyActor.GetGravityDirection then
        local gravityDirection = MyActor:GetGravityDirection()
        local projectileMovement = projectile:GetProjectileMovement()
        if projectileMovement then
            projectileMovement:SetGravityDirection(gravityDirection)
            if Config.bDebugLog then
                PrintToConsole(string.format("[RunnerCharacter] Set projectile gravity direction: (%.2f, %.2f, %.2f)",
                    gravityDirection.X, gravityDirection.Y, gravityDirection.Z))
            end
        end
    end

    -- 카메라 방향으로 발사
    projectile:FireInDirection(fireDirection, Config.ProjectileSpeed)

    if Config.bDebugLog then
        PrintToConsole(string.format("[RunnerCharacter] Projectile thrown! Dir: (%.2f, %.2f, %.2f)",
            fireDirection.X, fireDirection.Y, fireDirection.Z))
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 이동 로직 (Lua에서 구현!)
-- ════════════════════════════════════════════════════════════════════════════

function ProcessAutoForward(deltaTime)
    if not MyActor.MoveForward then
        return
    end

    MyActor:MoveForward(Config.AutoForwardSpeed)
end

function ProcessHorizontalMovement(deltaTime)
    -- 입력값이 0이면 조용히 리턴 (키를 뗐을 때)
    if Config.HorizontalInput == 0.0 then
        return
    end

    if not MyActor.GetRightDirection or not MyActor.AddMovementInput then
        return
    end

    -- 우측 방향 가져오기 (C++에서 중력 방향 고려해서 계산)
    local rightDir = MyActor:GetRightDirection()
    if not rightDir then
        return
    end

    -- 방향 벡터 생성 (AddInputVector가 방향을 정규화하기 때문에 입력값만 곱함)
    local direction = FVector(
        rightDir.X * Config.HorizontalInput,
        rightDir.Y * Config.HorizontalInput,
        rightDir.Z * Config.HorizontalInput
    )

    -- AddMovementInput 호출: (방향 벡터, 입력 크기)
    -- 중요: AddInputVector가 방향을 정규화하므로, 속도는 ScaleValue로 전달해야 함!
    MyActor:AddMovementInput(direction, Config.StrafeSpeed)
end

-- ════════════════════════════════════════════════════════════════════════════
-- 유틸리티 / 설정 함수
-- ════════════════════════════════════════════════════════════════════════════

-- 자동 전진 토글
function ToggleAutoForward()
    Config.bAutoForward = not Config.bAutoForward
    PrintToConsole("[RunnerCharacter] Auto Forward: " .. tostring(Config.bAutoForward))
end

-- 자동 전진 속도 변경
function SetAutoForwardSpeed(speed)
    Config.AutoForwardSpeed = speed
    PrintToConsole("[RunnerCharacter] Auto Forward Speed: " .. speed .. " cm/s")
end

-- 좌우 이동 속도 변경
function SetStrafeSpeed(speed)
    Config.StrafeSpeed = speed
    PrintToConsole("[RunnerCharacter] Strafe Speed: " .. speed .. " cm/s")
end

-- 중력 방향 변경 (4방향 벽면 전환용)
function SetGravityDirection(x, y, z)
    if MyActor.SetGravityDirection then
        MyActor:SetGravityDirection(x, y, z)
        PrintToConsole(string.format("[RunnerCharacter] Gravity → (%.2f, %.2f, %.2f)", x, y, z))
    end
end

-- 현재 상태 출력 (디버깅용)
function PrintStatus()
    PrintToConsole("═══════════════════════════════════════")
    PrintToConsole("[RunnerCharacter] Status:")
    PrintToConsole("  Auto Forward: " .. tostring(Config.bAutoForward))
    PrintToConsole("  Forward Speed: " .. Config.AutoForwardSpeed)
    PrintToConsole("  Strafe Speed: " .. Config.StrafeSpeed)
    PrintToConsole("  Horizontal Input: " .. Config.HorizontalInput)

    if MyActor.GetVelocity then
        local vel = MyActor:GetVelocity()
        PrintToConsole(string.format("  Velocity: (%.1f, %.1f, %.1f)", vel.X, vel.Y, vel.Z))
    end

    if MyActor.IsGrounded then
        PrintToConsole("  Grounded: " .. tostring(MyActor:IsGroundched()))
    end

    PrintToConsole("═══════════════════════════════════════")
end

-- ════════════════════════════════════════════════════════════════════════════
-- SpringArm 카메라 제어 함수
-- ════════════════════════════════════════════════════════════════════════════

-- 카메라 거리 설정 (값이 클수록 카메라가 멀어짐)
function SetCameraDistance(distance)
    if MyActor.GetSpringArm then
        local SpringArm = MyActor:GetSpringArm()
        if SpringArm then
            Config.TargetArmLength = distance
            SpringArm:SetTargetArmLength(distance)
            PrintToConsole("[RunnerCharacter] Camera distance set to: " .. distance)
        end
    end
end

-- 카메라 높이 오프셋 설정 (양수 = 위쪽, 음수 = 아래쪽)
function SetCameraHeight(height)
    if MyActor.GetSpringArm then
        local SpringArm = MyActor:GetSpringArm()
        if SpringArm then
            Config.SocketOffset = FVector(Config.SocketOffset.X, Config.SocketOffset.Y, height)
            SpringArm:SetSocketOffset(Config.SocketOffset)
            PrintToConsole("[RunnerCharacter] Camera height set to: " .. height)
        end
    end
end

-- 카메라가 바라보는 높이 오프셋 설정 (양수 = 위쪽을 더 바라봄)
function SetCameraTargetHeight(height)
    if MyActor.GetSpringArm then
        local SpringArm = MyActor:GetSpringArm()
        if SpringArm then
            Config.TargetOffset = FVector(Config.TargetOffset.X, Config.TargetOffset.Y, height)
            SpringArm:SetTargetOffset(Config.TargetOffset)
            PrintToConsole("[RunnerCharacter] Camera target height set to: " .. height)
        end
    end
end

-- 카메라 지연 속도 설정 (값이 클수록 빠르게 따라감)
function SetCameraLagSpeed(speed)
    if MyActor.GetSpringArm then
        local SpringArm = MyActor:GetSpringArm()
        if SpringArm then
            Config.CameraLagSpeed = speed
            SpringArm:SetCameraLagSpeed(speed)
            PrintToConsole("[RunnerCharacter] Camera lag speed set to: " .. speed)
        end
    end
end

-- 카메라 지연 활성화/비활성화
function ToggleCameraLag()
    if MyActor.GetSpringArm then
        local SpringArm = MyActor:GetSpringArm()
        if SpringArm then
            Config.EnableCameraLag = not Config.EnableCameraLag
            SpringArm:SetEnableCameraLag(Config.EnableCameraLag)
            PrintToConsole("[RunnerCharacter] Camera lag: " .. tostring(Config.EnableCameraLag))
        end
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 충돌 처리
-- ════════════════════════════════════════════════════════════════════════════

function OnOverlap(OverlappedComponent, OtherActor, OtherComp, ContactPoint, PenetrationDepth)
    --PrintToConsole("[RunnerCharacter] Overlapped: " .. OtherActor:GetName():ToString())
    --if Config.bDebugLog then
    --    PrintToConsole("[RunnerCharacter] Overlapped: " .. OtherActor:GetName():ToString())
    --end

    PrintToConsole("[RunnerCharacter] OnOverlap called with actor: ")
    -- TODO: 벽면 감지 및 중력 방향 전환 로직
    -- 예: OtherActor의 태그를 확인해서 "WallTrigger"면 중력 방향 변경
    if not CollisionUtility.IsObstacleActor(OtherActor) then
        --PrintToConsole("[ObstacleGenerator] Not an obstacle actor, returning");
        return;
    end

    PrintToConsole("[RunnerCharacter] OnOverlap Obstacle detected, processing player hit");

    -- 플레이어 사망 처리
    -- PrintToConsole("[ObstacleGenerator] Player hit an obstacle! Processing death sound...");
    -- StartCoroutine(function()
    --     SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/FailedSoundFx.mp3", false, SoundChannelType.SFX);
    --     coroutine.yield(1.5);
    --     SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/GameOverSoundFx.mp3", false, SoundChannelType.SFX);
    -- end);

    SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/PlayerHitFX.mp3", false, SoundChannelType.SFX);
    PrintToConsole("[RunnerCharacter] OnOverlap Sound played for player hit");
    DecreaseHealth(1);
    GetRunnerGameMode(GlobalObjectManager.GetPIEWorld()):OnDecreasePlayerHealth(MyActor, Config.Health);
    CameraUtility.AddVignetting(
            FVector4(1.0, 0.0, 0.0, 1.0),
            1.0,
            0.4,
            3.0
    );
    local dtManager = World:GetDeltaTimeManager()
    dtManager:ApplyHitStop(0.2);

    if Config.Health <= 0 then
        GetRunnerGameMode(GlobalObjectManager.GetPIEWorld()):OnPlayerDeath(MyActor);

        -- 카메라 셰이크 추가 (기본 파라미터 사용)
        CameraUtility.AddCameraShake();

        -- 화면 암전 효과 (5초에 걸쳐 검은색으로 FadeOut)
        -- FromAlpha: 1.0 (완전 투명, 원본 씬 보임)
        -- ToAlpha: 0.0 (완전 불투명, 검은색만 보임)
        -- Duration: 5.0초
        -- FadeColor: 검은색 (기본값)
        CameraUtility.StartCameraFade(1.0, 0.0, 5.0);
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 스크립트 로드 완료
-- ════════════════════════════════════════════════════════════════════════════

PrintToConsole("[RunnerCharacter] ✓ Lua script loaded successfully!")

--[[
════════════════════════════════════════════════════════════════════════════
SpringArm 카메라 설정 가이드
════════════════════════════════════════════════════════════════════════════

1. TargetArmLength (카메라 거리)
   - 값이 클수록: 카메라가 캐릭터에서 멀어짐 (넓은 시야)
   - 값이 작을수록: 카메라가 캐릭터에 가까워짐 (좁은 시야)
   - 권장 범위: 300 ~ 1000
   - 예: SetCameraDistance(600)

2. SocketOffset (스프링암 시작 위치)
   - X축: 전진 방향
     * 양수 = 캐릭터 앞쪽에서 시작 (카메라가 캐릭터 뒤를 따라감)
     * 음수 = 캐릭터 뒤쪽에서 시작 (카메라가 캐릭터 앞을 비춤)

   - Y축: 우측 방향
     * 양수 = 오른쪽에서 시작
     * 음수 = 왼쪽에서 시작

   - Z축: 위쪽 방향
     * 양수 = 캐릭터 위쪽에서 시작 (높은 각도)
     * 음수 = 캐릭터 아래쪽에서 시작 (낮은 각도)

   - 예: SetCameraHeight(50) -- 캐릭터 위 50cm에서 시작

3. TargetOffset (카메라가 바라보는 중심점)
   - X축: 전진 방향
     * 양수 = 캐릭터 앞쪽을 더 바라봄
     * 음수 = 캐릭터 뒤쪽을 더 바라봄

   - Y축: 우측 방향
     * 양수 = 캐릭터 오른쪽을 더 바라봄
     * 음수 = 캐릭터 왼쪽을 더 바라봄

   - Z축: 위쪽 방향
     * 양수 = 캐릭터 위쪽을 더 바라봄 (하늘을 더 보임)
     * 음수 = 캐릭터 아래쪽을 더 바라봄 (땅을 더 보임)

   - 예: SetCameraTargetHeight(50) -- 캐릭터 위 50cm 지점을 바라봄

4. EnableCameraLag (카메라 지연 효과)
   - true: 카메라가 부드럽게 따라감 (영화같은 느낌)
   - false: 카메라가 즉시 따라감 (반응이 빠름)
   - 예: ToggleCameraLag()

5. CameraLagSpeed (카메라 지연 속도)
   - 값이 클수록: 카메라가 빠르게 따라감
   - 값이 작을수록: 카메라가 느리게 따라감
   - 권장 범위: 1.0 ~ 10.0
   - 예: SetCameraLagSpeed(5.0)

════════════════════════════════════════════════════════════════════════════
사용 예제
════════════════════════════════════════════════════════════════════════════

-- 기본 3인칭 뷰 (캐릭터 뒤에서 약간 위를 바라봄)
SetCameraDistance(600)
SetCameraHeight(50)
SetCameraTargetHeight(0)

-- 탑뷰 느낌 (위에서 내려다봄)
SetCameraDistance(800)
SetCameraHeight(100)
SetCameraTargetHeight(-50)

-- 액션 영화 느낌 (낮은 각도, 캐릭터 뒤에서)
SetCameraDistance(400)
SetCameraHeight(20)
SetCameraTargetHeight(30)

-- 부드러운 카메라 움직임
SetCameraLagSpeed(3.0)  -- 느리게
ToggleCameraLag()       -- 활성화/비활성화

-- 빠른 카메라 움직임
SetCameraLagSpeed(10.0)  -- 빠르게

════════════════════════════════════════════════════════════════════════════
]]
