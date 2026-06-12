-- ============================================================
-- 仙器装备 UI (XianqiSystemUI)
-- 10 仙器槽(1-10) + 13 扩展槽(11-23，按部位)
-- 服务端: mod-xianmen-system/src/XianmenArtifactSlots.cpp
-- 协议: XIANQI 前缀，下行 U=<解锁位图>;E=<槽:物品ID:GUID,...>
--       上行走聊天命令 .仙器 查看/装备/卸下/解锁
-- ============================================================

local ADDON_NAME = "XianqiSystemUI"
local PREFIX = "XIANQI"
local PLUGIN_NAME = "XianqiSystemUI"
local ICON_SIZE = 20
local ICON_TEXTURE = "Interface\\Icons\\INV_Sword_39"
local SLOT_FIRST, SLOT_LAST = 1, 29
local SLOT_SIZE = 40
local QUESTION_MARK_ICON = "Interface\\Icons\\INV_Misc_QuestionMark"

XianqiSystemUIDB = XianqiSystemUIDB or {}

local slotButtons = {}          -- slotId -> button
local equipData = {}            -- slotId -> { itemId, itemGuid }
local unlockedSlots = {}        -- slotId -> true
local pendingIcons = {}         -- itemId -> true（等待 GET_ITEM_INFO_RECEIVED）
local iconButton
local pluginManagerRegistered = false
local pluginManagerConfigApplied = false
local dragSource = nil          -- { bag, slot, itemId } 拖起物品时的来源格

-- 槽位静态配置（与 _仙门_仙器槽位 数据一致；扩展槽 11-29 镜像官方 19 装备槽）
-- invSlotName 用于取官方空槽底图；equipLocs 用于光标高亮（仅扩展槽）
local SLOT_CONFIG = {
    [1]  = { name = "仙器槽一",    artifact = true },
    [2]  = { name = "仙器槽二",    artifact = true },
    [3]  = { name = "仙器槽三",    artifact = true },
    [4]  = { name = "仙器槽四",    artifact = true },
    [5]  = { name = "仙器槽五",    artifact = true },
    [6]  = { name = "仙器槽六",    artifact = true },
    [7]  = { name = "仙器槽七",    artifact = true },
    [8]  = { name = "仙器槽八",    artifact = true },
    [9]  = { name = "仙器槽九",    artifact = true },
    [10] = { name = "仙器槽十",    artifact = true },
    [11] = { name = "头部扩展槽",  invSlotName = "HeadSlot",          equipLocs = { INVTYPE_HEAD = true } },
    [12] = { name = "颈部扩展槽",  invSlotName = "NeckSlot",          equipLocs = { INVTYPE_NECK = true } },
    [13] = { name = "肩部扩展槽",  invSlotName = "ShoulderSlot",      equipLocs = { INVTYPE_SHOULDER = true } },
    [14] = { name = "衬衣扩展槽",  invSlotName = "ShirtSlot",         equipLocs = { INVTYPE_BODY = true } },
    [15] = { name = "胸部扩展槽",  invSlotName = "ChestSlot",         equipLocs = { INVTYPE_CHEST = true, INVTYPE_ROBE = true } },
    [16] = { name = "腰带扩展槽",  invSlotName = "WaistSlot",         equipLocs = { INVTYPE_WAIST = true } },
    [17] = { name = "腿部扩展槽",  invSlotName = "LegsSlot",          equipLocs = { INVTYPE_LEGS = true } },
    [18] = { name = "脚部扩展槽",  invSlotName = "FeetSlot",          equipLocs = { INVTYPE_FEET = true } },
    [19] = { name = "护腕扩展槽",  invSlotName = "WristSlot",         equipLocs = { INVTYPE_WRIST = true } },
    [20] = { name = "手套扩展槽",  invSlotName = "HandsSlot",         equipLocs = { INVTYPE_HAND = true } },
    [21] = { name = "戒指扩展槽1", invSlotName = "Finger0Slot",       equipLocs = { INVTYPE_FINGER = true } },
    [22] = { name = "戒指扩展槽2", invSlotName = "Finger1Slot",       equipLocs = { INVTYPE_FINGER = true } },
    [23] = { name = "饰品扩展槽1", invSlotName = "Trinket0Slot",      equipLocs = { INVTYPE_TRINKET = true } },
    [24] = { name = "饰品扩展槽2", invSlotName = "Trinket1Slot",      equipLocs = { INVTYPE_TRINKET = true } },
    [25] = { name = "披风扩展槽",  invSlotName = "BackSlot",          equipLocs = { INVTYPE_CLOAK = true } },
    [26] = { name = "主手扩展槽",  invSlotName = "MainHandSlot",      weapon = true, equipLocs = {
        INVTYPE_WEAPON = true, INVTYPE_2HWEAPON = true, INVTYPE_WEAPONMAINHAND = true,
        INVTYPE_WEAPONOFFHAND = true, INVTYPE_SHIELD = true, INVTYPE_HOLDABLE = true } },
    [27] = { name = "副手扩展槽",  invSlotName = "SecondaryHandSlot", weapon = true, equipLocs = {
        INVTYPE_WEAPON = true, INVTYPE_2HWEAPON = true, INVTYPE_WEAPONMAINHAND = true,
        INVTYPE_WEAPONOFFHAND = true, INVTYPE_SHIELD = true, INVTYPE_HOLDABLE = true } },
    [28] = { name = "远程扩展槽",  invSlotName = "RangedSlot",        weapon = true, equipLocs = {
        INVTYPE_RANGED = true, INVTYPE_RANGEDRIGHT = true, INVTYPE_THROWN = true, INVTYPE_RELIC = true } },
    [29] = { name = "战袍扩展槽",  invSlotName = "TabardSlot",        equipLocs = { INVTYPE_TABARD = true } },
}

local function Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff66ccff[仙器系统]|r " .. tostring(msg))
end

local function SendCommand(command)
    if command and command ~= "" then
        SendChatMessage(command, "SAY")
    end
end

local function RequestData()
    SendCommand(".仙器 查看")
end

-- ============================================================
-- 图标缓存（GetItemInfo 未缓存时显示问号并异步补图）
-- ============================================================

local function GetCachedItemIcon(itemId)
    XianqiSystemUIDB.iconCache = XianqiSystemUIDB.iconCache or {}
    local cached = XianqiSystemUIDB.iconCache[itemId]
    if cached then
        return cached
    end

    local icon = GetItemIcon and GetItemIcon(itemId)
    if not icon then
        local _, _, _, _, _, _, _, _, _, texture = GetItemInfo(itemId)
        icon = texture
    end

    if icon then
        XianqiSystemUIDB.iconCache[itemId] = icon
        return icon
    end

    pendingIcons[itemId] = true
    -- 触发客户端向服务器请求物品信息
    GameTooltip:SetOwner(UIParent, "ANCHOR_NONE")
    GameTooltip:SetHyperlink("item:" .. itemId)
    GameTooltip:Hide()
    return nil
end

-- ============================================================
-- 主框架：全部槽位停靠在官方角色面板上，透明无边框
-- ============================================================

local UI = CreateFrame("Frame", "XianqiSystemUIFrame", PaperDollFrame or CharacterFrame)
UI:SetAllPoints()
-- 抬高图层，避免被官方武器座等装饰贴图盖住
UI:SetFrameLevel(((CharacterFrame and CharacterFrame:GetFrameLevel()) or 1) + 10)
local extPanel = UI

local title = UI:CreateFontString(nil, "OVERLAY", "GameFontNormal")
title:SetText("|cffffd700仙器装备|r")

-- ============================================================
-- 槽位按钮
-- ============================================================

local function GetEmptySlotTexture(slotId)
    local config = SLOT_CONFIG[slotId]
    if config.artifact then
        return "Interface\\Paperdoll\\UI-PaperDoll-Slot-Relic"
    end

    if config.invSlotName and GetInventorySlotInfo then
        local ok, _, texture = pcall(GetInventorySlotInfo, config.invSlotName)
        if ok and texture then
            return texture
        end
    end

    return "Interface\\Paperdoll\\UI-Backpack-EmptySlot"
end

local function UpdateSlot(slotId)
    local button = slotButtons[slotId]
    if not button then
        return
    end

    local data = equipData[slotId]
    local unlocked = unlockedSlots[slotId]

    button.lockIcon:Hide()
    button.icon:SetVertexColor(1, 1, 1)

    if not unlocked then
        button.icon:SetTexture(GetEmptySlotTexture(slotId))
        button.icon:SetVertexColor(0.35, 0.35, 0.35)
        button.lockIcon:Show()
    elseif data and data.itemId then
        local icon = GetCachedItemIcon(data.itemId)
        button.icon:SetTexture(icon or QUESTION_MARK_ICON)
    else
        button.icon:SetTexture(GetEmptySlotTexture(slotId))
    end
end

local function UpdateAllSlots()
    for slotId = SLOT_FIRST, SLOT_LAST do
        UpdateSlot(slotId)
    end
end

-- ============================================================
-- 背包来源定位（穿戴命令需要 bag/slot）
-- ============================================================

local function RecordDragSource(bag, slot)
    if type(bag) == "number" and type(slot) == "number" and bag >= 0 and bag <= 4 then
        local itemId = GetContainerItemID and GetContainerItemID(bag, slot)
        dragSource = { bag = bag, slot = slot, itemId = itemId }
    end
end

hooksecurefunc("PickupContainerItem", function(bag, slot)
    RecordDragSource(bag, slot)
end)

local function FindItemInBags(itemId)
    for bag = 0, 4 do
        local numSlots = GetContainerNumSlots(bag)
        if numSlots and numSlots > 0 then
            for slot = 1, numSlots do
                if GetContainerItemID(bag, slot) == itemId then
                    return bag, slot
                end
            end
        end
    end
    return nil, nil
end

local delayFrame = CreateFrame("Frame")
local delayQueue = {}

local function DelayOnUpdate(self, elapsed)
    for i = #delayQueue, 1, -1 do
        local entry = delayQueue[i]
        entry.remaining = entry.remaining - elapsed
        if entry.remaining <= 0 then
            table.remove(delayQueue, i)
            entry.func()
        end
    end
    if #delayQueue == 0 then
        self:SetScript("OnUpdate", nil)
    end
end

local function After(seconds, func)
    table.insert(delayQueue, { remaining = seconds, func = func })
    delayFrame:SetScript("OnUpdate", DelayOnUpdate)
end

local function EquipCursorItem(slotId)
    local cursorType, itemId = GetCursorInfo()
    if cursorType ~= "item" or not itemId then
        return false
    end

    local bag, slot
    if dragSource and dragSource.itemId == itemId then
        bag, slot = dragSource.bag, dragSource.slot
    end

    -- 放回背包后再发命令（服务端按 bag/slot 取物品）
    ClearCursor()

    After(0.1, function()
        if not bag or GetContainerItemID(bag, slot) ~= itemId then
            bag, slot = FindItemInBags(itemId)
        end

        if not bag then
            Print("未能在背包中定位该物品，请重试。")
            return
        end

        SendCommand(string.format(".仙器 装备 %d %d %d", slotId, bag, slot))
        dragSource = nil
    end)

    return true
end

-- ============================================================
-- 光标高亮（拖着装备时点亮可放置的扩展槽）
-- ============================================================

local function UpdateCursorHighlights()
    local cursorType, itemId = GetCursorInfo()
    local equipLoc
    if cursorType == "item" and itemId then
        local _, _, _, _, _, _, _, _, loc = GetItemInfo(itemId)
        equipLoc = loc
    end

    for slotId = SLOT_FIRST, SLOT_LAST do
        local button = slotButtons[slotId]
        if button then
            local config = SLOT_CONFIG[slotId]
            local highlight = equipLoc and config.equipLocs and config.equipLocs[equipLoc] and unlockedSlots[slotId]
            if highlight then
                button.selected:Show()
            else
                button.selected:Hide()
            end
        end
    end
end

local cursorWatcher = CreateFrame("Frame")
local watcherElapsed = 0
cursorWatcher:SetScript("OnUpdate", function(_, elapsed)
    if not UI:IsVisible() then
        return
    end
    watcherElapsed = watcherElapsed + elapsed
    if watcherElapsed >= 0.1 then
        watcherElapsed = 0
        UpdateCursorHighlights()
    end
end)

-- ============================================================
-- 槽位交互
-- ============================================================

local function ConfirmUnlock(slotId)
    StaticPopupDialogs["XIANQI_UNLOCK_CONFIRM"] = {
        text = "确认解锁【" .. SLOT_CONFIG[slotId].name .. "】？\n（若配置了解锁需求会消耗对应材料）",
        button1 = "确认",
        button2 = "取消",
        OnAccept = function()
            SendCommand(".仙器 解锁 " .. slotId)
        end,
        timeout = 0,
        whileDead = true,
        hideOnEscape = true,
    }
    StaticPopup_Show("XIANQI_UNLOCK_CONFIRM")
end

local function SlotOnClick(self, mouseButton)
    local slotId = self.slotId

    if mouseButton == "RightButton" then
        if equipData[slotId] then
            SendCommand(".仙器 卸下 " .. slotId)
        end
        return
    end

    -- 左键：手持物品 → 穿戴；锁定槽 → 双击解锁
    if GetCursorInfo() == "item" then
        if not unlockedSlots[slotId] then
            Print("该槽位尚未解锁。")
            ClearCursor()
            return
        end
        EquipCursorItem(slotId)
        return
    end

    if not unlockedSlots[slotId] then
        local now = GetTime()
        if self.lastClick and now - self.lastClick < 0.3 then
            self.lastClick = nil
            ConfirmUnlock(slotId)
        else
            self.lastClick = now
        end
        return
    end

    -- 空手点已装备槽：聊天框输出链接
    local data = equipData[slotId]
    if data and data.itemId then
        local _, link = GetItemInfo(data.itemId)
        if link then
            Print(SLOT_CONFIG[slotId].name .. ": " .. link)
        end
    end
end

local function SlotOnEnter(self)
    local slotId = self.slotId
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")

    local data = equipData[slotId]
    if data and data.itemId then
        GameTooltip:SetHyperlink("item:" .. data.itemId)
        if SLOT_CONFIG[slotId].weapon then
            GameTooltip:AddLine("武器/盾牌扩展槽仅属性生效，不参与攻击。", 1, 0.6, 0.2)
        end
        GameTooltip:AddLine("右键卸下", 0.7, 0.7, 0.7)
    else
        GameTooltip:SetText(SLOT_CONFIG[slotId].name, 1, 0.82, 0)
        if not unlockedSlots[slotId] then
            GameTooltip:AddLine("未解锁 - 双击解锁该槽位", 1, 0.3, 0.3)
        elseif SLOT_CONFIG[slotId].artifact then
            GameTooltip:AddLine("放入对应的仙器以激活强大力量。", 0.7, 0.7, 0.7)
        else
            GameTooltip:AddLine("可放入对应部位的任意装备（仅属性生效）。", 0.7, 0.7, 0.7)
        end
    end

    GameTooltip:Show()
end

local function CreateSlotButton(slotId, parent, size)
    local button = CreateFrame("Button", "XianqiSlot" .. slotId, parent or UI)
    button:SetSize(size or SLOT_SIZE, size or SLOT_SIZE)
    button.slotId = slotId
    button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    button:RegisterForDrag("LeftButton")

    local bg = button:CreateTexture(nil, "BACKGROUND")
    bg:SetAllPoints()
    bg:SetTexture("Interface\\Buttons\\UI-EmptySlot-Disabled")
    bg:SetTexCoord(0.15, 0.85, 0.15, 0.85)

    local icon = button:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 2, -2)
    icon:SetPoint("BOTTOMRIGHT", -2, 2)
    button.icon = icon

    local selected = button:CreateTexture(nil, "OVERLAY")
    selected:SetAllPoints()
    selected:SetTexture("Interface\\Buttons\\ButtonHilight-Square")
    selected:SetBlendMode("ADD")
    selected:SetVertexColor(0.2, 1.0, 0.2)
    selected:Hide()
    button.selected = selected

    local lockIcon = button:CreateTexture(nil, "OVERLAY")
    lockIcon:SetSize(16, 16)
    lockIcon:SetPoint("CENTER")
    lockIcon:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-LOCK")
    lockIcon:SetTexCoord(0, 0.71875, 0, 0.875)
    lockIcon:Hide()
    button.lockIcon = lockIcon

    button:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")

    button:SetScript("OnClick", SlotOnClick)
    button:SetScript("OnReceiveDrag", function(self)
        if GetCursorInfo() == "item" then
            if unlockedSlots[self.slotId] then
                EquipCursorItem(self.slotId)
            else
                Print("该槽位尚未解锁。")
                ClearCursor()
            end
        end
    end)
    button:SetScript("OnEnter", SlotOnEnter)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)

    return button
end

-- 布局：
--   仙器槽 2×5 在独立小面板
--   扩展槽逐个锚定到官方装备格外侧：左列贴官方左列、右列贴官方右列、武器贴官方武器下方
--   （行与官方完全对齐，随角色面板显示/隐藏）
local EXT_SLOT_ANCHORS = {
    -- 左列（官方左列：头/颈/肩/披风/胸/衬衣/战袍/护腕）
    [11] = { official = "CharacterHeadSlot",          side = "LEFT" },
    [12] = { official = "CharacterNeckSlot",          side = "LEFT" },
    [13] = { official = "CharacterShoulderSlot",      side = "LEFT" },
    [25] = { official = "CharacterBackSlot",          side = "LEFT" },
    [15] = { official = "CharacterChestSlot",         side = "LEFT" },
    [14] = { official = "CharacterShirtSlot",         side = "LEFT" },
    [29] = { official = "CharacterTabardSlot",        side = "LEFT" },
    [19] = { official = "CharacterWristSlot",         side = "LEFT" },
    -- 右列（官方右列：手套/腰带/腿/脚/戒1/戒2/饰1/饰2）
    [20] = { official = "CharacterHandsSlot",         side = "RIGHT" },
    [16] = { official = "CharacterWaistSlot",         side = "RIGHT" },
    [17] = { official = "CharacterLegsSlot",          side = "RIGHT" },
    [18] = { official = "CharacterFeetSlot",          side = "RIGHT" },
    [21] = { official = "CharacterFinger0Slot",       side = "RIGHT" },
    [22] = { official = "CharacterFinger1Slot",       side = "RIGHT" },
    [23] = { official = "CharacterTrinket0Slot",      side = "RIGHT" },
    [24] = { official = "CharacterTrinket1Slot",      side = "RIGHT" },
    -- 底部（官方武器槽正下方）
    [26] = { official = "CharacterMainHandSlot",      side = "BOTTOM" },
    [27] = { official = "CharacterSecondaryHandSlot", side = "BOTTOM" },
    [28] = { official = "CharacterRangedSlot",        side = "BOTTOM" },
}

-- 与官方框体的间距（可微调）
local EXT_SIDE_GAP_LEFT = 5     -- 左列与官方左列的水平间距
local EXT_SIDE_GAP_RIGHT = 5    -- 右列与官方右列的水平间距
local EXT_BOTTOM_GAP = 38       -- 武器行与官方武器格的垂直间距
local ARTIFACT_GAP = -4         -- 仙器槽与扩展槽右列的水平间距（负值=可见图案贴合）
local ARTIFACT_COL_PAD = -10    -- 仙器槽两列间距（负值抵消贴图透明边距）
local ARTIFACT_ROW_PAD = -10    -- 仙器槽行间距（负值抵消贴图透明边距）

-- 仙器槽块顶部对齐官方右列第一格
local ARTIFACT_TOP_ANCHOR = "CharacterHandsSlot"

local function LayoutSlots()
    -- 扩展槽：锚定官方装备格，尺寸直接取官方格实际宽高保证完全一致
    for slotId, anchor in pairs(EXT_SLOT_ANCHORS) do
        local official = _G[anchor.official]
        local button = CreateSlotButton(slotId, extPanel)
        if official then
            button:SetSize(official:GetWidth(), official:GetHeight())
            if anchor.side == "LEFT" then
                button:SetPoint("RIGHT", official, "LEFT", -EXT_SIDE_GAP_LEFT, 0)
            elseif anchor.side == "RIGHT" then
                button:SetPoint("LEFT", official, "RIGHT", EXT_SIDE_GAP_RIGHT, 0)
            else
                button:SetPoint("TOP", official, "BOTTOM", 0, -EXT_BOTTOM_GAP)
            end
        else
            -- 官方按钮不存在时兜底挂在面板左上角，避免报错
            button:SetSize(37, 37)
            button:SetPoint("TOPLEFT", extPanel, "TOPLEFT", 0, 0)
        end
        slotButtons[slotId] = button
    end

    -- 仙器槽：紧凑 2列×5行，停靠在扩展槽右列外侧，顶部与官方右列第一格对齐
    local topAnchor = _G[ARTIFACT_TOP_ANCHOR]
    for i = 1, 10 do
        local row = math.floor((i - 1) / 2)
        local col = (i - 1) % 2
        local button = CreateSlotButton(i, extPanel)
        if topAnchor then
            local w = topAnchor:GetWidth()
            local h = topAnchor:GetHeight()
            button:SetSize(w, h)
            local offsetX = EXT_SIDE_GAP_RIGHT + w + ARTIFACT_GAP + col * (w + ARTIFACT_COL_PAD)
            local offsetY = -row * (h + ARTIFACT_ROW_PAD)
            button:SetPoint("TOPLEFT", topAnchor, "TOPRIGHT", offsetX, offsetY)
        else
            button:SetSize(37, 37)
            button:SetPoint("TOPLEFT", extPanel, "TOPLEFT", 0, 0)
        end
        slotButtons[i] = button
    end

    -- 标题：悬在仙器槽块上方
    if slotButtons[1] then
        title:ClearAllPoints()
        title:SetPoint("BOTTOMLEFT", slotButtons[1], "TOPLEFT", 0, 6)
    end
end

LayoutSlots()

-- ============================================================
-- 协议解析
-- U= 包重置全部状态（可带 ;E= 同包），E= 续包增量合并
-- ============================================================

local function ParseEquipEntries(payload)
    for entry in string.gmatch(payload or "", "[^,]+") do
        local slotStr, itemIdStr, guidStr = string.match(entry, "^(%d+):(%d+):(%d+)$")
        local slotId = tonumber(slotStr)
        local itemId = tonumber(itemIdStr)
        if slotId and itemId and SLOT_CONFIG[slotId] then
            equipData[slotId] = { itemId = itemId, itemGuid = tonumber(guidStr) or 0 }
        end
    end
end

local function HandleServerMessage(message)
    if not message or message == "" then
        return
    end

    local bitmapStr, rest = string.match(message, "^U=(%d+)(.*)$")
    if bitmapStr then
        local bitmap = tonumber(bitmapStr) or 0
        unlockedSlots = {}
        equipData = {}
        for slotId = SLOT_FIRST, SLOT_LAST do
            local bit = 2 ^ (slotId - 1)
            if math.floor(bitmap / bit) % 2 == 1 then
                unlockedSlots[slotId] = true
            end
        end

        local equipPayload = string.match(rest or "", "^;E=(.*)$")
        if equipPayload then
            ParseEquipEntries(equipPayload)
        end

        UpdateAllSlots()
        return
    end

    local equipPayload = string.match(message, "^E=(.*)$")
    if equipPayload then
        ParseEquipEntries(equipPayload)
        UpdateAllSlots()
        return
    end
end

-- ============================================================
-- 入口图标 + 插件管理器集成
-- ============================================================

local function ToggleUI()
    -- 槽位停靠在角色面板上，入口即开关角色面板
    ToggleCharacter("PaperDollFrame")
end

local function SaveIconPosition(button)
    if not button or not button.GetPoint then
        return
    end

    local point, _, _, x, y = button:GetPoint()
    if point and x and y then
        XianqiSystemUIDB.iconPosition = { point = point, x = x, y = y }
    end

    local pm = _G.PluginManagerClient
    if pm and pm.SaveFramePosition then
        pm:SaveFramePosition(PLUGIN_NAME, button)
    end
end

local function CreateIconButton()
    if iconButton then
        return iconButton
    end

    iconButton = CreateFrame("Button", "XianqiSystemIconButton", UIParent)
    iconButton:SetSize(ICON_SIZE, ICON_SIZE)
    iconButton:SetFrameStrata("MEDIUM")
    iconButton:SetFrameLevel(10)
    iconButton:SetMovable(true)
    iconButton:EnableMouse(true)
    iconButton:RegisterForDrag("LeftButton")
    if iconButton.SetClampedToScreen then
        iconButton:SetClampedToScreen(true)
    end

    local pos = XianqiSystemUIDB.iconPosition or { point = "TOP", x = 130, y = -10 }
    iconButton:SetPoint(pos.point or "TOP", UIParent, pos.point or "TOP", pos.x or 130, pos.y or -10)

    iconButton:SetNormalTexture(ICON_TEXTURE)
    local normalTexture = iconButton:GetNormalTexture()
    if normalTexture then
        normalTexture:SetAllPoints(iconButton)
        normalTexture:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    end

    iconButton:SetPushedTexture(ICON_TEXTURE)
    local pushedTexture = iconButton:GetPushedTexture()
    if pushedTexture then
        pushedTexture:SetAllPoints(iconButton)
        pushedTexture:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    end

    iconButton:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
    local highlight = iconButton:GetHighlightTexture()
    if highlight then
        highlight:SetAllPoints(iconButton)
    end

    local text = iconButton:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    text:SetPoint("TOP", iconButton, "BOTTOM", 0, -1)
    text:SetText("|cffffd700仙器|r")

    local isDragging = false
    local dragStartTime = 0

    iconButton:SetScript("OnDragStart", function(self)
        isDragging = true
        dragStartTime = GetTime and GetTime() or 0
        GameTooltip:Hide()
        self:StartMoving()
    end)

    iconButton:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        isDragging = false
        SaveIconPosition(self)
    end)

    iconButton:SetScript("OnMouseDown", function(_, button)
        if button == "LeftButton" then
            dragStartTime = GetTime and GetTime() or 0
        end
    end)

    iconButton:SetScript("OnMouseUp", function(_, button)
        local now = GetTime and GetTime() or 0
        if button == "LeftButton" and not isDragging and now - dragStartTime < 0.3 then
            ToggleUI()
        end
    end)

    iconButton:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText("仙器装备", 1, 1, 1)
        GameTooltip:AddLine("左键: 打开界面", 0.8, 0.8, 0.8)
        GameTooltip:AddLine("拖拽: 移动位置", 0.8, 0.8, 0.8)
        GameTooltip:Show()
    end)

    iconButton:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)

    iconButton:Hide()
    return iconButton
end

local function ApplyPluginManagerConfig(posX, posY, width, height, enabled)
    pluginManagerConfigApplied = true

    if not enabled then
        if iconButton then
            iconButton:Hide()
        end
        UI:Hide()
        return
    end

    UI:Show()

    local button = CreateIconButton()
    button:SetSize(ICON_SIZE, ICON_SIZE)

    if posX and posY then
        local parentWidth = UIParent:GetWidth() or 0
        local parentHeight = UIParent:GetHeight() or 0
        if parentWidth > 0 then
            posX = math.max(0, math.min(posX, parentWidth - button:GetWidth()))
        end
        if parentHeight > 0 then
            posY = math.max(0, math.min(posY, parentHeight - button:GetHeight()))
        end

        button:ClearAllPoints()
        button:SetPoint("BOTTOMLEFT", UIParent, "BOTTOMLEFT", posX, posY)
        XianqiSystemUIDB.iconPosition = { point = "BOTTOMLEFT", x = posX, y = posY }
    end

    button:Show()
    button:SetFrameStrata("MEDIUM")
    button:SetFrameLevel(10)
end

local function RegisterPluginManager()
    if pluginManagerRegistered then
        return true
    end

    local pm = _G.PluginManagerClient
    if pm and pm.RegisterPlugin then
        pm:RegisterPlugin(PLUGIN_NAME, function(message, parts)
            if parts and parts[1] == ".plugincfg" and parts[2] == PLUGIN_NAME then
                ApplyPluginManagerConfig(
                    tonumber(parts[3]),
                    tonumber(parts[4]),
                    tonumber(parts[5]),
                    tonumber(parts[6]),
                    tonumber(parts[7]) == 1)
            end
        end)
        pluginManagerRegistered = true
        return true
    end

    return false
end

local function SchedulePluginManagerRegistration()
    local elapsed = 0
    local timer = CreateFrame("Frame")
    timer:SetScript("OnUpdate", function(self, delta)
        elapsed = elapsed + delta
        RegisterPluginManager()

        if elapsed >= 5 then
            self:SetScript("OnUpdate", nil)
            if not pluginManagerConfigApplied then
                CreateIconButton():Show()
            end
        end
    end)
end

-- ============================================================
-- 事件
-- ============================================================

local eventFrame = CreateFrame("Frame")
eventFrame:RegisterEvent("ADDON_LOADED")
eventFrame:RegisterEvent("CHAT_MSG_ADDON")
eventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
eventFrame:RegisterEvent("GET_ITEM_INFO_RECEIVED")
eventFrame:SetScript("OnEvent", function(_, event, ...)
    if event == "ADDON_LOADED" then
        local loaded = ...
        if loaded == ADDON_NAME then
            if RegisterAddonMessagePrefix then
                pcall(RegisterAddonMessagePrefix, PREFIX)
            elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
                pcall(C_ChatInfo.RegisterAddonMessagePrefix, PREFIX)
            end
        end
        return
    end

    if event == "PLAYER_ENTERING_WORLD" then
        After(3, RequestData)
        return
    end

    if event == "GET_ITEM_INFO_RECEIVED" then
        local itemId = ...
        if itemId and pendingIcons[itemId] then
            pendingIcons[itemId] = nil
            UpdateAllSlots()
        end
        return
    end

    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        if prefix == PREFIX then
            HandleServerMessage(message)
        end
    end
end)

SLASH_XIANQISYSTEMUI1 = "/仙器"
SLASH_XIANQISYSTEMUI2 = "/xq"
SLASH_XIANQISYSTEMUI3 = "/xianqi"
SlashCmdList.XIANQISYSTEMUI = function()
    ToggleUI()
end

UI:SetScript("OnShow", function()
    RequestData()
    UpdateAllSlots()
end)

RegisterPluginManager()
SchedulePluginManagerRegistration()

_G.XianqiSystemUI = UI
