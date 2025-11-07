local _ENV = ...

-- BeginPlay: Actor가 생성되거나 레벨이 시작될 때 호출
function BeginPlay()
    -- PrintToConsole("[coin_collision Begin Play] ");
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

    if actorName == "DefaultActor" then
        -- PrintToConsole("[coin_collision] Coin collected!")
        GameMode:OnCoinCollected(1)
        PrintToConsole("In lua [coin_collision] Coin collected!")
        if not World then
            PrintToConsole("In lua [coin_collision] ERROR: World is nil!")
            return
        end
        local dtManager = World:GetDeltaTimeManager()
        dtManager:ApplySlomoEffect(10.0, 0.05)
        -- Slomo (2초간 30% 속도)
        -- dtManager:ApplyHitStop(2.0)
        -- MyActor:SetActorHiddenInGame(true)
        -- MyActor:DestroyAllComponents()
        MyActor:Destroy()
    end
end

-- Tick: 매 프레임마다 호출 (dt: 델타 타임)
function Tick(dt)
    -- PrintToConsole("[coin_collision] Tick] ");
end

-- 부활 작업
function Restart() end
