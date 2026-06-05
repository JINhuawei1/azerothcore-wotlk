local ADDON_NAME = ...
local PREFIX = "XIANMEN"
local PLUGIN_NAME = "XianmenSystemUI"
local ICON_TEXTURE = "Interface\\Icons\\Spell_Nature_NatureGuardian"
local ICON_SIZE = 20

XianmenSystemUIDB = XianmenSystemUIDB or {}

local UI = CreateFrame("Frame", "XianmenSystemUIFrame", UIParent)
UI:SetSize(760, 520)
UI:SetPoint("CENTER")
UI:SetFrameStrata("DIALOG")
UI:EnableMouse(true)
UI:SetMovable(true)
UI:RegisterForDrag("LeftButton")
UI:SetScript("OnDragStart", UI.StartMoving)
UI:SetScript("OnDragStop", UI.StopMovingOrSizing)
UI:Hide()

UI:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 14,
    insets = { left = 4, right = 4, top = 4, bottom = 4 },
})
UI:SetBackdropColor(0.04, 0.06, 0.07, 0.96)
UI:SetBackdropBorderColor(0.36, 0.82, 0.72, 0.95)

local state = {
    factionId = 0,
    factionName = "",
    level = 0,
    contribution = 0,
    unlockSlots = 0,
    leaderGuid = 0,
    isLeader = false,
    unlocked = {},
    active = {},
}

local factions = {}
local skills = {}
local selectedActive = {}
local currentTab = "factions"
local rows = {}
local iconButton = nil
local pluginManagerRegistered = false
local pluginManagerConfigApplied = false

local factionDescriptions = {
    [1] = "剑修流派，主打物理爆发，被动触发剑气与流血；核心属性为攻击强度、暴击、切割。",
    [2] = "丹道流派，主打采集与炼丹，通过属性丹永久累积属性；核心玩法为采集产出、炼丹、永久属性提升。",
    [3] = "符法流派，主打法术爆发，被动触发多重与连锁效果；核心属性为法术强度、急速、魔次。",
    [4] = "炼体流派，主打坦克、反伤与超高耐久；核心属性为耐力、护甲、超大生命。",
    [5] = "驭魂流派，主打武魂召唤，随门派修为提升多武魂与继承能力；核心玩法为召唤物强化、武魂继承。",
}

local chunkBuffer = {
    total = 0,
    received = 0,
    chunks = {},
}

local function Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff66ffcc[仙门系统]|r " .. tostring(msg))
end

local function Split(text, sep)
    local result = {}
    if not text or text == "" then
        return result
    end

    sep = sep or ","
    for part in string.gmatch(text, "([^" .. sep .. "]+)") do
        table.insert(result, part)
    end
    return result
end

local function ParseIdSet(text)
    local set = {}
    if text and text ~= "" then
        for id in string.gmatch(text, "(%d+)") do
            set[tonumber(id)] = true
        end
    end
    return set
end

local function Send(message)
    if not message or message == "" then
        return
    end

    local playerName = UnitName("player")
    if not playerName then
        return
    end

    if SendAddonMessage then
        SendAddonMessage(PREFIX, message, "WHISPER", playerName)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(PREFIX, message, "WHISPER", playerName)
    end
end

local function MakeText(parent, size, color)
    local text = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    text:SetJustifyH("LEFT")
    text:SetJustifyV("TOP")
    text:SetFont(STANDARD_TEXT_FONT, size or 12, "")
    if color then
        text:SetTextColor(color.r, color.g, color.b)
    end
    return text
end

local function MakeButton(parent, label, width, height)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetSize(width or 86, height or 24)
    button:SetText(label)
    return button
end

local function SetButtonEnabled(button, enabled)
    if not button then
        return
    end

    if button.SetEnabled then
        button:SetEnabled(enabled and true or false)
    elseif enabled then
        button:Enable()
    else
        button:Disable()
    end
end

local function ToggleUI()
    if UI:IsShown() then
        UI:Hide()
    else
        UI:Show()
        Send("REQ_ALL")
    end
end

local function SaveIconPosition(button)
    if not button or not button.GetPoint then
        return
    end

    local point, _, _, x, y = button:GetPoint()
    if point and x and y then
        XianmenSystemUIDB.iconPosition = {
            point = point,
            x = x,
            y = y,
        }
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

    iconButton = CreateFrame("Button", "XianmenSystemIconButton", UIParent)
    iconButton:SetSize(ICON_SIZE, ICON_SIZE)
    iconButton:SetFrameStrata("MEDIUM")
    iconButton:SetFrameLevel(10)
    iconButton:SetMovable(true)
    iconButton:EnableMouse(true)
    iconButton:RegisterForDrag("LeftButton")
    if iconButton.SetClampedToScreen then
        iconButton:SetClampedToScreen(true)
    end

    local pos = XianmenSystemUIDB.iconPosition or { point = "TOP", x = 100, y = -10 }
    iconButton:SetPoint(pos.point or "TOP", UIParent, pos.point or "TOP", pos.x or 100, pos.y or -10)

    iconButton:SetNormalTexture(ICON_TEXTURE)
    local normalTexture = iconButton:GetNormalTexture()
    if normalTexture then
        normalTexture:SetAllPoints(iconButton)
        normalTexture:SetTexCoord(0.07, 0.93, 0.07, 0.93)
        iconButton.icon = normalTexture
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
    text:SetText("|cff66ffcc仙门|r")
    iconButton.text = text

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
        GameTooltip:SetText("仙门系统", 1, 1, 1)
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
        XianmenSystemUIDB.iconPosition = {
            point = "BOTTOMLEFT",
            x = posX,
            y = posY,
        }
    end

    button:Show()
    button:SetFrameStrata("MEDIUM")
    button:SetFrameLevel(10)
end

local title = MakeText(UI, 18, { r = 0.40, g = 1.00, b = 0.84 })
title:SetPoint("TOPLEFT", 18, -16)
title:SetText("仙门系统")

local close = CreateFrame("Button", nil, UI, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

local statusText = MakeText(UI, 12, { r = 0.82, g = 0.88, b = 0.88 })
statusText:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -8)
statusText:SetSize(540, 38)

local refresh = MakeButton(UI, "刷新", 72, 24)
refresh:SetPoint("TOPRIGHT", -48, -42)
refresh:SetScript("OnClick", function()
    Send("REQ_ALL")
end)

local leave = MakeButton(UI, "退出仙门", 92, 24)
leave:SetPoint("RIGHT", refresh, "LEFT", -8, 0)
leave:SetScript("OnClick", function()
    StaticPopupDialogs["XIANMEN_LEAVE_CONFIRM"] = StaticPopupDialogs["XIANMEN_LEAVE_CONFIRM"] or {
        text = "确认退出当前仙门？",
        button1 = "确认",
        button2 = "取消",
        OnAccept = function() Send("LEAVE") end,
        timeout = 0,
        whileDead = true,
        hideOnEscape = true,
    }
    StaticPopup_Show("XIANMEN_LEAVE_CONFIRM")
end)

local tabPanel = CreateFrame("Frame", nil, UI)
tabPanel:SetPoint("TOPLEFT", 18, -72)
tabPanel:SetSize(724, 34)

local function SetTab(tab)
    currentTab = tab
    UI:Render()
end

local tabFactions = MakeButton(tabPanel, "门派", 78, 26)
tabFactions:SetPoint("LEFT", 0, 0)
tabFactions:SetScript("OnClick", function() SetTab("factions") end)

local tabSkills = MakeButton(tabPanel, "技能", 78, 26)
tabSkills:SetPoint("LEFT", tabFactions, "RIGHT", 8, 0)
tabSkills:SetScript("OnClick", function() SetTab("skills") end)

local tabLeader = MakeButton(tabPanel, "门主", 78, 26)
tabLeader:SetPoint("LEFT", tabSkills, "RIGHT", 8, 0)
tabLeader:SetScript("OnClick", function() SetTab("leader") end)

local content = CreateFrame("Frame", nil, UI)
content:SetPoint("TOPLEFT", 18, -112)
content:SetSize(724, 386)
content:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
content:SetBackdropColor(0.02, 0.03, 0.04, 0.82)
content:SetBackdropBorderColor(0.20, 0.42, 0.40, 0.9)

local function EnsureRows(count)
    for i = #rows + 1, count do
        local row = CreateFrame("Frame", nil, content)
        row:SetSize(692, 48)
        row.text = MakeText(row, 12, { r = 0.90, g = 0.92, b = 0.88 })
        row.text:SetPoint("LEFT", 10, 0)
        row.text:SetSize(560, 44)
        row.action = MakeButton(row, "操作", 92, 24)
        row.action:SetPoint("RIGHT", -10, 0)
        rows[i] = row
    end
end

local function HideRows()
    for _, row in ipairs(rows) do
        row:Hide()
        row.action:Show()
        row.action:SetScript("OnClick", nil)
    end
end

local function StatusLine()
    if state.factionId == 0 then
        return "尚未加入仙门"
    end

    local leader = state.isLeader and "是" or "否"
    return string.format("门派：%s  修为：%d/100  贡献：%s  解锁：%d/%d  门主：%s",
        state.factionName or "", state.level or 0, tostring(state.contribution or 0),
        UI:UnlockedCount(), state.unlockSlots or 0, leader)
end

function UI:UnlockedCount()
    local count = 0
    for _ in pairs(state.unlocked or {}) do
        count = count + 1
    end
    return count
end

local function ActiveSelectionCount()
    local count = 0
    for _ in pairs(selectedActive) do
        count = count + 1
    end
    return count
end

local function ResetSelectedActive()
    selectedActive = {}
    for id in pairs(state.active or {}) do
        selectedActive[id] = true
    end
end

function UI:RenderFactions()
    EnsureRows(math.max(#factions, 1))
    HideRows()

    if #factions == 0 then
        rows[1].text:SetText("暂无门派配置")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        return
    end

    for i, faction in ipairs(factions) do
        local row = rows[i]
        row:SetPoint("TOPLEFT", 16, -8 - (i - 1) * 52)
        local desc = faction.desc ~= "" and faction.desc or (factionDescriptions[faction.id] or "")
        row.text:SetText(string.format("%d. %s\n|cffb8c7bf%s|r", faction.id, faction.name, desc))
        row.action:SetText(state.factionId == faction.id and "已加入" or "加入")
        SetButtonEnabled(row.action, state.factionId == 0)
        row.action:SetScript("OnClick", function()
            Send("JOIN:" .. faction.id)
        end)
        row:Show()
    end
end

function UI:RenderSkills()
    EnsureRows(math.max(#skills, 1))
    HideRows()

    if state.factionId == 0 then
        rows[1].text:SetText("尚未加入仙门")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        return
    end

    if #skills == 0 then
        rows[1].text:SetText("暂无技能配置")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        return
    end

    for i, skill in ipairs(skills) do
        local row = rows[i]
        local unlocked = skill.unlocked == 1
        local active = skill.active == 1
        row:SetPoint("TOPLEFT", 16, -8 - (i - 1) * 36)
        row.text:SetText(string.format("%d. %s  [%s%s]\n%s",
            skill.id, skill.name, unlocked and "已解锁" or "未解锁", active and " / 生效" or "", skill.desc))
        row.action:SetText(unlocked and "已解锁" or "解锁")
        SetButtonEnabled(row.action, not unlocked)
        row.action:SetScript("OnClick", function()
            Send("UNLOCK:" .. skill.id)
        end)
        row:Show()
    end
end

function UI:RenderLeader()
    EnsureRows(math.max(#skills + 1, 1))
    HideRows()

    if state.factionId == 0 then
        rows[1].text:SetText("尚未加入仙门")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        return
    end

    local header = rows[1]
    header:SetPoint("TOPLEFT", 16, -10)
    header.text:SetText(string.format("当前门主GUID：%s  已选择：%d/5", tostring(state.leaderGuid or 0), ActiveSelectionCount()))
    header.action:SetText("设为生效")
    SetButtonEnabled(header.action, state.isLeader and ActiveSelectionCount() > 0)
    header.action:SetScript("OnClick", function()
        local list = {}
        for id in pairs(selectedActive) do
            table.insert(list, id)
        end
        table.sort(list)
        Send("SET_ACTIVE:" .. table.concat(list, ","))
    end)
    header:Show()

    for i, skill in ipairs(skills) do
        local row = rows[i + 1]
        local unlocked = skill.unlocked == 1
        local selected = selectedActive[skill.id] and true or false
        row:SetPoint("TOPLEFT", 16, -44 - (i - 1) * 34)
        row.text:SetText(string.format("%d. %s  [%s%s]\n%s",
            skill.id, skill.name, unlocked and "已解锁" or "未解锁", selected and " / 已选" or "", skill.desc))
        row.action:SetText(selected and "取消" or "选择")
        SetButtonEnabled(row.action, state.isLeader and unlocked)
        row.action:SetScript("OnClick", function()
            if selectedActive[skill.id] then
                selectedActive[skill.id] = nil
            else
                if ActiveSelectionCount() >= 5 then
                    Print("门主最多选择 5 个生效技能")
                    return
                end
                selectedActive[skill.id] = true
            end
            UI:Render()
        end)
        row:Show()
    end
end

function UI:Render()
    statusText:SetText(StatusLine())
    tabFactions:SetButtonState(currentTab == "factions" and "PUSHED" or "NORMAL")
    tabSkills:SetButtonState(currentTab == "skills" and "PUSHED" or "NORMAL")
    tabLeader:SetButtonState(currentTab == "leader" and "PUSHED" or "NORMAL")

    if currentTab == "factions" then
        UI:RenderFactions()
    elseif currentTab == "skills" then
        UI:RenderSkills()
    else
        UI:RenderLeader()
    end
end

local function ParseFactions(payload)
    factions = {}
    for entry in string.gmatch(payload or "", "[^~]+") do
        local id, name, desc = string.match(entry, "^(%d+)%^(.-)%^(.-)$")
        if not id then
            id, name = string.match(entry, "^(%d+)%^(.-)$")
            desc = ""
        end
        if id then
            table.insert(factions, {
                id = tonumber(id),
                name = name,
                desc = desc or factionDescriptions[tonumber(id)] or "",
            })
        end
    end
    table.sort(factions, function(a, b) return a.id < b.id end)
end

local function ParseState(payload)
    local factionId, factionName, level, contribution, unlockSlots, leaderGuid, isLeader, unlocked, active =
        string.match(payload or "", "^(%d+)|(.-)|(%d+)|(%d+)|(%d+)|(%d+)|(%d+)|(.*)|(.*)$")

    if not factionId then
        return
    end

    state.factionId = tonumber(factionId) or 0
    state.factionName = factionName or ""
    state.level = tonumber(level) or 0
    state.contribution = tonumber(contribution) or 0
    state.unlockSlots = tonumber(unlockSlots) or 0
    state.leaderGuid = tonumber(leaderGuid) or 0
    state.isLeader = tonumber(isLeader) == 1
    state.unlocked = ParseIdSet(unlocked)
    state.active = ParseIdSet(active)
    ResetSelectedActive()
end

local function ParseSkills(payload)
    skills = {}
    for entry in string.gmatch(payload or "", "[^~]+") do
        local id, factionId, order, name, stype, spellId, unlocked, active, desc =
            string.match(entry, "^(%d+)%^(%d+)%^(%d+)%^(.-)%^(%d+)%^(%d+)%^(%d+)%^(%d+)%^(.-)$")
        if id then
            table.insert(skills, {
                id = tonumber(id),
                factionId = tonumber(factionId),
                order = tonumber(order),
                name = name,
                type = tonumber(stype),
                spellId = tonumber(spellId),
                unlocked = tonumber(unlocked),
                active = tonumber(active),
                desc = desc,
            })
        end
    end
    table.sort(skills, function(a, b) return a.order < b.order end)
end

local function HandlePayload(message)
    if not message or message == "" then
        return
    end

    local chunkNum, totalChunks, chunkData = string.match(message, "^CHUNK:(%d+):(%d+):(.*)$")
    if chunkNum then
        chunkNum = tonumber(chunkNum)
        totalChunks = tonumber(totalChunks)
        if chunkNum == 1 then
            chunkBuffer.total = totalChunks
            chunkBuffer.received = 0
            chunkBuffer.chunks = {}
        end
        chunkBuffer.chunks[chunkNum] = chunkData
        chunkBuffer.received = chunkBuffer.received + 1
        if chunkBuffer.received >= chunkBuffer.total then
            local full = ""
            for i = 1, chunkBuffer.total do
                full = full .. (chunkBuffer.chunks[i] or "")
            end
            chunkBuffer.total = 0
            chunkBuffer.received = 0
            chunkBuffer.chunks = {}
            HandlePayload(full)
        end
        return
    end

    if message == "XM_OPEN" then
        UI:Show()
        return
    end

    local payload = string.match(message, "^XM_FACTIONS:(.*)$")
    if payload then
        ParseFactions(payload)
        UI:Render()
        return
    end

    payload = string.match(message, "^XM_STATE:(.*)$")
    if payload then
        ParseState(payload)
        UI:Render()
        return
    end

    payload = string.match(message, "^XM_SKILLS:(.*)$")
    if payload then
        ParseSkills(payload)
        UI:Render()
        return
    end

    payload = string.match(message, "^XM_RESULT:(.*)$")
    if payload then
        local action, ok, text = string.match(payload, "^(.-)%^(%d+)%^(.*)$")
        if text and text ~= "" then
            Print(text)
        end
        return
    end
end

local eventFrame = CreateFrame("Frame")
eventFrame:RegisterEvent("ADDON_LOADED")
eventFrame:RegisterEvent("CHAT_MSG_ADDON")
eventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "ADDON_LOADED" then
        local loaded = ...
        if loaded == ADDON_NAME then
            if RegisterAddonMessagePrefix then
                pcall(RegisterAddonMessagePrefix, PREFIX)
            elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
                pcall(C_ChatInfo.RegisterAddonMessagePrefix, PREFIX)
            end
            Send("REQ_ALL")
        end
        return
    end

    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        if prefix == PREFIX then
            HandlePayload(message)
        end
    end
end)

SLASH_XIANMENSYSTEMUI1 = "/仙门"
SLASH_XIANMENSYSTEMUI2 = "/xm"
SLASH_XIANMENSYSTEMUI3 = "/xianmen"
SlashCmdList.XIANMENSYSTEMUI = function()
    ToggleUI()
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

RegisterPluginManager()
SchedulePluginManagerRegistration()
UI:SetScript("OnShow", function()
    Send("REQ_ALL")
end)

_G.XianmenSystemUI = UI
