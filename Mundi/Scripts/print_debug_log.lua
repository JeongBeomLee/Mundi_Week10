local _ENV = ...

-- BeginPlay: Actor가 생성되거나 레벨이 시작될 때 호출
function BeginPlay()
    PrintToConsole("[Begin Play] ");
end

-- EndPlay: Actor가 제거되거나 레벨이 종료될 때 호출
function EndPlay()
    PrintToConsole("[End Play] ");
end

-- OnOverlap: 다른 Actor와 충돌했을 때 호출
function OnOverlap(
    OverlappedComponent,
    OtherActor,
    OtherComp,
    ContactPoint,
    PenetrationDepth
)
    PrintToConsole("[OnOverlap] ");
end

-- Tick: 매 프레임마다 호출 (dt: 델타 타임)
function Tick(dt)
    if MyActor == nil then
        PrintToConsole("MyActor is nil!")
        return
    end
    PrintToConsole("[MyActor's Name] ".. MyActor:GetName():ToString())
end

-- 부활 작업
function Restart() end
