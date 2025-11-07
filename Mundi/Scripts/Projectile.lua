local _ENV = ...
-- Projectile.lua
-- 발사체 동작 관리 스크립트
-- ProjectileActor에 부착되어 사용됩니다

-- ════════════════════════════════════════════════════════════════════════════
-- 외부 모듈
-- ════════════════════════════════════════════════════════════════════════════

local CollisionUtility = require("CollisionUtility")
local GlobalObjectManager = require("GlobalObjectManager")

-- ════════════════════════════════════════════════════════════════════════════
-- 설정
-- ════════════════════════════════════════════════════════════════════════════

local Config = {
    -- 디버그
    bDebugLog = true,
}

-- ════════════════════════════════════════════════════════════════════════════
-- 내부 변수
-- ════════════════════════════════════════════════════════════════════════════

local ProjectileMovement = nil
local CurrentLifetime = 0.0

-- ════════════════════════════════════════════════════════════════════════════
-- 생명주기 함수
-- ════════════════════════════════════════════════════════════════════════════

function BeginPlay()
    if Config.bDebugLog then
        PrintToConsole("[Projectile] BeginPlay - Projectile spawned. Name: " .. MyActor:GetName():ToString())
    end

    -- ProjectileMovement 컴포넌트 가져오기
    if MyActor.GetProjectileMovement then
        ProjectileMovement = MyActor:GetProjectileMovement()
        if ProjectileMovement and Config.bDebugLog then
            PrintToConsole("[Projectile] ProjectileMovement component found")
        end
    end
end

function Tick(deltaTime)
    CurrentLifetime = CurrentLifetime + deltaTime

    -- TODO: 여기에 커스텀 로직 추가 가능
    -- 예: 트레일 이펙트, 사운드, 호밍 등
end

function EndPlay()
    if Config.bDebugLog then
        PrintToConsole("[Projectile] EndPlay - Projectile destroyed")
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 스프라이트 애니메이션 스폰
-- ════════════════════════════════════════════════════════════════════════════

function SpawnSpriteAnimation(location)
    PrintToConsole("[Projectile] SpawnSpriteAnimation called")

    -- SpriteActor 소환 (CoinGenerator 방식 따라하기)
    local PieWorld = GlobalObjectManager.GetPIEWorld()
    if not PieWorld then
        PrintToConsole("[Projectile] ERROR: PieWorld is nil")
        return
    end

    local spriteTransform = FTransform()
    spriteTransform.Translation = location
    spriteTransform.Rotation = FQuat.MakeFromEuler(0, 0, 0)
    spriteTransform.Scale3D = FVector(3.0, 3.0, 3.0)  -- 스프라이트 크기 3배

    local spriteActor = PieWorld:SpawnActor(spriteTransform, "ASpriteActor")
    PrintToConsole("[Projectile] SpriteActor spawned: " .. tostring(spriteActor))

    if spriteActor then
        -- ParticleComponent 가져오기
        if spriteActor.GetParticleComponent then
            PrintToConsole("[Projectile] GetParticleComponent exists")
            local particleComp = spriteActor:GetParticleComponent()
            PrintToConsole("[Projectile] ParticleComponent: " .. tostring(particleComp))

            if particleComp then
                -- 스프라이트 시트 설정 (텍스처 경로, 행, 열, 프레임레이트)
                particleComp:SetSpriteSheet("Data/UI/Sprite/FireExplosion_6x6.dds", 6, 6, 30.0)
                PrintToConsole("[Projectile] SetSpriteSheet done")
                particleComp:SetLooping(false)
                PrintToConsole("[Projectile] SetLooping done")
                particleComp:Play()
                PrintToConsole("[Projectile] Play done")

                if Config.bDebugLog then
                    PrintToConsole("[Projectile] Sprite animation spawned at collision point")
                end
            else
                PrintToConsole("[Projectile] ERROR: ParticleComponent is nil")
            end
        else
            PrintToConsole("[Projectile] ERROR: GetParticleComponent does not exist")
        end

        -- 애니메이션 재생 시간 계산: 총 프레임 수 / FPS
        -- 6x6 = 36 프레임, 30 FPS = 1.2초
        local animationDuration = (6 * 6) / 30.0

        -- 애니메이션이 끝나면 자동 삭제 (애니메이션 시간 + 여유 0.1초)
        spriteActor:SetAutoDestroy(true, animationDuration + 0.1)
        PrintToConsole("[Projectile] SetAutoDestroy done, duration: " .. tostring(animationDuration + 0.1))
    else
        PrintToConsole("[Projectile] ERROR: SpriteActor is nil")
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 충돌 처리
-- ════════════════════════════════════════════════════════════════════════════

function OnOverlap(OverlappedComponent, OtherActor, OtherComp, ContactPoint, PenetrationDepth)
    -- 자기 자신은 무시
    if OtherActor == MyActor then
        return
    end

    -- 액터 이름 가져오기
    local actorName = OtherActor:GetName():ToString()

    if Config.bDebugLog then
        PrintToConsole("[Projectile] Hit: " .. actorName)
    end

    -- CollisionUtility를 사용하여 장애물인지 확인
    if CollisionUtility.IsObstacleActor(OtherActor) then
        if Config.bDebugLog then
            PrintToConsole("[Projectile] Hit Obstacle! Destroying both obstacle and projectile...")
        end
         PrintToConsole("[Projectile] Hit Obstacle MyActor:GetActorLocation()1")
        -- 충돌 위치에 스프라이트 애니메이션 소환
        local hitLocation = MyActor:GetLocation()
        PrintToConsole("[Projectile] MyActor:GetActorLocation()2")
        SpawnSpriteAnimation(hitLocation)

        -- 장애물 파괴
       -- if OtherActor.SetActorHiddenInGame then
         --   OtherActor:SetActorHiddenInGame(true)
        --end
        --if OtherActor.Destroy then
           -- OtherActor:Destroy()
        --end

        -- 발사체도 파괴
        if MyActor.SetActorHiddenInGame then
            MyActor:SetActorHiddenInGame(true)
        end
        if MyActor.Destroy then
            MyActor:Destroy()
        end
    else
        -- 장애물이 아닌 경우 통과
        if Config.bDebugLog then
            PrintToConsole("[Projectile] Hit non-obstacle actor, passing through...")
        end
    end
end

-- 부활 작업
function Restart() end

-- ════════════════════════════════════════════════════════════════════════════
-- 유틸리티 함수
-- ════════════════════════════════════════════════════════════════════════════

-- 발사체 속성 설정 (외부에서 호출 가능)
function SetSpeed(speed)
    if ProjectileMovement then
        ProjectileMovement:SetInitialSpeed(speed)
    end
end

function SetGravity(gravity)
    if ProjectileMovement then
        ProjectileMovement:SetGravity(gravity)
    end
end

function SetGravityDirection(gravityDirection)
    if ProjectileMovement then
        ProjectileMovement:SetGravityDirection(gravityDirection)
        if Config.bDebugLog then
            PrintToConsole(string.format("[Projectile] Gravity direction set: (%.2f, %.2f, %.2f)",
                gravityDirection.X, gravityDirection.Y, gravityDirection.Z))
        end
    end
end

function SetLifespan(lifespan)
    if ProjectileMovement then
        ProjectileMovement:SetProjectileLifespan(lifespan)
    end
end

-- ════════════════════════════════════════════════════════════════════════════
-- 스크립트 로드 완료
-- ════════════════════════════════════════════════════════════════════════════

PrintToConsole("[Projectile] ✓ Lua script loaded successfully!")
