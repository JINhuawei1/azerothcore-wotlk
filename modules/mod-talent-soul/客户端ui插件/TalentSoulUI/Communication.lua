-- 天赋之魂UI通信模块

TalentSoulComm = TalentSoulComm or {}

-- 本地变量
local pendingRequests = {}
local requestId = 0

-- 请求类型常量
local REQUEST_TYPE = {
    SKILL_LIST = "SKILL_LIST",
    SKILL_UPGRADE = "SKILL_UPGRADE",
    RESET_TALENT = "RESET_TALENT",
    TALENT_POINTS = "TALENT_POINTS",
}

-- 分块消息缓存
local chunkBuffer = {
    chunks = {},      -- 存储接收到的分块
    totalChunks = 0,  -- 总块数
    received = 0,     -- 已接收块数
}

local ADDON_PREFIX = (TalentSoulConfig and TalentSoulConfig.Communication and TalentSoulConfig.Communication.AddonPrefix) or "TALENTSOUL"
local USE_ADDON = not (TalentSoulConfig and TalentSoulConfig.Communication and TalentSoulConfig.Communication.UseAddon == false)

-- 注册 Addon 前缀
if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
end

-- 调试输出
local function PrintDebug(msg)
    if TalentSoulConfig and TalentSoulConfig.DebugMode then
        DEFAULT_CHAT_FRAME:AddMessage("|cff9933FF[天赋之魂通信]|r " .. tostring(msg))
    end
end

-- 错误输出
local function PrintError(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cffFF0000[天赋之魂]|r " .. tostring(msg))
end

-- 成功输出
local function PrintSuccess(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff00FF00[天赋之魂]|r " .. tostring(msg))
end

-- 信息输出
local function PrintInfo(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff9933FF[天赋之魂]|r " .. tostring(msg))
end

-- 生成请求ID
local function GenerateRequestId()
    requestId = requestId + 1
    return requestId
end

-- 发送服务器命令
local function ExecuteCommandThroughEditBox(command)
    local editBox = ChatEdit_GetActiveWindow()
    if editBox then
        local currentText = editBox:GetText()
        local cursorPos = editBox:GetCursorPosition()
        ChatEdit_OnEscapePressed(editBox)
        editBox._ts_backupText = currentText
        editBox._ts_backupCursor = cursorPos
        editBox._ts_restore = true
    end

    editBox = ChatEdit_ChooseBoxForSend()
    if not editBox then
        PrintError("未找到可用聊天编辑框")
        return false
    end

    PrintDebug("编辑框可用，准备执行命令")
    ChatEdit_ActivateChat(editBox)
    editBox:SetText(command)
    ChatEdit_SendText(editBox, 0)
    ChatEdit_OnEscapePressed(editBox)

    -- 恢复之前输入的文本（若存在）
    if editBox._ts_restore then
        PrintDebug("恢复玩家原始输入内容")
        ChatEdit_ActivateChat(editBox)
        editBox:SetText(editBox._ts_backupText or "")
        if editBox._ts_backupCursor then
            editBox:SetCursorPosition(editBox._ts_backupCursor)
        end
        editBox._ts_backupText = nil
        editBox._ts_backupCursor = nil
        editBox._ts_restore = nil
    end

    return true
end

-- 通过 Addon 通道发送请求
local function SendAddonRequest(message, callback, params, requestType)
    local reqId = GenerateRequestId()

    if callback then
        pendingRequests[reqId] = {
            callback = callback,
            time = GetTime(),
            params = params,
            requestType = requestType,  -- 记录请求类型
        }
    end

    PrintDebug(string.format("发送Addon请求[#%d][%s]: %s", reqId, requestType or "UNKNOWN", message))

    local playerName = UnitName("player")
    local sent = false

    if C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, message, "WHISPER", playerName)
        sent = true
    elseif SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, message, "WHISPER", playerName)
        sent = true
    end

    if not sent then
        PrintError("SendAddonMessage 不可用，将回退到聊天命令")
    end

    return reqId, sent
end

local function SendCommand(command, callback, params, requestType)
    local reqId = GenerateRequestId()

    if callback then
        pendingRequests[reqId] = {
            callback = callback,
            time = GetTime(),
            params = params,
            requestType = requestType,  -- 记录请求类型
        }
    end

    PrintDebug(string.format("发送命令[#%d][%s]: %s", reqId, requestType or "UNKNOWN", command))
    if not ExecuteCommandThroughEditBox(command) then
        PrintError("编辑框执行失败，将通过 SendChatMessage 发送")
        local playerName = UnitName("player")
        PrintDebug(string.format("SendChatMessage -> WHISPER %s", tostring(playerName)))
        SendChatMessage(command, "WHISPER", nil, playerName)
    else
        PrintDebug("命令已通过编辑框发送")
    end

    return reqId
end

-- 处理服务器响应
local function HandleResponse(message)
    -- 处理分块消息
    -- 格式: CHUNK:当前块:总块数:数据
    local chunkNum, totalChunks, chunkData = nil, nil, nil
    if type(message) == "string" then
        chunkNum, totalChunks, chunkData = message:match("^CHUNK:(%d+):(%d+):(.*)$")
    end
    if chunkNum and totalChunks and chunkData then
        chunkNum = tonumber(chunkNum)
        totalChunks = tonumber(totalChunks)

        PrintDebug(string.format("收到分块消息: %d/%d", chunkNum, totalChunks))

        -- 初始化或重置缓存
        if chunkNum == 1 then
            chunkBuffer.chunks = {}
            chunkBuffer.totalChunks = totalChunks
            chunkBuffer.received = 0
        end

        -- 存储分块
        if chunkBuffer.chunks and chunkNum then
            chunkBuffer.chunks[chunkNum] = chunkData
        end
        chunkBuffer.received = chunkBuffer.received + 1

        -- 检查是否收到所有分块
        if chunkBuffer.received >= chunkBuffer.totalChunks then
            -- 合并所有分块
            local fullMessage = ""
            for i = 1, chunkBuffer.totalChunks do
                if chunkBuffer.chunks[i] then
                    fullMessage = fullMessage .. chunkBuffer.chunks[i]
                end
            end

            PrintDebug("所有分块已接收，合并消息长度: " .. #fullMessage)

            -- 清空缓存
            chunkBuffer.chunks = {}
            chunkBuffer.totalChunks = 0
            chunkBuffer.received = 0

            -- 递归处理合并后的完整消息
            HandleResponse(fullMessage)
        end
        return
    end

    -- 处理技能列表响应
    -- 新格式: TALENTSOUL_SKILLS:请求的职业:玩家职业:总天赋点:已用天赋点:技能数据列表
    local payload = message and message:match("^TALENTSOUL_SKILLS:(.*)$") or nil
    if payload then
        PrintDebug("识别为技能列表响应，开始解析")

        -- 先解析响应中的职业ID
        local requestedClass, playerClass, totalPoints, usedPoints, list = string.match(payload, "^(%d+):(%d+):(%d+):(%d+):(.*)$")
        local responseClassId = tonumber(requestedClass)

        -- 收集需要处理的请求
        local matchedRequests = {}
        local unmatchedRequests = {}

        for reqId, request in pairs(pendingRequests) do
            if request.requestType == REQUEST_TYPE.SKILL_LIST then
                local reqClassId = request.params and request.params.requestedClassId
                -- 匹配条件：请求职业ID与响应职业ID相同，或者请求没有指定职业ID
                if reqClassId == nil or reqClassId == responseClassId then
                    table.insert(matchedRequests, {reqId = reqId, request = request})
                else
                    table.insert(unmatchedRequests, {reqId = reqId, request = request})
                end
            end
        end

        -- 处理匹配的请求
        for _, item in ipairs(matchedRequests) do
            local reqId = item.reqId
            local request = item.request
            if request.callback then
                local result = {
                    success = true,
                    message = message,
                    data = {}
                }

                result.requestedClass = responseClassId
                result.playerClass = tonumber(playerClass)
                result.totalPoints = tonumber(totalPoints)
                result.usedPoints = tonumber(usedPoints)

                if list and #list > 0 then
                    for entry in string.gmatch(list, "[^;]+") do
                        local spellId, skillClassId, gcdLevel, cdLevel, costLevel, damageLevel, gcdMax, cdMax, costMax, damageMax, reqPoints, desc =
                            string.match(entry, "^(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(.*)$")
                        if spellId then
                            table.insert(result.data, {
                                spellId = tonumber(spellId),
                                classId = tonumber(skillClassId),
                                gcdLevel = tonumber(gcdLevel),
                                cooldownLevel = tonumber(cdLevel),
                                costLevel = tonumber(costLevel),
                                damageLevel = tonumber(damageLevel),
                                gcdMax = tonumber(gcdMax),
                                cooldownMax = tonumber(cdMax),
                                costMax = tonumber(costMax),
                                damageMax = tonumber(damageMax),
                                requiredPoints = tonumber(reqPoints),
                                description = desc,
                            })
                        end
                    end
                end

                request.callback(true, result, request.params)
                PrintDebug(string.format("请求[#%d] 已完成，职业=%d，回调已执行", reqId, responseClassId or -1))
            end
            pendingRequests[reqId] = nil
        end

        -- 清理不匹配的旧请求（它们的响应可能已经丢失或者被新请求取代）
        for _, item in ipairs(unmatchedRequests) do
            PrintDebug(string.format("请求[#%d] 职业不匹配已清理: 请求=%s, 响应=%d",
                item.reqId, tostring(item.request.params and item.request.params.requestedClassId), responseClassId or -1))
            pendingRequests[item.reqId] = nil
        end

        return
    end

    -- 处理升级成功响应
    -- 格式: TALENTSOUL_UPGRADE:技能ID:类型:新等级:剩余天赋点
    local upSpell, upType, upLevel, remainPoints = nil, nil, nil, nil
    if type(message) == "string" then
        upSpell, upType, upLevel, remainPoints = message:match("^TALENTSOUL_UPGRADE:(%d+):(%d+):(%d+):(%d+)$")
    end
    if upSpell then
        for reqId, request in pairs(pendingRequests) do
            -- 只处理 SKILL_UPGRADE 类型的请求
            if request.requestType == REQUEST_TYPE.SKILL_UPGRADE and request.callback then
                request.callback(true, {
                    upgrade = true,
                    spellId = tonumber(upSpell),
                    upgradeType = tonumber(upType),
                    level = tonumber(upLevel),
                    remainPoints = tonumber(remainPoints),
                    message = message
                }, request.params)
            end
            pendingRequests[reqId] = nil
        end
        return
    end

    -- 处理升级失败的结构化响应
    -- 格式: TALENTSOUL_UPGRADE_FAIL:技能ID:类型:原因
    local failSpell, failType, failReason = nil, nil, nil
    if type(message) == "string" then
        failSpell, failType, failReason = message:match("^TALENTSOUL_UPGRADE_FAIL:(%d+):(%d+):(.+)$")
    end
    if failSpell then
        PrintDebug("收到升级失败响应: spellId=" .. failSpell .. " type=" .. failType .. " reason=" .. tostring(failReason))
        for reqId, request in pairs(pendingRequests) do
            -- 只处理 SKILL_UPGRADE 类型的请求
            if request.requestType == REQUEST_TYPE.SKILL_UPGRADE and request.callback then
                local reasonText = failReason or "UNKNOWN"
                -- 转换失败原因为用户友好的文本
                local reasonMap = {
                    ["MAX_LEVEL"] = "已达到最高等级",
                    ["NOT_ENOUGH_POINTS"] = "天赋点不足",
                    ["CONFIG_NOT_FOUND"] = "未找到配置",
                    ["SYSTEM_ERROR"] = "系统错误",
                    ["REQUIRE_TALENT_POINTS"] = "需要更多天赋点才能学习",
                    ["WRONG_CLASS"] = "职业不匹配",
                }
                local friendlyReason = reasonMap[reasonText] or reasonText
                request.callback(false, {
                    upgrade = false,
                    spellId = tonumber(failSpell),
                    upgradeType = tonumber(failType),
                    reason = reasonText,
                    message = friendlyReason
                }, request.params)
            end
            pendingRequests[reqId] = nil
        end
        return
    end

    -- 处理重置成功响应
    -- 格式: TALENTSOUL_RESET:返还天赋点数
    local resetPoints = nil
    if type(message) == "string" then
        resetPoints = message:match("^TALENTSOUL_RESET:(%d+)$")
    end
    if resetPoints then
        for reqId, request in pairs(pendingRequests) do
            -- 只处理 RESET_TALENT 类型的请求
            if request.requestType == REQUEST_TYPE.RESET_TALENT and request.callback then
                request.callback(true, {
                    reset = true,
                    returnedPoints = tonumber(resetPoints),
                    message = string.format(TalentSoulConfig.Messages.ResetSuccess, tonumber(resetPoints))
                }, request.params)
            end
            pendingRequests[reqId] = nil
        end
        return
    end

    -- 处理重置失败响应
    -- 格式: TALENTSOUL_RESET_FAIL:原因
    local resetFailReason = nil
    if type(message) == "string" then
        resetFailReason = message:match("^TALENTSOUL_RESET_FAIL:(.+)$")
    end
    if resetFailReason then
        for reqId, request in pairs(pendingRequests) do
            -- 只处理 RESET_TALENT 类型的请求
            if request.requestType == REQUEST_TYPE.RESET_TALENT and request.callback then
                request.callback(false, {
                    reset = false,
                    reason = resetFailReason,
                    message = TalentSoulConfig.Messages.ResetFailed
                }, request.params)
            end
            pendingRequests[reqId] = nil
        end
        return
    end

    -- 处理天赋点查询响应
    -- 格式: TALENTSOUL_POINTS:总天赋点:已用天赋点:可用天赋点
    local ptTotal, ptUsed, ptAvailable = nil, nil, nil
    if type(message) == "string" then
        ptTotal, ptUsed, ptAvailable = message:match("^TALENTSOUL_POINTS:(%d+):(%d+):(%d+)$")
    end
    if ptTotal then
        PrintDebug("收到天赋点查询响应: total=" .. ptTotal .. " used=" .. ptUsed .. " available=" .. ptAvailable)
        for reqId, request in pairs(pendingRequests) do
            -- 只处理 TALENT_POINTS 类型的请求
            if request.requestType == REQUEST_TYPE.TALENT_POINTS and request.callback then
                request.callback(true, {
                    points = true,
                    totalPoints = tonumber(ptTotal),
                    usedPoints = tonumber(ptUsed),
                    availablePoints = tonumber(ptAvailable),
                    message = message
                }, request.params)
            end
            pendingRequests[reqId] = nil
        end
        return
    end

    -- 若是"[天赋之魂] ..."的普通系统提示，记录日志后直接返回
    if type(message) == "string" and message:match("^%[天赋之魂%]%s") then
        PrintDebug("系统提示（等待结构化响应）: " .. tostring(message))
        return
    end

    -- 其他非结构化且与天赋之魂相关的提示
    if string.find(tostring(message), "天赋") then
        PrintDebug("相关提示（未触发回调）: " .. tostring(message))
        return
    end
end

-- 查询玩家技能数据
-- classId: 职业ID，nil/不传表示玩家当前职业，0表示全部职业
function TalentSoulComm:QuerySkills(callback, params, classId)
    -- 将classId保存到params中，用于响应时验证
    local requestParams = params or {}
    requestParams.requestedClassId = classId

    if USE_ADDON then
        local payload
        if classId ~= nil then
            payload = string.format("SKILL_LIST:%d", classId)
        else
            payload = "SKILL_LIST"
        end
        local _, sent = SendAddonRequest(payload, callback, requestParams, REQUEST_TYPE.SKILL_LIST)
        if not sent then
            local command = string.format("%s 查看", TalentSoulConfig.Communication.CommandPrefix)
            SendCommand(command, callback, requestParams, REQUEST_TYPE.SKILL_LIST)
        end
    else
        local command = string.format("%s 查看", TalentSoulConfig.Communication.CommandPrefix)
        SendCommand(command, callback, requestParams, REQUEST_TYPE.SKILL_LIST)
    end
end

-- 升级技能
-- upgradeType: 1=GCD, 2=冷却, 3=消耗, 4=伤害
function TalentSoulComm:UpgradeSkill(spellId, upgradeType, callback, params)
    local sid = tonumber(spellId)
    local utype = tonumber(upgradeType)
    if not sid or not utype then
        if callback then callback(false, { message = "无效的技能ID或升级类型" }, params) end
        return
    end

    if USE_ADDON then
        local payload = string.format("SKILL_UPGRADE:%d:%d", sid, utype)
        local _, sent = SendAddonRequest(payload, callback, params, REQUEST_TYPE.SKILL_UPGRADE)
        if not sent then
            local typeNames = { "公共cd", "冷却", "消耗", "伤害" }
            local command = string.format("%s 升级 %s %d", TalentSoulConfig.Communication.CommandPrefix, typeNames[utype] or "公共cd", sid)
            SendCommand(command, callback, params, REQUEST_TYPE.SKILL_UPGRADE)
        end
    else
        local typeNames = { "公共cd", "冷却", "消耗", "伤害" }
        local command = string.format("%s 升级 %s %d", TalentSoulConfig.Communication.CommandPrefix, typeNames[utype] or "公共cd", sid)
        SendCommand(command, callback, params, REQUEST_TYPE.SKILL_UPGRADE)
    end
end

-- 重置天赋
function TalentSoulComm:ResetTalent(callback, params)
    if USE_ADDON then
        local payload = "RESET_TALENT"
        local _, sent = SendAddonRequest(payload, callback, params, REQUEST_TYPE.RESET_TALENT)
        if not sent then
            local command = string.format("%s 重置", TalentSoulConfig.Communication.CommandPrefix)
            SendCommand(command, callback, params, REQUEST_TYPE.RESET_TALENT)
        end
    else
        local command = string.format("%s 重置", TalentSoulConfig.Communication.CommandPrefix)
        SendCommand(command, callback, params, REQUEST_TYPE.RESET_TALENT)
    end
end

-- 查询天赋点信息
function TalentSoulComm:QueryTalentPoints(callback, params)
    if USE_ADDON then
        local payload = "TALENT_POINTS"
        local _, sent = SendAddonRequest(payload, callback, params, REQUEST_TYPE.TALENT_POINTS)
        if not sent then
            local command = string.format("%s 天赋点", TalentSoulConfig.Communication.CommandPrefix)
            SendCommand(command, callback, params, REQUEST_TYPE.TALENT_POINTS)
        end
    else
        local command = string.format("%s 天赋点", TalentSoulConfig.Communication.CommandPrefix)
        SendCommand(command, callback, params, REQUEST_TYPE.TALENT_POINTS)
    end
end

-- 事件处理
local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_SYSTEM")
frame:RegisterEvent("CHAT_MSG_WHISPER")
frame:RegisterEvent("CHAT_MSG_ADDON")

frame:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_SYSTEM" or event == "CHAT_MSG_WHISPER" then
        local message = ...
        HandleResponse(message)
    elseif event == "CHAT_MSG_ADDON" then
        local prefix, message, channel, sender = ...
        if prefix == ADDON_PREFIX and sender == UnitName("player") then
            if type(message) == "string" then
                -- 忽略我们自己发出的请求
                if message:match("^SKILL_LIST") or message:match("^SKILL_UPGRADE:") or message:match("^RESET_TALENT") or message:match("^TALENT_POINTS") then
                    return
                end
                HandleResponse(message)
            end
        end
    end
end)

-- 超时检查
local function CheckTimeout()
    local currentTime = GetTime()
    for reqId, request in pairs(pendingRequests) do
        if currentTime - request.time > TalentSoulConfig.Communication.Timeout then
            if request.callback then
                request.callback(false, { message = TalentSoulConfig.Messages.Timeout })
            end
            pendingRequests[reqId] = nil
        end
    end
end

-- 定时器
local _tsTicker = CreateFrame("Frame")
local _tsElapsed = 0
_tsTicker:SetScript("OnUpdate", function(self, elapsed)
    _tsElapsed = _tsElapsed + elapsed
    if _tsElapsed >= 1 then
        _tsElapsed = 0
        CheckTimeout()
    end
end)

_G.TalentSoulComm = TalentSoulComm
PrintDebug("通信模块初始化完成")
