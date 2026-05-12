---@diagnostic disable: undefined-global
local ADDON_NAME = "PlayerAttributePanel"
local ADDON_PREFIX = "PATTRPANEL"
local REPLACE_OFFICIAL_STATS = true
local ATTRIBUTE_PANEL_WIDTH = 338
local ATTRIBUTE_PANEL_HEIGHT = 424
local ATTRIBUTE_PANEL_CONTENT_WIDTH = 246
local ATTRIBUTE_PANEL_REPLACE_X_OFFSET = -34
local ATTRIBUTE_PANEL_REPLACE_Y_OFFSET = -20
local ATTRIBUTE_PANEL_BACKGROUND_TEXTURE = "Interface\\AddOns\\" .. ADDON_NAME .. "\\Textures\\AttributePanelBackground"

local OFFICIAL_STATS_FRAME_NAMES = {
    "CharacterStatsPane",
    "CharacterAttributesFrame",
    "CharacterResistanceFrame",
    "PaperDollFrameStatsPane",
    "PlayerStatFrameLeft",
    "PlayerStatFrameRight",
    "PlayerStatFrameLeftDropDown",
    "PlayerStatFrameRightDropDown"
}

local OFFICIAL_PLAYER_HEALTH_TEXT_NAMES = {
    "PlayerFrameHealthBarText",
    "PlayerFrameHealthBarTextLeft",
    "PlayerFrameHealthBarTextRight",
    "PlayerFrameTextureFrameHealthBarText",
    "PlayerFrameTextureFrameHealthBarTextLeft",
    "PlayerFrameTextureFrameHealthBarTextRight"
}

local OFFICIAL_PLAYER_POWER_TEXT_NAMES = {
    "PlayerFrameManaBarText",
    "PlayerFrameManaBarTextLeft",
    "PlayerFrameManaBarTextRight",
    "PlayerFrameTextureFrameManaBarText",
    "PlayerFrameTextureFrameManaBarTextLeft",
    "PlayerFrameTextureFrameManaBarTextRight"
}

local OFFICIAL_TARGET_HEALTH_TEXT_NAMES = {
    "TargetFrameHealthBarText",
    "TargetFrameHealthBarTextLeft",
    "TargetFrameHealthBarTextRight",
    "TargetFrameTextureFrameHealthBarText",
    "TargetFrameTextureFrameHealthBarTextLeft",
    "TargetFrameTextureFrameHealthBarTextRight"
}

local OFFICIAL_TARGET_POWER_TEXT_NAMES = {
    "TargetFrameManaBarText",
    "TargetFrameManaBarTextLeft",
    "TargetFrameManaBarTextRight",
    "TargetFrameTextureFrameManaBarText",
    "TargetFrameTextureFrameManaBarTextLeft",
    "TargetFrameTextureFrameManaBarTextRight"
}

local TARGET_REFRESH_INTERVAL = 1.0
local TARGET_HEARTBEAT_INTERVAL = 5.0
local CUSTOM_PLAYER_FRAME_WIDTH = 372
local CUSTOM_PLAYER_FRAME_HEIGHT = 92
local CUSTOM_PLAYER_FRAME_X_OFFSET = 16
local CUSTOM_PLAYER_PORTRAIT_SIZE = 64
local CUSTOM_PLAYER_PORTRAIT_LEFT = 12
local CUSTOM_PLAYER_BAR_LEFT = 92
local CUSTOM_PLAYER_BAR_WIDTH = 210
local CUSTOM_PLAYER_HEALTH_BAR_TOP = -30
local CUSTOM_PLAYER_POWER_BAR_TOP = -52
local CUSTOM_TARGET_FRAME_WIDTH = 270
local CUSTOM_TARGET_FRAME_HEIGHT = 92
local CUSTOM_TARGET_PORTRAIT_SIZE = 64
local CUSTOM_TARGET_PORTRAIT_RIGHT = -48
local CUSTOM_TARGET_BAR_RIGHT = 36
local CUSTOM_TARGET_BAR_WIDTH = 160
local CUSTOM_TARGET_HEALTH_BAR_TOP = -30
local CUSTOM_TARGET_POWER_BAR_TOP = -52
local CUSTOM_TARGET_DEBUFF_COUNT = 16
local CUSTOM_TARGET_DEBUFF_COLUMNS = 8
local CUSTOM_TARGET_DEBUFF_SIZE = 16
local CUSTOM_TARGET_DEBUFF_SPACING = 1
local CUSTOM_TARGET_DEBUFF_TOP_OFFSET = 0
local UNIT_FRAME_TEXTURE_PATH = "Interface\\AddOns\\" .. ADDON_NAME .. "\\Textures\\UnitFrames\\"
local CUSTOM_RUNE_COUNT = 6
local CUSTOM_RUNE_SIZE = 16
local CUSTOM_RUNE_SPACING = 2
local CUSTOM_RUNE_TOP = -70

local CLASS_FRAME_COLORS = {
    WARRIOR = { 0.78, 0.61, 0.43 },
    PALADIN = { 0.96, 0.55, 0.73 },
    HUNTER = { 0.67, 0.83, 0.45 },
    ROGUE = { 1.00, 0.96, 0.41 },
    PRIEST = { 1.00, 1.00, 1.00 },
    DEATHKNIGHT = { 0.77, 0.12, 0.23 },
    SHAMAN = { 0.00, 0.44, 0.87 },
    MAGE = { 0.41, 0.80, 0.94 },
    WARLOCK = { 0.58, 0.51, 0.79 },
    DRUID = { 1.00, 0.49, 0.04 },
    NEUTRAL = { 0.78, 0.72, 0.58 }
}

local POWER_BAR_TEXTURES = {
    [0] = "Bar_Mana",
    [1] = "Bar_Rage",
    [2] = "Bar_Focus",
    [3] = "Bar_Energy",
    [6] = "Bar_RunicPower"
}

local POWER_TEXT_COLORS = {
    [0] = { 0.45, 0.65, 1.00 },
    [1] = { 1.00, 0.12, 0.08 },
    [2] = { 1.00, 0.58, 0.18 },
    [3] = { 1.00, 0.86, 0.10 },
    [6] = { 0.30, 0.95, 1.00 }
}

local RUNE_TYPE_TEXTURES = {
    [1] = "Rune_Blood",
    [2] = "Rune_Unholy",
    [3] = "Rune_Frost",
    [4] = "Rune_Death"
}

local DEFAULT_RUNE_TEXTURES = {
    "Rune_Blood",
    "Rune_Blood",
    "Rune_Frost",
    "Rune_Frost",
    "Rune_Unholy",
    "Rune_Unholy"
}

local CATEGORY_COLORS = {
    resource = { 0.30, 0.95, 0.45 },
    primary = { 1.00, 0.82, 0.20 },
    melee = { 1.00, 0.42, 0.25 },
    ranged = { 0.45, 0.95, 0.35 },
    spell = { 0.55, 0.75, 1.00 },
    defense = { 0.65, 0.95, 0.75 },
    resistance = { 0.95, 0.65, 1.00 },
    misc = { 0.90, 0.90, 0.90 }
}

local function GetCombatRatingValue(ratingId)
    if not GetCombatRating then
        return 0
    end

    return math.floor(tonumber(GetCombatRating(ratingId)) or 0)
end

local function GetChanceValue(func)
    if not func then
        return 0
    end

    return math.floor((tonumber(func()) or 0) * 100) / 100
end

local function GetSpellPowerValue()
    local maxBonus = 0

    if GetSpellBonusDamage then
        for school = 2, 7 do
            maxBonus = math.max(maxBonus, tonumber(GetSpellBonusDamage(school)) or 0)
        end
    end

    return math.floor(maxBonus)
end

local function GetResistanceValue(school)
    if not UnitResistance then
        return 0
    end

    local base, total = UnitResistance("player", school)
    return math.floor(tonumber(total or base) or 0)
end

local function AttrEntry(id, key, label, category, aliases, officialFunc, displayType)
    local serverKeys = { "ATTR_" .. tostring(id), tostring(id) }

    if aliases then
        for _, alias in ipairs(aliases) do
            table.insert(serverKeys, alias)
        end
    end

    return {
        id = id,
        key = key,
        serverKeys = serverKeys,
        label = label,
        category = category,
        displayType = displayType,
        officialFunc = officialFunc
    }
end

local function StatEntry(id, key, label, category, statIndex, aliases)
    local entry = AttrEntry(id, key, label, category, aliases, nil)
    entry.apiIndex = statIndex
    return entry
end

local ENTRY_DEFS = {
    AttrEntry(1, "maxHealth", "生命值", "resource", { "MAX_HEALTH", "HEALTH" }, function() return UnitHealthMax("player") or 0 end),
    AttrEntry(0, "maxMana", "法力值", "resource", { "MAX_MANA", "MANA" }, function() return UnitPowerMax("player", 0) or 0 end),
    StatEntry(4, "strength", "力量", "primary", 1, { "STRENGTH" }),
    StatEntry(3, "agility", "敏捷", "primary", 2, { "AGILITY" }),
    StatEntry(7, "stamina", "耐力", "primary", 3, { "STAMINA" }),
    StatEntry(5, "intellect", "智力", "primary", 4, { "INTELLECT" }),
    StatEntry(6, "spirit", "精神", "primary", 5, { "SPIRIT" }),
    AttrEntry(8, "trueDamage", "真实伤害", "misc", { "TRUE_DAMAGE" }, nil),
    AttrEntry(9, "cuttingDamage", "切割伤害", "misc", { "CUTTING_DAMAGE" }, nil),
    AttrEntry(10, "cooldownReduction", "冷却缩减", "misc", { "COOLDOWN_REDUCTION" }, nil),
    AttrEntry(11, "skillDamage", "技能伤害", "misc", { "SKILL_DAMAGE" }, nil),
    AttrEntry(12, "defenseRating", "防御等级", "defense", { "DEFENSE_RATING" }, function() return GetCombatRatingValue(CR_DEFENSE_SKILL or 2) end),
    AttrEntry(13, "dodgeRating", "躲闪等级", "defense", { "DODGE_RATING" }, function() return GetCombatRatingValue(CR_DODGE or 3) end),
    AttrEntry(14, "parryRating", "招架等级", "defense", { "PARRY_RATING" }, function() return GetCombatRatingValue(CR_PARRY or 4) end),
    AttrEntry(15, "blockRating", "格挡等级", "defense", { "BLOCK_RATING" }, function() return GetCombatRatingValue(CR_BLOCK or 5) end),
    AttrEntry(35, "resilienceRating", "韧性等级", "defense", { "RESILIENCE", "RESILIENCE_RATING" }, function() return GetCombatRatingValue(CR_CRIT_TAKEN_MELEE or 15) end),
    AttrEntry(31, "hitRating", "命中等级", "misc", { "HIT_RATING" }, function() return GetCombatRatingValue(CR_HIT_MELEE or 6) end),
    AttrEntry(32, "critRating", "暴击等级", "misc", { "CRIT_RATING" }, function() return GetCombatRatingValue(CR_CRIT_MELEE or 9) end),
    AttrEntry(36, "hasteRating", "急速等级", "misc", { "HASTE_RATING" }, function() return GetCombatRatingValue(CR_HASTE_MELEE or 18) end),
    AttrEntry(37, "expertiseRating", "精准等级", "melee", { "EXPERTISE", "EXPERTISE_RATING" }, function() return GetCombatRatingValue(CR_EXPERTISE or 24) end),
    AttrEntry(38, "attackPower", "攻击强度", "melee", { "ATTACK_POWER" }, function()
        if not UnitAttackPower then
            return 0
        end

        local base, posBuff, negBuff = UnitAttackPower("player")
        return math.floor((base or 0) + (posBuff or 0) + (negBuff or 0))
    end),
    AttrEntry(44, "armorPenetrationRating", "护甲穿透", "melee", { "ARMOR_PENETRATION", "ARMOR_PENETRATION_RATING" }, function() return GetCombatRatingValue(CR_ARMOR_PENETRATION or 25) end),
    AttrEntry(41, "spellHealingDone", "法术治疗", "spell", { "SPELL_HEALING", "SPELL_HEALING_DONE" }, function() return math.floor(tonumber(GetSpellBonusHealing and GetSpellBonusHealing()) or 0) end),
    AttrEntry(42, "spellDamageDone", "法术伤害", "spell", { "SPELL_DAMAGE", "SPELL_DAMAGE_DONE" }, GetSpellPowerValue),
    AttrEntry(43, "manaRegen", "每3秒法力回复", "resource", { "MANA_REGENERATION", "MANA_REGEN" }, nil),
    AttrEntry(45, "spellPower", "法术强度", "spell", { "SPELL_POWER" }, GetSpellPowerValue),
    AttrEntry(46, "healthRegen", "每3秒生命回复", "resource", { "HEALTH_REGEN" }, nil),
    AttrEntry(47, "spellPenetration", "法术穿透", "spell", { "SPELL_PENETRATION" }, function()
        if GetSpellPenetration then
            return math.floor(tonumber(GetSpellPenetration()) or 0)
        end

        return 0
    end),
    AttrEntry(48, "blockValue", "格挡值", "defense", { "BLOCK_VALUE" }, function() return GetShieldBlock and math.floor(tonumber(GetShieldBlock()) or 0) or 0 end),
    AttrEntry(-1, "mainHandDamage", "主手伤害", "melee", { "MAINHAND_DAMAGE" }, function()
        local minDamage, maxDamage = UnitDamage("player")
        minDamage = math.floor(tonumber(minDamage) or 0)
        maxDamage = math.floor(tonumber(maxDamage) or minDamage or 0)
        return tostring(minDamage) .. "~" .. tostring(maxDamage)
    end, "range"),
    AttrEntry(-2, "offHandDamage", "副手伤害", "melee", { "OFFHAND_DAMAGE" }, function()
        local _, _, minDamage, maxDamage = UnitDamage("player")
        minDamage = math.floor(tonumber(minDamage) or 0)
        maxDamage = math.floor(tonumber(maxDamage) or minDamage or 0)
        return tostring(minDamage) .. "~" .. tostring(maxDamage)
    end, "range"),
    AttrEntry(-3, "rangedDamage", "远程伤害", "ranged", { "RANGED_DAMAGE" }, function()
        if not UnitRangedDamage then
            return "0~0"
        end

        local _, minDamage, maxDamage = UnitRangedDamage("player")
        minDamage = math.floor(tonumber(minDamage) or 0)
        maxDamage = math.floor(tonumber(maxDamage) or minDamage or 0)
        return tostring(minDamage) .. "~" .. tostring(maxDamage)
    end, "range")
}

local State = {
    stats = {},
    realCurrentHealth = nil,
    realCurrentMana = nil,
    targetData = {
        token = nil,
        hasData = false,
        currentHealth = nil,
        maxHealth = nil,
        currentMana = nil,
        maxMana = nil,
        powerType = nil
    },
    statusText = "未同步",
    lastRequestAt = 0,
    lastTargetRequestAt = 0,
    targetRequestToken = 0,
    targetRefreshElapsed = 0,
    damageLayer = nil,
    damageTexts = {},
    damageTextElapsed = 0,
    combatTextConfigured = false,
    panel = nil,
    title = nil,
    scrollFrame = nil,
    content = nil,
    customPlayerFrame = nil,
    customPlayerFrameArt = nil,
    customPlayerPortrait = nil,
    customPlayerPortraitRing = nil,
    customPlayerNameText = nil,
    customPlayerLevelText = nil,
    customPlayerHealthFill = nil,
    customPlayerPowerFill = nil,
    customPlayerRunes = nil,
    playerHealthBackdrop = nil,
    playerHealthText = nil,
    playerPowerBackdrop = nil,
    playerPowerText = nil,
    playerHealthHooked = false,
    customTargetFrame = nil,
    customTargetFrameArt = nil,
    customTargetPortrait = nil,
    customTargetPortraitRing = nil,
    customTargetDebuffs = nil,
    customTargetNameText = nil,
    customTargetLevelText = nil,
    customTargetHealthFill = nil,
    customTargetPowerFill = nil,
    targetHealthBackdrop = nil,
    targetHealthText = nil,
    targetPowerBackdrop = nil,
    targetPowerText = nil,
    targetFrameHooked = false,
    hooked = false,
    officialFramesHooked = false,
    rows = {}
}

for _, entryDef in ipairs(ENTRY_DEFS) do
    State.stats[entryDef.key] = {
        real = nil,
        official = 0
    }
end

local function FormatLargeInteger(value)
    value = tostring(value or "")
    if value == "" then
        return ""
    end

    local sign, digits = value:match("^([%-]?)(%d+)$")
    if not digits then
        return value
    end

    local formatted = digits:reverse():gsub("(%d%d%d)", "%1,"):reverse():gsub("^,", "")
    return sign .. formatted
end

local function TrimNumberText(text)
    text = tostring(text or "")
    text = text:gsub("(%..-)0+$", "%1")
    text = text:gsub("%.$", "")
    return text
end

local function FormatChineseNumber(value)
    local text = tostring(value or "")
    local sign, digits = text:match("^([%-]?)(%d+)$")
    if not digits then
        return text
    end

    local numberValue = tonumber(digits)
    if not numberValue then
        return FormatLargeInteger(text)
    end

    local units = {
        { value = 10000000000000000, text = "京" },
        { value = 1000000000000, text = "兆" },
        { value = 100000000, text = "亿" },
        { value = 10000, text = "万" },
        { value = 1000, text = "千" },
        { value = 100, text = "百" }
    }

    for _, unit in ipairs(units) do
        if numberValue >= unit.value then
            local compact = numberValue / unit.value
            if compact >= 100 then
                return sign .. TrimNumberText(string.format("%.0f", compact)) .. unit.text
            elseif compact >= 10 then
                return sign .. TrimNumberText(string.format("%.1f", compact)) .. unit.text
            else
                return sign .. TrimNumberText(string.format("%.2f", compact)) .. unit.text
            end
        end
    end

    return sign .. digits
end

local function FormatDisplayValue(entryDef, value)
    if value == nil then
        return ""
    end

    local text = tostring(value)

    if entryDef and entryDef.displayType == "range" then
        local minValue, maxValue = text:match("^([%-]?%d+)~([%-]?%d+)$")
        if minValue and maxValue then
            return FormatChineseNumber(minValue) .. "-" .. FormatChineseNumber(maxValue)
        end
    end

    return FormatChineseNumber(text)
end

local NormalizeUnsignedIntegerText

local function ConfigureCombatTextCVar()
    if State.combatTextConfigured then
        return
    end

    State.combatTextConfigured = true
    if SetCVar then
        SetCVar("floatingCombatTextCombatDamage", "0")
    end
end

local function EnsureDamageLayer()
    if State.damageLayer then
        return State.damageLayer
    end

    local layer = CreateFrame("Frame", "PlayerAttributePanelDamageLayer", UIParent)
    layer:SetAllPoints(UIParent)
    layer:SetFrameStrata("HIGH")
    layer:SetFrameLevel(90)
    State.damageLayer = layer
    return layer
end

local function ApplyDamageTextScale(fontString, scale)
    scale = tonumber(scale) or 1

    if fontString.SetScale then
        fontString:SetScale(scale)
        return
    end

    if fontString.GetFont and fontString.SetFont then
        local font, size, flags = fontString:GetFont()
        if font and size then
            fontString:SetFont(font, math.max(8, math.floor(size * scale + 0.5)), flags)
            return
        end
    end

    if fontString.SetTextHeight then
        fontString:SetTextHeight(math.max(8, math.floor(28 * scale + 0.5)))
    end
end

local function UpdateDamageTexts(self, elapsed)
    local hasActive = false

    for _, item in ipairs(State.damageTexts) do
        if item.active then
            item.elapsed = item.elapsed + elapsed
            if item.elapsed >= item.duration then
                item.active = false
                item.text:Hide()
            else
                hasActive = true
                local progress = item.elapsed / item.duration
                item.text:ClearAllPoints()
                item.text:SetPoint("CENTER", UIParent, "CENTER", item.x, item.y + (progress * item.rise))
                item.text:SetAlpha(1 - progress)
            end
        end
    end

    if not hasActive then
        self:SetScript("OnUpdate", nil)
    end
end

local function AcquireDamageText()
    local layer = EnsureDamageLayer()

    for _, item in ipairs(State.damageTexts) do
        if not item.active then
            return item
        end
    end

    if #State.damageTexts >= 32 then
        return State.damageTexts[1]
    end

    local fontString = layer:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
    if fontString.SetShadowColor then
        fontString:SetShadowColor(0, 0, 0, 1)
    end
    if fontString.SetShadowOffset then
        fontString:SetShadowOffset(2, -2)
    end

    local item = {
        text = fontString,
        active = false,
        elapsed = 0,
        duration = 1.15,
        x = 0,
        y = 0,
        rise = 90
    }
    table.insert(State.damageTexts, item)
    return item
end

local function ShowDamageText(amountText, critical)
    amountText = NormalizeUnsignedIntegerText(amountText)
    if not amountText or amountText == "0" then
        return
    end

    local item = AcquireDamageText()
    local text = FormatChineseNumber(amountText)
    if critical then
        text = "暴击 " .. text
    end

    item.active = true
    item.elapsed = 0
    item.duration = critical and 1.35 or 1.05
    item.x = math.random(-90, 90)
    item.y = math.random(56, 112)
    item.rise = critical and 118 or 88

    item.text:SetText(text)
    item.text:SetAlpha(1)
    ApplyDamageTextScale(item.text, critical and 1.3 or 1.0)
    if critical then
        item.text:SetTextColor(1.00, 0.42, 0.08)
    else
        item.text:SetTextColor(1.00, 0.92, 0.22)
    end
    item.text:ClearAllPoints()
    item.text:SetPoint("CENTER", UIParent, "CENTER", item.x, item.y)
    item.text:Show()

    local layer = EnsureDamageLayer()
    layer:SetScript("OnUpdate", UpdateDamageTexts)
end

function NormalizeUnsignedIntegerText(value)
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

local function CompareUnsignedIntegerText(left, right)
    left = NormalizeUnsignedIntegerText(left) or "0"
    right = NormalizeUnsignedIntegerText(right) or "0"

    if #left ~= #right then
        return #left < #right and -1 or 1
    end

    if left == right then
        return 0
    end

    return left < right and -1 or 1
end

local function MultiplyUnsignedIntegerTextBySmall(value, multiplier)
    local digits = NormalizeUnsignedIntegerText(value)
    multiplier = math.floor(tonumber(multiplier) or 0)

    if not digits or digits == "0" or multiplier <= 0 then
        return "0"
    end

    local carry = 0
    local parts = {}

    for index = #digits, 1, -1 do
        local digit = tonumber(digits:sub(index, index)) or 0
        local product = (digit * multiplier) + carry
        table.insert(parts, 1, tostring(product % 10))
        carry = math.floor(product / 10)
    end

    while carry > 0 do
        table.insert(parts, 1, tostring(carry % 10))
        carry = math.floor(carry / 10)
    end

    return NormalizeUnsignedIntegerText(table.concat(parts)) or "0"
end

local function DivideUnsignedIntegerTextBySmall(value, divisor)
    local digits = NormalizeUnsignedIntegerText(value)
    divisor = math.floor(tonumber(divisor) or 0)

    if not digits or digits == "0" or divisor <= 0 then
        return "0"
    end

    local remainder = 0
    local parts = {}

    for index = 1, #digits do
        local digit = tonumber(digits:sub(index, index)) or 0
        local current = (remainder * 10) + digit
        local quotientDigit = math.floor(current / divisor)
        remainder = current - (quotientDigit * divisor)
        table.insert(parts, tostring(quotientDigit))
    end

    return NormalizeUnsignedIntegerText(table.concat(parts)) or "0"
end

local function CalculateScaledHealthText(maxHealthText, currentHealth, officialMaxHealth)
    local normalizedMax = NormalizeUnsignedIntegerText(maxHealthText)
    currentHealth = math.floor(tonumber(currentHealth) or 0)
    officialMaxHealth = math.floor(tonumber(officialMaxHealth) or 0)

    if not normalizedMax then
        return tostring(currentHealth)
    end

    if officialMaxHealth <= 0 or currentHealth <= 0 then
        return "0"
    end

    if currentHealth >= officialMaxHealth then
        return normalizedMax
    end

    local scaled = DivideUnsignedIntegerTextBySmall(MultiplyUnsignedIntegerTextBySmall(normalizedMax, currentHealth), officialMaxHealth)
    if CompareUnsignedIntegerText(scaled, normalizedMax) > 0 then
        return normalizedMax
    end

    if scaled == "0" and currentHealth > 0 then
        return "1"
    end

    return scaled
end

local function IsUnitDeadForResourceDisplay(unit)
    if UnitIsDeadOrGhost and UnitIsDeadOrGhost(unit) then
        return true
    end

    if UnitIsDead and UnitIsDead(unit) then
        return true
    end

    return false
end

local function GetScaledCurrentResourceText(maxValueText, currentValue, officialMaxValue, cachedValueText, unit)
    local cachedValue = NormalizeUnsignedIntegerText(cachedValueText)
    currentValue = math.floor(tonumber(currentValue) or 0)
    officialMaxValue = math.floor(tonumber(officialMaxValue) or 0)

    if currentValue <= 0 and cachedValue and cachedValue ~= "0" and not IsUnitDeadForResourceDisplay(unit) then
        return cachedValue
    end

    if cachedValue then
        return cachedValue
    end

    if currentValue > 0 and officialMaxValue > 0 then
        return CalculateScaledHealthText(maxValueText, currentValue, officialMaxValue)
    end

    return CalculateScaledHealthText(maxValueText, currentValue, officialMaxValue)
end

local function GetRealMaxHealthText()
    local statState = State.stats.maxHealth
    local realMaxHealth = statState and NormalizeUnsignedIntegerText(statState.real)
    if realMaxHealth then
        return realMaxHealth
    end

    return NormalizeUnsignedIntegerText(UnitHealthMax and UnitHealthMax("player") or 0) or "0"
end

local function GetRealCurrentHealthText(maxHealthText)
    local currentHealth = UnitHealth and UnitHealth("player") or 0
    local officialMaxHealth = UnitHealthMax and UnitHealthMax("player") or 0
    return GetScaledCurrentResourceText(maxHealthText, currentHealth, officialMaxHealth, State.realCurrentHealth, "player")
end

local function GetRealMaxManaText()
    local statState = State.stats.maxMana
    local realMaxMana = statState and NormalizeUnsignedIntegerText(statState.real)
    if realMaxMana then
        return realMaxMana
    end

    return NormalizeUnsignedIntegerText(UnitPowerMax and UnitPowerMax("player", 0) or 0) or "0"
end

local function GetRealCurrentManaText(maxManaText)
    local currentMana = UnitPower and UnitPower("player", 0) or 0
    local officialMaxMana = UnitPowerMax and UnitPowerMax("player", 0) or 0
    return GetScaledCurrentResourceText(maxManaText, currentMana, officialMaxMana, State.realCurrentMana, "player")
end

local function ResetTargetData()
    State.targetData.hasData = false
    State.targetData.currentHealth = nil
    State.targetData.maxHealth = nil
    State.targetData.currentMana = nil
    State.targetData.maxMana = nil
    State.targetData.powerType = nil
    State.targetSyncSnapshot = nil
    State.targetHeartbeatElapsed = 0
end

local function GetTargetMaxHealthText()
    local realMaxHealth = NormalizeUnsignedIntegerText(State.targetData.maxHealth)
    if realMaxHealth then
        return realMaxHealth
    end

    return NormalizeUnsignedIntegerText(UnitHealthMax and UnitHealthMax("target") or 0) or "0"
end

local function GetTargetCurrentHealthText(maxHealthText)
    local realCurrentHealth = NormalizeUnsignedIntegerText(State.targetData.currentHealth)
    if realCurrentHealth then
        return realCurrentHealth
    end

    local currentHealth = UnitHealth and UnitHealth("target") or 0
    local officialMaxHealth = UnitHealthMax and UnitHealthMax("target") or 0
    return CalculateScaledHealthText(maxHealthText, currentHealth, officialMaxHealth)
end

local function GetTargetMaxManaText()
    local realMaxMana = NormalizeUnsignedIntegerText(State.targetData.maxMana)
    if realMaxMana then
        return realMaxMana
    end

    return NormalizeUnsignedIntegerText(UnitPowerMax and UnitPowerMax("target", 0) or 0) or "0"
end

local function GetTargetCurrentManaText(maxManaText)
    local realCurrentMana = NormalizeUnsignedIntegerText(State.targetData.currentMana)
    if realCurrentMana then
        return realCurrentMana
    end

    local currentMana = UnitPower and UnitPower("target", 0) or 0
    local officialMaxMana = UnitPowerMax and UnitPowerMax("target", 0) or 0
    return CalculateScaledHealthText(maxManaText, currentMana, officialMaxMana)
end

local function GetEntryColor(entryDef)
    return unpack(CATEGORY_COLORS[entryDef.category or "misc"] or CATEGORY_COLORS.misc)
end

local function GetOfficialStat(statIndex)
    local base, effective = UnitStat("player", statIndex)
    if effective ~= nil then
        return effective
    end

    return base or 0
end

local function GetOfficialValue(entryDef)
    if entryDef.apiIndex then
        return GetOfficialStat(entryDef.apiIndex)
    end

    if entryDef.officialFunc then
        return entryDef.officialFunc()
    end

    return 0
end

local function SetStatusText(text)
    State.statusText = text or "未同步"
end

local function SafeHide(frame)
    if frame and frame.Hide then
        frame:Hide()
    end
end

local function SafeShow(frame)
    if frame and frame.Show then
        frame:Show()
    end
end

local function IsShown(frame)
    return frame and frame.IsShown and frame:IsShown()
end

local function IsPaperDollVisible()
    if not CharacterFrame or not IsShown(CharacterFrame) then
        return false
    end

    if PaperDollFrame and PaperDollFrame.IsShown then
        return PaperDollFrame:IsShown()
    end

    return true
end

local function ForEachOfficialStatsFrame(callback)
    for _, frameName in ipairs(OFFICIAL_STATS_FRAME_NAMES) do
        local frame = _G[frameName]
        if frame then
            callback(frame)
        end
    end

    for index = 1, 20 do
        local frame = _G["PlayerStatFrame" .. index]
        if frame then
            callback(frame)
        end
    end
end

local function ApplyOfficialStatsVisibility()
    if not REPLACE_OFFICIAL_STATS then
        return
    end

    if IsPaperDollVisible() then
        ForEachOfficialStatsFrame(SafeHide)
        if State.panel then
            State.panel:Show()
        end
    else
        ForEachOfficialStatsFrame(SafeShow)
        if State.panel then
            State.panel:Hide()
        end
    end
end

local function HookOfficialStatsFrames()
    if State.officialFramesHooked then
        return
    end

    ForEachOfficialStatsFrame(function(frame)
        if frame.HookScript then
            frame:HookScript("OnShow", function(self)
                if REPLACE_OFFICIAL_STATS and IsPaperDollVisible() then
                    self:Hide()
                end
            end)
        end
    end)

    State.officialFramesHooked = true
end

local function HideNamedStatusTexts(frameNames)
    for _, frameName in ipairs(frameNames) do
        local frame = _G[frameName]
        if frame then
            if frame.SetText then
                frame:SetText("")
            end
            if frame.SetAlpha then
                frame:SetAlpha(0)
            end
            if frame.Hide then
                frame:Hide()
            end
        end
    end
end

local function HideStatusBarTexts(statusBar, customText)
    if not statusBar then
        return
    end

    local textRegions = { statusBar:GetRegions() }
    for _, region in ipairs(textRegions) do
        if region and region ~= customText and region.SetText and region.SetAlpha then
            region:SetText("")
            region:SetAlpha(0)
            if region.Hide then
                region:Hide()
            end
        end
    end

    local statusBarTexts = {
        statusBar.TextString,
        statusBar.LeftText,
        statusBar.RightText,
        statusBar.textString,
        statusBar.leftText,
        statusBar.rightText
    }

    for _, frame in ipairs(statusBarTexts) do
        if frame and frame ~= customText then
            if frame.SetText then
                frame:SetText("")
            end
            if frame.SetAlpha then
                frame:SetAlpha(0)
            end
            if frame.Hide then
                frame:Hide()
            end
        end
    end
end

local function ConfigureMaskTexture(texture, width, height)
    texture:SetWidth(width)
    texture:SetHeight(height)
    if texture.SetDrawLayer then
        texture:SetDrawLayer("OVERLAY")
    end
    if texture.SetColorTexture then
        texture:SetColorTexture(0, 0, 0, 0.96)
    else
        texture:SetTexture(0, 0, 0, 0.96)
    end
end

local function RaiseUnitFrameText(fontString)
    if fontString.SetDrawLayer then
        fontString:SetDrawLayer("OVERLAY")
    end
end

local function SetSolidTexture(texture, red, green, blue, alpha)
    if texture.SetColorTexture then
        texture:SetColorTexture(red, green, blue, alpha)
    else
        texture:SetTexture(red, green, blue, alpha)
    end
end

local function GetUnitFrameTexture(assetName)
    return UNIT_FRAME_TEXTURE_PATH .. tostring(assetName or "")
end

local function SetTextureAsset(texture, assetName)
    if texture and texture.SetTexture and assetName then
        texture:SetTexture(GetUnitFrameTexture(assetName))
    end
end

local function NormalizeClassToken(classToken)
    classToken = string.upper(tostring(classToken or ""))
    if CLASS_FRAME_COLORS[classToken] then
        return classToken
    end

    return "NEUTRAL"
end

local function GetUnitClassToken(unit)
    if UnitClass then
        local _, classToken = UnitClass(unit)
        return NormalizeClassToken(classToken)
    end

    return "NEUTRAL"
end

local function GetUnitPowerTypeValue(unit, fallbackPowerType)
    local powerType = tonumber(fallbackPowerType)
    if powerType ~= nil then
        return powerType
    end

    if UnitPowerType then
        local unitPowerType = UnitPowerType(unit)
        if unitPowerType ~= nil then
            powerType = tonumber(unitPowerType)
        end
    end

    return powerType or 0
end

local function GetPowerBarTexture(powerType)
    return POWER_BAR_TEXTURES[tonumber(powerType) or 0] or POWER_BAR_TEXTURES[0]
end

local function ApplyPowerTextColor(fontString, powerType)
    if not fontString or not fontString.SetTextColor then
        return
    end

    local color = POWER_TEXT_COLORS[tonumber(powerType) or 0] or POWER_TEXT_COLORS[0]
    fontString:SetTextColor(color[1], color[2], color[3])
end

local function ApplyPowerBarStyle(fill, text, powerType)
    SetTextureAsset(fill, GetPowerBarTexture(powerType))
    ApplyPowerTextColor(text, powerType)
end

local function GetUnitPowerValueTexts(unit, powerType)
    powerType = tonumber(powerType) or 0

    local currentValue = 0
    local maxValue = 0
    if UnitPower then
        currentValue = UnitPower(unit, powerType) or UnitPower(unit) or 0
    end
    if UnitPowerMax then
        maxValue = UnitPowerMax(unit, powerType) or UnitPowerMax(unit) or 0
    end

    return tostring(math.floor(tonumber(currentValue) or 0)), tostring(math.floor(tonumber(maxValue) or 0))
end

local function HasPositiveUnsignedText(value)
    local normalized = NormalizeUnsignedIntegerText(value)
    return normalized ~= nil and normalized ~= "0"
end

local function SetCustomBarFillRatio(fill, ratio, barWidth)
    if not fill then
        return
    end

    ratio = tonumber(ratio) or 0
    if ratio <= 0 then
        fill:Hide()
        return
    elseif ratio > 1 then
        ratio = 1
    end

    barWidth = tonumber(barWidth) or CUSTOM_PLAYER_BAR_WIDTH
    fill:SetWidth(math.max(1, math.floor((barWidth * ratio) + 0.5)))
    if fill.SetTexCoord then
        fill:SetTexCoord(0, ratio, 0, 1)
    end
    fill:Show()
end

local function ApplyCustomUnitFrameStyle(frame, unit, isTarget)
    if not frame then
        return
    end

    local classToken = GetUnitClassToken(unit)
    if frame.frameArt then
        frame.frameArt:Hide()
    end
    SetTextureAsset(frame.portraitRing, "PortraitRing_" .. classToken)
    SetTextureAsset(frame.healthBg, "Bar_Backdrop")
    SetTextureAsset(frame.healthFill, "Bar_Health")

    local powerType = GetUnitPowerTypeValue(unit, isTarget and State.targetData.powerType or nil)
    SetTextureAsset(frame.powerBg, "Bar_Backdrop")
    ApplyPowerBarStyle(frame.powerFill, frame.powerText, powerType)

    if frame.nameText and frame.nameText.SetTextColor then
        local color = CLASS_FRAME_COLORS[classToken] or CLASS_FRAME_COLORS.NEUTRAL
        frame.nameText:SetTextColor(color[1], color[2], color[3])
    end
end

local function UpdateCustomPlayerRunes()
    local runes = State.customPlayerRunes
    if not runes then
        return
    end

    local classToken = GetUnitClassToken("player")
    if classToken ~= "DEATHKNIGHT" then
        for _, rune in ipairs(runes) do
            rune:Hide()
        end
        return
    end

    for index, rune in ipairs(runes) do
        local runeType = GetRuneType and GetRuneType(index) or nil
        local textureName = RUNE_TYPE_TEXTURES[tonumber(runeType)] or DEFAULT_RUNE_TEXTURES[index] or "Rune_Death"
        SetTextureAsset(rune, textureName)

        local ready = true
        if GetRuneCooldown then
            local _, _, runeReady = GetRuneCooldown(index)
            ready = runeReady ~= false
        end

        if rune.SetAlpha then
            rune:SetAlpha(ready and 1 or 0.35)
        end
        rune:Show()
    end
end

local function SetFrameAlpha(frame, alpha)
    if frame and frame.SetAlpha then
        frame:SetAlpha(alpha)
    end
end

local function HideFrameRegions(frame)
    if not frame or not frame.GetRegions then
        return
    end

    local regions = { frame:GetRegions() }
    for _, region in ipairs(regions) do
        SetFrameAlpha(region, 0)
        if region and region.SetText then
            region:SetText("")
        end
    end
end

local function IsCustomPlayerFrameNode(frame)
    if not frame then
        return false
    end

    if State.customPlayerFrame and frame == State.customPlayerFrame then
        return true
    end

    if frame.GetName then
        local name = frame:GetName()
        return name == "PlayerAttributePanelCustomPlayerFrame"
    end

    return false
end

local function IsCustomTargetFrameNode(frame)
    if not frame then
        return false
    end

    if State.customTargetFrame and frame == State.customTargetFrame then
        return true
    end

    if frame.GetName then
        local name = frame:GetName()
        return name == "PlayerAttributePanelCustomTargetFrame"
    end

    return false
end

local function HideOfficialPlayerFrameTree(frame)
    if not frame or IsCustomPlayerFrameNode(frame) then
        return
    end

    HideFrameRegions(frame)

    if not frame.GetChildren then
        return
    end

    local children = { frame:GetChildren() }
    for _, child in ipairs(children) do
        if not IsCustomPlayerFrameNode(child) then
            SetFrameAlpha(child, 0)
            HideOfficialPlayerFrameTree(child)
        end
    end
end

local function HideOfficialTargetFrameTree(frame)
    if not frame or IsCustomTargetFrameNode(frame) then
        return
    end

    HideFrameRegions(frame)

    if not frame.GetChildren then
        return
    end

    local children = { frame:GetChildren() }
    for _, child in ipairs(children) do
        if not IsCustomTargetFrameNode(child) then
            SetFrameAlpha(child, 0)
            HideOfficialTargetFrameTree(child)
        end
    end
end

local function HideOfficialPlayerHealthText()
    HideNamedStatusTexts(OFFICIAL_PLAYER_HEALTH_TEXT_NAMES)
    HideStatusBarTexts(_G.PlayerFrameHealthBar, State.playerHealthText)
end

local function HideOfficialPlayerPowerText()
    HideNamedStatusTexts(OFFICIAL_PLAYER_POWER_TEXT_NAMES)
    HideStatusBarTexts(_G.PlayerFrameManaBar, State.playerPowerText)
end

local function HideOfficialPlayerFrameVisuals()
    HideOfficialPlayerHealthText()
    HideOfficialPlayerPowerText()
    HideFrameRegions(_G.PlayerFrame)
    HideOfficialPlayerFrameTree(_G.PlayerFrame)

    local officialVisuals = {
        _G.PlayerFrameTexture,
        _G.PlayerFrameBackground,
        _G.PlayerFramePortrait,
        _G.PlayerFrameName,
        _G.PlayerLevelText,
        _G.PlayerFrameHealthBar,
        _G.PlayerFrameManaBar,
        _G.PlayerStatusTexture,
        _G.PlayerRestIcon,
        _G.PlayerAttackIcon,
        _G.PlayerPVPIcon,
        _G.PlayerFramePVPIcon,
        _G.PlayerFrameFlash,
        _G.PlayerHitIndicator
    }

    for _, frame in ipairs(officialVisuals) do
        SetFrameAlpha(frame, 0)
    end
end

local function PlayerHasMana(maxManaText)
    local unitPowerType = nil
    if UnitPowerType then
        unitPowerType = UnitPowerType("player")
    end

    if unitPowerType ~= nil and unitPowerType ~= 0 then
        return false
    end

    local normalizedMax = NormalizeUnsignedIntegerText(maxManaText)
    return normalizedMax ~= nil and normalizedMax ~= "0"
end

local function UpdateCustomPlayerIdentity()
    ApplyCustomUnitFrameStyle(State.customPlayerFrame, "player", false)

    if State.customPlayerPortrait and SetPortraitTexture then
        SetPortraitTexture(State.customPlayerPortrait, "player")
    end

    if State.customPlayerNameText then
        State.customPlayerNameText:SetText(UnitName and UnitName("player") or "")
    end

    if State.customPlayerLevelText then
        local level = UnitLevel and UnitLevel("player") or ""
        State.customPlayerLevelText:SetText(tostring(level or ""))
    end
end

local function UpdateCustomUnitBarFill(fill, unit, powerType, barWidth)
    if not fill then
        return
    end

    local currentValue = 0
    local maxValue = 0
    if powerType ~= nil then
        currentValue = UnitPower and UnitPower(unit, powerType) or 0
        maxValue = UnitPowerMax and UnitPowerMax(unit, powerType) or 0
    else
        currentValue = UnitHealth and UnitHealth(unit) or 0
        maxValue = UnitHealthMax and UnitHealthMax(unit) or 0
    end

    currentValue = tonumber(currentValue) or 0
    maxValue = tonumber(maxValue) or 0
    if maxValue <= 0 or currentValue <= 0 then
        fill:Hide()
        return
    end

    local ratio = currentValue / maxValue
    if ratio > 1 then
        ratio = 1
    elseif ratio < 0 then
        ratio = 0
    end

    SetCustomBarFillRatio(fill, ratio, barWidth)
end

local function GetUnsignedIntegerBarRatio(currentText, maxText)
    local currentValue = NormalizeUnsignedIntegerText(currentText)
    local maxValue = NormalizeUnsignedIntegerText(maxText)
    if not currentValue or not maxValue or currentValue == "0" or maxValue == "0" then
        return 0
    end

    if CompareUnsignedIntegerText(currentValue, maxValue) >= 0 then
        return 1
    end

    local function GetMantissa(value)
        local digits = math.min(12, #value)
        local lead = tonumber(value:sub(1, digits)) or 0
        return lead / math.pow(10, digits - 1)
    end

    local ratio = (GetMantissa(currentValue) / GetMantissa(maxValue)) * math.pow(10, #currentValue - #maxValue)
    if ratio > 1 then
        return 1
    elseif ratio < 0 then
        return 0
    end

    return ratio
end

local function UpdateCustomBarFillByText(fill, currentText, maxText, barWidth)
    if not fill then
        return
    end

    local ratio = GetUnsignedIntegerBarRatio(currentText, maxText)
    if ratio <= 0 then
        fill:Hide()
        return
    end

    SetCustomBarFillRatio(fill, ratio, barWidth)
end

local function CreateCustomPlayerFont(parent, template, width, red, green, blue)
    local fontString = parent:CreateFontString(nil, "OVERLAY", template)
    fontString:SetWidth(width)
    fontString:SetJustifyH("CENTER")
    fontString:SetTextColor(red, green, blue)
    if fontString.SetShadowColor then
        fontString:SetShadowColor(0, 0, 0, 1)
    end
    if fontString.SetShadowOffset then
        fontString:SetShadowOffset(1, -1)
    end
    RaiseUnitFrameText(fontString)
    return fontString
end

local function OpenPortraitUnitMenu(self)
    if not self then
        return
    end

    local unit = self.unit
    if (not unit or unit == "") and self.GetAttribute then
        unit = self:GetAttribute("unit")
    end

    if not unit or unit == "" then
        return
    end

    local dropdown = nil
    if unit == "player" then
        dropdown = _G.PlayerFrameDropDown
    elseif unit == "target" then
        dropdown = _G.TargetFrameDropDown
    end

    if dropdown and ToggleDropDownMenu then
        ToggleDropDownMenu(1, nil, dropdown, "cursor", 0, 0)
    end
end

local function EnsurePortraitUnitButton(frame, buttonName, unit, hitSize)
    if not frame or not frame.portrait then
        return
    end

    local parent = frame
    local button = frame.portraitButton
    if not button then
        if InCombatLockdown and InCombatLockdown() then
            return
        end
        button = CreateFrame("Button", buttonName, parent, "SecureUnitButtonTemplate")
        frame.portraitButton = button
    end

    if InCombatLockdown and InCombatLockdown() then
        return
    end

    if button.GetParent and button:GetParent() ~= parent and button.SetParent then
        button:SetParent(parent)
    end

    button:ClearAllPoints()
    button:SetPoint("CENTER", frame.portrait, "CENTER", 0, 0)
    button:SetWidth(hitSize)
    button:SetHeight(hitSize)
    button.unit = unit

    if button.EnableMouse then
        button:EnableMouse(true)
    end
    if button.RegisterForClicks then
        button:RegisterForClicks("AnyUp")
    end
    if button.SetAttribute then
        button:SetAttribute("unit", unit)
        button:SetAttribute("type1", "target")
        button:SetAttribute("*type1", "target")
        button:SetAttribute("type2", "menu")
        button:SetAttribute("*type2", "menu")
    end
    button.menu = OpenPortraitUnitMenu
    if button.SetFrameStrata and frame.GetFrameStrata then
        button:SetFrameStrata(frame:GetFrameStrata())
    end
    if button.SetFrameLevel and frame.GetFrameLevel then
        button:SetFrameLevel((frame:GetFrameLevel() or 0) + 30)
    end
    if button.SetScript and UnitFrame_OnEnter and UnitFrame_OnLeave then
        button:SetScript("OnEnter", UnitFrame_OnEnter)
        button:SetScript("OnLeave", UnitFrame_OnLeave)
    end
    button:Show()
end

local function HidePortraitUnitButton(frame)
    if frame and frame.portraitButton then
        frame.portraitButton:Hide()
    end
end

local function EnsureCustomPlayerFrame()
    local playerFrame = _G.PlayerFrame
    if not playerFrame then
        return
    end

    local frame = State.customPlayerFrame or _G.PlayerAttributePanelCustomPlayerFrame
    if not frame then
        frame = CreateFrame("Frame", "PlayerAttributePanelCustomPlayerFrame", playerFrame)
        frame:SetWidth(CUSTOM_PLAYER_FRAME_WIDTH)
        frame:SetHeight(CUSTOM_PLAYER_FRAME_HEIGHT)
        if frame.EnableMouse then
            frame:EnableMouse(false)
        end

        frame.bg = frame:CreateTexture(nil, "BACKGROUND")
        frame.bg:SetPoint("TOPLEFT", frame, "TOPLEFT", 4, -8)
        frame.bg:SetWidth(CUSTOM_PLAYER_FRAME_WIDTH - 18)
        frame.bg:SetHeight(CUSTOM_PLAYER_FRAME_HEIGHT - 18)
        SetSolidTexture(frame.bg, 0, 0, 0, 0)

        frame.frameArt = frame:CreateTexture(nil, "BACKGROUND")
        frame.frameArt:SetAllPoints(frame)

        frame.portrait = frame:CreateTexture(nil, "OVERLAY")
        frame.portrait:SetPoint("TOPLEFT", frame, "TOPLEFT", CUSTOM_PLAYER_PORTRAIT_LEFT, -14)
        frame.portrait:SetWidth(CUSTOM_PLAYER_PORTRAIT_SIZE)
        frame.portrait:SetHeight(CUSTOM_PLAYER_PORTRAIT_SIZE)

        frame.portraitRing = frame:CreateTexture(nil, "OVERLAY")
        frame.portraitRing:SetPoint("CENTER", frame.portrait, "CENTER", 0, 0)
        frame.portraitRing:SetWidth(CUSTOM_PLAYER_PORTRAIT_SIZE + 22)
        frame.portraitRing:SetHeight(CUSTOM_PLAYER_PORTRAIT_SIZE + 22)

        frame.levelText = CreateCustomPlayerFont(frame, "GameFontNormalSmall", 34, 1.00, 0.82, 0.10)
        frame.levelText:SetPoint("BOTTOMLEFT", frame.portrait, "BOTTOMLEFT", -6, 0)

        frame.nameText = CreateCustomPlayerFont(frame, "GameFontNormalSmall", 118, 1.00, 0.82, 0.10)
        frame.nameText:SetPoint("TOPLEFT", frame, "TOPLEFT", CUSTOM_PLAYER_BAR_LEFT, -14)

        frame.healthBg = frame:CreateTexture(nil, "ARTWORK")
        frame.healthBg:SetPoint("TOPLEFT", frame, "TOPLEFT", CUSTOM_PLAYER_BAR_LEFT, CUSTOM_PLAYER_HEALTH_BAR_TOP)
        frame.healthBg:SetWidth(CUSTOM_PLAYER_BAR_WIDTH)
        frame.healthBg:SetHeight(18)
        SetTextureAsset(frame.healthBg, "Bar_Backdrop")

        frame.healthFill = frame:CreateTexture(nil, "OVERLAY")
        frame.healthFill:SetPoint("LEFT", frame.healthBg, "LEFT", 0, 0)
        frame.healthFill:SetHeight(14)
        SetTextureAsset(frame.healthFill, "Bar_Health")

        frame.healthText = CreateCustomPlayerFont(frame, "GameFontHighlightSmall", CUSTOM_PLAYER_BAR_WIDTH, 0.30, 1.00, 0.30)
        frame.healthText:SetPoint("CENTER", frame.healthBg, "CENTER", 0, 0)

        frame.powerBg = frame:CreateTexture(nil, "ARTWORK")
        frame.powerBg:SetPoint("TOPLEFT", frame, "TOPLEFT", CUSTOM_PLAYER_BAR_LEFT, CUSTOM_PLAYER_POWER_BAR_TOP)
        frame.powerBg:SetWidth(CUSTOM_PLAYER_BAR_WIDTH)
        frame.powerBg:SetHeight(16)
        SetTextureAsset(frame.powerBg, "Bar_Backdrop")

        frame.powerFill = frame:CreateTexture(nil, "OVERLAY")
        frame.powerFill:SetPoint("LEFT", frame.powerBg, "LEFT", 0, 0)
        frame.powerFill:SetHeight(12)
        SetTextureAsset(frame.powerFill, "Bar_Mana")

        frame.powerText = CreateCustomPlayerFont(frame, "GameFontHighlightSmall", CUSTOM_PLAYER_BAR_WIDTH, 0.45, 0.65, 1.00)
        frame.powerText:SetPoint("CENTER", frame.powerBg, "CENTER", 0, 0)

        frame.runes = {}
        for index = 1, CUSTOM_RUNE_COUNT do
            local rune = frame:CreateTexture(nil, "OVERLAY")
            rune:SetWidth(CUSTOM_RUNE_SIZE)
            rune:SetHeight(CUSTOM_RUNE_SIZE)
            rune:SetPoint("TOPLEFT", frame, "TOPLEFT", CUSTOM_PLAYER_BAR_LEFT + ((index - 1) * (CUSTOM_RUNE_SIZE + CUSTOM_RUNE_SPACING)), CUSTOM_RUNE_TOP)
            SetTextureAsset(rune, DEFAULT_RUNE_TEXTURES[index] or "Rune_Death")
            rune:Hide()
            frame.runes[index] = rune
        end
    end

    frame:ClearAllPoints()
    frame:SetPoint("TOPLEFT", playerFrame, "TOPLEFT", CUSTOM_PLAYER_FRAME_X_OFFSET, 4)
    if frame.SetFrameStrata and playerFrame.GetFrameStrata then
        frame:SetFrameStrata(playerFrame:GetFrameStrata())
    end
    if frame.SetFrameLevel and playerFrame.GetFrameLevel then
        frame:SetFrameLevel((playerFrame:GetFrameLevel() or 0) + 100)
    end
    EnsurePortraitUnitButton(frame, "PlayerAttributePanelCustomPlayerPortraitButton", "player", CUSTOM_PLAYER_PORTRAIT_SIZE + 22)
    frame:Show()

    State.customPlayerFrame = frame
    State.customPlayerFrameArt = frame.frameArt
    State.customPlayerPortrait = frame.portrait
    State.customPlayerPortraitRing = frame.portraitRing
    State.customPlayerNameText = frame.nameText
    State.customPlayerLevelText = frame.levelText
    State.customPlayerHealthFill = frame.healthFill
    State.customPlayerPowerFill = frame.powerFill
    State.customPlayerRunes = frame.runes
    State.playerHealthBackdrop = frame.healthBg
    State.playerHealthText = frame.healthText
    State.playerPowerBackdrop = frame.powerBg
    State.playerPowerText = frame.powerText

    ApplyCustomUnitFrameStyle(frame, "player", false)
    UpdateCustomPlayerRunes()
    HideOfficialPlayerFrameVisuals()
    UpdateCustomPlayerIdentity()
end

local function UpdatePlayerPortraitHealthText()
    if not State.playerHealthText then
        return
    end

    HideOfficialPlayerFrameVisuals()
    UpdateCustomPlayerIdentity()

    local maxHealthText = GetRealMaxHealthText()
    local currentHealthText = GetRealCurrentHealthText(maxHealthText)

    State.playerHealthText:SetText(FormatChineseNumber(currentHealthText) .. "/" .. FormatChineseNumber(maxHealthText))
    UpdateCustomBarFillByText(State.customPlayerHealthFill, currentHealthText, maxHealthText, CUSTOM_PLAYER_BAR_WIDTH)
    if State.playerHealthBackdrop then
        State.playerHealthBackdrop:Show()
    end
    State.playerHealthText:Show()
end

local function UpdatePlayerPortraitPowerText()
    if not State.playerPowerText then
        return
    end

    local powerType = GetUnitPowerTypeValue("player", nil)
    ApplyPowerBarStyle(State.customPlayerPowerFill, State.playerPowerText, powerType)
    UpdateCustomPlayerRunes()

    local maxManaText = GetRealMaxManaText()
    if PlayerHasMana(maxManaText) then
        HideOfficialPlayerFrameVisuals()

        local currentManaText = GetRealCurrentManaText(maxManaText)
        State.playerPowerText:SetText(FormatChineseNumber(currentManaText) .. "/" .. FormatChineseNumber(maxManaText))
        UpdateCustomBarFillByText(State.customPlayerPowerFill, currentManaText, maxManaText, CUSTOM_PLAYER_BAR_WIDTH)
        if State.playerPowerBackdrop then
            State.playerPowerBackdrop:Show()
        end
        State.playerPowerText:Show()
    else
        local currentPowerText, maxPowerText = GetUnitPowerValueTexts("player", powerType)
        if HasPositiveUnsignedText(maxPowerText) then
            HideOfficialPlayerFrameVisuals()

            State.playerPowerText:SetText(FormatChineseNumber(currentPowerText) .. "/" .. FormatChineseNumber(maxPowerText))
            UpdateCustomBarFillByText(State.customPlayerPowerFill, currentPowerText, maxPowerText, CUSTOM_PLAYER_BAR_WIDTH)
            if State.playerPowerBackdrop then
                State.playerPowerBackdrop:Show()
            end
            State.playerPowerText:Show()
        else
            if State.customPlayerPowerFill then
                State.customPlayerPowerFill:Hide()
            end
            if State.playerPowerBackdrop then
                State.playerPowerBackdrop:Hide()
            end
            State.playerPowerText:Hide()
        end
    end
end

local function EnsurePlayerPortraitHealth()
    if State.playerHealthHooked then
        EnsureCustomPlayerFrame()
        UpdatePlayerPortraitHealthText()
        UpdatePlayerPortraitPowerText()
        return
    end

    local healthBar = _G.PlayerFrameHealthBar
    local powerBar = _G.PlayerFrameManaBar
    EnsureCustomPlayerFrame()
    if not State.customPlayerFrame then
        return
    end

    if healthBar and healthBar.HookScript then
        healthBar:HookScript("OnShow", function()
            EnsureCustomPlayerFrame()
            UpdatePlayerPortraitHealthText()
        end)
        healthBar:HookScript("OnValueChanged", UpdatePlayerPortraitHealthText)
    end

    if powerBar and powerBar.HookScript then
        powerBar:HookScript("OnShow", function()
            EnsureCustomPlayerFrame()
            UpdatePlayerPortraitPowerText()
        end)
        powerBar:HookScript("OnValueChanged", UpdatePlayerPortraitPowerText)
    end

    for _, frameName in ipairs(OFFICIAL_PLAYER_HEALTH_TEXT_NAMES) do
        local frame = _G[frameName]
        if frame and frame.HookScript then
            frame:HookScript("OnShow", HideOfficialPlayerFrameVisuals)
        end
    end

    for _, frameName in ipairs(OFFICIAL_PLAYER_POWER_TEXT_NAMES) do
        local frame = _G[frameName]
        if frame and frame.HookScript then
            frame:HookScript("OnShow", HideOfficialPlayerFrameVisuals)
        end
    end

    if hooksecurefunc and TextStatusBar_UpdateTextString then
        hooksecurefunc("TextStatusBar_UpdateTextString", function(bar)
            if bar == _G.PlayerFrameHealthBar then
                UpdatePlayerPortraitHealthText()
            elseif bar == _G.PlayerFrameManaBar then
                UpdatePlayerPortraitPowerText()
            end
        end)
    end

    if hooksecurefunc and PlayerFrame_Update then
        hooksecurefunc("PlayerFrame_Update", function()
            EnsureCustomPlayerFrame()
            UpdatePlayerPortraitHealthText()
            UpdatePlayerPortraitPowerText()
        end)
    end

    if hooksecurefunc and PlayerFrame_UpdateStatus then
        hooksecurefunc("PlayerFrame_UpdateStatus", HideOfficialPlayerFrameVisuals)
    end

    if _G.PlayerFrame and _G.PlayerFrame.HookScript then
        _G.PlayerFrame:HookScript("OnShow", function()
            EnsureCustomPlayerFrame()
            UpdatePlayerPortraitHealthText()
            UpdatePlayerPortraitPowerText()
        end)
    end

    State.playerHealthHooked = true
    UpdatePlayerPortraitHealthText()
    UpdatePlayerPortraitPowerText()
end

local function HideOfficialTargetHealthText()
    HideNamedStatusTexts(OFFICIAL_TARGET_HEALTH_TEXT_NAMES)
    HideStatusBarTexts(_G.TargetFrameHealthBar, State.targetHealthText)
end

local function HideOfficialTargetPowerText()
    HideNamedStatusTexts(OFFICIAL_TARGET_POWER_TEXT_NAMES)
    HideStatusBarTexts(_G.TargetFrameManaBar, State.targetPowerText)
end

local function HideOfficialTargetFrameVisuals()
    HideOfficialTargetHealthText()
    HideOfficialTargetPowerText()
    HideFrameRegions(_G.TargetFrame)
    HideOfficialTargetFrameTree(_G.TargetFrame)

    local officialVisuals = {
        _G.TargetFrameTextureFrame,
        _G.TargetFrameTextureFrameTexture,
        _G.TargetFrameBackground,
        _G.TargetFramePortrait,
        _G.TargetFrameName,
        _G.TargetFrameTextureFrameName,
        _G.TargetFrameHealthBar,
        _G.TargetFrameManaBar,
        _G.TargetFrameTextureFrameHealthBarText,
        _G.TargetFrameTextureFrameManaBarText,
        _G.TargetFrameFlash,
        _G.TargetFrameSpellBar
    }

    for _, frame in ipairs(officialVisuals) do
        SetFrameAlpha(frame, 0)
    end
end

local function TargetHasMana(maxManaText)
    local powerType = tonumber(State.targetData.powerType)
    if powerType ~= nil and powerType ~= 0 then
        return false
    end

    local unitPowerType = nil
    if UnitPowerType then
        unitPowerType = UnitPowerType("target")
    end

    if unitPowerType ~= nil and unitPowerType ~= 0 then
        return false
    end

    local normalizedMax = NormalizeUnsignedIntegerText(maxManaText)
    return normalizedMax ~= nil and normalizedMax ~= "0"
end

local function FormatAuraRemaining(expirationTime, duration)
    duration = tonumber(duration) or 0
    expirationTime = tonumber(expirationTime) or 0
    if duration <= 0 or expirationTime <= 0 or not GetTime then
        return ""
    end

    local remaining = expirationTime - GetTime()
    if remaining <= 0 then
        return "0"
    elseif remaining >= 60 then
        return tostring(math.ceil(remaining / 60)) .. "m"
    end

    return tostring(math.ceil(remaining))
end

local function IsPlayerCausedDebuff(unitCaster)
    unitCaster = string.lower(tostring(unitCaster or ""))
    return unitCaster == "player" or unitCaster == "pet" or unitCaster == "vehicle"
end

local function ReadTargetDebuff(index, filter)
    if UnitAura then
        return UnitAura("target", index, filter)
    end

    if UnitDebuff then
        return UnitDebuff("target", index, filter)
    end

    return nil
end

local function AddTargetDebuffAura(debuffs, seen, requirePlayerCaster, auraIndex, auraFilter, name, rank, icon, count, debuffType, duration, expirationTime, unitCaster, isStealable, shouldConsolidate, spellId)
    if not name or not icon then
        return
    end

    if requirePlayerCaster and not IsPlayerCausedDebuff(unitCaster) then
        return
    end

    local key = tostring(spellId or "") .. ":" .. tostring(name) .. ":" .. tostring(rank or "") .. ":" .. tostring(icon)
    if seen[key] then
        return
    end

    seen[key] = true
    table.insert(debuffs, {
        name = name,
        icon = icon,
        count = tonumber(count) or 0,
        debuffType = debuffType,
        duration = tonumber(duration) or 0,
        expirationTime = tonumber(expirationTime) or 0,
        spellId = spellId,
        auraIndex = auraIndex,
        auraFilter = auraFilter
    })
end

local function GetTargetPlayerDebuffs()
    local debuffs = {}
    local seen = {}

    for index = 1, 40 do
        AddTargetDebuffAura(debuffs, seen, false, index, "HARMFUL|PLAYER", ReadTargetDebuff(index, "HARMFUL|PLAYER"))
        if #debuffs >= CUSTOM_TARGET_DEBUFF_COUNT then
            return debuffs
        end
    end

    for index = 1, 40 do
        AddTargetDebuffAura(debuffs, seen, true, index, "HARMFUL", ReadTargetDebuff(index, "HARMFUL"))
        if #debuffs >= CUSTOM_TARGET_DEBUFF_COUNT then
            break
        end
    end

    return debuffs
end

local function HideCustomTargetDebuffs()
    if not State.customTargetDebuffs then
        return
    end

    for _, slot in ipairs(State.customTargetDebuffs) do
        slot.auraIndex = nil
        slot.auraFilter = nil
        slot:Hide()
    end
end

local function ShowCustomTargetDebuffTooltip(slot)
    if not slot or not slot.auraIndex or not GameTooltip then
        return
    end

    GameTooltip:SetOwner(slot, "ANCHOR_RIGHT")
    if GameTooltip.SetUnitAura then
        GameTooltip:SetUnitAura("target", slot.auraIndex, slot.auraFilter or "HARMFUL")
    elseif GameTooltip.SetUnitDebuff then
        GameTooltip:SetUnitDebuff("target", slot.auraIndex, slot.auraFilter or "HARMFUL")
    end
    GameTooltip:Show()
end

local function HideCustomTargetDebuffTooltip()
    if GameTooltip then
        GameTooltip:Hide()
    end
end

local function UpdateCustomTargetDebuffs()
    local slots = State.customTargetDebuffs
    if not slots then
        return
    end

    if not UnitExists or not UnitExists("target") then
        HideCustomTargetDebuffs()
        return
    end

    local debuffs = GetTargetPlayerDebuffs()
    for index, slot in ipairs(slots) do
        local debuff = debuffs[index]
        if debuff then
            slot.auraIndex = debuff.auraIndex
            slot.auraFilter = debuff.auraFilter
            slot.icon:SetTexture(debuff.icon)
            if debuff.count and debuff.count > 1 then
                slot.countText:SetText(tostring(debuff.count))
                slot.countText:Show()
            else
                slot.countText:SetText("")
                slot.countText:Hide()
            end
            slot.durationText:SetText(FormatAuraRemaining(debuff.expirationTime, debuff.duration))
            slot:Show()
        else
            slot.auraIndex = nil
            slot.auraFilter = nil
            slot:Hide()
        end
    end
end

local function UpdateCustomTargetIdentity()
    ApplyCustomUnitFrameStyle(State.customTargetFrame, "target", true)

    if State.customTargetPortrait and SetPortraitTexture then
        SetPortraitTexture(State.customTargetPortrait, "target")
    end

    if State.customTargetNameText then
        State.customTargetNameText:SetText(UnitName and UnitName("target") or "")
    end

    if State.customTargetLevelText then
        local level = UnitLevel and UnitLevel("target") or ""
        if tonumber(level) and tonumber(level) < 0 then
            level = "??"
        end
        State.customTargetLevelText:SetText(tostring(level or ""))
    end
end

local function EnsureCustomTargetFrame()
    local targetFrame = _G.TargetFrame
    if not targetFrame then
        return
    end

    local frame = State.customTargetFrame or _G.PlayerAttributePanelCustomTargetFrame
    if not frame then
        frame = CreateFrame("Frame", "PlayerAttributePanelCustomTargetFrame", targetFrame)
        frame:SetWidth(CUSTOM_TARGET_FRAME_WIDTH)
        frame:SetHeight(CUSTOM_TARGET_FRAME_HEIGHT)
        if frame.EnableMouse then
            frame:EnableMouse(false)
        end

        frame.bg = frame:CreateTexture(nil, "BACKGROUND")
        frame.bg:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -4, -8)
        frame.bg:SetWidth(CUSTOM_TARGET_FRAME_WIDTH - 18)
        frame.bg:SetHeight(CUSTOM_TARGET_FRAME_HEIGHT - 18)
        SetSolidTexture(frame.bg, 0, 0, 0, 0)

        frame.frameArt = frame:CreateTexture(nil, "BACKGROUND")
        frame.frameArt:SetAllPoints(frame)

        frame.portrait = frame:CreateTexture(nil, "OVERLAY")
        frame.portrait:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -CUSTOM_TARGET_PORTRAIT_RIGHT, -14)
        frame.portrait:SetWidth(CUSTOM_TARGET_PORTRAIT_SIZE)
        frame.portrait:SetHeight(CUSTOM_TARGET_PORTRAIT_SIZE)

        frame.portraitRing = frame:CreateTexture(nil, "OVERLAY")
        frame.portraitRing:SetPoint("CENTER", frame.portrait, "CENTER", 0, 0)
        frame.portraitRing:SetWidth(CUSTOM_TARGET_PORTRAIT_SIZE + 22)
        frame.portraitRing:SetHeight(CUSTOM_TARGET_PORTRAIT_SIZE + 22)

        frame.levelText = CreateCustomPlayerFont(frame, "GameFontNormalSmall", 34, 1.00, 0.82, 0.10)
        frame.levelText:SetPoint("BOTTOMRIGHT", frame.portrait, "BOTTOMRIGHT", 6, 0)

        frame.nameText = CreateCustomPlayerFont(frame, "GameFontNormalSmall", 118, 1.00, 0.82, 0.10)
        frame.nameText:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -CUSTOM_TARGET_BAR_RIGHT, -14)

        frame.healthBg = frame:CreateTexture(nil, "ARTWORK")
        frame.healthBg:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -CUSTOM_TARGET_BAR_RIGHT, CUSTOM_TARGET_HEALTH_BAR_TOP)
        frame.healthBg:SetWidth(CUSTOM_TARGET_BAR_WIDTH)
        frame.healthBg:SetHeight(18)
        SetTextureAsset(frame.healthBg, "Bar_Backdrop")

        frame.healthFill = frame:CreateTexture(nil, "OVERLAY")
        frame.healthFill:SetPoint("LEFT", frame.healthBg, "LEFT", 0, 0)
        frame.healthFill:SetHeight(14)
        SetTextureAsset(frame.healthFill, "Bar_Health")

        frame.healthText = CreateCustomPlayerFont(frame, "GameFontHighlightSmall", CUSTOM_TARGET_BAR_WIDTH, 1.00, 0.05, 0.05)
        frame.healthText:SetPoint("CENTER", frame.healthBg, "CENTER", 0, 0)

        frame.powerBg = frame:CreateTexture(nil, "ARTWORK")
        frame.powerBg:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -CUSTOM_TARGET_BAR_RIGHT, CUSTOM_TARGET_POWER_BAR_TOP)
        frame.powerBg:SetWidth(CUSTOM_TARGET_BAR_WIDTH)
        frame.powerBg:SetHeight(16)
        SetTextureAsset(frame.powerBg, "Bar_Backdrop")

        frame.powerFill = frame:CreateTexture(nil, "OVERLAY")
        frame.powerFill:SetPoint("LEFT", frame.powerBg, "LEFT", 0, 0)
        frame.powerFill:SetHeight(12)
        SetTextureAsset(frame.powerFill, "Bar_Mana")

        frame.powerText = CreateCustomPlayerFont(frame, "GameFontHighlightSmall", CUSTOM_TARGET_BAR_WIDTH, 0.45, 0.65, 1.00)
        frame.powerText:SetPoint("CENTER", frame.powerBg, "CENTER", 0, 0)

        frame.debuffs = {}
        for index = 1, CUSTOM_TARGET_DEBUFF_COUNT do
            local slot = CreateFrame("Frame", nil, frame)
            local column = (index - 1) % CUSTOM_TARGET_DEBUFF_COLUMNS
            local row = math.floor((index - 1) / CUSTOM_TARGET_DEBUFF_COLUMNS)
            local xOffset = column * (CUSTOM_TARGET_DEBUFF_SIZE + CUSTOM_TARGET_DEBUFF_SPACING)
            local yOffset = CUSTOM_TARGET_DEBUFF_TOP_OFFSET - (row * (CUSTOM_TARGET_DEBUFF_SIZE + CUSTOM_TARGET_DEBUFF_SPACING))
            slot:SetWidth(CUSTOM_TARGET_DEBUFF_SIZE)
            slot:SetHeight(CUSTOM_TARGET_DEBUFF_SIZE)
            slot:SetPoint("TOPLEFT", frame.powerBg, "BOTTOMLEFT", xOffset, yOffset)
            if slot.EnableMouse then
                slot:EnableMouse(true)
            end
            slot:SetScript("OnEnter", ShowCustomTargetDebuffTooltip)
            slot:SetScript("OnLeave", HideCustomTargetDebuffTooltip)

            slot.bg = slot:CreateTexture(nil, "BACKGROUND")
            slot.bg:SetAllPoints(slot)
            SetSolidTexture(slot.bg, 0, 0, 0, 0.82)

            slot.icon = slot:CreateTexture(nil, "ARTWORK")
            slot.icon:SetPoint("CENTER", slot, "CENTER", 0, 0)
            slot.icon:SetWidth(CUSTOM_TARGET_DEBUFF_SIZE - 2)
            slot.icon:SetHeight(CUSTOM_TARGET_DEBUFF_SIZE - 2)
            if slot.icon.SetTexCoord then
                slot.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
            end

            slot.countText = slot:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
            slot.countText:SetPoint("BOTTOMRIGHT", slot, "BOTTOMRIGHT", 2, -1)
            slot.countText:SetTextColor(1.00, 0.95, 0.20)
            if slot.countText.SetShadowColor then
                slot.countText:SetShadowColor(0, 0, 0, 1)
            end
            if slot.countText.SetShadowOffset then
                slot.countText:SetShadowOffset(1, -1)
            end
            if slot.countText.SetScale then
                slot.countText:SetScale(0.82)
            end

            slot.durationText = slot:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
            slot.durationText:SetPoint("TOP", slot, "TOP", 0, 1)
            slot.durationText:SetTextColor(1.00, 0.82, 0.10)
            if slot.durationText.SetShadowColor then
                slot.durationText:SetShadowColor(0, 0, 0, 1)
            end
            if slot.durationText.SetShadowOffset then
                slot.durationText:SetShadowOffset(1, -1)
            end
            if slot.durationText.SetScale then
                slot.durationText:SetScale(0.72)
            end

            slot:Hide()
            frame.debuffs[index] = slot
        end
    end

    frame:ClearAllPoints()
    frame:SetPoint("TOPRIGHT", targetFrame, "TOPRIGHT", 0, 4)
    if frame.SetFrameStrata and targetFrame.GetFrameStrata then
        frame:SetFrameStrata(targetFrame:GetFrameStrata())
    end
    if frame.SetFrameLevel and targetFrame.GetFrameLevel then
        frame:SetFrameLevel((targetFrame:GetFrameLevel() or 0) + 100)
    end

    State.customTargetFrame = frame
    State.customTargetFrameArt = frame.frameArt
    State.customTargetPortrait = frame.portrait
    State.customTargetPortraitRing = frame.portraitRing
    State.customTargetDebuffs = frame.debuffs
    State.customTargetNameText = frame.nameText
    State.customTargetLevelText = frame.levelText
    State.customTargetHealthFill = frame.healthFill
    State.customTargetPowerFill = frame.powerFill
    State.targetHealthBackdrop = frame.healthBg
    State.targetHealthText = frame.healthText
    State.targetPowerBackdrop = frame.powerBg
    State.targetPowerText = frame.powerText

    if UnitExists and UnitExists("target") then
        EnsurePortraitUnitButton(frame, "PlayerAttributePanelCustomTargetPortraitButton", "target", CUSTOM_TARGET_PORTRAIT_SIZE + 22)
        frame:Show()
        ApplyCustomUnitFrameStyle(frame, "target", true)
        HideOfficialTargetFrameVisuals()
        UpdateCustomTargetIdentity()
        UpdateCustomTargetDebuffs()
    else
        HidePortraitUnitButton(frame)
        HideCustomTargetDebuffs()
        frame:Hide()
    end
end

local function UpdateTargetFrameTexts()
    if not State.targetHealthText and not State.targetPowerText then
        EnsureCustomTargetFrame()
        if not State.targetHealthText and not State.targetPowerText then
            return
        end
    end

    if not UnitExists or not UnitExists("target") then
        if State.customTargetFrame then
            State.customTargetFrame:Hide()
        end
        HideCustomTargetDebuffs()
        if State.targetHealthText then
            State.targetHealthText:Hide()
        end
        if State.targetHealthBackdrop then
            State.targetHealthBackdrop:Hide()
        end
        if State.targetPowerBackdrop then
            State.targetPowerBackdrop:Hide()
        end
        if State.targetPowerText then
            State.targetPowerText:Hide()
        end
        return
    end

    EnsureCustomTargetFrame()
    HideOfficialTargetFrameVisuals()
    UpdateCustomTargetIdentity()
    UpdateCustomTargetDebuffs()

    if State.targetHealthText then
        local maxHealthText = GetTargetMaxHealthText()
        local currentHealthText = GetTargetCurrentHealthText(maxHealthText)
        State.targetHealthText:SetText(FormatChineseNumber(currentHealthText) .. "/" .. FormatChineseNumber(maxHealthText))
        UpdateCustomBarFillByText(State.customTargetHealthFill, currentHealthText, maxHealthText, CUSTOM_TARGET_BAR_WIDTH)
        if State.targetHealthBackdrop then
            State.targetHealthBackdrop:Show()
        end
        State.targetHealthText:Show()
    end

    if State.targetPowerText then
        local powerType = GetUnitPowerTypeValue("target", State.targetData.powerType)
        ApplyPowerBarStyle(State.customTargetPowerFill, State.targetPowerText, powerType)

        local maxManaText = GetTargetMaxManaText()
        local currentManaText = GetTargetCurrentManaText(maxManaText)

        if not TargetHasMana(maxManaText) then
            local currentPowerText, maxPowerText = GetUnitPowerValueTexts("target", powerType)
            if HasPositiveUnsignedText(maxPowerText) then
                currentManaText = currentPowerText
                maxManaText = maxPowerText
            end
        end

        if HasPositiveUnsignedText(maxManaText) then
            State.targetPowerText:SetText(FormatChineseNumber(currentManaText) .. "/" .. FormatChineseNumber(maxManaText))
            UpdateCustomBarFillByText(State.customTargetPowerFill, currentManaText, maxManaText, CUSTOM_TARGET_BAR_WIDTH)
            if State.targetPowerBackdrop then
                State.targetPowerBackdrop:Show()
            end
            State.targetPowerText:Show()
        else
            if State.customTargetPowerFill then
                State.customTargetPowerFill:Hide()
            end
            if State.targetPowerBackdrop then
                State.targetPowerBackdrop:Hide()
            end
            State.targetPowerText:Hide()
        end
    end
end

local function EnsureTargetFrameTexts()
    if State.targetFrameHooked then
        EnsureCustomTargetFrame()
        UpdateTargetFrameTexts()
        return
    end

    local targetFrame = _G.TargetFrame
    local healthBar = _G.TargetFrameHealthBar
    local powerBar = _G.TargetFrameManaBar
    if not targetFrame and not healthBar and not powerBar then
        return
    end

    EnsureCustomTargetFrame()

    if healthBar and healthBar.HookScript then
        healthBar:HookScript("OnShow", function()
            EnsureCustomTargetFrame()
            UpdateTargetFrameTexts()
        end)
        healthBar:HookScript("OnValueChanged", UpdateTargetFrameTexts)
    end

    if powerBar and powerBar.HookScript then
        powerBar:HookScript("OnShow", function()
            EnsureCustomTargetFrame()
            UpdateTargetFrameTexts()
        end)
        powerBar:HookScript("OnValueChanged", UpdateTargetFrameTexts)
    end

    if targetFrame and targetFrame.HookScript then
        targetFrame:HookScript("OnShow", function()
            EnsureCustomTargetFrame()
            UpdateTargetFrameTexts()
        end)
    end

    for _, frameName in ipairs(OFFICIAL_TARGET_HEALTH_TEXT_NAMES) do
        local frame = _G[frameName]
        if frame and frame.HookScript then
            frame:HookScript("OnShow", HideOfficialTargetFrameVisuals)
        end
    end

    for _, frameName in ipairs(OFFICIAL_TARGET_POWER_TEXT_NAMES) do
        local frame = _G[frameName]
        if frame and frame.HookScript then
            frame:HookScript("OnShow", HideOfficialTargetFrameVisuals)
        end
    end

    if hooksecurefunc and TextStatusBar_UpdateTextString then
        hooksecurefunc("TextStatusBar_UpdateTextString", function(bar)
            if bar == _G.TargetFrameHealthBar or bar == _G.TargetFrameManaBar then
                UpdateTargetFrameTexts()
            end
        end)
    end

    if hooksecurefunc and TargetFrame_Update then
        hooksecurefunc("TargetFrame_Update", function()
            EnsureCustomTargetFrame()
            UpdateTargetFrameTexts()
        end)
    end

    State.targetFrameHooked = true
    UpdateTargetFrameTexts()
end

local function UpdateOfficialStats()
    for _, entryDef in ipairs(ENTRY_DEFS) do
        State.stats[entryDef.key].official = GetOfficialValue(entryDef)
    end
end

local function UpdatePanelValues()
    if not State.panel then
        return
    end

    UpdateOfficialStats()

    for _, entryDef in ipairs(ENTRY_DEFS) do
        local statState = State.stats[entryDef.key]
        local row = State.rows[entryDef.key]

        if statState.real then
            row.realValue:SetText(FormatDisplayValue(entryDef, statState.real))
        else
            row.realValue:SetText("|cff808080同步中...|r")
        end
    end

    UpdatePlayerPortraitHealthText()
    UpdatePlayerPortraitPowerText()
end

local function LayoutPanel()
    if not State.panel or not CharacterFrame then
        return
    end

    State.panel:ClearAllPoints()

    if REPLACE_OFFICIAL_STATS then
        local screenWidth = GetScreenWidth and GetScreenWidth() or 0
        local right = CharacterFrame.GetRight and CharacterFrame:GetRight() or nil

        if right and screenWidth > 0 and (right + ATTRIBUTE_PANEL_REPLACE_X_OFFSET + State.panel:GetWidth() + 6) < screenWidth then
            State.panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", ATTRIBUTE_PANEL_REPLACE_X_OFFSET, ATTRIBUTE_PANEL_REPLACE_Y_OFFSET)
        else
            State.panel:SetPoint("TOPRIGHT", CharacterFrame, "TOPLEFT", -ATTRIBUTE_PANEL_REPLACE_X_OFFSET, ATTRIBUTE_PANEL_REPLACE_Y_OFFSET)
        end

        return
    end

    local screenWidth = GetScreenWidth and GetScreenWidth() or 0
    local right = CharacterFrame.GetRight and CharacterFrame:GetRight() or nil

    if right and screenWidth > 0 and (right + State.panel:GetWidth() + 24) < screenWidth then
        State.panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", 12, -24)
    else
        State.panel:SetPoint("TOPRIGHT", CharacterFrame, "TOPLEFT", -12, -24)
    end
end

local function SendStatsRequest(force)
    local playerName = UnitName("player")
    if not playerName then
        return
    end

    local now = GetTime and GetTime() or 0
    if not force and (now - State.lastRequestAt) < 0.25 then
        return
    end

    State.lastRequestAt = now
    SetStatusText("|cffffff00请求中...|r")

    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, "REQ_STATS", "WHISPER", playerName)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, "REQ_STATS", "WHISPER", playerName)
    end
end

local function SendTargetRequest(force)
    if not UnitExists or not UnitExists("target") then
        return
    end

    local playerName = UnitName("player")
    if not playerName then
        return
    end

    local now = GetTime and GetTime() or 0
    if not force and (now - State.lastTargetRequestAt) < 0.25 then
        return
    end

    State.lastTargetRequestAt = now
    State.targetRequestToken = (State.targetRequestToken or 0) + 1
    if State.targetRequestToken > 999999 then
        State.targetRequestToken = 1
    end

    State.targetData.token = tostring(State.targetRequestToken)
    local message = "REQ_TARGET:" .. State.targetData.token

    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, message, "WHISPER", playerName)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, message, "WHISPER", playerName)
    end
end

local function CreateColumnHeader(parent, text, xOffset, width)
    local header = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    header:SetPoint("TOPLEFT", parent, "TOPLEFT", xOffset, -52)
    header:SetWidth(width)
    header:SetJustifyH("CENTER")
    header:SetText(text)
    header:SetTextColor(0.95, 0.78, 0.32)
    if header.SetShadowColor then
        header:SetShadowColor(0, 0, 0, 1)
    end
    if header.SetShadowOffset then
        header:SetShadowOffset(1, -1)
    end
    return header
end

local function CreateStatRow(parent, entryDef, offsetY)
    local red, green, blue = GetEntryColor(entryDef)

    local label = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    label:SetPoint("TOPLEFT", parent, "TOPLEFT", 4, offsetY)
    label:SetWidth(112)
    label:SetJustifyH("LEFT")
    label:SetText(entryDef.label)
    label:SetTextColor(red, green, blue)
    if label.SetShadowColor then
        label:SetShadowColor(0, 0, 0, 1)
    end
    if label.SetShadowOffset then
        label:SetShadowOffset(1, -1)
    end

    local realValue = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    realValue:SetPoint("TOPLEFT", parent, "TOPLEFT", 116, offsetY)
    realValue:SetWidth(126)
    realValue:SetJustifyH("RIGHT")
    realValue:SetText("-")
    realValue:SetTextColor(math.min(red + 0.18, 1), math.min(green + 0.18, 1), math.min(blue + 0.18, 1))
    if realValue.SetShadowColor then
        realValue:SetShadowColor(0, 0, 0, 1)
    end
    if realValue.SetShadowOffset then
        realValue:SetShadowOffset(1, -1)
    end
    if realValue.SetWordWrap then
        realValue:SetWordWrap(false)
    end
    if realValue.SetNonSpaceWrap then
        realValue:SetNonSpaceWrap(false)
    end

    return {
        label = label,
        realValue = realValue
    }
end

local function EnsurePanel()
    if State.panel or not CharacterFrame then
        return
    end

    local panel = CreateFrame("Frame", "PlayerAttributePanelFrame", CharacterFrame)
    panel:SetWidth(ATTRIBUTE_PANEL_WIDTH)
    panel:SetHeight(ATTRIBUTE_PANEL_HEIGHT)
    panel:SetFrameStrata("HIGH")

    local fallbackBg = panel:CreateTexture(nil, "BACKGROUND")
    fallbackBg:SetAllPoints(panel)
    SetSolidTexture(fallbackBg, 0.02, 0.02, 0.02, 1)
    if fallbackBg.SetAlpha then
        fallbackBg:SetAlpha(0.10)
    end

    local background = panel:CreateTexture(nil, "BACKGROUND")
    background:SetAllPoints(panel)
    background:SetTexture(ATTRIBUTE_PANEL_BACKGROUND_TEXTURE)
    if background.SetTexCoord then
        background:SetTexCoord(0, 1, 0, 1)
    end

    local headerLine = panel:CreateTexture(nil, "ARTWORK")
    headerLine:SetPoint("TOPLEFT", panel, "TOPLEFT", 30, -68)
    headerLine:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -62, -68)
    headerLine:SetHeight(1)
    SetSolidTexture(headerLine, 0.90, 0.70, 0.26, 0.42)

    panel:Hide()

    local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", panel, "TOP", 0, -8)
    title:SetText("属性面板")
    title:SetTextColor(1.00, 0.82, 0.28)
    if title.SetShadowColor then
        title:SetShadowColor(0, 0, 0, 1)
    end
    if title.SetShadowOffset then
        title:SetShadowOffset(1, -1)
    end

    CreateColumnHeader(panel, "属性", 32, 104)
    CreateColumnHeader(panel, "数值", 146, 126)

    local scrollFrame = CreateFrame("ScrollFrame", "PlayerAttributePanelScrollFrame", panel, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", panel, "TOPLEFT", 28, -74)
    scrollFrame:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -58, 34)

    local scrollBar = _G[scrollFrame:GetName() .. "ScrollBar"]
    if scrollBar then
        scrollBar:ClearAllPoints()
        scrollBar:SetPoint("TOPLEFT", scrollFrame, "TOPRIGHT", 8, -16)
        scrollBar:SetPoint("BOTTOMLEFT", scrollFrame, "BOTTOMRIGHT", 8, 16)
    end

    local content = CreateFrame("Frame", nil, scrollFrame)
    content:SetWidth(ATTRIBUTE_PANEL_CONTENT_WIDTH)
    scrollFrame:SetScrollChild(content)
    scrollFrame:EnableMouseWheel(true)
    scrollFrame:SetScript("OnMouseWheel", function(self, delta)
        local current = self:GetVerticalScroll()
        local maxScroll = self:GetVerticalScrollRange()
        local nextScroll = current - (delta * 28)
        if nextScroll < 0 then
            nextScroll = 0
        elseif nextScroll > maxScroll then
            nextScroll = maxScroll
        end
        self:SetVerticalScroll(nextScroll)
    end)

    local startY = -4
    local rowGap = 18
    for index, entryDef in ipairs(ENTRY_DEFS) do
        State.rows[entryDef.key] = CreateStatRow(content, entryDef, startY - ((index - 1) * rowGap))
    end
    content:SetHeight((#ENTRY_DEFS * rowGap) + 12)

    State.panel = panel
    State.title = title
    State.scrollFrame = scrollFrame
    State.content = content

    if not State.hooked then
        CharacterFrame:HookScript("OnShow", function()
            EnsurePanel()
            LayoutPanel()
            ApplyOfficialStatsVisibility()
            UpdatePanelValues()
            SendStatsRequest(true)
        end)

        CharacterFrame:HookScript("OnHide", function()
            if State.panel then
                State.panel:Hide()
            end

            if REPLACE_OFFICIAL_STATS then
                ForEachOfficialStatsFrame(SafeShow)
            end
        end)

        if PaperDollFrame and PaperDollFrame.HookScript then
            PaperDollFrame:HookScript("OnShow", function()
                LayoutPanel()
                ApplyOfficialStatsVisibility()
                UpdatePanelValues()
                SendStatsRequest(true)
            end)

            PaperDollFrame:HookScript("OnHide", function()
                ApplyOfficialStatsVisibility()
            end)
        end

        State.hooked = true
    end

    HookOfficialStatsFrames()
    LayoutPanel()
    ApplyOfficialStatsVisibility()
    UpdatePanelValues()
end

local function HandleStatsPayload(message)
    local payload = message:match("^STATS:(.+)$")
    if not payload then
        return
    end

    local syncedCount = 0
    local payloadValues = {}

    for segment in string.gmatch(payload, "[^|]+") do
        local payloadKey, payloadValue = segment:match("^([^=]+)=(.*)$")
        if payloadKey and payloadValue then
            payloadValues[payloadKey] = payloadValue
        end
    end

    local currentHealthValue = payloadValues.CUR_HEALTH or payloadValues.CURRENT_HEALTH
    local hasCurrentHealth = currentHealthValue ~= nil
    if hasCurrentHealth then
        State.realCurrentHealth = NormalizeUnsignedIntegerText(currentHealthValue) or tostring(currentHealthValue)
    end

    local currentManaValue = payloadValues.CUR_MANA or payloadValues.CURRENT_MANA
    local hasCurrentMana = currentManaValue ~= nil
    if hasCurrentMana then
        State.realCurrentMana = NormalizeUnsignedIntegerText(currentManaValue) or tostring(currentManaValue)
    end

    for _, entryDef in ipairs(ENTRY_DEFS) do
        local statValue = nil

        for _, serverKey in ipairs(entryDef.serverKeys or {}) do
            statValue = payloadValues[serverKey]
            if statValue then
                break
            end
        end

        if statValue then
            State.stats[entryDef.key].real = statValue
            syncedCount = syncedCount + 1
        end
    end

    if syncedCount > 0 or hasCurrentHealth or hasCurrentMana then
        SetStatusText("|cff00ff00已同步|r")
        UpdatePanelValues()
        UpdatePlayerPortraitHealthText()
        UpdatePlayerPortraitPowerText()
    else
        SetStatusText("|cffff8000收到数据但字段未匹配|r")
    end
end

local function HandleTargetPayload(message)
    local payload = message:match("^TARGET:(.+)$")
    if not payload then
        return false
    end

    local payloadValues = {}
    for segment in string.gmatch(payload, "[^|]+") do
        local payloadKey, payloadValue = segment:match("^([^=]+)=(.*)$")
        if payloadKey and payloadValue then
            payloadValues[payloadKey] = payloadValue
        end
    end

    local token = payloadValues.TOKEN
    if token and State.targetData.token and token ~= State.targetData.token then
        return true
    end

    if payloadValues.NONE then
        ResetTargetData()
        EnsureTargetFrameTexts()
        UpdateTargetFrameTexts()
        return true
    end

    if not UnitExists or not UnitExists("target") then
        ResetTargetData()
        UpdateTargetFrameTexts()
        return true
    end

    State.targetData.hasData = true
    State.targetData.currentHealth = NormalizeUnsignedIntegerText(payloadValues.CUR_HEALTH or payloadValues.CURRENT_HEALTH) or payloadValues.CUR_HEALTH or payloadValues.CURRENT_HEALTH
    State.targetData.maxHealth = NormalizeUnsignedIntegerText(payloadValues.MAX_HEALTH or payloadValues.HEALTH) or payloadValues.MAX_HEALTH or payloadValues.HEALTH
    State.targetData.currentMana = NormalizeUnsignedIntegerText(payloadValues.CUR_MANA or payloadValues.CURRENT_MANA) or payloadValues.CUR_MANA or payloadValues.CURRENT_MANA
    State.targetData.maxMana = NormalizeUnsignedIntegerText(payloadValues.MAX_MANA or payloadValues.MANA) or payloadValues.MAX_MANA or payloadValues.MANA
    State.targetData.powerType = payloadValues.POWER_TYPE

    EnsureTargetFrameTexts()
    UpdateTargetFrameTexts()
    return true
end

local function HandleDamagePayload(message)
    local amountText, criticalText = message:match("^DMG:(%d+):(%d+):%d+:%d+:%d+$")
    if not amountText then
        return false
    end

    if _G.LargeDamageTextActive then
        return true
    end

    ShowDamageText(amountText, criticalText == "1")
    return true
end

local function OnAddonMessage(prefix, message, channel, sender)
    prefix = tostring(prefix or "")
    message = tostring(message or "")

    if prefix:match("^" .. ADDON_PREFIX .. "\t") then
        message = prefix:match("^" .. ADDON_PREFIX .. "\t(.+)$") or message
        prefix = ADDON_PREFIX
    elseif message:match("^" .. ADDON_PREFIX .. "\t") then
        message = message:match("^" .. ADDON_PREFIX .. "\t(.+)$") or message
        prefix = ADDON_PREFIX
    end

    if prefix ~= ADDON_PREFIX then
        return
    end

    if message == "REQ_STATS" or message == "REQ_TARGET" or message:match("^REQ_TARGET:") then
        return
    end

    if message:match("^STATS:") then
        HandleStatsPayload(message)
    elseif message:match("^TARGET:") then
        HandleTargetPayload(message)
    elseif message:match("^DMG:") then
        HandleDamagePayload(message)
    else
        SetStatusText("|cffff8000收到未知消息|r")
    end
end

local function HandlePlayerUpdateRequest(force)
    EnsurePanel()
    EnsurePlayerPortraitHealth()
    UpdatePanelValues()

    if State.panel and State.panel:IsShown() then
        SendStatsRequest(force)
    end
end

local function OnEventFrameUpdate(self, elapsed)
    elapsed = elapsed or 0
    State.targetRefreshElapsed = (State.targetRefreshElapsed or 0) + elapsed
    if State.targetRefreshElapsed < TARGET_REFRESH_INTERVAL then
        return
    end

    State.targetRefreshElapsed = 0
    if UnitExists and UnitExists("target") then
        EnsureTargetFrameTexts()
        UpdateCustomTargetDebuffs()

        -- 仅当目标 / HP / MP / 功率类型 真有变化时才轮询服务端；
        -- 否则只跑兜底心跳（覆盖 HuanJingSystem 这种本地 UnitHealth 不可见的虚拟血蓝）。
        local guid  = UnitGUID and UnitGUID("target") or nil
        local curHP = UnitHealth and UnitHealth("target") or 0
        local maxHP = UnitHealthMax and UnitHealthMax("target") or 0
        local curMP = UnitMana and UnitMana("target") or (UnitPower and UnitPower("target") or 0)
        local maxMP = (UnitManaMax and UnitManaMax("target")) or (UnitPowerMax and UnitPowerMax("target") or 0)
        local pType = UnitPowerType and UnitPowerType("target") or 0

        local snap = State.targetSyncSnapshot
        local changed = (not snap)
            or snap.guid ~= guid
            or snap.curHP ~= curHP or snap.maxHP ~= maxHP
            or snap.curMP ~= curMP or snap.maxMP ~= maxMP
            or snap.pType ~= pType

        State.targetHeartbeatElapsed = (State.targetHeartbeatElapsed or 0) + TARGET_REFRESH_INTERVAL
        if changed or State.targetHeartbeatElapsed >= TARGET_HEARTBEAT_INTERVAL then
            State.targetSyncSnapshot = {
                guid = guid, curHP = curHP, maxHP = maxHP,
                curMP = curMP, maxMP = maxMP, pType = pType,
            }
            State.targetHeartbeatElapsed = 0
            SendTargetRequest(false)
        end
    else
        State.targetSyncSnapshot = nil
        State.targetHeartbeatElapsed = 0
        HideCustomTargetDebuffs()
    end
end

local function SafeRegisterEvent(frame, eventName)
    if frame and frame.RegisterEvent then
        pcall(frame.RegisterEvent, frame, eventName)
    end
end

local EventFrame = CreateFrame("Frame")
EventFrame:RegisterEvent("ADDON_LOADED")
EventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
EventFrame:RegisterEvent("CHAT_MSG_ADDON")
EventFrame:RegisterEvent("PLAYER_TARGET_CHANGED")
EventFrame:RegisterEvent("CHARACTER_POINTS_CHANGED")
EventFrame:RegisterEvent("PLAYER_EQUIPMENT_CHANGED")
EventFrame:RegisterEvent("ACTIVE_TALENT_GROUP_CHANGED")
EventFrame:RegisterEvent("UNIT_AURA")
EventFrame:RegisterEvent("UNIT_HEALTH")
EventFrame:RegisterEvent("UNIT_MAXHEALTH")
EventFrame:RegisterEvent("UNIT_MANA")
EventFrame:RegisterEvent("UNIT_MAXMANA")
SafeRegisterEvent(EventFrame, "UNIT_RAGE")
SafeRegisterEvent(EventFrame, "UNIT_ENERGY")
SafeRegisterEvent(EventFrame, "UNIT_FOCUS")
SafeRegisterEvent(EventFrame, "UNIT_RUNIC_POWER")
SafeRegisterEvent(EventFrame, "UNIT_MAXRUNIC_POWER")
EventFrame:RegisterEvent("UNIT_DISPLAYPOWER")
SafeRegisterEvent(EventFrame, "RUNE_POWER_UPDATE")
SafeRegisterEvent(EventFrame, "RUNE_TYPE_UPDATE")
EventFrame:RegisterEvent("UNIT_STATS")
EventFrame:RegisterEvent("UNIT_ATTACK_POWER")
EventFrame:RegisterEvent("UNIT_RANGED_ATTACK_POWER")
EventFrame:RegisterEvent("PLAYER_DAMAGE_DONE_MODS")
EventFrame:RegisterEvent("COMBAT_RATING_UPDATE")
EventFrame:RegisterEvent("PLAYER_ALIVE")
EventFrame:RegisterEvent("PLAYER_DEAD")
EventFrame:RegisterEvent("PLAYER_UNGHOST")
EventFrame:RegisterEvent("UNIT_PORTRAIT_UPDATE")
EventFrame:RegisterEvent("PLAYER_LEVEL_UP")

EventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "ADDON_LOADED" then
        local addon = ...
        if addon == ADDON_NAME or addon == "Blizzard_CharacterUI" then
            ConfigureCombatTextCVar()
            EnsurePanel()
            EnsurePlayerPortraitHealth()
            EnsureTargetFrameTexts()
            HookOfficialStatsFrames()
            ApplyOfficialStatsVisibility()
            UpdatePanelValues()
            UpdateTargetFrameTexts()
        end
        return
    end

    if event == "PLAYER_ENTERING_WORLD" then
        ConfigureCombatTextCVar()
        EnsurePanel()
        EnsurePlayerPortraitHealth()
        EnsureTargetFrameTexts()
        ApplyOfficialStatsVisibility()
        UpdatePanelValues()
        UpdateTargetFrameTexts()
        SendStatsRequest(true)
        SendTargetRequest(true)
        return
    end

    if event == "CHAT_MSG_ADDON" then
        OnAddonMessage(...)
        return
    end

    if event == "PLAYER_TARGET_CHANGED" then
        ResetTargetData()
        EnsureTargetFrameTexts()
        UpdateTargetFrameTexts()
        SendTargetRequest(true)
        return
    end

    if event == "UNIT_HEALTH" or event == "UNIT_MAXHEALTH" then
        local unit = ...
        if unit == "player" then
            EnsurePlayerPortraitHealth()
            UpdatePlayerPortraitHealthText()

            if event == "UNIT_MAXHEALTH" then
                SendStatsRequest(false)
            end
        end
        if unit == "target" then
            EnsureTargetFrameTexts()
            UpdateTargetFrameTexts()
            SendTargetRequest(event == "UNIT_MAXHEALTH")
        end
        return
    end

    if event == "RUNE_POWER_UPDATE" or event == "RUNE_TYPE_UPDATE" then
        EnsurePlayerPortraitHealth()
        UpdatePlayerPortraitPowerText()
        return
    end

    if event == "UNIT_MANA" or event == "UNIT_MAXMANA" or event == "UNIT_RAGE" or event == "UNIT_ENERGY" or event == "UNIT_FOCUS" or event == "UNIT_RUNIC_POWER" or event == "UNIT_MAXRUNIC_POWER" or event == "UNIT_DISPLAYPOWER" then
        local unit = ...
        if unit == "player" then
            EnsurePlayerPortraitHealth()
            UpdatePlayerPortraitPowerText()

            if event == "UNIT_MAXMANA" then
                SendStatsRequest(false)
            end
        end
        if unit == "target" then
            EnsureTargetFrameTexts()
            UpdateTargetFrameTexts()
            SendTargetRequest(event == "UNIT_MAXMANA" or event == "UNIT_DISPLAYPOWER")
        end
        return
    end

    if event == "UNIT_AURA" or event == "UNIT_STATS" or event == "UNIT_ATTACK_POWER" or event == "UNIT_RANGED_ATTACK_POWER" then
        local unit = ...
        if unit == "player" then
            HandlePlayerUpdateRequest(false)
        elseif event == "UNIT_AURA" and unit == "target" then
            EnsureTargetFrameTexts()
            UpdateCustomTargetDebuffs()
        end
        return
    end

    if event == "PLAYER_ALIVE" or event == "PLAYER_DEAD" or event == "PLAYER_UNGHOST" then
        EnsurePlayerPortraitHealth()
        UpdatePlayerPortraitHealthText()
        UpdatePlayerPortraitPowerText()
        return
    end

    if event == "UNIT_PORTRAIT_UPDATE" then
        local unit = ...
        if unit == "player" then
            EnsurePlayerPortraitHealth()
            UpdateCustomPlayerIdentity()
        elseif unit == "target" then
            EnsureTargetFrameTexts()
            UpdateCustomTargetIdentity()
        end
        return
    end

    if event == "PLAYER_LEVEL_UP" then
        EnsurePlayerPortraitHealth()
        UpdateCustomPlayerIdentity()
        return
    end

    HandlePlayerUpdateRequest(false)
end)
EventFrame:SetScript("OnUpdate", OnEventFrameUpdate)

if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
end

SLASH_PLAYERATTRIBUTEPANEL1 = "/pattr"
SlashCmdList.PLAYERATTRIBUTEPANEL = function(message)
    message = string.lower(message or "")

    if message == "refresh" or message == "刷新" then
        SendStatsRequest(true)
        SendTargetRequest(true)
        return
    end

    if CharacterFrame and not CharacterFrame:IsShown() and ToggleCharacter then
        ToggleCharacter("PaperDollFrame")
    end

    EnsurePanel()
    LayoutPanel()
    ApplyOfficialStatsVisibility()

    if State.panel then
        State.panel:Show()
    end

    SendStatsRequest(true)
    SendTargetRequest(true)
end
