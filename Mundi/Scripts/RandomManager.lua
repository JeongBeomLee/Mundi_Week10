-- Global
RandomManager = {};

function RandomManager.SetNewRandomSeed()
    local Seed = math.floor(os.clock() * 1000);  -- 정수로 변환
    math.randomseed(Seed);
end

return RandomManager;