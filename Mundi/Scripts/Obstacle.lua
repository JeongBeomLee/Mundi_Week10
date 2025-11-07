local _ENV = ...

local CameraUtility = require("CameraUtility");

local Health = 5;

local BILLBOARD_COUNT = Health;  -- 빌보드 컴포넌트 개수
local BillboardComponents = {};  -- 빌보드 컴포넌트를 저장할 배열

local Cnt = 0;

-- BeginPlay: Actor가 생성되거나 레벨이 시작될 때 호출
function BeginPlay()

    -- PrintToConsole("[Obstacle] Begin Play, " .. Cnt);
    Cnt = Cnt + 1;
    -- PrintToConsole("[coin_collision Begin Play] ");
    -- 빌보드 컴포넌트를 여러 개 생성
    local centerY = -25.0
    local spacing = 18.0

    -- 빌보드 개수가 홀수/짝수일 때 중앙 정렬 계산
    local startOffset = -(BILLBOARD_COUNT - 1) * spacing / 2

    -- PrintToConsole("[Obstacle] Creaate BillBoard, " .. Cnt);
    for j = 1, BILLBOARD_COUNT do
        local yOffset = startOffset + (j - 1) * spacing
        local BillboardComponent = MyActor:CreateBillboardComponent("BillboardComponent" .. j);
        BillboardComponent:SetTextureName("Data/Icon/heart.png");
        BillboardComponent:SetRelativeLocation(FVector(-26.0, centerY + yOffset, 4.0));
        -- BillboardComponent:SetActive(false);
        
        -- 배열에 빌보드 컴포넌트 추가
        BillboardComponents[j] = BillboardComponent;
        
        -- PrintToConsole("[ObstacleGenerator] Obstacle BillboardComponent " .. j .. " created at Y: " .. (centerY + yOffset));
    end
end

-- EndPlay: Actor가 제거되거나 레벨이 종료될 때 호출
function EndPlay()
    -- PrintToConsole("[coin_collision] End Play] ");
end

-- OnOverlap: 다른 Actor와 충돌했을 때 호출
function OnOverlap(
    OverlappedComponent,
    OtherActor,
    OtherComp,
    ContactPoint,
    PenetrationDepth
)
    local actorName = OtherActor:GetName():ToString()
    -- PrintToConsole("[coin_collision] Collided with: " .. actorName)

    if actorName == "Projectile Actor" then
        SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/ObstacleHitFx.mp3", false, SoundChannelType.SFX);

        -- Health가 줄기 전에 현재 Health에 해당하는 빌보드를 숨김
        if Health > 0 and BillboardComponents[Health] then
            for j = 1, Health do
                BillboardComponents[Health]:SetActive(false);
            end
            -- PrintToConsole("[Obstacle] BillboardComponent found, calling SetActive")
            -- BillboardComponents[Health]:SetActive(false);
            -- PrintToConsole("[Obstacle] Billboard " .. Health .. " hidden")
        else
            -- PrintToConsole("[Obstacle] BillboardComponent at Health index is nil!")
        end
        
        Health = Health - 1
        
        if Health <= 0 then
            -- PrintToConsole("[coin_collision] Coin collected!")
            SoundManager:PlaySound2D("Data/Sounds/InfinityRunner/GetScoreFx.mp3", false, SoundChannelType.SFX);
            -- _G.GetRunnerGameMode(GlobalObjectManager.GetPIEWorld()):OnCoinCollected(1);
            CameraUtility.AddScoreAndShowGoldVignetting()
            MyActor:Destroy()
        else
            -- PrintToConsole("[coin_collision] Health remaining: " .. Health)
        end
    end
end

-- Tick: 매 프레임마다 호출 (dt: 델타 타임)
function Tick(dt)
    -- PrintToConsole("[coin_collision] Tick] ");
end

-- 부활 작업
function Restart() end
