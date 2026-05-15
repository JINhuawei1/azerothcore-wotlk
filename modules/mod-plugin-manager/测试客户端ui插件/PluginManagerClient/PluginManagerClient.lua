-- ========================================
-- 插件管理器客户端集成 - 共享监听器
-- ========================================
-- 说明:
--   1. 使用真正的 AddOn 通道与服务端通信
--   2. 玩家登录、重载UI、重新进世界时主动请求配置，并在无响应时短暂重试
--   3. 提供统一的坐标保存接口，供各插件在拖拽结束后回传服务器

if not C_Timer then
    C_Timer = {}

    function C_Timer.After(delay, func)
        local frame = CreateFrame("Frame")
        local elapsed = 0
        frame:SetScript("OnUpdate", function(self, delta)
            elapsed = elapsed + delta
            if elapsed >= delay then
                self:SetScript("OnUpdate", nil)
                func()
            end
        end)
    end
end

if not _G.PluginManagerClient then
    _G.PluginManagerClient = {}
end

local PMClient = _G.PluginManagerClient

PMClient.addonPrefix = "PLUGMGR"
PMClient.handlers = PMClient.handlers or {}
PMClient.configs = PMClient.configs or {}
PMClient.lastSavedLayouts = PMClient.lastSavedLayouts or {}
PMClient.debug = false
PMClient.initialized = PMClient.initialized or false
PMClient.hasRequestedInitialSync = PMClient.hasRequestedInitialSync or false
PMClient.hasCompletedInitialSync = PMClient.hasCompletedInitialSync or false
PMClient.isReceivingSync = PMClient.isReceivingSync or false
PMClient.syncGeneration = PMClient.syncGeneration or 0
PMClient.completedSyncGeneration = PMClient.completedSyncGeneration or 0
PMClient.lastRequestTime = PMClient.lastRequestTime or 0

local SYNC_RETRY_DELAYS = { 0.5, 2.0, 5.0 }

local function DebugPrint(message)
    if PMClient.debug then
        print(string.format("|cff00ffff[PluginManager]|r %s", tostring(message)))
    end
end

local function NormalizeNumber(value)
    value = tonumber(value)
    if not value then
        return 0
    end

    if value >= 0 then
        return math.floor(value + 0.5)
    end

    return math.ceil(value - 0.5)
end

local function GetClientTime()
    if GetTime then
        return GetTime()
    end

    return 0
end

local function BuildConfigMessage(pluginName, config)
    if not pluginName or type(config) ~= "table" then
        return nil, nil
    end

    local x = NormalizeNumber(config.x)
    local y = NormalizeNumber(config.y)
    local width = NormalizeNumber(config.width)
    local height = NormalizeNumber(config.height)
    local enabled = config.enabled and 1 or 0
    local message = string.format(".plugincfg %s %d %d %d %d %d", pluginName, x, y, width, height, enabled)
    local parts = { ".plugincfg", pluginName, tostring(x), tostring(y), tostring(width), tostring(height), tostring(enabled) }
    return message, parts
end

function PMClient:RegisterPlugin(pluginName, handler)
    if not pluginName or not handler then
        error("PluginManagerClient: 插件名称和处理函数不能为空")
        return false
    end

    if self.handlers[pluginName] then
        print(string.format("|cffff0000警告:|r 插件 '%s' 已经注册过了,将被覆盖", pluginName))
    end

    self.handlers[pluginName] = handler

    local config = self:GetPluginConfig(pluginName)
    if config then
        local message, parts = BuildConfigMessage(pluginName, config)
        if message and parts then
            local success, err = pcall(handler, message, parts)
            if not success then
                print(string.format("|cffff0000错误:|r 插件 '%s' 重放缓存配置时出错: %s", pluginName, tostring(err)))
            end
        end
    end

    if not self.hasRequestedInitialSync and not self.hasCompletedInitialSync then
        self:ScheduleConfigRequest("REGISTER_PLUGIN", false)
    end

    return true
end

function PMClient:UnregisterPlugin(pluginName)
    if self.handlers[pluginName] then
        self.handlers[pluginName] = nil
        return true
    end

    return false
end

function PMClient:DispatchMessage(message)
    if not message then
        return false
    end

    DebugPrint("收到配置消息: " .. tostring(message))

    if message == ".plugincfg_start" then
        self.isReceivingSync = true
        self.configs = {}
        for _, handler in pairs(self.handlers) do
            pcall(handler, message, { ".plugincfg_start" })
        end
        return true
    end

    if message == ".plugincfg_end" then
        self.isReceivingSync = false
        self.hasRequestedInitialSync = true
        self.hasCompletedInitialSync = true
        self.completedSyncGeneration = self.syncGeneration
        for _, handler in pairs(self.handlers) do
            pcall(handler, message, { ".plugincfg_end" })
        end
        return true
    end

    if message:match("^%.plugincfg%s+") then
        local parts = { strsplit(" ", message) }
        local pluginName = parts[2]

        if pluginName then
            self.configs[pluginName] = {
                x = NormalizeNumber(parts[3]),
                y = NormalizeNumber(parts[4]),
                width = NormalizeNumber(parts[5]),
                height = NormalizeNumber(parts[6]),
                enabled = tonumber(parts[7]) == 1
            }
        end

        if pluginName and self.handlers[pluginName] then
            local layoutKey = string.format("%d:%d:%d:%d",
                NormalizeNumber(parts[3]),
                NormalizeNumber(parts[4]),
                NormalizeNumber(parts[5]),
                NormalizeNumber(parts[6]))
            self.lastSavedLayouts[pluginName] = layoutKey

            local success, err = pcall(self.handlers[pluginName], message, parts)
            if not success then
                print(string.format("|cffff0000错误:|r 插件 '%s' 处理消息时出错: %s", pluginName, tostring(err)))
            end
        end

        return true
    end

    return false
end

function PMClient:GetPluginConfig(pluginName)
    return self.configs[pluginName]
end

function PMClient:ShouldShowPlugin(pluginName, fallbackEnabled)
    local config = self:GetPluginConfig(pluginName)
    if config ~= nil then
        return config.enabled == true
    end

    if not self.hasCompletedInitialSync then
        return false
    end

    return fallbackEnabled == true
end

function PMClient:SendAddonPayload(payload)
    local playerName = UnitName("player")
    if not payload or payload == "" or not playerName then
        return false
    end

    if SendAddonMessage then
        SendAddonMessage(self.addonPrefix, payload, "WHISPER", playerName)
        return true
    end

    if C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(self.addonPrefix, payload, "WHISPER", playerName)
        return true
    end

    return false
end

function PMClient:RequestPluginConfig(force)
    local now = GetClientTime()

    if self.hasRequestedInitialSync and self.hasCompletedInitialSync and not force then
        DebugPrint("本次会话已请求过配置，跳过重复同步")
        return false
    end

    if not force and self.lastRequestTime > 0 and now - self.lastRequestTime < 1 then
        return false
    end

    if self:SendAddonPayload("REQ_ALL") then
        self.hasRequestedInitialSync = true
        self.lastRequestTime = now
        DebugPrint("已发送插件配置请求")
        return true
    end

    return false
end

function PMClient:ScheduleConfigRequest(reason, force)
    self.syncGeneration = (self.syncGeneration or 0) + 1
    local generation = self.syncGeneration

    DebugPrint("安排插件配置同步: " .. tostring(reason))

    for _, delay in ipairs(SYNC_RETRY_DELAYS) do
        C_Timer.After(delay, function()
            if PMClient.syncGeneration ~= generation then
                return
            end

            if PMClient.completedSyncGeneration == generation then
                return
            end

            if not force and PMClient.hasCompletedInitialSync and not PMClient.isReceivingSync then
                return
            end

            PMClient:RequestPluginConfig(true)
        end)
    end
end

function PMClient:SavePluginPosition(pluginName, x, y, width, height)
    if not pluginName or pluginName == "" then
        return false
    end

    local normalizedX = NormalizeNumber(x)
    local normalizedY = NormalizeNumber(y)
    local normalizedWidth = NormalizeNumber(width or 0)
    local normalizedHeight = NormalizeNumber(height or 0)
    local cacheKey = string.format("%d:%d:%d:%d", normalizedX, normalizedY, normalizedWidth, normalizedHeight)

    if self.lastSavedLayouts[pluginName] == cacheKey then
        return false
    end

    if self:SendAddonPayload(string.format(
        "SAVE_POS:%s:%d:%d:%d:%d",
        pluginName,
        normalizedX,
        normalizedY,
        normalizedWidth,
        normalizedHeight))
    then
        self.lastSavedLayouts[pluginName] = cacheKey
        DebugPrint(string.format("已保存插件 %s 的坐标 (%d, %d)", pluginName, normalizedX, normalizedY))
        return true
    end

    return false
end

function PMClient:SaveFramePosition(pluginName, frame, width, height)
    if not frame or not frame.GetLeft or not frame.GetBottom then
        return false
    end

    local left = frame:GetLeft()
    local bottom = frame:GetBottom()
    if not left or not bottom then
        return false
    end

    local finalWidth = width
    local finalHeight = height

    if finalWidth == nil and frame.GetWidth then
        finalWidth = frame:GetWidth()
    end

    if finalHeight == nil and frame.GetHeight then
        finalHeight = frame:GetHeight()
    end

    return self:SavePluginPosition(pluginName, left, bottom, finalWidth or 0, finalHeight or 0)
end

function PMClient:RegisterAddonPrefix()
    if RegisterAddonMessagePrefix then
        RegisterAddonMessagePrefix(self.addonPrefix)
        return
    end

    if C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
        C_ChatInfo.RegisterAddonMessagePrefix(self.addonPrefix)
    end
end

function PMClient:Initialize()
    if self.initialized then
        return
    end

    self:RegisterAddonPrefix()

    local eventFrame = CreateFrame("Frame", "PluginManagerClientFrame")
    eventFrame:RegisterEvent("CHAT_MSG_ADDON")
    eventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
    eventFrame:RegisterEvent("PLAYER_LOGIN")
    eventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
    eventFrame:SetScript("OnEvent", function(_, event, ...)
        if event == "CHAT_MSG_ADDON" then
            local prefix, message = ...
            if prefix == PMClient.addonPrefix then
                PMClient:DispatchMessage(message)
            end
        elseif event == "CHAT_MSG_SYSTEM" then
            local message = ...
            PMClient:DispatchMessage(message)
        elseif event == "PLAYER_LOGIN" then
            PMClient:ScheduleConfigRequest("PLAYER_LOGIN", true)
        elseif event == "PLAYER_ENTERING_WORLD" then
            PMClient:ScheduleConfigRequest("PLAYER_ENTERING_WORLD", true)
        end
    end)

    ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", function(_, _, message)
        if type(message) == "string" and message:match("^%.plugincfg") then
            return true
        end

        return false
    end)

    self.initialized = true
    self.eventFrame = eventFrame
    DebugPrint("共享监听器已初始化")
end

PMClient:Initialize()

return PMClient
