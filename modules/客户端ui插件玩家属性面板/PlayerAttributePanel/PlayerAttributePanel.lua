local ADDON_NAME = "PlayerAttributePanel"
local ADDON_PREFIX = "PATTRPANEL"
local REPLACE_OFFICIAL_STATS = true

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
    "PlayerFrameHealthBarTextRight"
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
    AttrEntry(12, "defenseRating", "防御等级", "defense", { "DEFENSE_RATING" }, function() return GetCombatRatingValue(CR_DEFENSE_SKILL or 2) end),
    AttrEntry(13, "dodgeRating", "躲闪等级", "defense", { "DODGE_RATING" }, function() return GetCombatRatingValue(CR_DODGE or 3) end),
    AttrEntry(14, "parryRating", "招架等级", "defense", { "PARRY_RATING" }, function() return GetCombatRatingValue(CR_PARRY or 4) end),
    AttrEntry(15, "blockRating", "格挡等级", "defense", { "BLOCK_RATING" }, function() return GetCombatRatingValue(CR_BLOCK or 5) end),
    AttrEntry(16, "meleeHitRating", "近战命中等级", "melee", { "MELEE_HIT_RATING" }, function() return GetCombatRatingValue(CR_HIT_MELEE or 6) end),
    AttrEntry(17, "rangedHitRating", "远程命中等级", "ranged", { "RANGED_HIT_RATING" }, function() return GetCombatRatingValue(CR_HIT_RANGED or 7) end),
    AttrEntry(18, "spellHitRating", "法术命中等级", "spell", { "SPELL_HIT_RATING" }, function() return GetCombatRatingValue(CR_HIT_SPELL or 8) end),
    AttrEntry(19, "meleeCritRating", "近战暴击等级", "melee", { "MELEE_CRIT_RATING" }, function() return GetCombatRatingValue(CR_CRIT_MELEE or 9) end),
    AttrEntry(20, "rangedCritRating", "远程暴击等级", "ranged", { "RANGED_CRIT_RATING" }, function() return GetCombatRatingValue(CR_CRIT_RANGED or 10) end),
    AttrEntry(21, "spellCritRating", "法术暴击等级", "spell", { "SPELL_CRIT_RATING" }, function() return GetCombatRatingValue(CR_CRIT_SPELL or 11) end),
    AttrEntry(28, "meleeHasteRating", "近战急速等级", "melee", { "MELEE_HASTE_RATING" }, function() return GetCombatRatingValue(CR_HASTE_MELEE or 18) end),
    AttrEntry(29, "rangedHasteRating", "远程急速等级", "ranged", { "RANGED_HASTE_RATING" }, function() return GetCombatRatingValue(CR_HASTE_RANGED or 19) end),
    AttrEntry(30, "spellHasteRating", "法术急速等级", "spell", { "SPELL_HASTE_RATING" }, function() return GetCombatRatingValue(CR_HASTE_SPELL or 20) end),
    AttrEntry(31, "hitRating", "命中等级", "misc", { "HIT_RATING" }, function() return GetCombatRatingValue(CR_HIT_MELEE or 6) end),
    AttrEntry(32, "critRating", "暴击等级", "misc", { "CRIT_RATING" }, function() return GetCombatRatingValue(CR_CRIT_MELEE or 9) end),
    AttrEntry(35, "resilienceRating", "韧性等级", "defense", { "RESILIENCE", "RESILIENCE_RATING" }, function() return GetCombatRatingValue(CR_CRIT_TAKEN_MELEE or 15) end),
    AttrEntry(36, "hasteRating", "急速等级", "misc", { "HASTE_RATING" }, function() return GetCombatRatingValue(CR_HASTE_MELEE or 18) end),
    AttrEntry(37, "expertiseRating", "精准等级", "melee", { "EXPERTISE", "EXPERTISE_RATING" }, function() return GetCombatRatingValue(CR_EXPERTISE or 24) end),
    AttrEntry(38, "attackPower", "攻击强度", "melee", { "ATTACK_POWER" }, function()
        if not UnitAttackPower then
            return 0
        end

        local base, posBuff, negBuff = UnitAttackPower("player")
        return math.floor((base or 0) + (posBuff or 0) + (negBuff or 0))
    end),
    AttrEntry(39, "rangedAttackPower", "远程攻击强度", "ranged", { "RANGED_ATTACK_POWER" }, function()
        if not UnitRangedAttackPower then
            return 0
        end

        local base, posBuff, negBuff = UnitRangedAttackPower("player")
        return math.floor((base or 0) + (posBuff or 0) + (negBuff or 0))
    end),
    AttrEntry(41, "spellHealingDone", "法术治疗", "spell", { "SPELL_HEALING", "SPELL_HEALING_DONE" }, function() return math.floor(tonumber(GetSpellBonusHealing and GetSpellBonusHealing()) or 0) end),
    AttrEntry(42, "spellDamageDone", "法术伤害", "spell", { "SPELL_DAMAGE", "SPELL_DAMAGE_DONE" }, GetSpellPowerValue),
    AttrEntry(43, "manaRegen", "法力回复", "spell", { "MANA_REGEN", "MANA_REGENERATION" }, function()
        local base = GetManaRegen and GetManaRegen() or 0
        return math.floor((tonumber(base) or 0) * 5)
    end),
    AttrEntry(44, "armorPenetrationRating", "护甲穿透等级", "melee", { "ARMOR_PENETRATION", "ARMOR_PENETRATION_RATING" }, function() return GetCombatRatingValue(CR_ARMOR_PENETRATION or 25) end),
    AttrEntry(45, "spellPower", "法术强度", "spell", { "SPELL_POWER" }, GetSpellPowerValue),
    AttrEntry(46, "healthRegen", "生命值回复", "resource", { "HEALTH_REGEN" }, function()
        if GetUnitHealthRegen then
            local base = GetUnitHealthRegen("player")
            return math.floor((tonumber(base) or 0) * 5)
        end

        return 0
    end),
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
    statusText = "未同步",
    lastRequestAt = 0,
    damageLayer = nil,
    damageTexts = {},
    damageTextElapsed = 0,
    combatTextConfigured = false,
    panel = nil,
    title = nil,
    scrollFrame = nil,
    content = nil,
    playerHealthText = nil,
    playerHealthHooked = false,
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
    item.text:SetScale(critical and 1.3 or 1.0)
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

local function GetRealMaxHealthText()
    local statState = State.stats.maxHealth
    local realMaxHealth = statState and NormalizeUnsignedIntegerText(statState.real)
    if realMaxHealth then
        return realMaxHealth
    end

    return NormalizeUnsignedIntegerText(UnitHealthMax and UnitHealthMax("player") or 0) or "0"
end

local function GetRealCurrentHealthText(maxHealthText)
    local realCurrentHealth = NormalizeUnsignedIntegerText(State.realCurrentHealth)
    if realCurrentHealth then
        return realCurrentHealth
    end

    local currentHealth = UnitHealth and UnitHealth("player") or 0
    local officialMaxHealth = UnitHealthMax and UnitHealthMax("player") or 0
    return CalculateScaledHealthText(maxHealthText, currentHealth, officialMaxHealth)
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

local function HideOfficialPlayerHealthText()
    for _, frameName in ipairs(OFFICIAL_PLAYER_HEALTH_TEXT_NAMES) do
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

    local healthBar = _G.PlayerFrameHealthBar
    if healthBar then
        local textRegions = { healthBar:GetRegions() }
        for _, region in ipairs(textRegions) do
            if region and region ~= State.playerHealthText and region.SetText and region.SetAlpha then
                region:SetText("")
                region:SetAlpha(0)
                if region.Hide then
                    region:Hide()
                end
            end
        end

        local healthBarTexts = {
            healthBar.TextString,
            healthBar.LeftText,
            healthBar.RightText,
            healthBar.textString,
            healthBar.leftText,
            healthBar.rightText
        }

        for _, frame in ipairs(healthBarTexts) do
            if frame and frame ~= State.playerHealthText then
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
end

local function UpdatePlayerPortraitHealthText()
    if not State.playerHealthText then
        return
    end

    HideOfficialPlayerHealthText()

    local maxHealthText = GetRealMaxHealthText()
    local currentHealthText = GetRealCurrentHealthText(maxHealthText)

    State.playerHealthText:SetText(FormatChineseNumber(currentHealthText) .. "/" .. FormatChineseNumber(maxHealthText))
    State.playerHealthText:Show()
end

local function EnsurePlayerPortraitHealth()
    if State.playerHealthHooked then
        UpdatePlayerPortraitHealthText()
        return
    end

    local healthBar = _G.PlayerFrameHealthBar
    local parent = healthBar or _G.PlayerFrame
    if not parent then
        return
    end

    local healthText = _G.PlayerAttributePanelPlayerHealthText
    if not healthText then
        healthText = parent:CreateFontString("PlayerAttributePanelPlayerHealthText", "OVERLAY", "GameFontHighlightSmall")
    end

    healthText:ClearAllPoints()
    if healthBar then
        healthText:SetPoint("CENTER", healthBar, "CENTER", 0, 0)
    else
        healthText:SetPoint("TOPLEFT", parent, "TOPLEFT", 112, -36)
    end
    healthText:SetWidth(260)
    healthText:SetJustifyH("CENTER")
    healthText:SetTextColor(0.30, 1.00, 0.30)
    if healthText.SetWordWrap then
        healthText:SetWordWrap(false)
    end
    if healthText.SetNonSpaceWrap then
        healthText:SetNonSpaceWrap(false)
    end
    if healthText.SetShadowColor then
        healthText:SetShadowColor(0, 0, 0, 1)
    end
    if healthText.SetShadowOffset then
        healthText:SetShadowOffset(1, -1)
    end

    State.playerHealthText = healthText

    if healthBar and healthBar.HookScript then
        healthBar:HookScript("OnShow", UpdatePlayerPortraitHealthText)
        healthBar:HookScript("OnValueChanged", UpdatePlayerPortraitHealthText)
    end

    for _, frameName in ipairs(OFFICIAL_PLAYER_HEALTH_TEXT_NAMES) do
        local frame = _G[frameName]
        if frame and frame.HookScript then
            frame:HookScript("OnShow", HideOfficialPlayerHealthText)
        end
    end

    if hooksecurefunc and TextStatusBar_UpdateTextString then
        hooksecurefunc("TextStatusBar_UpdateTextString", function(bar)
            if bar == _G.PlayerFrameHealthBar then
                UpdatePlayerPortraitHealthText()
            end
        end)
    end

    State.playerHealthHooked = true
    UpdatePlayerPortraitHealthText()
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
end

local function LayoutPanel()
    if not State.panel or not CharacterFrame then
        return
    end

    State.panel:ClearAllPoints()

    if REPLACE_OFFICIAL_STATS then
        local screenWidth = GetScreenWidth and GetScreenWidth() or 0
        local right = CharacterFrame.GetRight and CharacterFrame:GetRight() or nil

        if right and screenWidth > 0 and (right + State.panel:GetWidth() - 22) < screenWidth then
            State.panel:SetPoint("TOPLEFT", CharacterFrame, "TOPRIGHT", -30, -44)
        else
            State.panel:SetPoint("TOPRIGHT", CharacterFrame, "TOPLEFT", -8, -44)
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

local function CreateColumnHeader(parent, text, xOffset, width)
    local header = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    header:SetPoint("TOPLEFT", parent, "TOPLEFT", xOffset, -48)
    header:SetWidth(width)
    header:SetJustifyH("CENTER")
    header:SetText(text)
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

    local realValue = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    realValue:SetPoint("TOPLEFT", parent, "TOPLEFT", 118, offsetY)
    realValue:SetWidth(166)
    realValue:SetJustifyH("RIGHT")
    realValue:SetText("-")
    realValue:SetTextColor(math.min(red + 0.18, 1), math.min(green + 0.18, 1), math.min(blue + 0.18, 1))
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
    panel:SetWidth(372)
    panel:SetHeight(400)
    panel:SetFrameStrata("HIGH")
    panel:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 16,
        insets = { left = 5, right = 5, top = 5, bottom = 5 }
    })
    panel:SetBackdropColor(0, 0, 0, 0.9)
    panel:Hide()

    local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", panel, "TOP", 0, -16)
    title:SetText("玩家属性面板")

    CreateColumnHeader(panel, "属性", 18, 112)
    CreateColumnHeader(panel, "数值", 126, 166)

    local scrollFrame = CreateFrame("ScrollFrame", "PlayerAttributePanelScrollFrame", panel, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", panel, "TOPLEFT", 16, -56)
    scrollFrame:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -54, 20)

    local scrollBar = _G[scrollFrame:GetName() .. "ScrollBar"]
    if scrollBar then
        scrollBar:ClearAllPoints()
        scrollBar:SetPoint("TOPLEFT", scrollFrame, "TOPRIGHT", 8, -16)
        scrollBar:SetPoint("BOTTOMLEFT", scrollFrame, "BOTTOMRIGHT", 8, 16)
    end

    local content = CreateFrame("Frame", nil, scrollFrame)
    content:SetWidth(286)
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

    if syncedCount > 0 or hasCurrentHealth then
        SetStatusText("|cff00ff00已同步|r")
        UpdatePanelValues()
        UpdatePlayerPortraitHealthText()
    else
        SetStatusText("|cffff8000收到数据但字段未匹配|r")
    end
end

local function HandleDamagePayload(message)
    local amountText, criticalText = message:match("^DMG:(%d+):(%d+):%d+:%d+:%d+$")
    if not amountText then
        return false
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

    if message == "REQ_STATS" then
        return
    end

    if message:match("^STATS:") then
        HandleStatsPayload(message)
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

local EventFrame = CreateFrame("Frame")
EventFrame:RegisterEvent("ADDON_LOADED")
EventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
EventFrame:RegisterEvent("CHAT_MSG_ADDON")
EventFrame:RegisterEvent("CHARACTER_POINTS_CHANGED")
EventFrame:RegisterEvent("PLAYER_EQUIPMENT_CHANGED")
EventFrame:RegisterEvent("ACTIVE_TALENT_GROUP_CHANGED")
EventFrame:RegisterEvent("UNIT_AURA")
EventFrame:RegisterEvent("UNIT_HEALTH")
EventFrame:RegisterEvent("UNIT_MAXHEALTH")
EventFrame:RegisterEvent("UNIT_STATS")
EventFrame:RegisterEvent("UNIT_ATTACK_POWER")
EventFrame:RegisterEvent("UNIT_RANGED_ATTACK_POWER")
EventFrame:RegisterEvent("PLAYER_DAMAGE_DONE_MODS")
EventFrame:RegisterEvent("COMBAT_RATING_UPDATE")
EventFrame:RegisterEvent("PLAYER_ALIVE")
EventFrame:RegisterEvent("PLAYER_DEAD")
EventFrame:RegisterEvent("PLAYER_UNGHOST")

EventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "ADDON_LOADED" then
        local addon = ...
        if addon == ADDON_NAME or addon == "Blizzard_CharacterUI" then
            ConfigureCombatTextCVar()
            EnsurePanel()
            EnsurePlayerPortraitHealth()
            HookOfficialStatsFrames()
            ApplyOfficialStatsVisibility()
            UpdatePanelValues()
        end
        return
    end

    if event == "PLAYER_ENTERING_WORLD" then
        ConfigureCombatTextCVar()
        EnsurePanel()
        EnsurePlayerPortraitHealth()
        ApplyOfficialStatsVisibility()
        UpdatePanelValues()
        SendStatsRequest(true)
        return
    end

    if event == "CHAT_MSG_ADDON" then
        OnAddonMessage(...)
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
        return
    end

    if event == "UNIT_AURA" or event == "UNIT_STATS" or event == "UNIT_ATTACK_POWER" or event == "UNIT_RANGED_ATTACK_POWER" then
        local unit = ...
        if unit == "player" then
            HandlePlayerUpdateRequest(false)
        end
        return
    end

    if event == "PLAYER_ALIVE" or event == "PLAYER_DEAD" or event == "PLAYER_UNGHOST" then
        EnsurePlayerPortraitHealth()
        UpdatePlayerPortraitHealthText()
        return
    end

    HandlePlayerUpdateRequest(false)
end)

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
end
