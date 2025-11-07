CollisionUtility = {};
CollisionUtility.OBSTACLE_OBJ_FILE_PATH = "Data/Model/backup.obj";
CollisionUtility.OBSTACLE_OBJBIN_FILE_PATH = "DerivedDataCache/Model/backup.obj.bin";

function CollisionUtility.IsObstacleActor(Actor)

    -- Actor를 StaticMeshActor로 캐스팅 (파라미터 이름 수정!)
    local StaticMeshActor = _G.CastToStaticMeshActor(Actor);
    if (StaticMeshActor == nil) then
        return false;
    end

    -- StaticMeshComponent 가져오기
    local StaticMeshComponent = StaticMeshActor:GetStaticMeshComponent();
    if (StaticMeshComponent == nil) then
        return false;
    end

    -- StaticMesh 가져오기
    local StaticMesh = StaticMeshComponent:GetStaticMesh();
    if (StaticMesh == nil) then
        return false;
    end

    -- 경로 비교 (모듈 멤버로 접근!)
    local FilePath = StaticMesh:GetCacheFilePath();

    if (FilePath == CollisionUtility.OBSTACLE_OBJ_FILE_PATH or
        FilePath == CollisionUtility.OBSTACLE_OBJBIN_FILE_PATH) then
        return true;
    end

    return false;
end

return CollisionUtility;