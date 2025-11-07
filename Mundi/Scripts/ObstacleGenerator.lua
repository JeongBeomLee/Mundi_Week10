local _ENV = ...

local Queue = require("Queue");
local GlobalObjectManager = require("GlobalObjectManager");
local RandomManager = require("RandomManager");
local CollisionUtility = require("CollisionUtility");
local CameraUtility = require("CameraUtility");

-- local RunnerCharacter = require("RunnerCharacter");

local DEFAULT_HORIZONTAL_SPAWN_RANGE = 10 * 2;  -- 맵 기준 한 변의 블록 개수 * 블록 크기
local DEFAULT_VERTICAL_SPAWN_RANGE = 10 * 2;
local DEFAULT_SCALE = 0.06;  -- 스케일 10분의 1로 축소 (0.4 -> 0.04)

local DEFAULT_SPAWN_DELAY_MIN = 2.0;
local DEFAULT_SPAWN_DELAY_MAX = 4.0;

local DEFAULT_POOL_SIZE = 50;

local OBSTACLE_OBJ_FILE_PATH = "Data/Model/backup.obj";

local BILLBOARD_COUNT = 5;  -- 빌보드 컴포넌트 개수

local HorizontalSpawnRange = DEFAULT_HORIZONTAL_SPAWN_RANGE;
local VerticalSpawnRange = DEFAULT_VERTICAL_SPAWN_RANGE;
local Scale = DEFAULT_SCALE;

local SpawnDelayMin = DEFAULT_SPAWN_DELAY_MIN;
local SpawnDelayMax = DEFAULT_SPAWN_DELAY_MAX;

local ObstaclesSpawned = Queue.new();

local PoolSize = DEFAULT_POOL_SIZE;
local ObstaclePool = Queue.new();

-- 충돌 컴포넌트가 포함되어 있으면 다른 물체와 OnOVerlap될 수 있으므로
-- Pool마다 STORAGE_POSITION을 다르게 해야 함.
local STORAGE_POSITION = FTransform();
STORAGE_POSITION.Translation = FVector(-5000.0, -100.0, 0.0);
STORAGE_POSITION.Scale3D = FVector(Scale, Scale, Scale);
STORAGE_POSITION.Rotation = FQuat.MakeFromEuler(0, 0, 0);

local function InitializePool()
    local PieWorld = GlobalObjectManager.GetPIEWorld();

    for i = 1, PoolSize do
        local Obstacle = PieWorld:SpawnActor(STORAGE_POSITION);
        local StaticMeshComponent = Obstacle:GetStaticMeshComponent();
        StaticMeshComponent:SetStaticMesh(OBSTACLE_OBJ_FILE_PATH);

        local CapsuleComponent = Obstacle:CreateCapsuleComponent("CollisionComponent");
        CapsuleComponent:SetCapsuleSize(35.0, 5.5, true);
        CapsuleComponent:SetRelativeLocation(FVector(-14.4, -24.0, 5.0));

        -- PrintToConsole("[ObstacleGenerator] Obstacle CapsuleComponent created.");

        -- -- 빌보드 컴포넌트를 여러 개 생성
        -- local centerY = 2.5
        -- local spacing = 2.5

        -- -- 빌보드 개수가 홀수/짝수일 때 중앙 정렬 계산
        -- local startOffset = -(BILLBOARD_COUNT - 1) * spacing / 2

        -- for j = 1, BILLBOARD_COUNT do
        --     local yOffset = startOffset + (j - 1) * spacing
        --     local BillboardComponent = Obstacle:CreateBillboardComponent("BillboardComponent" .. j);
        --     BillboardComponent:SetTextureName("Data/UI/Icons/Pawn_64x.png");
        --     BillboardComponent:SetRelativeLocation(FVector(-5.0, centerY + yOffset, 4.0));
            
        --     PrintToConsole("[ObstacleGenerator] Obstacle BillboardComponent " .. j .. " created at Y: " .. (centerY + yOffset));
        -- end

        Obstacle:AttachScript("Obstacle.lua");

        -- PrintToConsole("[ObstacleGenerator] Obstacle script attached.");

        Queue.push(ObstaclePool, Obstacle);
    end
end

local function GetObstacleSpawnLocation()
    local ObstacleSpawnLocation = FVector(
            MyActor:GetLocation().X + (40.0 - 5.0) * 2, -- (MapSize + MapStart) * Scale
            math.random(-HorizontalSpawnRange / 2 + 2, HorizontalSpawnRange / 2 - 2.0), -- -MapWidth / 2.0 + Scale ...
            math.random(-VerticalSpawnRange / 2 + 2, VerticalSpawnRange / 2 - 2.0)
    );
    return ObstacleSpawnLocation;
end

local function SpawnObstacle()
    local Obstacle = Queue.pop(ObstaclePool);
    if (Obstacle == nil) then
        PrintToConsole("[ObstacleGenerator] WARNING: ObstaclePool is empty!");
        return;
    end
    local ObstacleLocation = GetObstacleSpawnLocation();
    Obstacle:SetLocation(ObstacleLocation);
    Queue.push(ObstaclesSpawned, Obstacle);
end

local function SpawnNextObstacle()
    while (true) do
        if IsGamePlaying(GlobalObjectManager.GetPIEWorld()) then
            SpawnObstacle();
        end
        coroutine.yield(math.random(SpawnDelayMin, SpawnDelayMax));
    end
end

local function CheckObstacleLocationAndWithDraw()
    local Oldest = Queue.head(ObstaclesSpawned);
    if (Oldest == nil) then
        return;
    end

    if (Oldest:GetLocation().X < MyActor:GetLocation().X - 5.0) then
        Oldest:SetTransform(STORAGE_POSITION);
        Queue.pop(ObstaclesSpawned);
        Queue.push(ObstaclePool, Oldest);  -- ObstaclePool로 반환
    end
end

-- Template functions
function BeginPlay()
    PrintToConsole("[ObstacleGenerator] Begin Play");
    RandomManager.SetNewRandomSeed();

    -- 풀 초기화
    InitializePool();

    ---- CameraUtility 초기화
    --CameraUtility.Initialize(GlobalObjectManager.GetPIEWorld());

    -- 코루틴으로 장애물 스폰 시작 (한 번만 실행)
    StartCoroutine(function()
        SpawnNextObstacle();
    end);
end

function EndPlay()
    PrintToConsole("[ObstacleGenerator] End Play");

    -- 모든 활성 카메라 셰이크 제거
    CameraUtility.ClearAllCameraShakes();

    -- 카메라 페이드 중지 및 초기화 (화면 정상화)
    CameraUtility.StopCameraFade();
end

function OnOverlap(OverlappedComponent, OtherActor, OtherComp, ContactPoint, PenetrationDepth)
    -- PrintToConsole("[ObstacleGenerator] OnOverlap called!");

--     if not CollisionUtility.IsObstacleActor(OtherActor) then
--         --PrintToConsole("[ObstacleGenerator] Not an obstacle actor, returning");
--         return;
--     end

--     -- 플레이어 사망 처리
--     -- PrintToConsole("[ObstacleGenerator] Player hit an obstacle! Processing death sound...");
--     -- StartCoroutine(function()
--     --     SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/FailedSoundFx.mp3", false, SoundChannelType.SFX);
--     --     coroutine.yield(1.5);
--     --     SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/GameOverSoundFx.mp3", false, SoundChannelType.SFX);
--     -- end);

--     SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/PlayerHitFX.mp3", false, SoundChannelType.SFX);
--     RunnerCharacter.DecreaseHealth(1);
    
--     if RunnerCharacter.GetHealth() <= 0 then
--         GetRunnerGameMode(GlobalObjectManager.GetPIEWorld()):OnPlayerDeath(MyActor);

--         -- 카메라 셰이크 추가 (기본 파라미터 사용)
--         CameraUtility.AddCameraShake();

--         -- 레터 박스 추가
--         CameraUtility.AddLetterBox(0.15, 1.0, 1.0);  -- Height: 0.2, Duration: 0.5초

--         -- 화면 암전 효과 (5초에 걸쳐 검은색으로 FadeOut)
--         -- FromAlpha: 1.0 (완전 투명, 원본 씬 보임)
--         -- ToAlpha: 0.0 (완전 불투명, 검은색만 보임)
--         -- Duration: 5.0초
--         -- FadeColor: 검은색 (기본값)
--         CameraUtility.StartCameraFade(1.0, 0.0, 5.0);
--     end
end

function Tick(dt)
    CheckObstacleLocationAndWithDraw();

    -- 비활성화된 카메라 셰이크 제거
    CameraUtility.RemoveDisabledCameraShakes();
end

-- 부활 작업
function Restart()
    while not Queue.isEmpty(ObstaclesSpawned) do
        local Obstacle = Queue.pop(ObstaclesSpawned);
        if Obstacle ~= nil then
            Obstacle:SetTransform(STORAGE_POSITION);
            Queue.push(ObstaclePool);
        end
    end

    -- 카메라 페이드 중지 및 초기화 (화면 정상화)
    CameraUtility.StopCameraFade();
end