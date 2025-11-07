local _ENV = ...

local Queue = require("Queue");
local GlobalObjectManager = require("GlobalObjectManager");
local RandomManager = require("RandomManager");
local CameraUtility = require("CameraUtility");

local DEFAULT_HORIZONTAL_SPAWN_RANGE = 10 * 2;  -- 맵 기준 한 변의 블록 개수 * 블록 크기
local DEFAULT_VERTICAL_SPAWN_RANGE = 10 * 2;
local DEFAULT_SCALE = 0.05;

local DEFAULT_SPAWN_DELAY_MIN = 3.0;
local DEFAULT_SPAWN_DELAY_MAX = 5.0;

local DEFAULT_POOL_SIZE = 30;

local HorizontalSpawnRange = DEFAULT_HORIZONTAL_SPAWN_RANGE;
local VerticalSpawnRange = DEFAULT_VERTICAL_SPAWN_RANGE;
local Scale = DEFAULT_SCALE;

local SpawnDelayMin = DEFAULT_SPAWN_DELAY_MIN;
local SpawnDelayMax = DEFAULT_SPAWN_DELAY_MAX;

local CoinsSpawned = Queue.new();

local PoolSize = DEFAULT_POOL_SIZE;
local CoinPool = Queue.new();

-- 충돌 컴포넌트가 포함되어 있으면 다른 물체와 OnOverlap될 수 있으므로
-- Pool마다 STORAGE_POSITION을 다르게 해야 함.
local STORAGE_POSITION = FTransform();
STORAGE_POSITION.Translation = FVector(-1000.0, -200.0, 0.0);
STORAGE_POSITION.Scale3D = FVector(Scale, Scale, Scale);
STORAGE_POSITION.Rotation = FQuat.MakeFromEuler(0, 90, 0);

local function IsCoinActor(Actor)
    if (Actor == nil) then
        return false;
    end

    -- Actor 이름으로 코인인지 확인
    local actorName = Actor:GetName():ToString();
    if (actorName == "Coin") then
        return true;
    end

    return false;
end

local function RemoveCoinFromSpawned(CoinToRemove)
    -- CoinsSpawned에서 특정 코인을 제거
    local tempQueue = Queue.new();
    local found = false;

    while not Queue.isEmpty(CoinsSpawned) do
        local coin = Queue.pop(CoinsSpawned);
        if coin == CoinToRemove then
            found = true;
            -- 이 코인은 tempQueue에 넣지 않음 (제거)
        else
            Queue.push(tempQueue, coin);
        end
    end

    -- tempQueue의 내용을 다시 CoinsSpawned로 이동
    while not Queue.isEmpty(tempQueue) do
        Queue.push(CoinsSpawned, Queue.pop(tempQueue));
    end

    return found;
end

local function InitializePool()
    local PieWorld = GlobalObjectManager.GetPIEWorld();

    for i = 1, PoolSize do
        local Coin = PieWorld:SpawnActor(STORAGE_POSITION, "ACoinActor");
        Queue.push(CoinPool, Coin);
    end

    PrintToConsole("[CoinGenerator] Pool initialized with " .. PoolSize .. " coins");
end

local function GetCoinSpawnLocation()
    local CoinSpawnLocation = FVector(
            MyActor:GetLocation().X + (40.0 - 5.0) * 2, -- (MapSize + MapStart) * Scale
            math.random(-HorizontalSpawnRange / 2 + 2, HorizontalSpawnRange / 2 - 2.0), -- -MapWidth / 2.0 + Scale ...
            math.random(-VerticalSpawnRange / 2 + 2, VerticalSpawnRange / 2 - 2.0)
    );
    return CoinSpawnLocation;
end

local function SpawnCoin()
    local Coin = Queue.pop(CoinPool);
    if (Coin == nil) then
        PrintToConsole("[CoinGenerator] WARNING: CoinPool is empty!");
        return;
    end
    local CoinLocation = GetCoinSpawnLocation();
    Coin:SetLocation(CoinLocation);
    Coin:SetRotation(FVector(0.0, 90.0, 0.0));
    Coin:SetScale(FVector(Scale, Scale, Scale));
    Queue.push(CoinsSpawned, Coin);
end

local function SpawnNextCoin()
    while (true) do
        if IsGamePlaying(GlobalObjectManager.GetPIEWorld()) then
            SpawnCoin();
        end
        coroutine.yield(math.random(SpawnDelayMin, SpawnDelayMax));
    end
end

local function CheckCoinLocationAndWithdraw()
    local Oldest = Queue.head(CoinsSpawned);
    if (Oldest == nil) then
        return;
    end

    if (Oldest:GetLocation().X < MyActor:GetLocation().X - 5.0) then
        Oldest:SetTransform(STORAGE_POSITION);
        Queue.pop(CoinsSpawned);
        Queue.push(CoinPool, Oldest);  -- CoinPool로 반환
    end
end

-- Template functions
function BeginPlay()
    PrintToConsole("[CoinGenerator] Begin Play");
    RandomManager.SetNewRandomSeed();

    -- 풀 초기화
    InitializePool();

    -- 코루틴으로 코인 스폰 시작 (한 번만 실행)
    StartCoroutine(function()
        SpawnNextCoin();
    end);
end

function EndPlay()
    PrintToConsole("[CoinGenerator] End Play");
end

function OnOverlap(OverlappedComponent, OtherActor, OtherComp, ContactPoint, PenetrationDepth)
    if OtherActor == nil then
        return;
    end

    -- OtherActor가 코인인지 확인
    if (IsCoinActor(OtherActor)) then
        -- 점수 추가
        SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/GetScoreFx.mp3", false, SoundChannelType.SFX);
        -- _G.GetRunnerGameMode(GlobalObjectManager.GetPIEWorld()):OnCoinCollected(1);
        CameraUtility.AddScoreAndShowGoldVignetting()
        PrintToConsole("[CoinGenerator] Coin collected!");
        
        -- 슬로우 모션 효과 테스트
        -- local dtManager = World:GetDeltaTimeManager()
        -- dtManager:ApplySlomoEffect(2.0, 0.05, TimeDilationPriority.Low)
        -- dtManager:ApplyHitStop(1.0)

        -- 코인을 Pool로 회수
        OtherActor:SetTransform(STORAGE_POSITION);
        RemoveCoinFromSpawned(OtherActor);
        Queue.push(CoinPool, OtherActor);
    end
end

function Tick(dt)
    CheckCoinLocationAndWithdraw();
end

-- 부활 작업
function Restart()
    while not Queue.isEmpty(CoinsSpawned) do
        local Coin = Queue.pop(CoinsSpawned);
        if Coin ~= nil then
            Coin:SetTransform(STORAGE_POSITION);
            Queue.push(CoinPool, Coin);
        end
    end
end
