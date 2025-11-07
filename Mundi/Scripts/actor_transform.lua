local _ENV = ...
-- actor_transform.lua
-- Actor Transform 제어 예제 스크립트
-- Lua와 Delegate를 활용한 Actor World Transform 변경

-- 전역 변수
local rotationSpeed = 90.0  -- 초당 회전 각도
local moveSpeed = 50.0      -- 초당 이동 속도
local elapsed = 0.0

-- BeginPlay: Actor 초기화
function BeginPlay()
    PrintToConsole("[Lua] Actor Transform Script Initialized")
    
    -- -- 초기 위치 설정
    -- local startPos = FVector.new(0, 0, 100)
    -- actor:SetLocation(startPos)
    
    -- -- 초기 스케일 설정
    -- local startScale = FVector.new(1, 1, 1)
    -- actor:SetScale(startScale)
    
    PrintToConsole("[Lua] Initial transform set")
end

-- Tick: 매 프레임 업데이트
function Tick(deltaTime)
    -- -- 위치 변경
    -- local newLocation = FVector.new(100, 200, 300)
    -- actor:SetLocation(newLocation * deltaTime)

    -- 스케일 변경
    local newScale = FVector.new(2, 2, 2)
    MyActor:SetScale(newScale)

    -- 회전 변경
    local newRotation = FQuat.MakeFromEuler(10, 80, 20)
    MyActor:SetRotation(newRotation)

    -- 상대 위치 이동
    local deltaLocation = FVector.new(0, 10, 0)
    MyActor:AddWorldLocation(deltaLocation * deltaTime)

    -- elapsed = elapsed + deltaTime
    
    -- -- 1. 원형 궤도 운동
    -- local radius = 200
    -- local x = math.cos(elapsed) * radius
    -- local y = math.sin(elapsed) * radius
    -- local z = 100 + math.sin(elapsed * 2) * 50  -- 위아래로 진동
    
    -- local newPos = FVector.new(x, y, z)
    -- actor:SetLocation(newPos)
    
    -- -- 2. 크기 변화 (펄스 효과)
    -- local scaleFactor = 1.0 + math.sin(elapsed * 3) * 0.3
    -- local newScale = FVector.new(scaleFactor, scaleFactor, scaleFactor)
    -- actor:SetScale(newScale)
    
    -- -- 3. Transform 변경 알림 (Delegate 호출)
    -- if notifyTransformChanged then
    --     notifyTransformChanged(
    --         actor:GetLocation(),
    --         actor:GetRotation(),
    --         actor:GetScale()
    --     )
    -- end
end

-- OnOverlap: 다른 Actor와 충돌했을 때 호출
function OnOverlap(
    OverlappedComponent,
    OtherActor,
    OtherComp,
    ContactPoint,
    PenetrationDepth
)
    PrintToConsole("[Lua] OnOverlap Called");
end

-- EndPlay: 정리 작업
function EndPlay()
    PrintToConsole("[Lua] Actor Transform Script Ended")
    PrintToConsole("[Lua] Total elapsed time: " .. elapsed)
end

-- 부활 작업
function Restart() end

-- 커스텀 함수: 특정 위치로 이동
function MoveTo(x, y, z)
    local targetPos = FVector.new(x, y, z)
    MyActor:SetLocation(targetPos)
    PrintToConsole("[Lua] Moved to: " .. x .. ", " .. y .. ", " .. z)
end

-- 커스텀 함수: 크기 설정
function SetUniformScale(scale)
    local newScale = FVector.new(scale, scale, scale)
    actor:SetScale(newScale)
    MyActor("[Lua] Scale set to: " .. scale)
end

-- 커스텀 함수: 상대 이동
function MoveBy(dx, dy, dz)
    local deltaPos = FVector.new(dx, dy, dz)
    MyActor:AddWorldLocation(deltaPos)
    PrintToConsole("[Lua] Moved by: " .. dx .. ", " .. dy .. ", " .. dz)
end

PrintToConsole("[Lua] actor_transform.lua loaded successfully!")
