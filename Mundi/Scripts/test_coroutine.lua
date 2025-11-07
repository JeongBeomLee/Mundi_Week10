local _ENV = ...

-- BeginPlay: Actor가 생성되거나 레벨이 시작될 때 호출
function BeginPlay()
    PrintToConsole("[Begin Play] ");

    -- ✅ 코루틴으로 감싸서 시작
    StartCoroutine(function()
        MoveWithCo()
    end)
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
    -- if MyActor == nil then
    --     PrintToConsole("MyActor is nil!")
    --     return
    -- end
    -- PrintToConsole("[MyActor's Name] ".. MyActor:GetName():ToString())
end

-- 부활 작업
function Restart() end

function MoveWithCo()
    PrintToConsole("In lua: Coroutine Start:".. MyActor:GetName():ToString());
    -- 스케일 변경
    local newScale = FVector.new(2, 2, 2);
    MyActor:SetScale(newScale);
    -- PrintToConsole("In lua: [Coroutine Scale] ");

    -- 회전 변경
    local newRotation = FQuat.MakeFromEuler(10, 80, 20);
    MyActor:SetRotation(newRotation);
    -- PrintToConsole("In lua: [Coroutine Rotation] ");

    -- 상대 위치 이동
    local deltaLocation = FVector.new(5, 0, 0);

    MyActor:AddWorldLocation(deltaLocation);
    -- PrintToConsole("In lua: [Coroutine Move 1] ");
    coroutine.yield(0.5);
    PrintToConsole("In lua: end yield 1 ".. MyActor:GetName():ToString());

    MyActor:AddWorldLocation(deltaLocation);
    -- PrintToConsole("In lua: [Coroutine Move 2] ");
    coroutine.yield(2.0);
    PrintToConsole("In lua: end yield 2 ".. MyActor:GetName():ToString());

    MyActor:AddWorldLocation(deltaLocation);
    -- -- PrintToConsole("In lua: [Coroutine Move 3] ");
    -- local count = 0
    -- coroutine.yield(function()
    --     count = count + 1
    --     -- PrintToConsole("In lua: [Coroutine Count 4] ");
    --     return count >= 3              -- resume after 3 checks
    -- end)
    coroutine.yield(4.0);
    PrintToConsole("In lua: end yield 3 ".. MyActor:GetName():ToString());
end
