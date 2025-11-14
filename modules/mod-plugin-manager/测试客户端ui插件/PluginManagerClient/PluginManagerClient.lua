-- ========================================
-- 插件管理器客户端集成 - 共享监听器
-- ========================================
-- 说明: 此文件提供一个共享的CHAT_MSG_SYSTEM监听器
--       避免多个插件重复注册事件导致冲突
-- 用法: 在每个插件中require此文件,然后注册自己的处理函数

-- 创建全局命名空间
if not _G.PluginManagerClient then
    _G.PluginManagerClient = {}
end

local PMClient = _G.PluginManagerClient

-- 已注册的插件处理函数列表
PMClient.handlers = PMClient.handlers or {}

-- 调试模式
PMClient.debug = false  -- 关闭调试

-- 注册插件处理函数
function PMClient:RegisterPlugin(pluginName, handler)
    if not pluginName or not handler then
        error("PluginManagerClient: 插件名称和处理函数不能为空")
        return false
    end

    if self.handlers[pluginName] then
        print(string.format("|cffff0000警告:|r 插件 '%s' 已经注册过了,将被覆盖", pluginName))
    end

    self.handlers[pluginName] = handler

    -- 已禁用插件注册日志输出
    -- print(string.format("|cff00ffff[PluginManager]|r 注册插件 '%s'", pluginName))

    return true
end

-- 取消注册插件
function PMClient:UnregisterPlugin(pluginName)
    if self.handlers[pluginName] then
        self.handlers[pluginName] = nil
        if self.debug then
            print(string.format("|cff00ff00插件管理器:|r 取消注册插件 '%s'", pluginName))
        end
        return true
    end
    return false
end

-- 分发消息到对应的插件
function PMClient:DispatchMessage(message)
    if not message then
        return false  -- 返回false表示消息未被处理
    end

    if self.debug then
        print(string.format("|cff00ffff[PluginManager]|r 收到消息: %s", message))
    end

    -- 过滤插件配置消息
    -- 格式: .plugincfg <插件名称> <X坐标> <Y坐标> <宽度> <高度> <启用状态>
    if message:match("^%.plugincfg%s+") then
        local parts = {strsplit(" ", message)}
        local pluginName = parts[2]

        if self.debug then
            print(string.format("|cff00ffff[PluginManager]|r 解析插件配置: 插件=%s", tostring(pluginName)))
        end

        if pluginName and self.handlers[pluginName] then
            if self.debug then
                print(string.format("|cff00ffff[PluginManager]|r 找到处理器,分发给: %s", pluginName))
            end
            -- 调用对应插件的处理函数
            local success, err = pcall(self.handlers[pluginName], message, parts)
            if not success then
                print(string.format("|cffff0000错误:|r 插件 '%s' 处理消息时出错: %s", pluginName, tostring(err)))
            elseif self.debug then
                print(string.format("|cff00ffff[PluginManager]|r 消息分发成功: %s", pluginName))
            end
        else
            if self.debug then
                if not pluginName then
                    print("|cff00ffff[PluginManager]|r 警告: 无法解析插件名称")
                else
                    print(string.format("|cff00ffff[PluginManager]|r 警告: 未找到插件 '%s' 的处理器", pluginName))
                end
            end
        end
        return true  -- 返回true表示已处理，应该隐藏

    -- 配置开始标记
    elseif message == ".plugincfg_start" then
        if self.debug then
            print("|cff00ffff[PluginManager]|r 开始接收插件配置")
        end
        -- 通知所有插件配置开始
        for pluginName, handler in pairs(self.handlers) do
            pcall(handler, message, {".plugincfg_start"})
        end
        return true  -- 返回true表示已处理，应该隐藏

    -- 配置结束标记
    elseif message == ".plugincfg_end" then
        if self.debug then
            print("|cff00ffff[PluginManager]|r 插件配置接收完成")
        end
        -- 通知所有插件配置结束
        for pluginName, handler in pairs(self.handlers) do
            pcall(handler, message, {".plugincfg_end"})
        end
        return true  -- 返回true表示已处理，应该隐藏
    end

    return false  -- 其他消息未被处理
end

-- 请求插件配置
function PMClient:RequestPluginConfig()
    if self.debug then
        print("|cff00ff00插件管理器:|r 请求插件配置...")
    end
    SendChatMessage(".插件 同步", "SAY")
end

-- 初始化事件监听器(只执行一次)
function PMClient:Initialize()
    if self.initialized then
        return
    end

    -- 创建共享的事件帧
    local eventFrame = CreateFrame("Frame", "PluginManagerClientFrame")
    eventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
    eventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
    eventFrame:SetScript("OnEvent", function(_, event, message)
        if event == "CHAT_MSG_SYSTEM" then
            PMClient:DispatchMessage(message)
        elseif event == "PLAYER_ENTERING_WORLD" then
            -- 延迟3秒后请求插件配置,确保客户端完全加载
            C_Timer.After(3, function()
                PMClient:RequestPluginConfig()
            end)
        end
    end)

    -- 添加聊天框消息过滤器，隐藏 .plugincfg 相关的系统消息
    ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", function(self, event, message, ...)
        -- 隐藏插件配置消息，防止在聊天框显示
        if message:match("^%.plugincfg") then
            return true  -- 返回true表示过滤该消息，不显示在聊天框
        end
        return false  -- 返回false表示不过滤，正常显示
    end)

    self.initialized = true
    self.eventFrame = eventFrame

    if self.debug then
        print("|cff00ff00插件管理器:|r 共享监听器已初始化")
    end
end

-- 自动初始化
PMClient:Initialize()

-- 返回模块
return PMClient
