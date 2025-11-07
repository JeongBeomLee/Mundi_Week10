-- Global
Queue = {}

function Queue.new()
    return {first = 0, last = -1, items = {}}
end

function Queue.push(queue, value)
    queue.last = queue.last + 1
    queue.items[queue.last] = value
end

function Queue.pop(queue)
    if queue.first > queue.last then
        return nil  -- 비어있음
    end

    local value = queue.items[queue.first]
    queue.items[queue.first] = nil
    queue.first = queue.first + 1
    return value
end

function Queue.head(queue)
    if queue.first > queue.last then
        return nil  -- 비어있음
    end
    return queue.items[queue.first];
end

function Queue.isEmpty(queue)
    return queue.first > queue.last
end

-- 모듈 반환 (require가 이 값을 받음)
return Queue