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

if UISpecialFrames then
    table.insert(UISpecialFrames, UI:GetName())
end

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
    maxLevel = 100,
    contribution = 0,
    historyContribution = 0,
    nextUpgradeRequirement = 0,
    unlockSlots = 0,
    leaderGuid = 0,
    isLeader = false,
    unlocked = {},
    active = {},
    personalActive = {},
    maxActiveSkills = 10,
    maxPersonalActiveSkills = 5,
}

local factions = {}
local skills = {}
local members = {}
local selectedActive = {}
local currentTab = "factions"
local rows = {}
local iconButton = nil
local pluginManagerRegistered = false
local pluginManagerConfigApplied = false
local memberPage = 1
local MEMBERS_PER_PAGE = 8
local leaderSelectionDirty = false
local CONTENT_INSET_X = 8
local CONTENT_INSET_Y = 8
local SCROLL_VIEW_WIDTH = 684
local SCROLL_VIEW_HEIGHT = 370
local SCROLL_CHILD_WIDTH = 660
local ROW_WIDTH = 660
local ResetScrollPosition

local factionDescriptions = {
    [1] = "剑修流派，主打物理爆发，被动触发剑气与流血；核心属性为攻击强度、暴击、切割。",
    [2] = "符法流派，主打法术爆发，被动触发多重与连锁效果；核心属性为法术强度、急速、魔次。",
    [3] = "炼体流派，主打坦克、反伤与超高耐久；核心属性为耐力、护甲、超大生命。",
    [4] = "驭魂流派，主打武魂召唤，随门派修为提升多武魂与继承能力；核心玩法为召唤物强化、武魂继承。",
}

local factionDescriptionColors = {
    [1] = "|cff66ccff",
    [2] = "|cffff88ff",
    [3] = "|cffffcc66",
    [4] = "|cffc088ff",
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

local function SplitFields(text, sep)
    local result = {}
    text = text or ""
    sep = sep or "|"

    local startPos = 1
    while true do
        local sepStart = string.find(text, sep, startPos, true)
        if not sepStart then
            table.insert(result, string.sub(text, startPos))
            break
        end

        table.insert(result, string.sub(text, startPos, sepStart - 1))
        startPos = sepStart + string.len(sep)
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
    if ResetScrollPosition then
        ResetScrollPosition()
    end
    if tab == "members" then
        Send("REQ_MEMBERS")
    end
    UI:Render()
end

local tabFactions = MakeButton(tabPanel, "门派", 78, 26)
tabFactions:SetPoint("LEFT", 0, 0)
tabFactions:SetScript("OnClick", function() SetTab("factions") end)

local tabSkills = MakeButton(tabPanel, "技能", 78, 26)
tabSkills:SetPoint("LEFT", tabFactions, "RIGHT", 8, 0)
tabSkills:SetScript("OnClick", function() SetTab("skills") end)

local tabActive = MakeButton(tabPanel, "生效", 78, 26)
tabActive:SetPoint("LEFT", tabSkills, "RIGHT", 8, 0)
tabActive:SetScript("OnClick", function() SetTab("active") end)

local tabLeader = MakeButton(tabPanel, "门主", 78, 26)
tabLeader:SetPoint("LEFT", tabActive, "RIGHT", 8, 0)
tabLeader:SetScript("OnClick", function() SetTab("leader") end)

local tabMembers = MakeButton(tabPanel, "成员", 78, 26)
tabMembers:SetPoint("LEFT", tabLeader, "RIGHT", 8, 0)
tabMembers:SetScript("OnClick", function() SetTab("members") end)

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

local scrollFrame = CreateFrame("ScrollFrame", "XianmenSystemUIContentScrollFrame", content, "UIPanelScrollFrameTemplate")
scrollFrame:SetPoint("TOPLEFT", CONTENT_INSET_X, -CONTENT_INSET_Y)
scrollFrame:SetSize(SCROLL_VIEW_WIDTH, SCROLL_VIEW_HEIGHT)
scrollFrame:EnableMouseWheel(true)

local scrollChild = CreateFrame("Frame", nil, scrollFrame)
scrollChild:SetSize(SCROLL_CHILD_WIDTH, SCROLL_VIEW_HEIGHT)
scrollFrame:SetScrollChild(scrollChild)

local scrollBar = _G["XianmenSystemUIContentScrollFrameScrollBar"]
if scrollBar then
    scrollBar:ClearAllPoints()
    scrollBar:SetPoint("TOPLEFT", scrollFrame, "TOPRIGHT", 4, -16)
    scrollBar:SetPoint("BOTTOMLEFT", scrollFrame, "BOTTOMRIGHT", 4, 16)
    scrollBar:SetMinMaxValues(0, 0)
    scrollBar:SetValueStep(36)
    scrollBar:SetValue(0)
    scrollBar:Hide()
end

local function SetScrollOffset(value)
    local maxValue = scrollFrame.scrollMax or 0
    value = tonumber(value) or 0
    if value < 0 then
        value = 0
    elseif value > maxValue then
        value = maxValue
    end

    scrollFrame:SetVerticalScroll(value)
    if scrollBar then
        scrollBar:SetValue(value)
    end
end

ResetScrollPosition = function()
    SetScrollOffset(0)
end

local function UpdateScrollContentHeight(height)
    height = math.max(SCROLL_VIEW_HEIGHT, tonumber(height) or SCROLL_VIEW_HEIGHT)
    scrollChild:SetHeight(height)
    scrollFrame.scrollMax = math.max(0, height - SCROLL_VIEW_HEIGHT)

    if scrollBar then
        scrollBar:SetMinMaxValues(0, scrollFrame.scrollMax)
        if scrollFrame.scrollMax > 0 then
            scrollBar:Show()
        else
            scrollBar:Hide()
        end
    end

    SetScrollOffset(scrollFrame:GetVerticalScroll() or 0)
end

if scrollBar then
    scrollBar:SetScript("OnValueChanged", function(self, value)
        scrollFrame:SetVerticalScroll(value or 0)
    end)
end

scrollFrame:SetScript("OnMouseWheel", function(self, delta)
    SetScrollOffset((self:GetVerticalScroll() or 0) - (delta or 0) * 36)
end)

local memberPager = CreateFrame("Frame", nil, content)
memberPager:SetSize(210, 26)
memberPager:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -16, 10)
memberPager:Hide()

local memberPrev = MakeButton(memberPager, "上一页", 70, 24)
memberPrev:SetPoint("LEFT", 0, 0)
memberPrev:SetScript("OnClick", function()
    if memberPage > 1 then
        memberPage = memberPage - 1
        UI:Render()
    end
end)

local memberPageText = MakeText(memberPager, 12, { r = 0.82, g = 0.88, b = 0.88 })
memberPageText:SetPoint("LEFT", memberPrev, "RIGHT", 8, 2)
memberPageText:SetSize(44, 18)

local memberNext = MakeButton(memberPager, "下一页", 70, 24)
memberNext:SetPoint("LEFT", memberPageText, "RIGHT", 8, -2)
memberNext:SetScript("OnClick", function()
    memberPage = memberPage + 1
    UI:Render()
end)

local function SetMemberPagerShown(shown)
    if shown then
        memberPager:Show()
    else
        memberPager:Hide()
    end
end

local function EnsureRows(count)
    for i = #rows + 1, count do
        local row = CreateFrame("Frame", nil, scrollChild)
        row:SetSize(ROW_WIDTH, 48)
        row.icon = CreateFrame("Button", nil, row)
        row.icon:SetSize(28, 28)
        row.icon:SetPoint("LEFT", 10, 0)
        row.icon:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
        row.icon:Hide()
        row.text = MakeText(row, 12, { r = 0.90, g = 0.92, b = 0.88 })
        row.text:SetPoint("LEFT", 10, 0)
        row.text:SetSize(560, 44)
        row.action = MakeButton(row, "操作", 92, 24)
        row.action:SetPoint("RIGHT", -4, 0)
        rows[i] = row
    end
end

local function HideRows()
    SetMemberPagerShown(false)
    UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
    for _, row in ipairs(rows) do
        row:Hide()
        row:ClearAllPoints()
        row.icon:Hide()
        row.icon:SetScript("OnEnter", nil)
        row.icon:SetScript("OnLeave", nil)
        row.text:ClearAllPoints()
        row.text:SetPoint("LEFT", 10, 0)
        row.text:SetSize(560, 44)
        row.action:Show()
        SetButtonEnabled(row.action, true)
        row.action:SetScript("OnClick", nil)
    end
end

local function GetSkillIconTexture(skill)
    if skill and skill.spellId and skill.spellId > 0 and GetSpellTexture then
        local texture = GetSpellTexture(skill.spellId)
        if texture then
            return texture
        end
    end

    return ICON_TEXTURE
end

local function FormatDecimal(value)
    value = tonumber(value) or 0
    if math.abs(value - math.floor(value)) < 0.001 then
        return tostring(math.floor(value))
    end

    return string.format("%.2f", value)
end

local function FormatSignedDecimal(value)
    value = tonumber(value) or 0
    local prefix = value >= 0 and "+" or "-"
    return prefix .. FormatDecimal(math.abs(value))
end

local function GetSkillCurrentValue(skill)
    local level = tonumber(state.level) or 0
    local base = tonumber(skill and skill.baseValue) or 0
    local growth = tonumber(skill and skill.growthPer10) or 0
    return base + growth * math.floor(level / 10)
end

local function IsLingfaGuantongSkill(skill)
    return skill
        and tonumber(skill.factionId) == 2
        and tonumber(skill.order) == 2
end

local function AddLingfaGuantongTooltip(base, growth, current, levelBonus, level, levelSteps)
    GameTooltip:AddLine(string.format("当前加成：法术暴击 +%s%%，法术穿透 +%s", FormatDecimal(current), FormatDecimal(current)), 0.66, 1.00, 0.66, true)
    if growth ~= 0 then
        GameTooltip:AddLine(string.format("等级加成：法术暴击 %s%%，法术穿透 %s（修为%d级，%d次成长）",
            FormatSignedDecimal(levelBonus), FormatSignedDecimal(levelBonus), level, levelSteps), 0.66, 1.00, 0.66, true)
    end
    GameTooltip:AddLine(string.format("成长：基础 暴击 %s%% / 穿透 %s，每10级 +%s%% / +%s",
        FormatDecimal(base), FormatDecimal(base), FormatDecimal(growth), FormatDecimal(growth)), 0.82, 0.82, 0.82, true)
end

local function AddSkillGeneratedTooltip(skill)
    if not skill then
        return
    end

    local desc = skill.desc or ""
    local trigger = skill.triggerParam or ""
    local base = tonumber(skill.baseValue) or 0
    local growth = tonumber(skill.growthPer10) or 0
    local level = tonumber(state.level) or 0
    local levelSteps = math.floor(level / 10)
    local levelBonus = growth * levelSteps
    local current = GetSkillCurrentValue(skill)

    if desc ~= "" then
        GameTooltip:AddLine(desc, 0.95, 0.55, 1.00, true)
    end

    if trigger ~= "" then
        GameTooltip:AddLine("触发条件：" .. trigger, 0.60, 0.85, 1.00, true)
    end

    if base ~= 0 or growth ~= 0 then
        if IsLingfaGuantongSkill(skill) then
            AddLingfaGuantongTooltip(base, growth, current, levelBonus, level, levelSteps)
        else
            local label = "当前数值"
            if skill.type == 1 then
                label = "当前加成"
            elseif skill.type == 2 then
                label = "当前触发系数"
            elseif skill.type == 3 then
                label = "当前系统系数"
            end

            GameTooltip:AddLine(string.format("%s：%s%%", label, FormatDecimal(current)), 0.66, 1.00, 0.66, true)
            if growth ~= 0 then
                GameTooltip:AddLine(string.format("等级加成：%s%%（修为%d级，%d次成长）", FormatSignedDecimal(levelBonus), level, levelSteps), 0.66, 1.00, 0.66, true)
            end
            GameTooltip:AddLine(string.format("成长：基础 %s%%，每10级 +%s%%", FormatDecimal(base), FormatDecimal(growth)), 0.82, 0.82, 0.82, true)
        end
    end
end

local function ShowSkillTooltip(owner)
    local skill = owner.skill
    if not skill then
        return
    end

    GameTooltip:SetOwner(owner, "ANCHOR_RIGHT")
    GameTooltip:ClearLines()
    GameTooltip:SetText(skill.name or "未知技能", 1, 0.82, 0.18)

    AddSkillGeneratedTooltip(skill)

    if skill.spellId and skill.spellId > 0 then
        GameTooltip:AddLine("法术ID：" .. tostring(skill.spellId), 1.00, 0.82, 0.00)
    end

    GameTooltip:Show()
end

local function ConfigureSkillRow(row, skill, displayStatus, tooltipStatus)
    row.icon:Show()
    row.icon:EnableMouse(true)
    row.icon:SetFrameLevel(row:GetFrameLevel() + 5)
    row.icon.skill = skill
    row.icon.skillStatus = tooltipStatus
    row.icon:SetNormalTexture(GetSkillIconTexture(skill))
    local texture = row.icon:GetNormalTexture()
    if texture then
        texture:SetAllPoints(row.icon)
        texture:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    end
    row.icon:SetScript("OnEnter", ShowSkillTooltip)
    row.icon:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)

    row.text:ClearAllPoints()
    row.text:SetPoint("LEFT", row.icon, "RIGHT", 8, 0)
    row.text:SetSize(470, 28)
    row.text:SetText(string.format("%s  %s", skill.name or "未知技能", displayStatus or ""))
end

local function FormatNumber(value)
    if type(value) == "string" then
        local text = string.match(value, "^%s*(.-)%s*$") or ""
        if text == "" then
            return "0"
        end
        if string.match(text, "^%d+$") then
            return text
        end
    end

    value = tonumber(value) or 0
    return tostring(math.floor(value))
end

local function ColorFactionDescription(factionId, desc)
    return string.format("%s%s|r", factionDescriptionColors[factionId] or "|cffb8c7bf", desc or "")
end

local PersonalActiveCount

local function StatusLine()
    if state.factionId == 0 then
        return "尚未加入仙门"
    end

    local leader = state.isLeader and "是" or "否"
    return string.format("门派：%s  修为：%d/%d  门主：%s\n当日贡献：%s  历史贡献：%s  解锁：%d/%d  生效：%d/%d",
        state.factionName or "", state.level or 0, state.maxLevel or 100, leader,
        FormatNumber(state.contribution), FormatNumber(state.historyContribution),
        UI:UnlockedCount(), state.unlockSlots or 0,
        PersonalActiveCount(), state.maxPersonalActiveSkills or 5)
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

local function IsValidPersonalActiveSkillId(id)
    id = tonumber(id)
    if not id then
        return false
    end

    for _, skill in ipairs(skills) do
        if skill.id == id then
            return skill.active == 1 and skill.unlocked == 1
        end
    end

    return false
end

function PersonalActiveCount()
    local count = 0
    for id in pairs(state.personalActive or {}) do
        if IsValidPersonalActiveSkillId(id) then
            count = count + 1
        end
    end
    return count
end

local function GetActiveSkillList()
    local result = {}
    for _, skill in ipairs(skills) do
        if skill.active == 1 then
            table.insert(result, skill)
        end
    end
    return result
end

local function EffectiveSkillCount(activeList)
    local count = 0
    for _, skill in ipairs(activeList or GetActiveSkillList()) do
        if skill.unlocked == 1 and ((state.personalActive and state.personalActive[skill.id]) or skill.personalActive == 1) then
            count = count + 1
        end
    end
    return count
end

local function BuildSelectedActiveList()
    local list = {}
    for id in pairs(selectedActive) do
        table.insert(list, tonumber(id) or id)
    end
    table.sort(list)
    for index, id in ipairs(list) do
        list[index] = tostring(id)
    end
    return list
end

local function SubmitSelectedActive()
    local list = BuildSelectedActiveList()
    Send("SET_ACTIVE:" .. table.concat(list, ","))
end

local function BuildPersonalActiveList()
    local list = {}
    local normalized = {}
    for id in pairs(state.personalActive or {}) do
        if IsValidPersonalActiveSkillId(id) then
            local numericId = tonumber(id) or id
            table.insert(list, numericId)
            normalized[numericId] = true
        end
    end
    state.personalActive = normalized
    table.sort(list)
    for index, id in ipairs(list) do
        list[index] = tostring(id)
    end
    return list
end

local function SubmitPersonalActive()
    local list = BuildPersonalActiveList()
    Send("SET_PERSONAL_ACTIVE:" .. table.concat(list, ","))
end

local function ResetSelectedActive()
    selectedActive = {}
    for id in pairs(state.active or {}) do
        selectedActive[id] = true
    end
    leaderSelectionDirty = false
end

local function ClearJoinedState()
    state.factionId = 0
    state.factionName = ""
    state.level = 0
    state.maxLevel = 100
    state.contribution = 0
    state.historyContribution = 0
    state.nextUpgradeRequirement = 0
    state.unlockSlots = 0
    state.leaderGuid = 0
    state.isLeader = false
    state.unlocked = {}
    state.active = {}
    state.personalActive = {}
    state.maxActiveSkills = 10
    state.maxPersonalActiveSkills = 5
    skills = {}
    members = {}
    selectedActive = {}
    leaderSelectionDirty = false
    memberPage = 1
    currentTab = "factions"
end

local function GetCurrentFaction()
    for _, faction in ipairs(factions) do
        if faction.id == state.factionId then
            return faction
        end
    end

    if state.factionId and state.factionId > 0 then
        return {
            id = state.factionId,
            name = state.factionName or "",
            desc = factionDescriptions[state.factionId] or "",
        }
    end

    return nil
end

function UI:RenderFactions()
    EnsureRows(math.max(#factions, 4))
    HideRows()

    if state.factionId and state.factionId > 0 then
        local faction = GetCurrentFaction()
        local desc = faction and faction.desc ~= "" and faction.desc or (factionDescriptions[state.factionId] or "")
        local factionName = state.factionName ~= "" and state.factionName or (faction and faction.name) or "未知门派"
        local atMax = (state.level or 0) >= (state.maxLevel or 100)
        local nextNeed = state.nextUpgradeRequirement or 0

        rows[1]:SetPoint("TOPLEFT", 16, -10)
        rows[1].text:SetText(string.format("%d. %s\n%s",
            state.factionId, factionName, ColorFactionDescription(state.factionId, desc)))
        rows[1].action:Hide()
        rows[1]:Show()

        rows[2]:SetPoint("TOPLEFT", 16, -68)
        rows[2].text:SetText(string.format("修为等级：%d/%d\n历史贡献：%s",
            state.level or 0, state.maxLevel or 100, FormatNumber(state.historyContribution)))
        rows[2].action:SetText(atMax and "已满级" or "升级")
        SetButtonEnabled(rows[2].action, not atMax)
        rows[2].action:SetScript("OnClick", function()
            Send("UPGRADE")
        end)
        rows[2]:Show()

        rows[3]:SetPoint("TOPLEFT", 16, -126)
        if atMax then
            rows[3].text:SetText("下一修为：已满级\n完成门派日常仍可获得当日贡献，用于门主结算。")
        else
            rows[3].text:SetText(string.format("下一修为需要历史贡献：%s / %s\n完成门派日常可累计历史贡献，达到需求后点击升级。",
                FormatNumber(state.historyContribution), FormatNumber(nextNeed)))
        end
        rows[3].action:Hide()
        rows[3]:Show()

        rows[4]:SetPoint("TOPLEFT", 16, -184)
        rows[4].text:SetText(string.format("当日贡献：%s\n技能解锁名额：%d/%d（每 10 级获得 1 个）",
            FormatNumber(state.contribution), UI:UnlockedCount(), state.unlockSlots or 0))
        rows[4].action:Hide()
        rows[4]:Show()
        UpdateScrollContentHeight(236)
        return
    end

    if #factions == 0 then
        rows[1].text:SetText("暂无门派配置")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    for i, faction in ipairs(factions) do
        local row = rows[i]
        row:SetPoint("TOPLEFT", 16, -8 - (i - 1) * 52)
        local desc = faction.desc ~= "" and faction.desc or (factionDescriptions[faction.id] or "")
        row.text:SetText(string.format("%d. %s\n%s", faction.id, faction.name, ColorFactionDescription(faction.id, desc)))
        row.action:SetText(state.factionId == faction.id and "已加入" or "加入")
        SetButtonEnabled(row.action, state.factionId == 0)
        row.action:SetScript("OnClick", function()
            Send("JOIN:" .. faction.id)
        end)
        row:Show()
    end

    UpdateScrollContentHeight(14 + #factions * 52)
end

function UI:RenderSkills()
    EnsureRows(math.max(#skills, 1))
    HideRows()

    if state.factionId == 0 then
        rows[1].text:SetText("请先加入仙门才能看到对应门派的技能")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    if #skills == 0 then
        rows[1].text:SetText("请先加入仙门才能看到对应门派的技能")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    for i, skill in ipairs(skills) do
        local row = rows[i]
        local unlocked = skill.unlocked == 1
        local active = skill.active == 1
        local personalActive = (state.personalActive and state.personalActive[skill.id]) or skill.personalActive == 1
        row:SetPoint("TOPLEFT", 16, -8 - (i - 1) * 36)
        local activeStatus = ""
        if active then
            activeStatus = " / 门主开放"
        end
        if personalActive then
            activeStatus = activeStatus .. (active and unlocked and " / 个人生效" or " / 个人已选")
        end
        local status = string.format("|cffb8c7bf[%s%s]|r", unlocked and "已解锁" or "未解锁", activeStatus)
        ConfigureSkillRow(row, skill, status, string.format("%s%s", unlocked and "已解锁" or "未解锁", activeStatus))
        row.action:SetText(unlocked and "已解锁" or "解锁")
        SetButtonEnabled(row.action, not unlocked)
        row.action:SetScript("OnClick", function()
            Send("UNLOCK:" .. skill.id)
        end)
        row:Show()
    end

    UpdateScrollContentHeight(24 + #skills * 36)
end

function UI:RenderActiveSkills()
    EnsureRows(math.max(#skills + 1, 2))
    HideRows()

    if state.factionId == 0 then
        rows[1].text:SetText("请先加入仙门才能查看门派生效技能")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    if #skills == 0 then
        rows[1].text:SetText("请先加入仙门才能查看门派生效技能")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    local activeList = GetActiveSkillList()
    local effectiveCount = EffectiveSkillCount(activeList)

    local header = rows[1]
    header:SetPoint("TOPLEFT", 16, -10)
    header.text:SetText(string.format("门主开放：%d/%d  你选择：%d/%d  实际生效：%d\n只能选择已解锁且门主开放的技能。",
        #activeList,
        state.maxActiveSkills or 10,
        PersonalActiveCount(),
        state.maxPersonalActiveSkills or 5,
        effectiveCount))
    header.action:SetText("刷新")
    header.action:SetScript("OnClick", function()
        Send("REQ_ALL")
    end)
    header:Show()

    if #activeList == 0 then
        rows[2]:SetPoint("TOPLEFT", 16, -68)
        rows[2].text:SetText("门主尚未开放门派技能")
        rows[2].action:Hide()
        rows[2]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    for i, skill in ipairs(activeList) do
        local row = rows[i + 1]
        local unlocked = skill.unlocked == 1
        local selected = state.personalActive and state.personalActive[skill.id]
        local effective = unlocked and selected
        row:SetPoint("TOPLEFT", 16, -48 - (i - 1) * 42)
        local status
        if effective then
            status = "|cff66ff66[个人生效]|r"
        elseif unlocked then
            status = "|cffffcc66[可选择]|r"
        else
            status = "|cffff9966[未解锁]|r"
        end
        ConfigureSkillRow(row, skill, status, effective and "个人生效" or (unlocked and "门主开放，可选择" or "门主开放，你未解锁"))
        if not unlocked then
            row.action:SetText("解锁")
            SetButtonEnabled(row.action, true)
            row.action:SetScript("OnClick", function()
                Send("UNLOCK:" .. skill.id)
            end)
        else
            row.action:SetText(selected and "取消" or "选择")
            SetButtonEnabled(row.action, selected or PersonalActiveCount() < (state.maxPersonalActiveSkills or 5))
            row.action:SetScript("OnClick", function()
                if state.personalActive[skill.id] then
                    state.personalActive[skill.id] = nil
                else
                    if PersonalActiveCount() >= (state.maxPersonalActiveSkills or 5) then
                        Print(string.format("成员最多选择 %d 个个人生效技能", state.maxPersonalActiveSkills or 5))
                        return
                    end
                    state.personalActive[skill.id] = true
                end
                SubmitPersonalActive()
                UI:Render()
            end)
        end
        row:Show()
    end

    UpdateScrollContentHeight(62 + #activeList * 42)
end

function UI:RenderLeader()
    EnsureRows(math.max(#skills + 1, 1))
    HideRows()

    if state.factionId == 0 then
        rows[1].text:SetText("请先加入仙门才能看到对应门派的技能")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    local header = rows[1]
    header:SetPoint("TOPLEFT", 16, -10)
    header.text:SetText(string.format("当前门主GUID：%s  门主开放：%d/%d",
        tostring(state.leaderGuid or 0),
        ActiveSelectionCount(),
        state.maxActiveSkills or 10))
    header.action:Hide()
    header:Show()

    for i, skill in ipairs(skills) do
        local skillId = skill.id
        local skillRef = skill
        local row = rows[i + 1]
        local unlocked = skill.unlocked == 1
        local selected = selectedActive[skillId] and true or false
        row:SetPoint("TOPLEFT", 16, -44 - (i - 1) * 34)
        local status = string.format("|cffb8c7bf[%s%s]|r", unlocked and "已解锁" or "未解锁", selected and " / 门主开放" or "")
        ConfigureSkillRow(row, skillRef, status, string.format("%s%s", unlocked and "已解锁" or "未解锁", selected and " / 门主开放" or ""))
        row.action:SetText(selected and "取消" or "选择")
        SetButtonEnabled(row.action, state.isLeader)
        row.action:SetScript("OnClick", function()
            if selectedActive[skillId] then
                selectedActive[skillId] = nil
            else
                if ActiveSelectionCount() >= (state.maxActiveSkills or 10) then
                    Print(string.format("门主最多开放 %d 个门派技能", state.maxActiveSkills or 10))
                    return
                end
                selectedActive[skillId] = true
            end
            leaderSelectionDirty = true
            SubmitSelectedActive()
            UI:Render()
        end)
        row:Show()
    end

    UpdateScrollContentHeight(64 + #skills * 34)
end

function UI:RenderMembers()
    EnsureRows(MEMBERS_PER_PAGE + 1)
    HideRows()

    if state.factionId == 0 then
        rows[1].text:SetText("请先加入仙门才能查看成员列表")
        rows[1].action:Hide()
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    if #members == 0 then
        rows[1].text:SetText("当前仙门暂无成员数据")
        rows[1].action:SetText("刷新")
        rows[1].action:SetScript("OnClick", function()
            Send("REQ_MEMBERS")
        end)
        rows[1]:SetPoint("TOPLEFT", 16, -14)
        rows[1]:Show()
        UpdateScrollContentHeight(SCROLL_VIEW_HEIGHT)
        return
    end

    local pageCount = math.max(1, math.ceil(#members / MEMBERS_PER_PAGE))
    if memberPage > pageCount then
        memberPage = pageCount
    elseif memberPage < 1 then
        memberPage = 1
    end

    local header = rows[1]
    header:SetPoint("TOPLEFT", 16, -10)
    header.text:SetText(string.format("仙门成员：%d 人  第 %d/%d 页", #members, memberPage, pageCount))
    header.action:SetText("刷新")
    header.action:SetScript("OnClick", function()
        Send("REQ_MEMBERS")
    end)
    header:Show()

    local startIndex = (memberPage - 1) * MEMBERS_PER_PAGE + 1
    local endIndex = math.min(#members, startIndex + MEMBERS_PER_PAGE - 1)
    local rowIndex = 2
    for i = startIndex, endIndex do
        local member = members[i]
        local row = rows[rowIndex]
        local leaderTag = member.isLeader and " |cffffd700[门主]|r" or ""
        local onlineTag = member.online and " |cff66ff66[在线]|r" or " |cff888888[离线]|r"
        row:SetPoint("TOPLEFT", 16, -44 - (rowIndex - 2) * 40)
        row.text:SetText(string.format("%s%s%s  GUID:%d\n修为:%d/%d  当日:%s  历史:%s  解锁:%d/%d  生效:%d/%d",
            member.name ~= "" and member.name or "未知角色",
            leaderTag,
            onlineTag,
            member.guid or 0,
            member.level or 0,
            state.maxLevel or 100,
            FormatNumber(member.contribution),
            FormatNumber(member.historyContribution),
            member.unlockedCount or 0,
            member.unlockSlots or 0,
            member.personalActiveCount or 0,
            state.maxPersonalActiveSkills or 5))
        row.action:SetText(member.online and "在线" or "离线")
        SetButtonEnabled(row.action, false)
        row:Show()
        rowIndex = rowIndex + 1
    end

    SetMemberPagerShown(pageCount > 1)
    memberPageText:SetText(string.format("%d/%d", memberPage, pageCount))
    SetButtonEnabled(memberPrev, memberPage > 1)
    SetButtonEnabled(memberNext, memberPage < pageCount)
    UpdateScrollContentHeight(360)
end

function UI:Render()
    statusText:SetText(StatusLine())
    SetButtonEnabled(leave, state.factionId and state.factionId > 0)
    tabFactions:SetButtonState(currentTab == "factions" and "PUSHED" or "NORMAL")
    tabSkills:SetButtonState(currentTab == "skills" and "PUSHED" or "NORMAL")
    tabActive:SetButtonState(currentTab == "active" and "PUSHED" or "NORMAL")
    tabLeader:SetButtonState(currentTab == "leader" and "PUSHED" or "NORMAL")
    tabMembers:SetButtonState(currentTab == "members" and "PUSHED" or "NORMAL")

    if currentTab == "factions" then
        UI:RenderFactions()
    elseif currentTab == "skills" then
        UI:RenderSkills()
    elseif currentTab == "active" then
        UI:RenderActiveSkills()
    elseif currentTab == "leader" then
        UI:RenderLeader()
    else
        UI:RenderMembers()
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
    local previousFactionId = state.factionId or 0
    local fields = SplitFields(payload or "", "|")
    if #fields < 9 then
        return
    end

    if #fields >= 12 then
        state.factionId = tonumber(fields[1]) or 0
        state.factionName = fields[2] or ""
        state.level = tonumber(fields[3]) or 0
        state.maxLevel = tonumber(fields[4]) or 100
        state.contribution = tonumber(fields[5]) or 0
        state.historyContribution = tonumber(fields[6]) or 0
        state.nextUpgradeRequirement = tonumber(fields[7]) or 0
        state.unlockSlots = tonumber(fields[8]) or 0
        state.leaderGuid = tonumber(fields[9]) or 0
        state.isLeader = tonumber(fields[10]) == 1
        state.unlocked = ParseIdSet(fields[11])
        state.active = ParseIdSet(fields[12])
        state.personalActive = ParseIdSet(fields[13])
        state.maxActiveSkills = tonumber(fields[14]) or 10
        state.maxPersonalActiveSkills = tonumber(fields[15]) or 5
    else
        state.factionId = tonumber(fields[1]) or 0
        state.factionName = fields[2] or ""
        state.level = tonumber(fields[3]) or 0
        state.maxLevel = 100
        state.contribution = tonumber(fields[4]) or 0
        state.historyContribution = 0
        state.nextUpgradeRequirement = 0
        state.unlockSlots = tonumber(fields[5]) or 0
        state.leaderGuid = tonumber(fields[6]) or 0
        state.isLeader = tonumber(fields[7]) == 1
        state.unlocked = ParseIdSet(fields[8])
        state.active = ParseIdSet(fields[9])
        state.personalActive = {}
        state.maxActiveSkills = 10
        state.maxPersonalActiveSkills = 5
    end

    if state.factionId == 0 then
        state.factionName = ""
        state.unlocked = {}
        state.active = {}
        state.personalActive = {}
        skills = {}
        selectedActive = {}
        if previousFactionId > 0 then
            currentTab = "factions"
        end
    end

    if not (leaderSelectionDirty and currentTab == "leader") then
        ResetSelectedActive()
    end
end

local function ParseSkills(payload)
    skills = {}
    for entry in string.gmatch(payload or "", "[^~]+") do
        local fields = SplitFields(entry, "^")
        if #fields >= 9 then
            local hasPersonalFlag = #fields >= 13
            local id = fields[1]
            local factionId = fields[2]
            local order = fields[3]
            local name = fields[4]
            local stype = fields[5]
            local spellId = fields[6]
            local unlocked = fields[7]
            local active = fields[8]
            local personalActive = hasPersonalFlag and fields[9] or (state.personalActive and state.personalActive[tonumber(id)] and "1" or "0")
            local baseValue = hasPersonalFlag and fields[10] or (#fields >= 12 and fields[9] or "0")
            local growthPer10 = hasPersonalFlag and fields[11] or (#fields >= 12 and fields[10] or "0")
            local triggerParam = hasPersonalFlag and fields[12] or (#fields >= 12 and fields[11] or "")
            local desc = hasPersonalFlag and fields[13] or (#fields >= 12 and fields[12] or fields[9])

            table.insert(skills, {
                id = tonumber(id),
                factionId = tonumber(factionId),
                order = tonumber(order),
                name = name,
                type = tonumber(stype),
                spellId = tonumber(spellId),
                unlocked = tonumber(unlocked),
                active = tonumber(active),
                personalActive = tonumber(personalActive),
                baseValue = tonumber(baseValue) or 0,
                growthPer10 = tonumber(growthPer10) or 0,
                triggerParam = triggerParam or "",
                desc = desc,
            })
        end
    end
    table.sort(skills, function(a, b) return a.order < b.order end)
end

local function ParseMembers(payload)
    members = {}
    for entry in string.gmatch(payload or "", "[^~]+") do
        local fields = SplitFields(entry, "^")
        if #fields >= 8 then
            local guid = fields[1]
            local name = fields[2]
            local level = fields[3]
            local contribution = fields[4]
            local historyContribution = fields[5]
            local unlockedCount = fields[6]
            local unlockSlots = state.unlockSlots or 0
            local personalActiveCount = "0"
            local isLeader = fields[7]
            local online = fields[8]

            if #fields >= 10 then
                unlockSlots = fields[7]
                personalActiveCount = fields[8]
                isLeader = fields[9]
                online = fields[10]
            elseif #fields >= 9 then
                unlockSlots = fields[7]
                isLeader = fields[8]
                online = fields[9]
            else
                unlockSlots = state.unlockSlots or 0
            end

            table.insert(members, {
                guid = tonumber(guid),
                name = name or "",
                level = tonumber(level) or 0,
                contribution = tonumber(contribution) or 0,
                historyContribution = historyContribution or "0",
                unlockedCount = tonumber(unlockedCount) or 0,
                unlockSlots = tonumber(unlockSlots) or 0,
                personalActiveCount = tonumber(personalActiveCount) or 0,
                isLeader = tonumber(isLeader) == 1,
                online = tonumber(online) == 1,
            })
        end
    end

    local pageCount = math.max(1, math.ceil(#members / MEMBERS_PER_PAGE))
    if memberPage > pageCount then
        memberPage = pageCount
    end
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

    payload = string.match(message, "^XM_MEMBERS:(.*)$")
    if payload then
        ParseMembers(payload)
        UI:Render()
        return
    end

    payload = string.match(message, "^XM_RESULT:(.*)$")
    if payload then
        local action, ok, text = string.match(payload, "^(.-)%^(%d+)%^(.*)$")
        if action == "SET_ACTIVE" then
            leaderSelectionDirty = false
            if tonumber(ok) ~= 1 then
                Send("REQ_ALL")
            end
        end
        if action == "SET_PERSONAL_ACTIVE" and tonumber(ok) ~= 1 then
            Send("REQ_ALL")
        end
        if action == "LEAVE" and tonumber(ok) == 1 then
            ClearJoinedState()
            UI:Render()
            Send("REQ_ALL")
        end
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
