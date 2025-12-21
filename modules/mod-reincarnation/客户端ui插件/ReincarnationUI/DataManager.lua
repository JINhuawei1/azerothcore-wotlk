-- 转身系统UI数据管理模块

local ReincarnationData = {}

-- 本地数据缓存
local playerData = {
    reincarnationLevel = 0,   -- 转身等级
    bonusStats = 0,           -- 全属性加成百分比
    bonusTalent = 0,          -- 额外天赋点
    playerLevel = 0,          -- 角色等级
    nextBonusStats = 0,       -- 下一转属性加成
    nextBonusTalent = 0,      -- 下一转天赋点
}
local dataLoaded = false
local pendingRequest = false

-- 调试输出
local function PrintDebug(msg)
    if ReincarnationConfig and ReincarnationConfig.DebugMode then
        DEFAULT_CHAT_FRAME:AddMessage("|cff9966FF[转身数据]|r " .. tostring(msg))
    end
end

-- 获取转身等级
function ReincarnationData:GetReincarnationLevel()
    return playerData.reincarnationLevel or 0
end

-- 获取全属性加成
function ReincarnationData:GetBonusStats()
    return playerData.bonusStats or 0
end

-- 获取额外天赋点
function ReincarnationData:GetBonusTalent()
    return playerData.bonusTalent or 0
end

-- 获取玩家等级
function ReincarnationData:GetPlayerLevel()
    -- 始终从游戏API获取实时等级
    return UnitLevel("player")
end

-- 获取下一转属性加成
function ReincarnationData:GetNextLevelBonus()
    if playerData.nextBonusStats and playerData.nextBonusStats > 0 then
        return playerData.nextBonusStats
    end
    -- 默认每级增加5%全属性
    local currentLevel = self:GetReincarnationLevel()
    return 5
end

-- 获取下一转天赋点
function ReincarnationData:GetNextLevelTalent()
    if playerData.nextBonusTalent and playerData.nextBonusTalent > 0 then
        return playerData.nextBonusTalent
    end
    -- 默认每级增加10点天赋
    return 10
end

-- 检查是否可以转身
function ReincarnationData:CanReincarnate()
    local level = UnitLevel("player")
    return level >= 80
end

-- 设置转身数据
function ReincarnationData:SetReincarnationData(level, stats, talent, pLevel, nextStats, nextTalent)
    playerData.reincarnationLevel = level or 0
    playerData.bonusStats = stats or 0
    playerData.bonusTalent = talent or 0
    playerData.playerLevel = pLevel or UnitLevel("player")
    playerData.nextBonusStats = nextStats or 5
    playerData.nextBonusTalent = nextTalent or 10
    dataLoaded = true
    PrintDebug(string.format("转身数据更新: 等级=%d 属性加成=%.1f%% 天赋点=%d 下一转: +%.1f%% +%d点",
        playerData.reincarnationLevel, playerData.bonusStats, playerData.bonusTalent,
        playerData.nextBonusStats, playerData.nextBonusTalent))
end

-- 清空数据缓存
function ReincarnationData:InvalidateCache()
    dataLoaded = false
    PrintDebug("已清理数据缓存")
end

-- 数据是否已加载
function ReincarnationData:IsDataLoaded()
    return dataLoaded
end

-- 请求转身数据
local function requestReincarnationData(callback)
    if pendingRequest then
        PrintDebug("请求已在进行中")
        return
    end

    pendingRequest = true
    PrintDebug("发送转身数据请求")

    if type(ReincarnationComm) == "table" and type(ReincarnationComm.QueryInfo) == "function" then
        ReincarnationComm:QueryInfo(function(success, result, params)
            pendingRequest = false

            if success and result then
                ReincarnationData:SetReincarnationData(
                    result.reincarnationLevel,
                    result.bonusStats,
                    result.bonusTalent,
                    result.playerLevel,
                    result.nextBonusStats,
                    result.nextBonusTalent
                )

                if callback then
                    callback(true, playerData)
                end
            else
                PrintDebug("转身数据加载失败")
                if callback then
                    callback(false, nil)
                end
            end
        end)
    else
        pendingRequest = false
        PrintDebug("通信模块不可用")
        if callback then
            callback(false, nil)
        end
    end
end

-- 确保数据已加载
function ReincarnationData:EnsureData(callback)
    if dataLoaded then
        if callback then
            callback(true, playerData)
        end
        return true
    end

    requestReincarnationData(callback)
    return false
end

-- 刷新数据
function ReincarnationData:Refresh(callback)
    self:InvalidateCache()
    requestReincarnationData(callback)
end

-- 执行转身
function ReincarnationData:DoReincarnate(callback)
    if not self:CanReincarnate() then
        if callback then
            callback(false, { message = ReincarnationConfig.Messages.NotEnoughLevel })
        end
        return
    end

    PrintDebug("发送转身请求")

    if type(ReincarnationComm) == "table" and type(ReincarnationComm.DoReincarnate) == "function" then
        ReincarnationComm:DoReincarnate(function(success, result, params)
            if success and result and result.success then
                -- 更新本地数据
                self:SetReincarnationData(
                    result.reincarnationLevel,
                    result.bonusStats,
                    result.bonusTalent
                )

                if callback then
                    callback(true, result)
                end
            else
                if callback then
                    callback(false, result)
                end
            end
        end)
    else
        PrintDebug("通信模块不可用")
        if callback then
            callback(false, { message = "通信模块不可用" })
        end
    end
end

-- 获取属性配置列表
function ReincarnationData:GetAttributeList()
    return ReincarnationConfig.Attributes or {}
end

_G.ReincarnationData = ReincarnationData
