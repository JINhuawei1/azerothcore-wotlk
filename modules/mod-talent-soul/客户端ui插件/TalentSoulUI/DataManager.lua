-- 天赋之魂UI数据管理模块

local TalentSoulData = {}

-- 本地数据缓存
local skillCache = {}
local playerData = {
    totalPoints = 0,      -- 总天赋点（等于玩家等级）
    usedPoints = 0,       -- 已使用天赋点
    classId = nil,        -- 玩家职业ID
}
local selectedSkill = nil
local pendingRequests = {}

-- 调试输出
local function PrintDebug(msg)
    if TalentSoulConfig and TalentSoulConfig.DebugMode then
        DEFAULT_CHAT_FRAME:AddMessage("|cff9933FF[天赋数据]|r " .. tostring(msg))
    end
end

-- 获取玩家职业
function TalentSoulData:GetPlayerClass()
    if not playerData.classId then
        local _, class = UnitClass("player")
        -- 转换为职业ID
        local classMap = {
            WARRIOR = 1,
            PALADIN = 2,
            HUNTER = 3,
            ROGUE = 4,
            PRIEST = 5,
            DEATHKNIGHT = 6,
            SHAMAN = 7,
            MAGE = 8,
            WARLOCK = 9,
            DRUID = 11,
        }
        playerData.classId = classMap[class] or 1
    end
    return playerData.classId
end

-- 获取玩家等级（即总天赋点）
function TalentSoulData:GetPlayerLevel()
    return UnitLevel("player")
end

-- 获取总天赋点
function TalentSoulData:GetTotalPoints()
    return playerData.totalPoints or self:GetPlayerLevel()
end

-- 获取已使用天赋点
function TalentSoulData:GetUsedPoints()
    return playerData.usedPoints or 0
end

-- 获取可用天赋点
function TalentSoulData:GetAvailablePoints()
    return self:GetTotalPoints() - self:GetUsedPoints()
end

-- 设置天赋点数据
function TalentSoulData:SetTalentPoints(total, used)
    playerData.totalPoints = total or self:GetPlayerLevel()
    playerData.usedPoints = used or 0
    PrintDebug(string.format("天赋点更新: 总计=%d 已用=%d 可用=%d", playerData.totalPoints, playerData.usedPoints, self:GetAvailablePoints()))
end

-- 清空技能缓存
function TalentSoulData:InvalidateSkillCache()
    skillCache = {}
    PrintDebug("已清理技能缓存")
end

-- 更新单个技能等级
function TalentSoulData:UpdateSkillLevel(spellId, upgradeType, newLevel)
    for _, skill in ipairs(skillCache) do
        if skill.spellId == spellId then
            if upgradeType == 1 then
                skill.gcdLevel = newLevel
            elseif upgradeType == 2 then
                skill.cooldownLevel = newLevel
            elseif upgradeType == 3 then
                skill.costLevel = newLevel
            elseif upgradeType == 4 then
                skill.damageLevel = newLevel
            end
            PrintDebug(string.format("技能等级更新: spellId=%d type=%d level=%d", spellId, upgradeType, newLevel))
            return skill
        end
    end
    return nil
end

-- 增加已用天赋点
function TalentSoulData:AddUsedPoint(count)
    count = count or 1
    playerData.usedPoints = (playerData.usedPoints or 0) + count
    PrintDebug(string.format("天赋点消耗: +%d，剩余=%d", count, self:GetAvailablePoints()))
end

-- 重置已用天赋点
function TalentSoulData:ResetUsedPoints()
    local returned = playerData.usedPoints or 0
    playerData.usedPoints = 0
    PrintDebug(string.format("天赋点重置: 返还=%d", returned))
    return returned
end

-- 请求技能数据
-- classId: 职业ID，nil表示玩家当前职业，0表示全部职业
local function requestSkillData(callback, classId)
    local requestKey = "skills_" .. tostring(classId or "default")

    if pendingRequests[requestKey] then
        if callback then
            table.insert(pendingRequests[requestKey], callback)
        end
        PrintDebug("技能请求已在进行中: " .. requestKey)
        return
    end

    pendingRequests[requestKey] = {}
    if callback then
        table.insert(pendingRequests[requestKey], callback)
    end

    PrintDebug(string.format("发送技能数据请求: classId=%s", tostring(classId)))

    if type(TalentSoulComm) == "table" and type(TalentSoulComm.QuerySkills) == "function" then
        TalentSoulComm:QuerySkills(function(success, result, params)
            local callbacks = pendingRequests[requestKey] or {}
            pendingRequests[requestKey] = nil

            if success and result and result.data then
                -- 更新天赋点信息
                TalentSoulData:SetTalentPoints(result.totalPoints, result.usedPoints)

                -- 解析技能数据
                local list = {}
                for _, entry in ipairs(result.data) do
                    local spellName, _, spellIcon = GetSpellInfo(entry.spellId)
                    local icon = spellIcon or "Interface\\Icons\\INV_Misc_QuestionMark"
                    local name = spellName or ("技能" .. entry.spellId)

                    table.insert(list, {
                        spellId = entry.spellId,
                        name = name,
                        icon = icon,
                        description = entry.description,
                        classId = entry.classId or 0,  -- 职业ID，0表示全职业
                        gcdLevel = entry.gcdLevel or 0,
                        cooldownLevel = entry.cooldownLevel or 0,
                        costLevel = entry.costLevel or 0,
                        damageLevel = entry.damageLevel or 0,
                        gcdMax = entry.gcdMax or 10,
                        cooldownMax = entry.cooldownMax or 10,
                        costMax = entry.costMax or 10,
                        damageMax = entry.damageMax or 10,
                        requiredPoints = entry.requiredPoints or 0,
                    })
                end

                skillCache = list
                PrintDebug(string.format("技能数据加载完成: 共%d个技能", #list))

                for _, cb in ipairs(callbacks) do
                    if cb then
                        cb(true, list)
                    end
                end
            else
                PrintDebug("技能数据加载失败")
                for _, cb in ipairs(callbacks) do
                    if cb then
                        cb(false, {})
                    end
                end
            end
        end, nil, classId)
    else
        PrintDebug("通信模块不可用")
        local callbacks = pendingRequests[requestKey] or {}
        pendingRequests[requestKey] = nil
        for _, cb in ipairs(callbacks) do
            if cb then
                cb(false, {})
            end
        end
    end
end

-- 获取技能列表（从缓存）
function TalentSoulData:GetSkillList()
    return skillCache or {}
end

-- 确保技能数据已加载
-- classId: 职业ID，nil表示玩家当前职业，0表示全部职业
function TalentSoulData:EnsureSkillData(callback, classId)
    -- 如果指定了classId，总是重新请求数据
    if classId ~= nil then
        requestSkillData(callback, classId)
        return false
    end

    if #skillCache > 0 then
        if callback then
            callback(true, skillCache)
        end
        return true
    end

    requestSkillData(callback, nil)
    return false
end

-- 刷新数据
-- classId: 职业ID，nil表示玩家当前职业，0表示全部职业
function TalentSoulData:Refresh(callback, classId)
    self:InvalidateSkillCache()
    requestSkillData(callback, classId)
end

-- 设置选中的技能
function TalentSoulData:SetSelectedSkill(skill)
    selectedSkill = skill
    PrintDebug("选中技能: " .. (skill and skill.name or "无"))
end

-- 获取选中的技能
function TalentSoulData:GetSelectedSkill()
    return selectedSkill
end

-- 获取技能总等级（所有属性等级之和）
function TalentSoulData:GetSkillTotalLevel(skill)
    if not skill then return 0 end
    return (skill.gcdLevel or 0) + (skill.cooldownLevel or 0) + (skill.costLevel or 0) + (skill.damageLevel or 0)
end

-- 检查技能是否已学习（至少有一个属性已升级）
function TalentSoulData:IsSkillLearned(skill)
    return self:GetSkillTotalLevel(skill) > 0
end

-- 获取技能特定属性等级
function TalentSoulData:GetSkillAttributeLevel(skill, attributeType)
    if not skill then return 0 end
    if attributeType == 1 then return skill.gcdLevel or 0
    elseif attributeType == 2 then return skill.cooldownLevel or 0
    elseif attributeType == 3 then return skill.costLevel or 0
    elseif attributeType == 4 then return skill.damageLevel or 0
    end
    return 0
end

-- 获取技能特定属性最大等级
function TalentSoulData:GetSkillAttributeMax(skill, attributeType)
    if not skill then return 10 end
    if attributeType == 1 then return skill.gcdMax or 10
    elseif attributeType == 2 then return skill.cooldownMax or 10
    elseif attributeType == 3 then return skill.costMax or 10
    elseif attributeType == 4 then return skill.damageMax or 10
    end
    return 10
end

-- 检查是否可以升级某个属性
function TalentSoulData:CanUpgradeAttribute(skill, attributeType)
    if not skill then return false, "未选择技能" end

    -- 检查天赋点
    if self:GetAvailablePoints() <= 0 then
        return false, "天赋点不足"
    end

    -- 检查技能是否首次学习需要天赋点
    if not self:IsSkillLearned(skill) and skill.requiredPoints > 0 then
        if self:GetUsedPoints() < skill.requiredPoints then
            return false, string.format("需要 %d 天赋点才能学习", skill.requiredPoints)
        end
    end

    -- 检查等级上限
    local currentLevel = self:GetSkillAttributeLevel(skill, attributeType)
    local maxLevel = self:GetSkillAttributeMax(skill, attributeType)
    if currentLevel >= maxLevel then
        return false, "已达到最高等级"
    end

    return true, nil
end

-- 获取职业配置
function TalentSoulData:GetClassConfig(classId)
    classId = classId or self:GetPlayerClass()
    return TalentSoulConfig.Classes[classId] or TalentSoulConfig.Classes[0]
end

_G.TalentSoulData = TalentSoulData
