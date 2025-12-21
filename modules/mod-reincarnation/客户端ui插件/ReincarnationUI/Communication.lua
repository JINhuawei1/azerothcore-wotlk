-- 转身系统UI通信模块

ReincarnationComm = ReincarnationComm or {}

-- 本地变量
local pendingRequests = {}
local requestId = 0

-- 请求类型常量
local REQUEST_TYPE = {
    INFO = "INFO",
    REINCARNATE = "REINCARNATE",
}

local ADDON_PREFIX = (ReincarnationConfig and ReincarnationConfig.Communication and ReincarnationConfig.Communication.AddonPrefix) or "REINCARNATION"
local USE_ADDON = not (ReincarnationConfig and ReincarnationConfig.Communication and ReincarnationConfig.Communication.UseAddon == false)

-- 注册 Addon 前缀
if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
end

-- 调试输出
local function PrintDebug(msg)
    if ReincarnationConfig and ReincarnationConfig.DebugMode then
        DEFAULT_CHAT_FRAME:AddMessage("|cff9966FF[转身通信]|r " .. tostring(msg))
    end
end

-- 生成请求ID
local function GenerateRequestId()
    requestId = requestId + 1
    return requestId
end

-- 临时存储解析的信息数据
local parsedInfoData = {
    reincarnationLevel = 0,
    bonusStats = 0,
    bonusTalent = 0,
    playerLevel = 0,
    nextBonusStats = 0,
    nextBonusTalent = 0,
    messageCount = 0,
    lastUpdateTime = 0,
    inNextLevelSection = false,  -- 是否在"下一转"区域
}

-- 发送服务器命令
local function ExecuteCommandThroughEditBox(command)
    local editBox = ChatEdit_GetActiveWindow()
    if editBox then
        local currentText = editBox:GetText()
        local cursorPos = editBox:GetCursorPosition()
        ChatEdit_OnEscapePressed(editBox)
        editBox._ri_backupText = currentText
        editBox._ri_backupCursor = cursorPos
        editBox._ri_restore = true
    end

    editBox = ChatEdit_ChooseBoxForSend()
    if not editBox then
        return false
    end

    ChatEdit_ActivateChat(editBox)
    editBox:SetText(command)
    ChatEdit_SendText(editBox, 0)
    ChatEdit_OnEscapePressed(editBox)

    -- 恢复之前输入的文本
    if editBox._ri_restore then
        ChatEdit_ActivateChat(editBox)
        editBox:SetText(editBox._ri_backupText or "")
        if editBox._ri_backupCursor then
            editBox:SetCursorPosition(editBox._ri_backupCursor)
        end
        editBox._ri_backupText = nil
        editBox._ri_backupCursor = nil
        editBox._ri_restore = nil
    end

    return true
end

-- 发送命令
local function SendCommand(command, callback, params, requestType)
    local reqId = GenerateRequestId()

    if callback then
        pendingRequests[reqId] = {
            callback = callback,
            time = GetTime(),
            params = params,
            requestType = requestType,
        }
    end

    PrintDebug(string.format("发送命令[#%d][%s]: %s", reqId, requestType or "UNKNOWN", command))

    -- 重置解析数据
    if requestType == REQUEST_TYPE.INFO then
        parsedInfoData.messageCount = 0
        parsedInfoData.lastUpdateTime = GetTime()
        parsedInfoData.inNextLevelSection = false
    end

    if not ExecuteCommandThroughEditBox(command) then
        local playerName = UnitName("player")
        SendChatMessage(command, "WHISPER", nil, playerName)
    end

    return reqId
end

-- 完成INFO请求的回调
local function CompleteInfoRequest()
    for reqId, request in pairs(pendingRequests) do
        if request.requestType == REQUEST_TYPE.INFO and request.callback then
            PrintDebug(string.format("完成INFO请求: level=%d stats=%.1f talent=%d",
                parsedInfoData.reincarnationLevel,
                parsedInfoData.bonusStats,
                parsedInfoData.bonusTalent))
            request.callback(true, {
                reincarnationLevel = parsedInfoData.reincarnationLevel,
                bonusStats = parsedInfoData.bonusStats,
                bonusTalent = parsedInfoData.bonusTalent,
                playerLevel = parsedInfoData.playerLevel,
                nextBonusStats = parsedInfoData.nextBonusStats,
                nextBonusTalent = parsedInfoData.nextBonusTalent,
            }, request.params)
            pendingRequests[reqId] = nil
        end
    end
end

-- 处理服务器响应
local function HandleResponse(message)
    if type(message) ~= "string" then return end

    PrintDebug("收到消息: " .. message)

    -- 解析转身信息查询的多行响应
    -- 格式1: |cffffd700当前转身等级:|r X 转
    -- 格式2: 当前转身等级: X 转
    local level = message:match("当前转身等级:%|r%s*(%d+)%s*转")
    if not level then
        level = message:match("当前转身等级:%s*(%d+)%s*转")
    end
    if level then
        parsedInfoData.reincarnationLevel = tonumber(level) or 0
        parsedInfoData.messageCount = parsedInfoData.messageCount + 1
        parsedInfoData.inNextLevelSection = false
        PrintDebug("解析转身等级: " .. level)
        return
    end

    -- 检测是否进入"下一转"区域
    if message:match("下一转") and message:match("奖励") then
        parsedInfoData.inNextLevelSection = true
        parsedInfoData.messageCount = parsedInfoData.messageCount + 1
        PrintDebug("进入下一转区域")
        return
    end

    -- 格式1: |cffffd700全属性加成:|r +X.X%
    -- 格式2: 全属性加成: +X.X%
    local stats = message:match("全属性加成:%|r%s*%+([%d%.]+)%%")
    if not stats then
        stats = message:match("全属性加成:%s*%+([%d%.]+)%%")
    end
    if stats and not message:match("累计") then
        if parsedInfoData.inNextLevelSection then
            -- 这是下一转的加成
            parsedInfoData.nextBonusStats = tonumber(stats) or 0
            PrintDebug("解析下一转属性加成: " .. stats)
        else
            -- 这是当前总加成
            parsedInfoData.bonusStats = tonumber(stats) or 0
            PrintDebug("解析当前全属性加成: " .. stats)
        end
        parsedInfoData.messageCount = parsedInfoData.messageCount + 1
        return
    end

    -- 格式1: |cffffd700额外天赋点:|r +X
    -- 格式2: 额外天赋点: +X
    local talent = message:match("额外天赋点:%|r%s*%+(%d+)")
    if not talent then
        talent = message:match("额外天赋点:%s*%+(%d+)")
    end
    if talent then
        parsedInfoData.bonusTalent = tonumber(talent) or 0
        parsedInfoData.messageCount = parsedInfoData.messageCount + 1
        PrintDebug("解析额外天赋点: " .. talent)
        return
    end

    -- 格式1: |cffffd700当前角色等级:|r X级
    -- 格式2: 当前角色等级: X级
    local playerLevel = message:match("当前角色等级:%|r%s*(%d+)级")
    if not playerLevel then
        playerLevel = message:match("当前角色等级:%s*(%d+)级")
    end
    if playerLevel then
        parsedInfoData.playerLevel = tonumber(playerLevel) or 0
        parsedInfoData.messageCount = parsedInfoData.messageCount + 1
        PrintDebug("解析角色等级: " .. playerLevel)
        return
    end

    -- 下一转奖励 - 天赋点
    -- 格式: |cff00ffff天赋点奖励:|r +X 或 天赋点奖励: +X
    local nextTalent = message:match("天赋点奖励:%|r%s*%+(%d+)")
    if not nextTalent then
        nextTalent = message:match("天赋点奖励:%s*%+(%d+)")
    end
    if nextTalent and not message:match("累计") then
        parsedInfoData.nextBonusTalent = tonumber(nextTalent) or 0
        parsedInfoData.messageCount = parsedInfoData.messageCount + 1
        PrintDebug("解析下一转天赋点: " .. nextTalent)
        return
    end

    -- 检测信息查询结束 (分隔线)
    if message:match("========") and parsedInfoData.messageCount > 0 then
        PrintDebug("检测到信息结束标记")
        parsedInfoData.inNextLevelSection = false
        CompleteInfoRequest()
        return
    end

    -- 处理转身成功响应
    -- 格式: |cff00ff00恭喜你完成第X次转身！|r
    local newLevel = message:match("恭喜你完成第(%d+)次转身")
    if newLevel then
        PrintDebug("收到转身成功响应: " .. newLevel)
        -- 继续收集后续消息中的数据
        parsedInfoData.reincarnationLevel = tonumber(newLevel) or 0
        return
    end

    -- 转身成功后的属性加成消息
    -- 格式: |cff00ffff全属性加成: +X% (累计: Y%)|r
    local singleBonus, totalStats = message:match("全属性加成:%s*%+(%d+)%%%s*%(累计:%s*(%d+)%%%)")
    if totalStats then
        PrintDebug("转身成功 - 累计属性: " .. totalStats)
        for _, request in pairs(pendingRequests) do
            if request.requestType == REQUEST_TYPE.REINCARNATE then
                parsedInfoData.bonusStats = tonumber(totalStats) or 0
            end
        end
        return
    end

    -- 转身成功后的天赋点消息
    -- 格式: |cffff00ff天赋点奖励: +X点 (累计: Y点)|r
    local singleTalent, totalTalent = message:match("天赋点奖励:%s*%+(%d+)点%s*%(累计:%s*(%d+)点%)")
    if totalTalent then
        PrintDebug("转身成功 - 累计天赋点: " .. totalTalent)
        parsedInfoData.bonusTalent = tonumber(totalTalent) or 0
        -- 转身成功消息全部收集完毕，触发回调
        for reqId, request in pairs(pendingRequests) do
            if request.requestType == REQUEST_TYPE.REINCARNATE and request.callback then
                request.callback(true, {
                    success = true,
                    reincarnationLevel = parsedInfoData.reincarnationLevel,
                    bonusStats = parsedInfoData.bonusStats,
                    bonusTalent = parsedInfoData.bonusTalent,
                }, request.params)
                pendingRequests[reqId] = nil
            end
        end
        return
    end

    -- 处理转身失败响应
    if message:match("转身失败") or message:match("等级不足") or message:match("需要.*级") then
        PrintDebug("收到转身失败响应")
        for reqId, request in pairs(pendingRequests) do
            if request.requestType == REQUEST_TYPE.REINCARNATE and request.callback then
                request.callback(false, {
                    success = false,
                    message = message
                }, request.params)
                pendingRequests[reqId] = nil
            end
        end
        return
    end
end

-- 查询转身信息
function ReincarnationComm:QueryInfo(callback, params)
    local command = string.format("%s 信息", ReincarnationConfig.Communication.CommandPrefix)
    SendCommand(command, callback, params, REQUEST_TYPE.INFO)
end

-- 执行转身
function ReincarnationComm:DoReincarnate(callback, params)
    local command = string.format("%s 升级", ReincarnationConfig.Communication.CommandPrefix)
    SendCommand(command, callback, params, REQUEST_TYPE.REINCARNATE)
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
                if message:match("^INFO") or message:match("^REINCARNATE") then
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
        if currentTime - request.time > ReincarnationConfig.Communication.Timeout then
            -- 如果是INFO请求且已收集到数据，尝试完成
            if request.requestType == REQUEST_TYPE.INFO and parsedInfoData.messageCount > 0 then
                CompleteInfoRequest()
            else
                if request.callback then
                    request.callback(false, { message = ReincarnationConfig.Messages.Timeout })
                end
            end
            pendingRequests[reqId] = nil
        end
    end
end

-- 定时器
local tickFrame = CreateFrame("Frame")
local elapsed = 0
tickFrame:SetScript("OnUpdate", function(self, dt)
    elapsed = elapsed + dt
    if elapsed >= 1 then
        elapsed = 0
        CheckTimeout()
    end
end)

_G.ReincarnationComm = ReincarnationComm
PrintDebug("通信模块初始化完成")
