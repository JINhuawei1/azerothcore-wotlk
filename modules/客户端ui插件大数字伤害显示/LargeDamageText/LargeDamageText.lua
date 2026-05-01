local ADDON_NAME = "LargeDamageText"
local ADDON_PREFIX = "PATTRPANEL"

local DEFAULT_DB = {
    enabled = true,
    compact = true,
    disableOfficial = true,
    anchorX = 0,
    anchorY = 96,
    fontScale = 1.0,
    maxTexts = 48
}

local SCHOOL_COLORS = {
    [1] = { 1.00, 0.86, 0.18 },
    [2] = { 1.00, 0.96, 0.62 },
    [4] = { 1.00, 0.34, 0.18 },
    [8] = { 0.30, 1.00, 0.42 },
    [16] = { 0.40, 0.78, 1.00 },
    [32] = { 0.74, 0.46, 1.00 },
    [64] = { 1.00, 0.46, 0.88 }
}

local SCHOOL_ORDER = { 64, 32, 16, 8, 4, 2, 1 }

local DAMAGE_FLOAT_LANES = {
    { -72, 34, -18, 72 },
    { 72, 34, 18, 72 },
    { -38, 58, -12, 78 },
    { 38, 58, 12, 78 },
    { 0, 78, 0, 84 },
    { -108, 52, -20, 74 },
    { 108, 52, 20, 74 },
    { 0, 104, 0, 90 }
}

local DAMAGE_FLOAT_MAX_TEXTS = 8
local DAMAGE_MERGE_WINDOW = 0.22

local OFFICIAL_DAMAGE_CVARS = {
    "floatingCombatTextCombatDamage",
    "floatingCombatTextCombatDamageAllAutos",
    "floatingCombatTextCombatLogPeriodicSpells",
    "floatingCombatTextPetMeleeDamage",
    "floatingCombatTextPetSpellDamage",
    "CombatDamage",
    "CombatDamageAllAutos",
    "CombatLogPeriodicSpells",
    "PetMeleeDamage",
    "PetSpellDamage"
}

local OFFICIAL_DAMAGE_CVAR_LOOKUP = {}
for _, cvarName in ipairs(OFFICIAL_DAMAGE_CVARS) do
    OFFICIAL_DAMAGE_CVAR_LOOKUP[cvarName] = true
    OFFICIAL_DAMAGE_CVAR_LOOKUP[string.lower(cvarName)] = true
end

local State = {
    layer = nil,
    pool = {},
    activeCount = 0,
    sequence = 0,
    lastControlMessageAt = 0,
    lastReadyAck = "未确认",
    applyingOfficialState = false
}

_G.LargeDamageTextActive = true

local function CopyDefaults()
    if type(LargeDamageTextDB) ~= "table" then
        LargeDamageTextDB = {}
    end

    for key, value in pairs(DEFAULT_DB) do
        if LargeDamageTextDB[key] == nil then
            LargeDamageTextDB[key] = value
        end
    end
end

local function NormalizeUnsignedIntegerText(value)
    local text = tostring(value or "")
    text = text:gsub(",", "")
    local digits = text:match("^(%d+)$")
    if not digits then
        return nil
    end

    digits = digits:gsub("^0+", "")
    if digits == "" then
        return "0"
    end

    return digits
end

local function FormatWithCommas(value)
    local digits = NormalizeUnsignedIntegerText(value)
    if not digits then
        return tostring(value or "")
    end

    local left = #digits % 3
    if left == 0 then
        left = 3
    end

    local parts = { digits:sub(1, left) }
    for index = left + 1, #digits, 3 do
        table.insert(parts, digits:sub(index, index + 2))
    end

    return table.concat(parts, ",")
end

local function SplitFourDigitGroups(digits)
    local groups = {}
    local firstLength = #digits % 4
    if firstLength == 0 then
        firstLength = 4
    end

    table.insert(groups, digits:sub(1, firstLength))
    for index = firstLength + 1, #digits, 4 do
        table.insert(groups, digits:sub(index, index + 3))
    end

    return groups
end

local function TrimTrailingZero(text)
    text = text:gsub("0+$", "")
    text = text:gsub("%.$", "")
    return text
end

local function FormatCompactChinese(value)
    local digits = NormalizeUnsignedIntegerText(value)
    if not digits then
        return tostring(value or "")
    end

    if #digits <= 4 then
        return digits
    end

    local units = { "", "万", "亿", "兆", "京", "垓", "秭", "穰" }
    local groups = SplitFourDigitGroups(digits)
    local unitIndex = #groups
    local unitText = units[unitIndex] or ("e" .. tostring((unitIndex - 1) * 4))
    local head = groups[1]
    local nextGroup = groups[2] or ""

    local decimalCount = 2
    if #head >= 3 then
        decimalCount = 0
    elseif #head == 2 then
        decimalCount = 1
    end

    if decimalCount <= 0 then
        return head .. unitText
    end

    nextGroup = nextGroup .. string.rep("0", 4 - #nextGroup)
    local decimals = TrimTrailingZero(nextGroup:sub(1, decimalCount))
    if decimals == "" then
        return head .. unitText
    end

    return head .. "." .. decimals .. unitText
end

local function FormatDamage(value)
    if LargeDamageTextDB and LargeDamageTextDB.compact then
        return FormatCompactChinese(value)
    end

    return FormatWithCommas(value)
end

local function ApplyOfficialCombatTextState()
    if not LargeDamageTextDB or not LargeDamageTextDB.disableOfficial then
        return
    end

    if not SetCVar and not ConsoleExec then
        return
    end

    State.applyingOfficialState = true
    for _, cvarName in ipairs(OFFICIAL_DAMAGE_CVARS) do
        if SetCVar then
            pcall(SetCVar, cvarName, "0")
        end
        if ConsoleExec then
            pcall(ConsoleExec, cvarName .. " 0")
        end
    end
    State.applyingOfficialState = false

    if CombatText_UpdateDisplayedMessages then
        pcall(CombatText_UpdateDisplayedMessages)
    end
end

local function SendControlMessage(message, force)
    local now = GetTime and GetTime() or 0
    if not force and (now - State.lastControlMessageAt) < 1 then
        return
    end

    local playerName = UnitName and UnitName("player") or nil
    if not playerName then
        return
    end

    State.lastControlMessageAt = now
    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, message, "WHISPER", playerName)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, message, "WHISPER", playerName)
    end
end

local function SendReadyState(force)
    CopyDefaults()

    if LargeDamageTextDB.enabled and LargeDamageTextDB.disableOfficial then
        SendControlMessage("LDT_READY", force)
    else
        SendControlMessage("LDT_OFF", force)
    end
end

local function EnsureLayer()
    if State.layer then
        return State.layer
    end

    local layer = CreateFrame("Frame", "LargeDamageTextLayer", UIParent)
    layer:SetAllPoints(UIParent)
    layer:SetFrameStrata("HIGH")
    layer:SetFrameLevel(120)
    State.layer = layer
    return layer
end

local function ApplyTextScale(fontString, scale)
    scale = tonumber(scale) or 1

    if fontString.SetScale then
        fontString:SetScale(scale)
        return
    end

    if fontString.SetFont then
        local font = fontString.largeDamageBaseFont
        local size = fontString.largeDamageBaseSize
        local flags = fontString.largeDamageBaseFlags

        if (not font or not size) and fontString.GetFont then
            font, size, flags = fontString:GetFont()
            fontString.largeDamageBaseFont = font
            fontString.largeDamageBaseSize = size
            fontString.largeDamageBaseFlags = flags
        end

        if font and size then
            fontString:SetFont(font, math.max(8, math.floor(size * scale + 0.5)), flags)
            return
        end
    end

    if fontString.SetTextHeight then
        fontString:SetTextHeight(math.max(8, math.floor(28 * scale + 0.5)))
    end
end

local function FormatDamageLine(baseText, hitCount)
    return baseText
end

local function ResolveSpellName(spellId)
    spellId = tonumber(spellId) or 0
    if spellId <= 0 or not GetSpellInfo then
        return nil
    end

    local name = GetSpellInfo(spellId)
    if type(name) ~= "string" or name == "" then
        return nil
    end

    return name
end

local function FindMergeItem(mergeKey)
    for _, item in ipairs(State.pool) do
        if item.active and item.mergeKey == mergeKey and item.elapsed <= DAMAGE_MERGE_WINDOW then
            return item
        end
    end

    return nil
end

local function GetFloatLane()
    local laneCount = #DAMAGE_FLOAT_LANES
    local startIndex = ((State.sequence - 1) % laneCount) + 1

    for offset = 0, laneCount - 1 do
        local index = ((startIndex + offset - 1) % laneCount) + 1
        local available = true

        for _, item in ipairs(State.pool) do
            if item.active and item.laneIndex == index and item.elapsed < 0.32 then
                available = false
                break
            end
        end

        if available then
            return DAMAGE_FLOAT_LANES[index], index
        end
    end

    return DAMAGE_FLOAT_LANES[startIndex], startIndex
end

local function TrimActiveFloatTexts()
    local activeCount = 0
    for _, item in ipairs(State.pool) do
        if item.active then
            activeCount = activeCount + 1
        end
    end

    while activeCount > DAMAGE_FLOAT_MAX_TEXTS do
        local oldest = nil
        for _, item in ipairs(State.pool) do
            if item.active and (not oldest or item.elapsed > oldest.elapsed) then
                oldest = item
            end
        end

        if not oldest then
            return
        end

        oldest.active = false
        oldest.text:Hide()
        State.activeCount = math.max(0, State.activeCount - 1)
        activeCount = activeCount - 1
    end
end

local function ApplyFloatPosition(item, progress)
    local eased = 1 - ((1 - progress) * (1 - progress))
    item.text:ClearAllPoints()
    item.text:SetPoint("CENTER", UIParent, "CENTER", item.x + (item.drift * progress), item.y + (item.rise * eased))
end

local function ConfigureFontString(fontString)
    fontString:SetJustifyH("CENTER")
    fontString:SetJustifyV("MIDDLE")
    if fontString.SetWidth then
        fontString:SetWidth(360)
    end
    if fontString.SetHeight then
        fontString:SetHeight(64)
    end
    if fontString.GetFont then
        local font, size, flags = fontString:GetFont()
        fontString.largeDamageBaseFont = font
        fontString.largeDamageBaseSize = size
        fontString.largeDamageBaseFlags = "OUTLINE"
        if font and fontString.SetFont then
            fontString:SetFont(font, math.max(14, size or 28), "OUTLINE")
        end
    end
    if fontString.SetShadowColor then
        fontString:SetShadowColor(0, 0, 0, 0.95)
    end
    if fontString.SetShadowOffset then
        fontString:SetShadowOffset(1, -1)
    end
    if fontString.SetWordWrap then
        fontString:SetWordWrap(false)
    end
    if fontString.SetNonSpaceWrap then
        fontString:SetNonSpaceWrap(false)
    end
end

local function AcquireText()
    local layer = EnsureLayer()

    for _, item in ipairs(State.pool) do
        if not item.active then
            return item
        end
    end

    local maxTexts = tonumber(LargeDamageTextDB and LargeDamageTextDB.maxTexts) or DEFAULT_DB.maxTexts
    maxTexts = math.min(maxTexts, DAMAGE_FLOAT_MAX_TEXTS + 2)
    if #State.pool >= maxTexts then
        local oldest = State.pool[1]
        for _, item in ipairs(State.pool) do
            if item.elapsed > oldest.elapsed then
                oldest = item
            end
        end

        if oldest.active then
            oldest.active = false
            State.activeCount = math.max(0, State.activeCount - 1)
        end
        return oldest
    end

    local fontString = layer:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
    ConfigureFontString(fontString)

    local item = {
        text = fontString,
        active = false,
        elapsed = 0,
        duration = 1.15,
        laneIndex = 0,
        x = 0,
        y = 0,
        rise = 92,
        drift = 0,
        scale = 1,
        startAlpha = 1,
        hitCount = 1,
        mergeKey = nil,
        displayText = nil
    }

    table.insert(State.pool, item)
    return item
end

local function UpdateTexts(self, elapsed)
    local hasActive = false

    for _, item in ipairs(State.pool) do
        if item.active then
            item.elapsed = item.elapsed + elapsed
            if item.elapsed >= item.duration then
                item.active = false
                item.text:Hide()
                State.activeCount = math.max(0, State.activeCount - 1)
            else
                hasActive = true
                local progress = item.elapsed / item.duration
                local fadeStart = item.critical and 0.72 or 0.58
                local alphaProgress = math.max(0, (progress - fadeStart) / (1 - fadeStart))
                ApplyFloatPosition(item, progress)
                item.text:SetAlpha((item.startAlpha or 1) * (1 - alphaProgress))
                ApplyTextScale(item.text, item.scale)
            end
        end
    end

    if not hasActive then
        self:SetScript("OnUpdate", nil)
    end
end

local function GetSchoolColor(schoolMask)
    schoolMask = tonumber(schoolMask) or 1

    for _, mask in ipairs(SCHOOL_ORDER) do
        local color = SCHOOL_COLORS[mask]
        if bit and bit.band and bit.band(schoolMask, mask) ~= 0 then
            return color[1], color[2], color[3]
        end
    end

    return SCHOOL_COLORS[1][1], SCHOOL_COLORS[1][2], SCHOOL_COLORS[1][3]
end

local function GetDamageVisual(amountText, critical, schoolMask)
    local red, green, blue = GetSchoolColor(schoolMask)
    local digitCount = string.len(amountText or "")
    local scale = critical and 1.18 or 0.88

    if digitCount >= 12 then
        scale = scale * 0.82
    elseif digitCount <= 5 then
        scale = scale * 0.96
    end

    if critical then
        local school = tonumber(schoolMask) or 1
        if bit and bit.band and bit.band(school, 1) ~= 0 then
            red, green, blue = 1.00, 0.38, 0.08
        else
            red = math.min(1, red + 0.22)
            green = math.min(1, green + 0.06)
            blue = math.max(0, blue - 0.10)
        end
    end

    return red, green, blue, scale
end

local function ShowDamage(amountText, critical, schoolMask, spellName)
    CopyDefaults()

    if not LargeDamageTextDB or not LargeDamageTextDB.enabled then
        return
    end

    amountText = NormalizeUnsignedIntegerText(amountText)
    if not amountText or amountText == "0" then
        return
    end

    local damageText = FormatDamage(amountText)
    local text = damageText
    local hasSpellName = type(spellName) == "string" and spellName ~= ""
    if hasSpellName then
        text = spellName .. " " .. damageText
    end
    if critical then
        if hasSpellName then
            text = spellName .. " 暴击 " .. damageText
        else
            text = "暴击 " .. damageText
        end
    end

    State.sequence = State.sequence + 1
    local baseX = tonumber(LargeDamageTextDB.anchorX) or DEFAULT_DB.anchorX
    local baseY = tonumber(LargeDamageTextDB.anchorY) or DEFAULT_DB.anchorY
    local baseScale = tonumber(LargeDamageTextDB.fontScale) or DEFAULT_DB.fontScale
    local red, green, blue, visualScale = GetDamageVisual(amountText, critical, schoolMask)
    if hasSpellName then
        visualScale = visualScale * 0.92
    end
    local mergeKey = tostring(critical and 1 or 0) .. ":" .. tostring(tonumber(schoolMask) or 1) .. ":" .. text
    local item = FindMergeItem(mergeKey)

    if item then
        item.hitCount = (item.hitCount or 1) + 1
        item.elapsed = math.min(item.elapsed or 0, 0.34)
        item.duration = critical and 1.28 or 1.08
        item.scale = baseScale * visualScale
        item.text:SetText(FormatDamageLine(item.displayText or text, item.hitCount))
        item.text:SetTextColor(red, green, blue)
        item.text:SetAlpha(1)
        ApplyTextScale(item.text, item.scale)
        EnsureLayer():SetScript("OnUpdate", UpdateTexts)
        return
    end

    local laneInfo, laneIndex = GetFloatLane()
    TrimActiveFloatTexts()
    item = AcquireText()

    item.active = true
    item.elapsed = 0
    item.duration = critical and 1.28 or 1.08
    item.laneIndex = laneIndex
    item.critical = critical
    item.x = baseX + laneInfo[1] + math.random(-5, 5)
    item.y = baseY + laneInfo[2] + math.random(-3, 3)
    item.rise = laneInfo[4] + (critical and 18 or 0)
    item.drift = laneInfo[3]
    item.scale = baseScale * visualScale
    item.startAlpha = critical and 1 or 0.92
    item.hitCount = 1
    item.mergeKey = mergeKey
    item.displayText = text

    item.text:SetText(FormatDamageLine(text, item.hitCount))
    item.text:SetTextColor(red, green, blue)
    item.text:SetAlpha(item.startAlpha)
    ApplyTextScale(item.text, item.scale)
    if item.text.SetWidth then
        item.text:SetWidth(hasSpellName and 520 or (critical and 460 or 340))
    end
    if item.text.SetHeight then
        item.text:SetHeight(critical and 58 or 46)
    end
    item.text:ClearAllPoints()
    ApplyFloatPosition(item, 0)
    item.text:Show()

    State.activeCount = State.activeCount + 1
    EnsureLayer():SetScript("OnUpdate", UpdateTexts)
end

local function NormalizeAddonMessage(prefix, message)
    prefix = tostring(prefix or "")
    message = tostring(message or "")

    local embedded = prefix:match("^" .. ADDON_PREFIX .. "\t(.+)$")
    if embedded then
        return ADDON_PREFIX, embedded
    end

    embedded = message:match("^" .. ADDON_PREFIX .. "\t(.+)$")
    if embedded then
        return ADDON_PREFIX, embedded
    end

    return prefix, message
end

local function HandleDamagePayload(message)
    local amountText, criticalText, schoolMask, spellId = message:match("^DMG:(%d+):(%d+):(%d+):(%d+):%d+$")
    if not amountText then
        return
    end

    ShowDamage(amountText, criticalText == "1", schoolMask, ResolveSpellName(spellId))
end

local function HandleAckPayload(message)
    local enabled = message:match("^LDT_ACK:(%d+)$")
    if not enabled then
        return
    end

    State.lastReadyAck = enabled == "1" and "服务端已接管" or "服务端已恢复官方"
end

local function OnAddonMessage(prefix, message)
    prefix, message = NormalizeAddonMessage(prefix, message)
    if prefix ~= ADDON_PREFIX then
        return
    end

    if message:match("^DMG:") then
        HandleDamagePayload(message)
    elseif message:match("^LDT_ACK:") then
        HandleAckPayload(message)
    end
end

local function PrintStatus()
    CopyDefaults()

    local enabledText = LargeDamageTextDB.enabled and "开启" or "关闭"
    local compactText = LargeDamageTextDB.compact and "中文单位" or "完整数字"
    local officialText = LargeDamageTextDB.disableOfficial and "关闭官方伤害" or "保留官方伤害"
    print("|cff33ff99大数字伤害显示|r " .. enabledText .. "，" .. compactText .. "，" .. officialText .. "，" .. State.lastReadyAck)
end

local function HandleSlashCommand(message)
    CopyDefaults()

    message = string.lower(tostring(message or ""))

    if message == "on" or message == "开启" then
        LargeDamageTextDB.enabled = true
        SendReadyState(true)
        PrintStatus()
        return
    elseif message == "off" or message == "关闭" then
        LargeDamageTextDB.enabled = false
        SendReadyState(true)
        PrintStatus()
        return
    elseif message == "compact" or message == "中文" then
        LargeDamageTextDB.compact = true
        PrintStatus()
        return
    elseif message == "full" or message == "完整" then
        LargeDamageTextDB.compact = false
        PrintStatus()
        return
    elseif message == "official" or message == "官方" then
        LargeDamageTextDB.disableOfficial = not LargeDamageTextDB.disableOfficial
        ApplyOfficialCombatTextState()
        SendReadyState(true)
        PrintStatus()
        return
    elseif message == "ready" or message == "握手" then
        SendReadyState(true)
        print("|cff33ff99大数字伤害显示|r 已重新发送接管握手")
        return
    elseif message == "status" or message == "状态" then
        PrintStatus()
        return
    end

    local testAmount = message:match("^test%s+(%d+)$") or message:match("^测试%s+(%d+)$")
    if testAmount then
        ShowDamage(testAmount, true, 1)
        return
    end

    ShowDamage("123456789012345", true, 1)
    PrintStatus()
end

local function SafeHandleSlashCommand(message)
    local ok, err = pcall(HandleSlashCommand, message)
    if not ok then
        print("|cffff0000大数字伤害显示错误:|r " .. tostring(err))
    end
end

local EventFrame = CreateFrame("Frame")
EventFrame:RegisterEvent("ADDON_LOADED")
EventFrame:RegisterEvent("PLAYER_LOGIN")
EventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
EventFrame:RegisterEvent("CVAR_UPDATE")
EventFrame:RegisterEvent("CHAT_MSG_ADDON")

EventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "ADDON_LOADED" then
        local addon = ...
        if addon == ADDON_NAME then
            CopyDefaults()
            ApplyOfficialCombatTextState()
        end
        return
    end

    if event == "PLAYER_LOGIN" then
        CopyDefaults()
        ApplyOfficialCombatTextState()
        return
    end

    if event == "PLAYER_ENTERING_WORLD" then
        CopyDefaults()
        ApplyOfficialCombatTextState()
        SendReadyState(true)
        return
    end

    if event == "CVAR_UPDATE" then
        if State.applyingOfficialState then
            return
        end

        local cvarName = ...
        cvarName = tostring(cvarName or "")
        if OFFICIAL_DAMAGE_CVAR_LOOKUP[cvarName] or OFFICIAL_DAMAGE_CVAR_LOOKUP[string.lower(cvarName)] then
            ApplyOfficialCombatTextState()
        end
        return
    end

    if event == "CHAT_MSG_ADDON" then
        OnAddonMessage(...)
    end
end)

if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
end

CopyDefaults()

SLASH_LARGEDAMAGETEXT1 = "/bdmg"
SLASH_LARGEDAMAGETEXT2 = "/ldt"
SLASH_LARGEDAMAGETEXT3 = "/ldmg"
SLASH_LARGEDAMAGETEXT4 = "/大数字伤害"
SlashCmdList["LARGEDAMAGETEXT"] = SafeHandleSlashCommand
