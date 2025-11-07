-- Global
GlobalObjectManager = {};

function GlobalObjectManager.GetPIEWorld()
    local PIEWorld = GEngine:GetPIEWorld();
    if PIEWorld == nil then
        error("failed to get pie world", 2);
    end
    return PIEWorld;
end

return GlobalObjectManager;