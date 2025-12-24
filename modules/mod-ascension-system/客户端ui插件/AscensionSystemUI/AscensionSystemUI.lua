-- 飞升系统UI插件
-- 额外18个装备槽位系统

local AscensionUI = {}
AscensionUI.slots = {}
AscensionUI.slotData = {}
AscensionUI.slotUnlocked = {}  -- 槽位解锁状态
AscensionUI.isLoaded = false

-- Addon 通信前缀
local ASCENSION_ADDON_PREFIX = "ASCENSION"
local TOOLTIP_ADDON_PREFIX = "UITQ"  -- MagicHitGrowthTooltip使用的前缀

-- 注册 Addon 消息前缀
if RegisterAddonMessagePrefix then
    pcall(RegisterAddonMessagePrefix, ASCENSION_ADDON_PREFIX)
    pcall(RegisterAddonMessagePrefix, TOOLTIP_ADDON_PREFIX)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    pcall(C_ChatInfo.RegisterAddonMessagePrefix, ASCENSION_ADDON_PREFIX)
    pcall(C_ChatInfo.RegisterAddonMessagePrefix, TOOLTIP_ADDON_PREFIX)
end

-- 发送Addon消息的统一函数
local function SendAscensionAddonMessage(message)
    local playerName = UnitName("player")
    if SendAddonMessage then
        SendAddonMessage(ASCENSION_ADDON_PREFIX, message, "WHISPER", playerName)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ASCENSION_ADDON_PREFIX, message, "WHISPER", playerName)
    end
end

-- 发送命令到服务器
local function SendAscensionCommand(command)
    SendAscensionAddonMessage("CMD:" .. command)
    SendChatMessage(command, "SAY")
end

-- 用户提示
local function UserPrint(msg)
    print("|cFFFFD700[飞升系统]|r " .. tostring(msg))
end

-- 从按钮获取槽位ID
local function GetSlotIdFromButton(button)
    if not button then return nil end

    if button.slotId then
        return button.slotId
    end

    local name = button:GetName()
    if name then
        local id = string.match(name, "AscensionSlot(%d+)")
        if id then
            return tonumber(id)
        end
    end

    return nil
end

-- 槽位配置（包含默认槽位图标）
local SLOT_CONFIG = {
    [0]  = {name = "头部",   invType = "INVTYPE_HEAD",     emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Head"},
    [1]  = {name = "颈部",   invType = "INVTYPE_NECK",     emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Neck"},
    [2]  = {name = "肩部",   invType = "INVTYPE_SHOULDER", emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Shoulder"},
    [3]  = {name = "衬衣",   invType = "INVTYPE_BODY",     emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Shirt"},
    [4]  = {name = "胸甲",   invType = "INVTYPE_CHEST",    emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest"},
    [5]  = {name = "腰带",   invType = "INVTYPE_WAIST",    emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Waist"},
    [6]  = {name = "腿部",   invType = "INVTYPE_LEGS",     emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Legs"},
    [7]  = {name = "脚部",   invType = "INVTYPE_FEET",     emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Feet"},
    [8]  = {name = "手腕",   invType = "INVTYPE_WRIST",    emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Wrists"},
    [9]  = {name = "手套",   invType = "INVTYPE_HAND",     emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Hands"},
    [10] = {name = "戒指1",  invType = "INVTYPE_FINGER",   emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Finger"},
    [11] = {name = "戒指2",  invType = "INVTYPE_FINGER",   emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Finger"},
    [12] = {name = "饰品1",  invType = "INVTYPE_TRINKET",  emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Trinket"},
    [13] = {name = "饰品2",  invType = "INVTYPE_TRINKET",  emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Trinket"},
    [14] = {name = "披风",   invType = "INVTYPE_CLOAK",    emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest"},
    [15] = {name = "主手",   invType = "INVTYPE_WEAPON",   emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-MainHand"},
    [16] = {name = "副手",   invType = "INVTYPE_SHIELD",   emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-SecondaryHand"},
    [17] = {name = "远程",   invType = "INVTYPE_RANGED",   emptyIcon = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Ranged"},
}

-- 初始化函数
function AscensionUI_OnLoad(self)
    self:SetToplevel(true)
    self:EnableMouse(true)
    self:SetMovable(true)
    self:RegisterForDrag("LeftButton")

    table.insert(UISpecialFrames, "AscensionMainFrame")

    self:RegisterEvent("CHAT_MSG_SYSTEM")
    self:RegisterEvent("CHAT_MSG_ADDON")
    self:RegisterEvent("ADDON_LOADED")
    self:RegisterEvent("PLAYER_LOGIN")
    self:RegisterEvent("BAG_UPDATE")

    AscensionUI_InitSlots()

    if AscensionMainFrameCloseButton then
        AscensionMainFrameCloseButton:SetScript("OnClick", function()
            AscensionMainFrame:Hide()
        end)
    end
end

-- 初始化装备槽位
-- preserveData: 如果为true，保留现有的slotData和slotUnlocked数据
function AscensionUI_InitSlots(preserveData)
    for slotId = 0, 17 do
        local slotButton = _G["AscensionSlot" .. slotId]
        if slotButton then
            slotButton.slotId = slotId
            slotButton.slotConfig = SLOT_CONFIG[slotId]

            slotButton:RegisterForDrag("LeftButton")
            slotButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")

            if not preserveData then
                AscensionUI.slotUnlocked[slotId] = false
                AscensionUI.slotData[slotId] = nil
            end

            local icon = _G[slotButton:GetName() .. "Icon"]
            local lock = _G[slotButton:GetName() .. "Lock"]

            if icon then
                if AscensionUI.slotUnlocked[slotId] then
                    local config = SLOT_CONFIG[slotId]
                    local emptyIcon = config and config.emptyIcon or "Interface\\PaperDoll\\UI-Backpack-EmptySlot"
                    icon:SetTexture(emptyIcon)
                    icon:SetVertexColor(1, 1, 1)
                    icon:SetDesaturated(false)
                    if lock then
                        lock:Hide()
                    end
                else
                    icon:SetTexture("Interface\\PaperDoll\\UI-Backpack-EmptySlot")
                    icon:SetVertexColor(0.3, 0.3, 0.3)
                    icon:SetDesaturated(true)
                    if lock then
                        lock:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
                        lock:SetVertexColor(1, 0.2, 0.2)
                        lock:Show()
                    end
                end
            end

            AscensionUI.slots[slotId] = slotButton
        end
    end
end

-- 显示界面
function AscensionUI_OnShow()
    if AscensionCharacterModel then
        AscensionCharacterModel:SetUnit("player")
        AscensionCharacterModel:SetRotation(0)
    end

    AscensionUI_RequestData()
end

-- 隐藏界面
function AscensionUI_OnHide()
end

-- 事件处理
function AscensionUI_OnEvent(self, event, ...)
    if event == "CHAT_MSG_SYSTEM" then
        local message = ...
        AscensionUI_HandleServerMessage(message)
    elseif event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        if prefix == ASCENSION_ADDON_PREFIX then
            AscensionUI_HandleServerMessage(message)
        end
    elseif event == "ADDON_LOADED" then
        local addonName = ...
        if addonName == "AscensionSystemUI" then
            AscensionUI_LoadSettings()
            AscensionUI.isLoaded = true
        end
    elseif event == "PLAYER_LOGIN" then
        C_Timer.After(2, function()
            AscensionUI_RequestData()
        end)
    elseif event == "BAG_UPDATE" then
        if AscensionMainFrame and AscensionMainFrame:IsShown() then
            AscensionUI_UpdateAllSlots()
        end
    end
end

-- 请求服务器数据
function AscensionUI_RequestData()
    SendAscensionCommand(".飞升 查看")
end

-- 处理服务器消息
function AscensionUI_HandleServerMessage(message)
    if not message then return end

    if string.find(message, "ASCENSION_HIDDEN:ASCENSION_DATA:") then
        local dataStr = string.gsub(message, "ASCENSION_HIDDEN:ASCENSION_DATA:", "")
        AscensionUI_ParseSlotData(dataStr)
        return
    end

    if string.find(message, "ASCENSION_DATA:") then
        local dataStr = string.gsub(message, "ASCENSION_DATA:", "")
        AscensionUI_ParseSlotData(dataStr)
        return
    end
end

-- 预缓存物品信息
local function PreCacheItemInfo(itemId)
    if itemId and itemId > 0 then
        -- 调用 GetItemInfo 会触发客户端向服务器请求物品信息
        GetItemInfo(itemId)
    end
end

-- 解析槽位数据
function AscensionUI_ParseSlotData(dataStr)
    if not dataStr or dataStr == "" then
        return
    end

    for slotId = 0, 17 do
        AscensionUI.slotData[slotId] = nil
        AscensionUI.slotUnlocked[slotId] = false
    end

    local slotEntries = {strsplit(";", dataStr)}

    for _, slotInfo in ipairs(slotEntries) do
        if slotInfo and slotInfo ~= "" then
            local parts = {strsplit(":", slotInfo)}

            if #parts >= 4 then
                local slotId = tonumber(parts[1])
                local itemId = tonumber(parts[2])
                local itemGuid = tonumber(parts[3])
                local unlocked = tonumber(parts[4]) == 1

                if slotId and slotId >= 0 and slotId <= 17 then
                    AscensionUI.slotUnlocked[slotId] = unlocked

                    if itemId and itemGuid and itemId > 0 then
                        AscensionUI.slotData[slotId] = {
                            itemId = itemId,
                            itemGuid = itemGuid
                        }
                        -- 预缓存物品信息
                        PreCacheItemInfo(itemId)
                    end
                end
            end
        end
    end

    -- 延迟更新UI，等待物品信息缓存
    C_Timer.After(0.3, function()
        AscensionUI_UpdateAllSlots()
    end)
end

-- 更新所有槽位显示
function AscensionUI_UpdateAllSlots()
    local needInit = true
    for _, _ in pairs(AscensionUI.slots) do
        needInit = false
        break
    end

    if needInit then
        AscensionUI_InitSlots(true)
    end

    for slotId = 0, 17 do
        AscensionUI_UpdateSlot(slotId)
    end
end

-- 更新单个槽位显示
-- retryCount: 重试次数，用于避免无限循环
function AscensionUI_UpdateSlot(slotId, retryCount)
    retryCount = retryCount or 0
    local maxRetries = 5

    local slotButton = AscensionUI.slots[slotId]
    if not slotButton then
        return
    end

    local icon = _G[slotButton:GetName() .. "Icon"]
    local lock = _G[slotButton:GetName() .. "Lock"]
    local border = _G[slotButton:GetName() .. "Border"]
    if not icon then
        return
    end

    local slotData = AscensionUI.slotData[slotId]
    local isUnlocked = AscensionUI.slotUnlocked[slotId]
    local config = SLOT_CONFIG[slotId]

    if not isUnlocked then
        icon:SetTexture("Interface\\PaperDoll\\UI-Backpack-EmptySlot")
        icon:SetVertexColor(0.3, 0.3, 0.3)
        icon:SetDesaturated(true)
        if lock then
            lock:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
            lock:SetVertexColor(1, 0.2, 0.2)
            lock:SetWidth(24)
            lock:SetHeight(24)
            lock:Show()
        end
        if border then
            border:SetVertexColor(0.5, 0.5, 0.5)
        end
    elseif slotData and slotData.itemId > 0 then
        local _, _, itemRarity, _, _, _, _, _, _, itemTexture = GetItemInfo(slotData.itemId)

        if itemTexture then
            icon:SetTexture(itemTexture)
            icon:SetVertexColor(1, 1, 1)
            icon:SetDesaturated(false)
        else
            -- 物品信息尚未缓存，先显示加载中图标
            icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
            icon:SetVertexColor(0.7, 0.7, 0.7)
            icon:SetDesaturated(false)

            -- 如果还有重试次数，延迟后重试
            if retryCount < maxRetries then
                C_Timer.After(0.5 + retryCount * 0.3, function()
                    AscensionUI_UpdateSlot(slotId, retryCount + 1)
                end)
            end
        end

        if lock then
            lock:Hide()
        end

        if border and itemRarity then
            local r, g, b = GetItemQualityColor(itemRarity)
            border:SetVertexColor(r, g, b)
        elseif border then
            border:SetVertexColor(1, 1, 1)
        end
    else
        local emptyIcon = config and config.emptyIcon or "Interface\\PaperDoll\\UI-Backpack-EmptySlot"
        icon:SetTexture(emptyIcon)
        icon:SetVertexColor(1, 1, 1)
        icon:SetDesaturated(false)

        if lock then
            lock:Hide()
        end
        if border then
            border:SetVertexColor(1, 1, 1)
        end
    end
end

-- 槽位鼠标进入
function AscensionSlot_OnEnter(self)
    local slotId = GetSlotIdFromButton(self)
    if not slotId then return end

    local slotData = AscensionUI.slotData[slotId]
    local config = self.slotConfig or SLOT_CONFIG[slotId]
    local isUnlocked = AscensionUI.slotUnlocked[slotId]
    local slotName = config and config.name or ("槽位" .. slotId)

    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")

    if not isUnlocked then
        GameTooltip:SetText("|cFFFF0000飞升槽位: " .. slotName .. " [未解锁]|r")
        GameTooltip:AddLine("|cFFFFFF00双击解锁此槽位|r", 1, 1, 1)
        GameTooltip:AddLine("解锁后可装备对应类型的装备", 0.7, 0.7, 0.7)
    elseif slotData and slotData.itemId > 0 then
        -- 设置全局变量，让MagicHitGrowthTooltip插件知道这是飞升系统的物品
        -- 这样它可以使用正确的GUID来查询自定义属性
        ASCENSION_TOOLTIP_ITEM_ID = slotData.itemId
        ASCENSION_TOOLTIP_ITEM_GUID = slotData.itemGuid

        -- 显示基础物品tooltip
        GameTooltip:SetHyperlink("item:" .. slotData.itemId)
    else
        GameTooltip:SetText("|cFF00FF00飞升槽位: " .. slotName .. " [已解锁]|r")
        GameTooltip:AddLine("拖拽装备到此槽位", 1, 1, 1)
        GameTooltip:AddLine("右键点击卸下装备", 0.7, 0.7, 0.7)
    end

    GameTooltip:Show()
end

-- 槽位鼠标离开
function AscensionSlot_OnLeave(self)
    -- 清除全局变量
    ASCENSION_TOOLTIP_ITEM_ID = nil
    ASCENSION_TOOLTIP_ITEM_GUID = nil
    GameTooltip:Hide()
end

-- 双击检测变量
AscensionUI.lastClickTime = {}
AscensionUI.DOUBLE_CLICK_TIME = 0.3

-- 槽位点击
function AscensionSlot_OnClick(self, button)
    local slotId = GetSlotIdFromButton(self)
    if not slotId then return end

    local currentTime = GetTime()
    local lastClick = AscensionUI.lastClickTime[slotId] or 0

    if button == "LeftButton" and (currentTime - lastClick) < AscensionUI.DOUBLE_CLICK_TIME then
        AscensionUI.lastClickTime[slotId] = 0
        AscensionSlot_HandleDoubleClick(self, slotId)
        return
    end

    AscensionUI.lastClickTime[slotId] = currentTime

    local slotData = AscensionUI.slotData[slotId]
    local isUnlocked = AscensionUI.slotUnlocked[slotId]

    if not isUnlocked then
        UserPrint("此槽位尚未解锁，双击可解锁槽位")
        return
    end

    if button == "RightButton" then
        if slotData and slotData.itemId > 0 then
            AscensionUI_UnequipSlot(slotId)
        end
    elseif button == "LeftButton" then
        local cursorType = GetCursorInfo()
        if cursorType == "item" then
            AscensionUI_EquipCursorItem(slotId)
        elseif slotData and slotData.itemId > 0 then
            local _, link = GetItemInfo(slotData.itemId)
            if link then
                ChatFrame1:AddMessage("飞升装备: " .. link)
            end
        end
    end
end

-- 处理双击事件
function AscensionSlot_HandleDoubleClick(self, slotId)
    local isUnlocked = AscensionUI.slotUnlocked[slotId]

    if not isUnlocked then
        local config = self.slotConfig or SLOT_CONFIG[slotId]
        local slotName = config and config.name or ("槽位" .. slotId)

        UserPrint("正在解锁槽位: " .. slotName)
        AscensionUI_UnlockSlot(slotId)
    end
end

-- 槽位双击解锁
function AscensionSlot_OnDoubleClick(self, button)
    local slotId = GetSlotIdFromButton(self)
    if not slotId then
        return
    end

    local isUnlocked = AscensionUI.slotUnlocked[slotId]

    if not isUnlocked then
        AscensionUI_UnlockSlot(slotId)
    end
end

-- 解锁槽位
function AscensionUI_UnlockSlot(slotId)
    local command = string.format(".飞升 解锁 %d", slotId)
    SendAscensionCommand(command)
    UserPrint("正在解锁槽位...")
end

-- 槽位拖拽开始
function AscensionSlot_OnDragStart(self)
end

-- 槽位接收拖拽
function AscensionSlot_OnReceiveDrag(self)
    local slotId = GetSlotIdFromButton(self)
    if not slotId then return end

    local cursorType = GetCursorInfo()
    if cursorType == "item" then
        AscensionUI_EquipCursorItem(slotId)
    end
end

-- 记录拖拽物品的来源位置
AscensionUI.dragSourceBag = nil
AscensionUI.dragSourceSlot = nil

-- 钩住背包槽位的拖拽事件来记录来源位置
local function HookContainerButtons()
    for bag = 0, 4 do
        local bagFrameName = "ContainerFrame" .. (bag + 1)
        local bagFrame = _G[bagFrameName]
        if bagFrame then
            local numSlots = GetContainerNumSlots(bag)
            for slot = 1, numSlots do
                local buttonName = bagFrameName .. "Item" .. (numSlots - slot + 1)
                local button = _G[buttonName]
                if button and not button.ascensionHooked then
                    button.ascensionHooked = true
                    local originalOnDragStart = button:GetScript("OnDragStart")
                    button:SetScript("OnDragStart", function(self, ...)
                        -- 记录来源位置
                        AscensionUI.dragSourceBag = bag
                        AscensionUI.dragSourceSlot = slot
                        if originalOnDragStart then
                            originalOnDragStart(self, ...)
                        end
                    end)
                end
            end
        end
    end
end

-- 延迟钩住背包按钮
local hookBagFrame = CreateFrame("Frame")
hookBagFrame:RegisterEvent("BAG_OPEN")
hookBagFrame:RegisterEvent("BAG_UPDATE")
hookBagFrame:SetScript("OnEvent", function()
    C_Timer.After(0.5, HookContainerButtons)
end)

-- 装备光标物品到槽位
function AscensionUI_EquipCursorItem(slotId)
    local cursorType, cursorItemId, cursorItemLink = GetCursorInfo()

    if cursorType ~= "item" then
        UserPrint("请先拾取一个物品")
        return
    end

    -- 从itemLink中提取真正的物品ID
    local realItemId = nil
    if cursorItemLink then
        realItemId = tonumber(string.match(cursorItemLink, "item:(%d+)"))
    end

    -- 先把物品放回背包
    ClearCursor()

    -- 稍微延迟后发送命令
    C_Timer.After(0.1, function()
        local bag, slot = nil, nil

        -- 优先用realItemId查找
        if realItemId then
            bag, slot = AscensionUI_FindItemInBags(realItemId)
        end

        -- 如果没找到，尝试用cursorItemId查找
        if not bag and cursorItemId then
            bag, slot = AscensionUI_FindItemInBags(cursorItemId)
        end

        if not bag then
            UserPrint("背包中未找到该物品")
            return
        end

        local command = string.format(".飞升 装备 %d %d %d", slotId, bag, slot)
        SendAscensionCommand(command)
    end)
end

-- 在背包中查找物品
function AscensionUI_FindItemInBags(targetItemId)
    for bag = 0, 4 do
        local numSlots = GetContainerNumSlots(bag)
        for slot = 1, numSlots do
            local itemId = GetContainerItemID(bag, slot)
            if itemId == targetItemId then
                return bag, slot
            end
        end
    end
    return nil, nil
end

-- 卸下槽位装备
function AscensionUI_UnequipSlot(slotId)
    local command = string.format(".飞升 卸下 %d", slotId)
    SendAscensionCommand(command)
end

-- 切换界面显示
function AscensionUI_Toggle()
    if AscensionMainFrame then
        if AscensionMainFrame:IsShown() then
            AscensionMainFrame:Hide()
        else
            AscensionMainFrame:Show()
        end
    end
end

-- 快捷按钮加载
function AscensionQuickButton_OnLoad(self)
    self:RegisterForDrag("LeftButton")
    self:RegisterForClicks("LeftButtonUp")

    if AscensionSystemUICharSettings and AscensionSystemUICharSettings.quickButtonPos then
        local pos = AscensionSystemUICharSettings.quickButtonPos
        self:ClearAllPoints()
        self:SetPoint(pos.point, UIParent, pos.relativePoint, pos.x, pos.y)
    end
end

-- 保存快捷按钮位置
function AscensionUI_SaveQuickButtonPosition()
    local button = AscensionQuickButton
    if not button then return end

    if not AscensionSystemUICharSettings then
        AscensionSystemUICharSettings = {}
    end

    local point, _, relativePoint, x, y = button:GetPoint()
    AscensionSystemUICharSettings.quickButtonPos = {
        point = point,
        relativePoint = relativePoint,
        x = x,
        y = y
    }
end

-- 加载设置
function AscensionUI_LoadSettings()
    if not AscensionSystemUISettings then
        AscensionSystemUISettings = {}
    end

    if not AscensionSystemUICharSettings then
        AscensionSystemUICharSettings = {}
    end
end

-- 注册斜杠命令
SLASH_ASCENSION1 = "/ascension"
SLASH_ASCENSION2 = "/飞升"
SlashCmdList["ASCENSION"] = function(msg)
    if msg == "reload" then
        AscensionUI_RequestData()
    else
        AscensionUI_Toggle()
    end
end

-- 聊天过滤器 - 隐藏飞升系统的隐藏消息
local function AscensionChatFilter(self, event, message, ...)
    if message and string.find(message, "ASCENSION_HIDDEN:") then
        AscensionUI_HandleServerMessage(message)
        return true
    end
    return false
end

ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", AscensionChatFilter)

-- 与官方角色面板绑定 (按C键打开/关闭)
local function HookCharacterFrame()
    if not CharacterFrame then
        return
    end

    local originalShow = CharacterFrame.Show
    local originalHide = CharacterFrame.Hide

    CharacterFrame.Show = function(self, ...)
        -- 先调用原始函数，确保角色界面正常打开
        local success, err = pcall(originalShow, self, ...)
        if not success then
            print("|cFFFF0000[飞升系统] CharacterFrame.Show 错误:|r " .. tostring(err))
        end
        -- 然后尝试显示飞升界面
        pcall(function()
            if AscensionMainFrame then
                AscensionMainFrame:Show()
                AscensionMainFrame:ClearAllPoints()
                AscensionMainFrame:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", 10, 0)
            end
        end)
    end

    CharacterFrame.Hide = function(self, ...)
        -- 先调用原始函数，确保角色界面正常关闭
        local success, err = pcall(originalHide, self, ...)
        if not success then
            print("|cFFFF0000[飞升系统] CharacterFrame.Hide 错误:|r " .. tostring(err))
        end
        -- 然后尝试隐藏飞升界面
        pcall(function()
            if AscensionMainFrame and AscensionMainFrame:IsShown() then
                AscensionMainFrame:Hide()
            end
        end)
    end

    if ToggleCharacter then
        local originalToggle = ToggleCharacter
        ToggleCharacter = function(tab, ...)
            -- 先调用原始函数
            local success, err = pcall(originalToggle, tab, ...)
            if not success then
                print("|cFFFF0000[飞升系统] ToggleCharacter 错误:|r " .. tostring(err))
            end
            -- 然后尝试同步飞升界面状态
            pcall(function()
                if CharacterFrame:IsShown() then
                    if AscensionMainFrame then
                        AscensionMainFrame:Show()
                        AscensionMainFrame:ClearAllPoints()
                        AscensionMainFrame:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", 10, 0)
                    end
                else
                    if AscensionMainFrame and AscensionMainFrame:IsShown() then
                        AscensionMainFrame:Hide()
                    end
                end
            end)
        end
    end
end

local hookFrame = CreateFrame("Frame")
hookFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
hookFrame:SetScript("OnEvent", function(self, event)
    if event == "PLAYER_ENTERING_WORLD" then
        C_Timer.After(1, function()
            HookCharacterFrame()
        end)
        self:UnregisterEvent("PLAYER_ENTERING_WORLD")
    end
end)
