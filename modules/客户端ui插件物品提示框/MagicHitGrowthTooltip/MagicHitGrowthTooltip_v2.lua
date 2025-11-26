-- UnifiedItemTooltip.lua
-- 统一的物品提示框插件 - 根据服务器数据动态显示所有属性

local ADDON_NAME = "UnifiedItemTooltip"
local ADDON_PREFIX = "UITQ"  -- Addon消息前缀（最多16字符）- UnifiedItemTooltipQuery
local ADDON_PREFIX_ALT = "ITEMENHANCE"  -- 备用前缀（兼容服务器可能使用的其他前缀）

-- ============================================================================
-- 配置
-- ============================================================================

local DEFAULTS = {
    debug = false,  -- 默认关闭调试模式
    queryInterval = 1,      -- 查询间隔（秒）- 只在所有数据齐全时生效
    timeout = 15,           -- 查询超时（秒）- 增加到15秒以应对服务器延迟
    emptyCooldown = 30,     -- 无数据冷却时间（秒）
    cacheExpiration = 300,  -- 缓存有效期（秒）- 默认5分钟

    -- 各系统开关
    systems = {
        magic = true,           -- 魔次系统
        growth = true,          -- 成长系统
        skills = true,          -- 追加技能
        runes = true,           -- 符文系统
        enhancement = true,     -- 强化系统
        identification = true,  -- 鉴定系统
        sets = true            -- 套装系统
    },

    -- 颜色配置
    colors = {
        header = "|cffff8000",
        value = "|cff00ff00",
        name = "|cffffffff",
        special = "|cff0080ff",
        reset = "|r"
    }
}

-- 初始化配置
UnifiedItemTooltipDB = UnifiedItemTooltipDB or {}
for k, v in pairs(DEFAULTS) do
    if UnifiedItemTooltipDB[k] == nil then
        if type(v) == "table" then
            UnifiedItemTooltipDB[k] = {}
            for k2, v2 in pairs(v) do
                UnifiedItemTooltipDB[k][k2] = v2
            end
        else
            UnifiedItemTooltipDB[k] = v
        end
    end
end

local DB = UnifiedItemTooltipDB

-- ============================================================================
-- 调试工具
-- ============================================================================

local function DebugPrint(...)
    if not DB.debug then return end
    local msg = ""
    for i = 1, select("#", ...) do
        msg = msg .. tostring(select(i, ...)) .. " "
    end
    print("|cff00ffff[统一提示框]|r " .. msg)
end

-- ============================================================================
-- 常量定义
-- ============================================================================

-- 属性名称映射
local ATTR_NAMES = {
    [0] = "法力值", [1] = "生命值", [3] = "敏捷", [4] = "力量",
    [5] = "智力", [6] = "精神", [7] = "耐力", [12] = "防御等级",
    [13] = "躲闪等级", [14] = "招架等级", [15] = "格挡等级",
    [31] = "命中等级", [32] = "暴击等级", [35] = "韧性等级",
    [36] = "急速等级", [37] = "精准等级", [38] = "攻击强度",
    [39] = "远程强度", [43] = "法术穿透", [44] = "护甲穿透",
    [45] = "法术强度", [46] = "冰霜抗性", [47] = "火焰抗性",
    [48] = "自然抗性", [49] = "暗影抗性", [50] = "神圣抗性"
}

-- 颜色常量：粉色/红色/重置
local COLOR_PINK = "|cffff69b4"   -- 粉色，用于显示“倍率xN”
local COLOR_RED  = "|cffff0000"   -- 红色，用于显示“= 数值”
local COLOR_RESET = "|r"

-- 符文图标
local RUNE_ICONS = {
    EMPTY = "|cFF555555□|r",
    [1] = "|cFFFF4D4D■|r",  -- 红色
    [2] = "|cFF4DA6FF■|r",  -- 蓝色
    [3] = "|cFFFFD24D■|r",  -- 黄色
    [4] = "|cFFC67CFF■|r"   -- 紫色
}

-- ============================================================================
-- 全局状态
-- ============================================================================

local State = {
    -- 统一的数据缓存：key -> { itemID, guid, data }
    cache = {},

    -- 查询状态：key -> { started, timeout, systems = { systemName = sentTime } }
    pending = {},

    -- 最后查询时间：key -> timestamp
    lastQuery = {},

    -- 无数据冷却：key -> timestamp
    noDataUntil = {},

    -- 缓存创建时间：key -> timestamp
    cacheTime = {},

    -- 提示框元数据：tooltip -> { key, rendered, loadingLines }
    tooltips = {},

    -- 性能统计：systemName -> { totalQueries, totalTime, maxTime, minTime }
    stats = {},

    -- 全局查询计数器
    queryId = 0
}

local MakeKey       -- 提前声明，供幻境相关函数使用
local RenderTooltip -- 提前声明，供幻境相关更新调用
local RefreshUnifiedFrame -- 提前声明，供四联大框刷新使用

-- 记录当前按物品键关联的统一四联大框，用于异步刷新
local UnifiedFramesByKey = {}

-- 幻境系统数据缓存（独立于服务器批量查询）
local HuanJingState = {
    cache = {},   -- key -> { itemID, guid, multiplier, attributeData, identificationData, timestamp }
    pending = {}, -- key -> lastQueryTime
    COOLDOWN = 3, -- 查询冷却（秒）
    EXPIRE = 60   -- 缓存有效期（秒）
}

-- 记录当前正在显示的“官方”提示框（GameTooltip / ItemRefTooltip / ShoppingTooltip）
-- 用于在收到幻境倍率后回写左侧官方属性行（例如：156 力量 → 156 + 倍率x20 = 3120 力量）
local HuanJingOfficialTooltips = {}

-- 尝试从缓存获取有效的幻境数据
local function HuanJingGetData(key)
    local data = HuanJingState.cache[key]
    if not data then return nil end

    local now = GetTime()
    if not data.timestamp or (now - data.timestamp) > HuanJingState.EXPIRE then
        HuanJingState.cache[key] = nil
        return nil
    end

    return data
end

-- 渲染幻境属性块：按照“4 + 4”风格显示（原值 + 幻境追加值）
local function RenderHuanJingAttributes(tooltip, hjData, meta)
    if not tooltip or not hjData then return end

    meta = meta or {}
    meta.rendered = meta.rendered or {}
    if meta.rendered.huanjing then return end

    -- 解析属性数据，格式：属性类型 原值 增强值,属性类型 原值 增强值
    local attributes = {}

    local function parseAttributes(src)
        if not src or src == "" then return end

        for attrStr in string.gmatch(src, "([^,]+)") do
            local parts = {}
            for part in string.gmatch(attrStr, "([^%s]+)") do
                table.insert(parts, part)
            end

            if #parts >= 3 then
                local attrType = parts[1]
                local originalValue = tonumber(parts[2])
                local enhancedValue = tonumber(parts[3])

                if originalValue and enhancedValue then
                    table.insert(attributes, {
                        type = attrType,
                        original = originalValue,
                        enhanced = enhancedValue
                    })
                end
            end
        end
    end

    -- 官方基础属性 + 鉴定系统属性都视为“追加属性”的原始值和增强值
    parseAttributes(hjData.attributeData)
    parseAttributes(hjData.identificationData)

    if #attributes == 0 then return end

    tooltip:AddLine(" ")
    tooltip:AddLine(DB.colors.header .. "幻境属性" .. DB.colors.reset)

    for _, attr in ipairs(attributes) do
        local name
        local attrTypeNum = tonumber(attr.type)

        if attrTypeNum then
            name = ATTR_NAMES[attrTypeNum] or ("属性" .. attrTypeNum)
        else
            if attr.type == "armor" then
                name = "护甲"
            elseif attr.type:match("^resist%d+") then
                local resistNames = {
                    resist0 = "神圣抗性",
                    resist1 = "火焰抗性",
                    resist2 = "自然抗性",
                    resist3 = "冰霜抗性",
                    resist4 = "暗影抗性",
                    resist5 = "奥术抗性"
                }
                name = resistNames[attr.type] or "抗性"
            else
                name = attr.type
            end
        end

        local bonus = attr.enhanced - attr.original
        if bonus ~= 0 then
            tooltip:AddDoubleLine(
                string.format("%d + %d", attr.original, bonus),
                name,
                0, 1, 0,
                0, 1, 0
            )
        end
    end

    meta.rendered.huanjing = true
    tooltip:Show()
end

-- 发送幻境倍率查询（带简单冷却，避免刷屏）
local function HuanJingRequest(itemID, guid, key, now)
    if not itemID or not guid or guid == 0 then return end

    -- 已有有效缓存则不再查询
    if HuanJingGetData(key) then
        return
    end

    local last = HuanJingState.pending[key]
    if last and (now - last) < HuanJingState.COOLDOWN then
        return
    end

    local addonMessage = string.format("HUANJING_QUERY:%d:%d", itemID, guid)

    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    end

    HuanJingState.pending[key] = now
end

-- 将幻境倍率应用到“官方”物品提示框（左侧原版 GameTooltip）
local function ApplyHuanJingToOfficialTooltip(tooltip)
    if not tooltip or not tooltip:IsShown() then return end

    local state = HuanJingOfficialTooltips[tooltip]
    if not state or state.applied then return end

    local hjData = HuanJingGetData(state.key)
    if not hjData or not hjData.multiplier or hjData.multiplier <= 1 then
        return
    end

    local mult = hjData.multiplier

    -- 只处理主属性：敏捷(3)、力量(4)、智力(5)、精神(6)、耐力(7)
    local baseTypes = {
        [3] = true, -- 敏捷
        [4] = true, -- 力量
        [5] = true, -- 智力
        [6] = true, -- 精神
        [7] = true, -- 耐力
    }

    local baseAttrsByName = {}

    if hjData.attributeData and hjData.attributeData ~= "" then
        for attrStr in string.gmatch(hjData.attributeData, "([^,]+)") do
            local parts = {}
            for part in string.gmatch(attrStr, "([^%s]+)") do
                table.insert(parts, part)
            end

            if #parts >= 3 then
                local attrTypeNum = tonumber(parts[1])
                local originalValue = tonumber(parts[2])
                local enhancedValue = tonumber(parts[3])

                if attrTypeNum and originalValue and enhancedValue and baseTypes[attrTypeNum] then
                    local name = ATTR_NAMES[attrTypeNum]
                    if name then
                        baseAttrsByName[name] = {
                            original = originalValue,
                            enhanced = enhancedValue
                        }
                    end
                end
            end
        end
    end

    if not next(baseAttrsByName) then
        return
    end

    local numLines = tooltip:NumLines()
    local changed = false

    for i = 1, numLines do
        local leftText = _G[tooltip:GetName() .. "TextLeft" .. i]
        if leftText then
            local text = leftText:GetText()
            if text and text ~= "" then
                local colorPrefix = text:match("^(|c%x%x%x%x%x%x%x%x)") or ""
                local colorSuffix = text:match("(|r)$") or ""

                local clean = text:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "")

                for name, info in pairs(baseAttrsByName) do
                    -- 匹配类似 “+156 力量” 的官方属性行
                    local amountStr = clean:match("^%+?(%-?%d+)%s*" .. name .. "%s*$")
                    if amountStr then
                        local original = info.original or tonumber(amountStr) or 0
                        local enhanced = info.enhanced or (original * mult)

                        -- 与右侧“基础/追加属性”保持一致的配色：倍率为粉色，最终值为红色
                        local newCoreText = string.format("%d + %s倍率x%d%s %s= %d%s %s",
                            original,
                            COLOR_PINK, mult, COLOR_RESET,
                            COLOR_RED, enhanced, COLOR_RESET,
                            name)

                        leftText:SetText(colorPrefix .. newCoreText .. colorSuffix)
                        changed = true
                        break
                    end
                end
            end
        end
    end

    if changed then
        state.applied = true
        tooltip:Show()
    end
end

-- 在现有tooltip上尝试渲染幻境属性（如果缓存中已有数据）
local function HuanJingTryRender(tooltip, itemID, guid, meta)
    local key = MakeKey(itemID, guid, nil, nil, false)
    local data = HuanJingGetData(key)
    if data then
        RenderHuanJingAttributes(tooltip, data, meta)
    end
end

-- 处理来自服务器的幻境系统系统消息
local function HuanJingHandleSystemMessage(message)
    if not message or not message:find("^%[幻境系统%]") then
        return
    end

    -- 兼容多种响应格式
    local itemId, itemGuid, multiplier, attributeData, identificationData =
        message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性倍率:([%d%.]+) 属性数据:(.+) 鉴定数据:(.+)")

    if not itemId then
        itemId, itemGuid, multiplier, attributeData =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性倍率:([%d%.]+) 属性数据:(.+)")
        identificationData = nil
    end

    if not itemId then
        itemId, itemGuid, multiplier =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性倍率:([%d%.]+)")
        attributeData = nil
        identificationData = nil
    end

    if not itemId or not itemGuid or not multiplier then
        return
    end

    itemId = tonumber(itemId)
    itemGuid = tonumber(itemGuid)
    multiplier = tonumber(multiplier)

    if not itemId or not itemGuid or not multiplier then
        return
    end

    local key = MakeKey(itemId, itemGuid, nil, nil, false)

    HuanJingState.cache[key] = {
        itemID = itemId,
        guid = itemGuid,
        multiplier = multiplier,
        attributeData = attributeData or "",
        identificationData = identificationData or "",
        timestamp = GetTime()
    }

    HuanJingState.pending[key] = nil

    -- 更新当前正在显示同一件装备的官方提示框（左侧）
    for t, s in pairs(HuanJingOfficialTooltips) do
        if t:IsShown() and s.key == key then
            ApplyHuanJingToOfficialTooltip(t)
        end
    end

    -- 如果当前有正在显示该物品的提示框，刷新基础/追加属性的显示（追加“原值 + 幻境加成”）
    for tooltip, meta in pairs(State.tooltips) do
        if not tooltip.UIT_IsUnifiedFrame and tooltip:IsShown() and meta.key == key then
            meta.rendered = meta.rendered or {}
            meta.rendered.identification = nil
            meta.rendered.enhancement = nil
            meta.rendered.growth = nil

            RenderTooltip(tooltip, itemId, itemGuid)
        end
    end

    -- 同步刷新统一四联大框中的幻境相关数值（即使当前未显示，也会根据数据智能决定是否显示）
    local frames = UnifiedFramesByKey[key]
    if frames then
        for _, frame in ipairs(frames) do
            RefreshUnifiedFrame(frame, itemId, itemGuid)
        end
    end
end

-- 处理来自服务器的幻境系统Addon消息
local function HuanJingHandleAddonMessage(message)
    if not message or not message:match("^HUANJING_DATA:") then
        return
    end

    local itemId, itemGuid, multiplier, rest =
        message:match("^HUANJING_DATA:(%d+):(%d+):([%d%.]+):?(.*)$")

    if not itemId or not itemGuid or not multiplier then
        return
    end

    local attributeData, identificationData
    if rest and rest ~= "" then
        local sep = rest:find("|", 1, true)
        if sep then
            attributeData = rest:sub(1, sep - 1)
            identificationData = rest:sub(sep + 1)
        else
            attributeData = rest
        end
    end

    itemId = tonumber(itemId)
    itemGuid = tonumber(itemGuid)
    multiplier = tonumber(multiplier)

    if not itemId or not itemGuid or not multiplier then
        return
    end

    local key = MakeKey(itemId, itemGuid, nil, nil, false)

    HuanJingState.cache[key] = {
        itemID = itemId,
        guid = itemGuid,
        multiplier = multiplier,
        attributeData = attributeData or "",
        identificationData = identificationData or "",
        timestamp = GetTime()
    }

    HuanJingState.pending[key] = nil

    -- 更新当前正在显示同一件装备的官方提示框（左侧）
    for t, s in pairs(HuanJingOfficialTooltips) do
        if t:IsShown() and s.key == key then
            ApplyHuanJingToOfficialTooltip(t)
        end
    end

    for tooltip, meta in pairs(State.tooltips) do
        if not tooltip.UIT_IsUnifiedFrame and tooltip:IsShown() and meta.key == key then
            meta.rendered = meta.rendered or {}
            meta.rendered.identification = nil
            meta.rendered.enhancement = nil
            meta.rendered.growth = nil

            RenderTooltip(tooltip, itemId, itemGuid)
        end
    end

    -- 同步刷新统一四联大框中的幻境相关数值
    local frames = UnifiedFramesByKey[key]
    if frames then
        for _, frame in ipairs(frames) do
            RefreshUnifiedFrame(frame, itemId, itemGuid)
        end
    end
end

-- ============================================================================
-- 工具函数
-- ============================================================================

-- 生成缓存键
MakeKey = function(itemID, guid, bag, slot, equipFlag)
    if guid and guid > 0 then
        return string.format("G:%d:%d", itemID, guid)
    end
    if equipFlag then
        return string.format("E:%d:%d", slot or -1, itemID)
    end
    if bag and bag >= 0 and slot and slot >= 0 then
        return string.format("B:%d:%d:%d", bag, slot, itemID)
    end
    return string.format("U:%d", itemID)
end

-- 扫描背包查找物品的 GUID
local function ScanBagsForItem(itemID)
    if not itemID then return nil end

    -- 扫描背包 (0-4)
    for bag = 0, 4 do
        local numSlots = GetContainerNumSlots(bag)
        for slot = 1, numSlots do
            local link = GetContainerItemLink(bag, slot)
            if link then
                local bagItemID, bagGUID = tonumber(string.match(link, "item:(%d+)")), nil

                if bagItemID == itemID then
                    -- 尝试从链接中提取 GUID
                    local itemString = string.match(link, "item[%-?%d:]+")
                    if itemString then
                        local parts = { strsplit(":", itemString) }
                        if #parts >= 7 then
                            local val = tonumber(parts[7])
                            if val and val > 1000000 then
                                bagGUID = val - 1000000
                            elseif val and val > 0 then
                                bagGUID = val
                            end
                        end
                        if not bagGUID and #parts >= 8 then
                            local val = tonumber(parts[8])
                            if val and val > 0 then
                                bagGUID = val
                            end
                        end
                    end

                    if bagGUID and bagGUID > 0 then
                        return bagGUID, bag, slot
                    end
                end
            end
        end
    end

    return nil
end

-- 扫描装备栏查找物品的 GUID
local function ScanEquipmentForItem(itemID)
    if not itemID then return nil end

    -- 遍历所有装备槽位
    for slot = 0, 19 do
        local link = GetInventoryItemLink("player", slot)
        if link then
            local equipItemID, equipGUID = tonumber(string.match(link, "item:(%d+)")), nil

            if equipItemID == itemID then
                -- 尝试从链接中提取 GUID
                local itemString = string.match(link, "item[%-?%d:]+")
                if itemString then
                    local parts = { strsplit(":", itemString) }
                    if #parts >= 7 then
                        local val = tonumber(parts[7])
                        if val and val > 1000000 then
                            equipGUID = val - 1000000
                        elseif val and val > 0 then
                            equipGUID = val
                        end
                    end
                    if not equipGUID and #parts >= 8 then
                        local val = tonumber(parts[8])
                        if val and val > 0 then
                            equipGUID = val
                        end
                    end
                end

                if equipGUID and equipGUID > 0 then
                    return equipGUID, nil, slot, true
                end
            end
        end
    end

    return nil
end

-- 从物品链接提取ID和GUID
local function ExtractItemInfo(itemLink)
    if not itemLink then return nil end

    local itemID = tonumber(string.match(itemLink, "item:(%d+)"))
    if not itemID then return nil end

    local itemString = string.match(itemLink, "item[%-?%d:]+")
    local guid = nil

    if itemString then
        local parts = { strsplit(":", itemString) }

        -- 尝试从多个位置提取GUID
        -- 位置7: suffixID/uniqueID（随机附魔或唯一ID）
        -- 位置8: uniqueID（唯一ID）
        -- 位置9: level
        -- 位置10-13: bonusIDs等

        local possibleGuidPositions = {7, 8, 9, 10, 11, 12, 13}

        for _, pos in ipairs(possibleGuidPositions) do
            if parts[pos] then
                local val = tonumber(parts[pos])
                if val then
                    -- 检查是否是有效的GUID
                    -- GUID通常是一个大于0的整数
                    -- 有些服务器会在GUID上加1000000
                    if val > 1000000 then
                        guid = val - 1000000
                        break
                    elseif val > 0 and val < 1000000 then
                        -- 可能的GUID范围
                        guid = val
                        break
                    end
                end
            end
        end
    end

    return itemID, guid
end

-- 增强版物品信息提取 - 如果链接中没有 GUID，尝试从背包/装备栏查找
local function ExtractItemInfoEnhanced(itemLink)
    if not itemLink then return nil end

    local itemID, guid = ExtractItemInfo(itemLink)
    if not itemID then return nil end

    -- 如果链接中已经有有效的 GUID，直接返回
    if guid and guid > 0 then
        return itemID, guid, nil, nil, false
    end

    -- 没有 GUID，尝试从装备栏查找
    local equipGUID, _, equipSlot, isEquipped = ScanEquipmentForItem(itemID)
    if equipGUID and equipGUID > 0 then
        return itemID, equipGUID, nil, equipSlot, true
    end

    -- 从背包查找
    local bagGUID, bag, slot = ScanBagsForItem(itemID)
    if bagGUID and bagGUID > 0 then
        return itemID, bagGUID, bag, slot, false
    end

    -- 如果都找不到，返回 itemID 和 nil GUID
    return itemID, nil, nil, nil, false
end

local function WrapText(text, maxCharsPerLine)
    if not text or text == "" then return {text} end

    -- 默认每行200个“显示字节”：ASCII算1，中文算2，大约可以放下100个汉字
    maxCharsPerLine = maxCharsPerLine or 200

    -- 提取颜色代码
    local colorCode = text:match("^(|c%x%x%x%x%x%x%x%x)")

    -- 移除颜色代码后的纯文本
    local plainText = text:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "")

    -- 如果文本不长，直接返回
    local displayLen = 0
    local i = 1
    while i <= #plainText do
        local c = string.byte(plainText, i)
        if c > 0 and c <= 127 then
            displayLen = displayLen + 1
            i = i + 1
        elseif c >= 224 and c <= 239 then
            -- 3字节UTF8字符（中文）
            displayLen = displayLen + 2
            i = i + 3
        else
            i = i + 1
        end
    end

    if displayLen <= maxCharsPerLine then
        return {text}
    end

    -- 分割文本
    local lines = {}
    local currentLine = ""
    local currentLen = 0
    local i = 1

    while i <= #plainText do
        local c = string.byte(plainText, i)
        local charBytes = 1
        local charDisplayLen = 1

        if c > 0 and c <= 127 then
            -- ASCII字符
            charBytes = 1
            charDisplayLen = 1
        elseif c >= 224 and c <= 239 then
            -- 3字节UTF8字符（中文）
            charBytes = 3
            charDisplayLen = 2
        elseif c >= 194 and c <= 223 then
            -- 2字节UTF8字符
            charBytes = 2
            charDisplayLen = 2
        elseif c >= 240 and c <= 244 then
            -- 4字节UTF8字符
            charBytes = 4
            charDisplayLen = 2
        end

        -- 检查是否需要换行
        if currentLen + charDisplayLen > maxCharsPerLine and currentLine ~= "" then
            -- 添加当前行（带颜色代码）
            if colorCode then
                table.insert(lines, colorCode .. currentLine .. "|r")
            else
                table.insert(lines, currentLine)
            end
            currentLine = ""
            currentLen = 0
        end

        -- 添加字符到当前行
        currentLine = currentLine .. string.sub(plainText, i, i + charBytes - 1)
        currentLen = currentLen + charDisplayLen
        i = i + charBytes
    end

    -- 添加最后一行
    if currentLine ~= "" then
        if colorCode then
            table.insert(lines, colorCode .. currentLine .. "|r")
        else
            table.insert(lines, currentLine)
        end
    end

    return lines
end


-- ============================================================================
-- 数据解析器 - 只使用批量查询解析器（addon格式）
-- ============================================================================

local Parsers = {}

-- 批量数据解析器（解析ALL_MODULE_DATA消息）
-- 这是唯一的解析器，处理服务器通过addon消息返回的批量数据
function Parsers.BatchQuery(message)
    -- 格式：ALL_MODULE_DATA:itemID:guid:base:additional:growth:enhancement:skills:magic:rune:set
    if not message:match("^ALL_MODULE_DATA:") then
        return nil
    end

    -- 性能监控：开始解析
    local parseStartTime = GetTime()

    local parts = { strsplit(":", message) }
    if #parts < 3 then
        return nil
    end

    local itemID = tonumber(parts[2])
    local guid = tonumber(parts[3])

    if not itemID or not guid or guid == 0 then
        return nil
    end

    -- 解析所有系统数据
    local baseAttributes = parts[4] or ""
    local additionalAttributes = parts[5] or ""
    local growthData = parts[6] or ""
    local enhancementData = parts[7] or ""
    local skillsData = parts[8] or ""
    local magicData = parts[9] or ""
    local runeData = parts[10] or ""

    -- 套装字段在消息末尾，可能包含多个":"，因此需要把第11段之后的内容重新拼回去
    local setData = ""
    if #parts >= 11 then
        setData = table.concat(parts, ":", 11)
    end

    local result = {
        type = "batch",
        itemID = itemID,
        guid = guid,
        systems = {}
    }

    -- 解析鉴定系统数据（基础属性和追加属性）
    if baseAttributes ~= "" or additionalAttributes ~= "" then
        local baseAttrs = {}
        local additionalAttrs = {}

        -- 解析基础属性
        if baseAttributes ~= "" then
            for pair in string.gmatch(baseAttributes, "([^,]+)") do
                local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
                if attrType and value then
                    table.insert(baseAttrs, {
                        type = tonumber(attrType),
                        value = tonumber(value)
                    })
                end
            end
        end

        -- 解析追加属性
        if additionalAttributes ~= "" then
            for pair in string.gmatch(additionalAttributes, "([^,]+)") do
                local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
                if attrType and value then
                    table.insert(additionalAttrs, {
                        type = tonumber(attrType),
                        value = tonumber(value)
                    })
                end
            end
        end

        -- 只要有基础属性或追加属性，就创建鉴定系统数据
        if #baseAttrs > 0 or #additionalAttrs > 0 then
            result.systems.identification = {
                type = "identification",
                itemID = itemID,
                guid = guid,
                baseAttributes = baseAttrs,
                additionalAttributes = additionalAttrs
            }
        end
    end

    -- 解析成长系统数据（格式：level|currentExp|requiredExp|attrs）
    if growthData ~= "" then
        local gParts = { strsplit("|", growthData) }
        if #gParts >= 3 then
            local attrs = {}
            if #gParts >= 4 and gParts[4] ~= "" then
                for pair in string.gmatch(gParts[4], "([^,]+)") do
                    local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
                    if attrType and value then
                        table.insert(attrs, {
                            type = tonumber(attrType),
                            value = tonumber(value)
                        })
                    end
                end
            end

            result.systems.growth = {
                type = "growth",
                itemID = itemID,
                guid = guid,
                level = tonumber(gParts[1]),
                currentExp = tonumber(gParts[2]),
                requiredExp = tonumber(gParts[3]),
                attributes = attrs
            }
        end
    end

    -- 解析强化系统数据（格式：level|attrs）
    if enhancementData ~= "" then
        local eParts = { strsplit("|", enhancementData) }
        if #eParts >= 1 then
            local attrs = {}
            if #eParts >= 2 and eParts[2] ~= "" then
                for pair in string.gmatch(eParts[2], "([^,]+)") do
                    local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
                    if attrType and value then
                        table.insert(attrs, {
                            type = tonumber(attrType),
                            value = tonumber(value)
                        })
                    end
                end
            end

            result.systems.enhancement = {
                type = "enhancement",
                itemID = itemID,
                guid = guid,
                level = tonumber(eParts[1]),
                attributes = attrs
            }
        end
    end

    -- 解析技能数据（简化格式：skillId,skillId,skillId 或完整格式：skillId|skillName|level,skillId|skillName|level）
    if skillsData ~= "" then
        local skills = {}

        -- 检查是否包含竖线（完整格式）
        if string.find(skillsData, "|") then
            -- 完整格式解析
            for skillInfo in string.gmatch(skillsData, "([^,]+)") do
                local sParts = { strsplit("|", skillInfo) }
                if #sParts >= 3 then
                    table.insert(skills, {
                        id = tonumber(sParts[1]),
                        name = sParts[2],
                        level = tonumber(sParts[3])
                    })
                end
            end
        else
            -- 简化格式解析（只有技能ID）
            for skillId in string.gmatch(skillsData, "([^,]+)") do
                local id = tonumber(skillId)
                if id then
                    table.insert(skills, {
                        id = id,
                        name = "技能" .. id,  -- 默认名称
                        level = 1  -- 默认等级
                    })
                end
            end
        end

        if #skills > 0 then
            result.systems.skills = {
                type = "skills",
                itemID = itemID,
                guid = guid,
                skills = skills
            }
        end
    end

    -- 解析魔次数据（格式：ids|values 或 configId|count|desc,configId|count|desc）
    if magicData ~= "" then
        local configs = {}

        -- 检查是否包含竖线（可能是 ids|values 格式）
        if string.find(magicData, "|") then
            -- 尝试 ids|values 格式
            local parts = { strsplit("|", magicData) }
            if #parts == 2 then
                -- 格式：ids|values（如 "2,1,5|100,200,300"）
                local ids = {}
                local values = {}

                for id in string.gmatch(parts[1], "([^,]+)") do
                    table.insert(ids, tonumber(id))
                end

                for value in string.gmatch(parts[2], "([^,]+)") do
                    table.insert(values, tonumber(value))
                end

                -- 配对ID和值
                for i = 1, #ids do
                    table.insert(configs, {
                        id = ids[i],
                        count = values[i] or 0,
                        desc = "魔次" .. (ids[i] or "")
                    })
                end
            else
                -- 完整格式：configId|count|desc,configId|count|desc
                for magicInfo in string.gmatch(magicData, "([^,]+)") do
                    local mParts = { strsplit("|", magicInfo) }
                    if #mParts >= 3 then
                        table.insert(configs, {
                            id = tonumber(mParts[1]),
                            count = tonumber(mParts[2]),
                            desc = mParts[3]
                        })
                    end
                end
            end
        else
            -- 简化格式：只有ID列表（如 "2,1,5"）
            for configId in string.gmatch(magicData, "([^,]+)") do
                local id = tonumber(configId)
                if id then
                    table.insert(configs, {
                        id = id,
                        count = 0,
                        desc = "魔次" .. id
                    })
                end
            end
        end

        if #configs > 0 then
            result.systems.magic = {
                type = "magic",
                itemID = itemID,
                guid = guid,
                configs = configs
            }
        end
    end

    -- 解析符文数据（格式：totalSlots|slotIds|runeItemIds）
    if runeData ~= "" then
        local rParts = { strsplit("|", runeData) }
        if #rParts >= 2 then
            -- 解析插槽总数（第一个字段）
            local totalSlots = 0
            local slotIds = ""
            local runeItemIds = ""

            -- 处理第一个字段：可能是纯数字，也可能是"数字,数字,..."格式
            local firstPart = rParts[1]
            if string.find(firstPart, ",") then
                -- 包含逗号，提取第一个数字作为totalSlots
                local firstNum = string.match(firstPart, "^(%d+)")
                totalSlots = tonumber(firstNum) or 0
                slotIds = firstPart  -- 整个字符串作为插槽ID列表
            else
                -- 纯数字
                totalSlots = tonumber(firstPart) or 0
                if #rParts >= 3 then
                    slotIds = rParts[2]
                    runeItemIds = rParts[3]
                else
                    runeItemIds = rParts[2]
                end
            end

            -- 计算已填充的插槽数（符文ID不为0的数量）
            local filledCount = 0
            if #rParts >= 2 then
                local runeIds = rParts[#rParts]  -- 最后一个字段是符文物品ID
                for id in string.gmatch(runeIds, "([^,]+)") do
                    if tonumber(id) and tonumber(id) > 0 then
                        filledCount = filledCount + 1
                    end
                end
            end

            result.systems.runes = {
                type = "runes",
                itemID = itemID,
                guid = guid,
                totalSlots = totalSlots,
                filledSlots = filledCount,
                slotIds = slotIds,
                runeItemIds = runeItemIds,
                slots = {}  -- 简化版，不解析详细槽位数据
            }
        end
    end

    -- 解析套装数据（格式：setId 或 setId:setName:attrs:effects）
    if setData ~= "" then
        local sParts = { strsplit(":", setData) }
        if #sParts >= 1 then
            local attrs = {}
            local effects = {}

            -- 解析套装属性（attrs，形如 "4 20,7 30"）
            if #sParts >= 3 and sParts[3] ~= "" then
                for pair in string.gmatch(sParts[3], "([^,]+)") do
                    local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
                    if attrType and value then
                        table.insert(attrs, {
                            type = tonumber(attrType),
                            value = tonumber(value)
                        })
                    end
                end
            end

            -- 解析套装效果（effects，服务器格式："件数|效果描述,件数|效果描述"）
            if #sParts >= 4 and sParts[4] ~= "" then
                for eff in string.gmatch(sParts[4], "([^,]+)") do
                    local countStr, desc = eff:match("(%d+)%|(.*)")
                    local count = tonumber(countStr)
                    if count and desc and desc ~= "" then
                        table.insert(effects, {
                            count = count,
                            desc = desc
                        })
                    end
                end
            end

            result.systems.sets = {
                type = "sets",
                itemID = itemID,
                guid = guid,
                setId = tonumber(sParts[1]),
                setName = sParts[2] or "套装",  -- 如果没有setName，使用默认值
                attributes = attrs,
                effects = effects
            }
        end
    end

    -- 统计解析的系统数量
    local sysCount = 0
    for _ in pairs(result.systems) do sysCount = sysCount + 1 end

    return result
end


-- ============================================================================
-- 渲染器 - 统一的渲染逻辑
-- ============================================================================

local Renderers = {}

-- 渲染魔次属性
function Renderers.Magic(tooltip, data)
    if not data.configs or #data.configs == 0 then return end

    tooltip:AddLine(" ")
    tooltip:AddLine(DB.colors.header .. "魔次属性" .. DB.colors.reset)

    local total = 0
    for _, config in ipairs(data.configs) do
        tooltip:AddLine(string.format("%s%s: %s%d%s",
            DB.colors.value, config.desc,
            DB.colors.value, config.count,
            DB.colors.reset))
        total = total + config.count
    end

    if total > 0 then
        tooltip:AddLine(DB.colors.value .. "总计: " .. total .. DB.colors.reset)
    end
end

-- 渲染成长属性
function Renderers.Growth(tooltip, data)
    tooltip:AddLine(" ")
    tooltip:AddLine("|cffff8000追加成长|r")

    if data.level and data.level > 0 then
        tooltip:AddLine(string.format("%s等级: %s%d%s",
            DB.colors.value, DB.colors.value, data.level, DB.colors.reset))
    end

    if data.currentExp and data.requiredExp and data.requiredExp > 0 then
        tooltip:AddLine(string.format("%s经验: %s%d/%d%s",
            DB.colors.value, DB.colors.value,
            data.currentExp, data.requiredExp, DB.colors.reset))
    end

    if data.attributes and #data.attributes > 0 then
        for _, attr in ipairs(data.attributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            tooltip:AddLine(string.format("%s+%d %s%s",
                DB.colors.value, attr.value, name, DB.colors.reset))
        end
    end
end

-- 判断是否是官方属性行
local function IsOfficialStatLine(text)
    if not text or text == "" then return false end

    -- 如果已经是自定义属性（包含颜色代码），不隐藏
    if text:match("|cFF%x%x%x%x%x%x") or text:match("|cff%x%x%x%x%x%x") then
        return false
    end

    -- 官方属性特征
    local officialPatterns = {
        "^%+%d+",                           -- +50 这样的开头
        "%+%d+$",                           -- 这样的结尾
        "装备:%s",                          -- 装备:xxx
        "增加.+技能",                       -- 增加xxx技能
        "提高.+百分比",                     -- 提高xx百分比
        "增强.+%d+",                        -- 增强xx+数字
        "^[%w%s]+%+%d+",                    -- 属性名+数字
        "使用时",                           -- 使用时效果
        "韧性",                             -- 单独的属性词
        "护甲",                             -- 护甲属性
        "攻击强度",                         -- 攻击强度
        "暴击",                             -- 暴击相关
        "命中",                             -- 命中相关
        "闪躲",                             -- 闪躲相关
        "格挡",                             -- 格挡相关
        "防御等级",                         -- 防御等级
    }

    for _, pattern in ipairs(officialPatterns) do
        if text:match(pattern) then
            return true
        end
    end

    return false
end

-- 渲染鉴定基础属性
function Renderers.Identification(tooltip, data)
    if not data.baseAttributes or #data.baseAttributes == 0 then return end

    -- 查找并替换官方属性行
    local numLines = tooltip:NumLines()
    local firstStatLine = nil
    local lastStatLine = nil

    for i = 1, numLines do
        local leftText = _G[tooltip:GetName() .. "TextLeft" .. i]
        if leftText then
            local text = leftText:GetText() or ""

            if text ~= "" and IsOfficialStatLine(text) then
                if not firstStatLine then
                    firstStatLine = i
                end
                lastStatLine = i
            end
        end
    end

    -- 如果找到了官方属性，直接替换文本
    if firstStatLine and lastStatLine then
        DebugPrint("找到官方属性行范围:", firstStatLine, "~", lastStatLine)

        -- 第一行显示标题
        local leftText = _G[tooltip:GetName() .. "TextLeft" .. firstStatLine]
        if leftText then
            leftText:SetText("|cFFFFFF00基础属性|r")
        end

        -- 从第二行开始显示基础属性
        local attrIndex = 1
        for i = firstStatLine + 1, lastStatLine do
            local leftText = _G[tooltip:GetName() .. "TextLeft" .. i]
            if leftText and attrIndex <= #data.baseAttributes then
                local attr = data.baseAttributes[attrIndex]
                local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
                local displayText = string.format("|cff00ff00+%d %s|r", attr.value, name)
                leftText:SetText(displayText)
                attrIndex = attrIndex + 1
            elseif leftText then
                -- 清除多余的行
                leftText:SetText("")
            end
        end

        -- 如果基础属性数量超过了官方属性行数，添加新行
        if attrIndex <= #data.baseAttributes then
            for i = attrIndex, #data.baseAttributes do
                local attr = data.baseAttributes[i]
                local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
                local displayText = string.format("|cff00ff00+%d %s|r", attr.value, name)
                tooltip:AddLine(displayText)
            end
        end
    else
        -- 如果没找到官方属性位置，直接在底部添加
        tooltip:AddLine(" ")
        tooltip:AddLine("|cFFFFFF00基础属性|r")

        for _, attr in ipairs(data.baseAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local displayText = string.format("|cff00ff00+%d %s|r", attr.value, name)
            tooltip:AddLine(displayText)
        end
    end

    -- 显示追加属性（如果有）
    if data.additionalAttributes and #data.additionalAttributes > 0 then
        tooltip:AddLine(" ")
        tooltip:AddLine("|cff00ff00追加属性|r")

        for _, attr in ipairs(data.additionalAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local displayText = string.format("|cff00ff00+%d %s|r", attr.value, name)
            tooltip:AddLine(displayText)
        end
    end

    tooltip:Show()
end

-- 渲染强化属性
function Renderers.Enhancement(tooltip, data)
    if not data.level or data.level == 0 then return end

    tooltip:AddLine(" ")
    tooltip:AddLine(DB.colors.header .. "追加强化" .. DB.colors.reset)
    tooltip:AddLine(string.format("%s等级: +%d%s",
        DB.colors.value, data.level, DB.colors.reset))

    if data.attributes and #data.attributes > 0 then
        for _, attr in ipairs(data.attributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            tooltip:AddLine(string.format("%s+%d %s%s",
                DB.colors.value, attr.value, name, DB.colors.reset))
        end
    end
end

-- 渲染追加技能
function Renderers.Skills(tooltip, data)
    if not data.skills or #data.skills == 0 then return end

    tooltip:AddLine(" ")
    tooltip:AddLine(DB.colors.header .. "追加技能" .. DB.colors.reset)

    for _, skill in ipairs(data.skills) do
        local text = DB.colors.value .. skill.name
        if skill.level > 1 then
            text = text .. " " .. DB.colors.special .. "(等级 " .. skill.level .. ")"
        end
        text = text .. DB.colors.reset

        -- 使用WrapText函数将长文本分行显示，每行约支持100个汉字（200显示字节）
        local lines = WrapText(text, 200)
        for _, line in ipairs(lines) do
            tooltip:AddLine(line)
        end
    end
end

-- 渲染符文系统
function Renderers.Runes(tooltip, data)
    if not data.totalSlots or data.totalSlots == 0 then return end

    tooltip:AddLine(" ")
    tooltip:AddLine(DB.colors.header .. "符文系统" .. DB.colors.reset)

    -- 显示槽位数量信息
    tooltip:AddLine(string.format("%s符文槽位: %s%d/%d%s",
        DB.colors.value, DB.colors.value,
        data.filledSlots or 0, data.totalSlots, DB.colors.reset))

    -- 渲染槽位图标（每行10个）
    local perRow = 10
    local icons = {}

    -- 首先建立符文映射 slotId -> rune
    local runeMap = {}
    if data.slots and #data.slots > 0 then
        for _, slot in ipairs(data.slots) do
            if slot.slotId then
                runeMap[slot.slotId] = slot
            end
        end
    end

    -- 渲染所有槽位
    for i = 1, data.totalSlots do
        local slot = runeMap[i]
        local icon

        if slot and slot.runeId and slot.runeId > 0 then
            -- 已镶嵌符文，显示红色方块
            icon = "|cFFFF4D4D■|r"
        else
            -- 空槽位，显示灰色方块
            icon = "|cFF555555□|r"
        end

        table.insert(icons, icon)

        -- 每10个槽位换一行
        if #icons == perRow then
            tooltip:AddLine(table.concat(icons, ""))
            icons = {}
        end
    end

    -- 渲染剩余的槽位
    if #icons > 0 then
        tooltip:AddLine(table.concat(icons, ""))
    end
end

-- 渲染套装信息
function Renderers.Sets(tooltip, data)
    if not data.setId or data.setId == 0 then return end

    tooltip:AddLine(" ")
    tooltip:AddLine("|cffff8000追加套装|r")  -- 橙色标题

    -- 显示套装属性
    if data.attributes and #data.attributes > 0 then
        tooltip:AddLine(DB.colors.value .. "套装属性:" .. DB.colors.reset)
        for _, attr in ipairs(data.attributes) do
            local attrName = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local sign = attr.value >= 0 and "+ " or "- "
            tooltip:AddLine(string.format("  %s%s%s %s%s%d%s",
                DB.colors.name, attrName, DB.colors.reset,
                DB.colors.value, sign, attr.value,
                DB.colors.reset))
        end
    end

    -- 显示套装效果
    if data.effects and #data.effects > 0 then
        tooltip:AddLine(DB.colors.value .. "套装效果:" .. DB.colors.reset)
        for _, effect in ipairs(data.effects) do
            tooltip:AddLine(string.format("  %s(%d) %s%s%s",
                DB.colors.value, effect.count,
                DB.colors.special, effect.desc,
                DB.colors.reset))
        end
    end
end


-- ============================================================================
-- 缓存管理
-- ============================================================================

-- 保存数据到缓存
local function CacheData(itemID, guid, data)
    if not itemID or not guid or guid == 0 then return end

    local key = MakeKey(itemID, guid, nil, nil, false)
    local now = GetTime()

    if not State.cache[key] then
        State.cache[key] = {
            itemID = itemID,
            guid = guid,
            systems = {}
        }
        -- 记录缓存创建时间
        State.cacheTime[key] = now
    end

    -- 清除缓存对象的isEmpty和noData标记（如果有）
    State.cache[key].isEmpty = nil
    State.cache[key].noData = nil

    -- 合并数据，确保新数据没有isEmpty标记
    data.isEmpty = nil
    State.cache[key].systems[data.type] = data

    -- 计算并记录响应时间
    local pending = State.pending[key]
    if pending and pending.systems and pending.systems[data.type] then
        local responseTime = now - pending.systems[data.type]

        -- 更新统计信息
        if not State.stats[data.type] then
            State.stats[data.type] = {
                totalQueries = 0,
                totalTime = 0,
                maxTime = 0,
                minTime = 999999
            }
        end
        local stats = State.stats[data.type]
        stats.totalQueries = stats.totalQueries + 1
        stats.totalTime = stats.totalTime + responseTime
        stats.maxTime = math.max(stats.maxTime, responseTime)
        stats.minTime = math.min(stats.minTime, responseTime)

        -- 清除该系统的查询记录
        pending.systems[data.type] = nil
    end

    -- 修复：不要立即清除pending，而是等所有系统完成后再清除
    -- State.pending[key] = nil  -- 旧代码导致超时机制失效

    -- 检查是否所有启用的系统都已有数据
    local allSystemsCached = true
    for systemName, enabled in pairs(DB.systems) do
        if enabled and not State.cache[key].systems[systemName] then
            allSystemsCached = false
            break
        end
    end

    -- 只有当所有系统数据都齐全时才清除pending和冷却期，并更新缓存时间
    if allSystemsCached then
        State.pending[key] = nil
        State.noDataUntil[key] = nil
        State.cacheTime[key] = now  -- 更新缓存时间为所有数据完成的时间
    end

    return key
end

local function GetCachedData(itemID, guid)
    if not itemID or not guid or guid == 0 then return nil end

    local key = MakeKey(itemID, guid, nil, nil, false)
    return State.cache[key]
end

-- 检查是否应该跳过查询
local function ShouldSkipQuery(key)
    local now = GetTime()

    -- 正在查询中（包括短时间内已发送过查询）
    if State.pending[key] then
        return true
    end

    -- 检查最近查询时间（防止短时间内重复查询）
    -- 即使没有缓存，也要等待至少0.5秒才能再次查询
    if State.lastQuery[key] and (now - State.lastQuery[key]) < 0.5 then
        return true
    end

    -- 检查是否所有启用的系统都已有缓存数据
    if State.cache[key] then
        local cached = State.cache[key]

        -- 如果整个缓存对象被标记为isEmpty，清除它并允许重新查询
        if cached.isEmpty then
            State.cache[key] = nil
            State.cacheTime[key] = nil
            return false  -- 允许查询
        end

        local allSystemsCached = true

        -- 检查每个启用的系统是否都有数据
        for systemName, enabled in pairs(DB.systems) do
            if enabled then
                local systemData = cached.systems[systemName]
                -- 只有当系统数据完全不存在时才需要重新查询
                -- isEmpty标记表示服务器确认没有数据，这也是有效的缓存
                if not systemData then
                    allSystemsCached = false
                    break
                end
            end
        end

        -- 只有当所有启用的系统都有数据时才检查缓存时间
        if allSystemsCached then
            local cacheAge = State.cacheTime[key] and (now - State.cacheTime[key]) or 999

            -- 如果缓存超过配置的有效期，清除缓存并允许重新查询
            if cacheAge >= DB.cacheExpiration then
                State.cache[key] = nil
                State.cacheTime[key] = nil
                return false  -- 允许查询
            else
                return true
            end
        end
    end

    -- 在冷却期
    if State.noDataUntil[key] and now < State.noDataUntil[key] then
        return true
    end

    return false
end

-- ============================================================================
-- 查询管理
-- ============================================================================

-- 发送查询（使用Addon消息）
local function DoSendQuery(itemID, guid, key)
    local now = GetTime()

    -- 立即标记为查询中，防止重复发送
    State.queryId = State.queryId + 1
    local queryId = State.queryId
    
    State.lastQuery[key] = now
    State.pending[key] = { 
        started = now, 
        systems = {},
        queryId = queryId,
        queryStartTime = now
    }

    -- 获取已缓存的系统
    local cached = State.cache[key]
    local cachedSystems = {}
    if cached and cached.systems then
        for systemName, _ in pairs(cached.systems) do
            cachedSystems[systemName] = true
        end
    end

    -- 使用Addon消息格式发送查询（避免聊天速率限制）
    -- 重要：消息内容不包含前缀！前缀由SendAddonMessage的第一个参数指定
    -- 发送: SendAddonMessage("UITQ", "QUERY:itemID:guid", ...)
    -- 服务器收到: "UITQ<TAB>QUERY:itemID:guid"
    local addonMessage = string.format("QUERY:%d:%d", itemID, guid)

    -- 标记所有系统为查询中
    for systemName, enabled in pairs(DB.systems) do
        if enabled and not cachedSystems[systemName] then
            State.pending[key].systems[systemName] = now
        end
    end

    -- 统计当前pending查询数量
    local pendingCount = 0
    for _ in pairs(State.pending) do
        pendingCount = pendingCount + 1
    end

    -- 记录发送前的时间
    local sendBeforeTime = GetTime()

    -- 使用SendAddonMessage发送（不受聊天速率限制）
    -- 兼容WoW 3.3.5和零售版API
    local sendSuccess = false
    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
        sendSuccess = true
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
        sendSuccess = true
    end
    
    -- 记录发送后的时间
    local sendAfterTime = GetTime()
    local sendDuration = (sendAfterTime - sendBeforeTime) * 1000  -- 转换为毫秒
    
    -- 输出发送耗时
    if sendDuration > 10 then
        print(string.format("|cff00ffff[统一提示框]|r |cffff8000[Q%d 发送耗时]|r %.0fms |cffff0000(异常)|r",
            queryId, sendDuration))
    end

    -- 如果pending查询过多，显示警告
    if pendingCount > 5 then
        print(string.format("|cff00ffff[统一提示框]|r |cffff0000[警告]|r 当前有 %d 个查询等待响应，可能存在查询堆积", pendingCount))

        -- 显示等待最久的5个查询
        local sortedPending = {}
        for pKey, pData in pairs(State.pending) do
            if pData.queryStartTime then
                table.insert(sortedPending, {
                    key = pKey,
                    queryId = pData.queryId,
                    startTime = pData.queryStartTime,
                    waitTime = now - pData.queryStartTime
                })
            end
        end

        -- 按等待时间排序
        table.sort(sortedPending, function(a, b) return a.waitTime > b.waitTime end)

        -- 显示前5个
        print("  |cffffcc00等待最久的查询:|r")
        for i = 1, math.min(5, #sortedPending) do
            local item = sortedPending[i]
            print(string.format("    Q%s: %s 已等待 %.1fs",
                item.queryId or "?", item.key, item.waitTime))
        end
    end

    -- 设置超时机制
    C_Timer.After(DB.timeout, function()
        if State.pending[key] and State.pending[key].started == now then
            -- 超时了，为所有缺失的系统创建空数据
            local cached = State.cache[key]
            if cached then
                for systemName, enabled in pairs(DB.systems) do
                    if enabled and not cached.systems[systemName] then
                        cached.systems[systemName] = {
                            type = systemName,
                            itemID = itemID,
                            guid = guid,
                            isEmpty = true
                        }
                    end
                end
            else
                State.cache[key] = {
                    itemID = itemID,
                    guid = guid,
                    systems = {},
                    isEmpty = true
                }
            end

            State.pending[key] = nil
            State.noDataUntil[key] = nil
        end
    end)
end

-- 发送查询（直接发送，Addon消息不受速率限制）
local function SendQuery(itemID, guid)
    if not itemID or not guid or guid == 0 then return end

    local key = MakeKey(itemID, guid, nil, nil, false)
    local now = GetTime()

    -- 先检查是否应该跳过
    if ShouldSkipQuery(key) then
        -- 跳过查询（缓存命中或正在查询中）
        -- 如果是因为pending而跳过，且等待时间过长，显示警告
        if State.pending[key] and State.pending[key].queryStartTime then
            local waitTime = now - State.pending[key].queryStartTime
            if waitTime > 3 and (not State.pending[key].warnShown or now - State.pending[key].warnShown > 5) then
                State.pending[key].warnShown = now
                DebugPrint(string.format("[警告] itemID=%d guid=%d 已等待%.1f秒，服务器响应缓慢", 
                    itemID, guid, waitTime))
            end
        end
        return
    end

    -- 直接发送查询（Addon消息不受聊天速率限制）
    DoSendQuery(itemID, guid, key)
end

-- ============================================================================
-- 提示框处理
-- ============================================================================

-- 获取或创建提示框元数据
local function GetTooltipMeta(tooltip, key)
    if not State.tooltips[tooltip] or State.tooltips[tooltip].key ~= key then
        State.tooltips[tooltip] = {
            key = key,
            rendered = {}
        }
    end
    return State.tooltips[tooltip]
end

-- 清除提示框元数据
local function ClearTooltipMeta(tooltip)
    State.tooltips[tooltip] = nil
end

-- 统一渲染所有基础属性（鉴定、强化、成长）
local function RenderUnifiedBaseAttributes(tooltip, cached, meta)
    -- 收集所有基础属性相关的数据
    local identData = cached.systems.identification
    local enhanceData = cached.systems.enhancement
    local growthData = cached.systems.growth

    -- 如果三个系统都没有数据，则不渲染
    if not identData and not enhanceData and not growthData then
        return
    end

    -- 只收集鉴定系统的属性（不混入强化和成长）
    local baseAttributes = {}
    local additionalAttributes = {}

    -- 官方基础属性（如力量/敏捷/智力/耐力/精神），用于和幻境倍率一起显示
    local officialBaseAttributes = {}

    -- 1. 收集鉴定系统的基础属性
    if identData and identData.baseAttributes and #identData.baseAttributes > 0 then
        for _, attr in ipairs(identData.baseAttributes) do
            table.insert(baseAttributes, {
                type = attr.type,
                value = attr.value,
                source = "identification_base"
            })
        end
    end

    -- 2. 收集鉴定系统的追加属性（单独显示）
    if identData and identData.additionalAttributes and #identData.additionalAttributes > 0 then
        for _, attr in ipairs(identData.additionalAttributes) do
            table.insert(additionalAttributes, {
                type = attr.type,
                value = attr.value,
                source = "identification_additional"
            })
        end
    end

    -- 幻境倍率信息（用于显示“原始值 x 幻境倍率 = 最终值”）
    local hjData
    do
        local key = MakeKey(cached.itemID, cached.guid, nil, nil, false)
        hjData = HuanJingGetData and HuanJingGetData(key) or nil
    end

    -- 从提示框元数据中获取官方基础属性（力量/敏捷/智力/耐力/精神），配合幻境倍率展示
    if meta and meta.officialStats and hjData and hjData.multiplier and hjData.multiplier > 1 then
        local stats = meta.officialStats
        local mult = hjData.multiplier

        local baseStatConfig = {
            { key = "ITEM_MOD_STRENGTH_SHORT",  name = _G.ITEM_MOD_STRENGTH_SHORT or "力量" },
            { key = "ITEM_MOD_AGILITY_SHORT",   name = _G.ITEM_MOD_AGILITY_SHORT or "敏捷" },
            { key = "ITEM_MOD_INTELLECT_SHORT", name = _G.ITEM_MOD_INTELLECT_SHORT or "智力" },
            { key = "ITEM_MOD_STAMINA_SHORT",   name = _G.ITEM_MOD_STAMINA_SHORT or "耐力" },
            { key = "ITEM_MOD_SPIRIT_SHORT",    name = _G.ITEM_MOD_SPIRIT_SHORT or "精神" },
        }

        for _, conf in ipairs(baseStatConfig) do
            local amount = stats[conf.key]
            if amount and amount ~= 0 then
                table.insert(officialBaseAttributes, {
                    name = conf.name,
                    value = amount,
                    multiplier = mult
                })
            end
        end
    end

    -- 结合幻境系统数据：按顺序为基础属性和追加属性记录“原值/增强值”
    local hjBaseInfo = {}       -- index -> { original, enhanced }
    local hjAdditionalInfo = {} -- index -> { original, enhanced }

    do
        if hjData and hjData.identificationData and hjData.identificationData ~= "" then
            local hjList = {}

            for attrStr in string.gmatch(hjData.identificationData, "([^,]+)") do
                local parts = {}
                for part in string.gmatch(attrStr, "([^%s]+)") do
                    table.insert(parts, part)
                end

                if #parts >= 3 then
                    local attrTypeNum = tonumber(parts[1])
                    local originalValue = tonumber(parts[2])
                    local enhancedValue = tonumber(parts[3])

                    if attrTypeNum and originalValue and enhancedValue then
                        table.insert(hjList, {
                            type = attrTypeNum,
                            original = originalValue,
                            enhanced = enhancedValue
                        })
                    end
                end
            end

            local baseCount = #baseAttributes

            -- 前 baseCount 条对应基础属性
            for i = 1, baseCount do
                local attr = baseAttributes[i]
                local hj = hjList[i]
                if attr and hj and hj.type == attr.type then
                    hjBaseInfo[i] = { original = hj.original, enhanced = hj.enhanced }
                end
            end

            -- 后面的对应追加属性
            for j = 1, #additionalAttributes do
                local idx = baseCount + j
                local attr = additionalAttributes[j]
                local hj = hjList[idx]
                if attr and hj and hj.type == attr.type then
                    hjAdditionalInfo[j] = { original = hj.original, enhanced = hj.enhanced }
                end
            end
        end
    end

    -- 直接在官方属性之后追加展示鉴定系统的基础属性
    local hasBaseSection = (#officialBaseAttributes > 0) or (#baseAttributes > 0)
    if hasBaseSection then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "基础属性" .. DB.colors.reset)

        -- 先显示官方基础属性（力/敏/智/耐/精），带幻境倍率
        for _, attr in ipairs(officialBaseAttributes) do
            local baseValue = attr.value
            local mult = attr.multiplier
            local enhanced = baseValue * mult

            local leftText = string.format("%d + %s倍率x%d%s %s= %d%s",
                baseValue,
                COLOR_PINK, mult, COLOR_RESET,
                COLOR_RED, enhanced, COLOR_RESET)

            tooltip:AddDoubleLine(
                leftText,
                attr.name,
                0, 1, 0,
                0, 1, 0
            )
        end

        -- 再显示鉴定系统的基础属性
        for index, attr in ipairs(baseAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local hjInfo = hjBaseInfo[index]
            local leftText

            if hjData and hjData.multiplier and hjData.multiplier > 1 and hjInfo then
                local mult = hjData.multiplier
                local original = hjInfo.original or attr.value
                local enhanced = hjInfo.enhanced or (original * mult)
                leftText = string.format("%d + %s倍率x%d%s %s= %d%s",
                    original,
                    COLOR_PINK, mult, COLOR_RESET,
                    COLOR_RED, enhanced, COLOR_RESET)
            else
                leftText = string.format("+%d", attr.value)
            end

            tooltip:AddDoubleLine(
                leftText,
                name,
                0, 1, 0,  -- 左列：绿色
                0, 1, 0   -- 右列：绿色
            )
        end
    end

    -- 在tooltip末尾添加鉴定系统的追加属性（如果有）
    if #additionalAttributes > 0 then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "追加属性" .. DB.colors.reset)

        -- 只显示鉴定系统随机生成的追加属性（不再混入官方基础属性）
        for index, attr in ipairs(additionalAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local hjInfo = hjAdditionalInfo[index]
            local leftText

            if hjData and hjData.multiplier and hjData.multiplier > 1 and hjInfo then
                local mult = hjData.multiplier
                local original = hjInfo.original or attr.value
                local enhanced = hjInfo.enhanced or (original * mult)
                leftText = string.format("%d + %s倍率x%d%s %s= %d%s",
                    original,
                    COLOR_PINK, mult, COLOR_RESET,
                    COLOR_RED, enhanced, COLOR_RESET)
            else
                leftText = string.format("+%d", attr.value)
            end

            tooltip:AddDoubleLine(
                leftText,
                name,
                0, 1, 0,  -- 左列：绿色
                0, 1, 0   -- 右列：绿色
            )
        end
    end

    -- 渲染强化系统的属性（独立区块）
    -- 只要有强化数据（等级或属性），就显示强化区块
    if enhanceData and (
        (enhanceData.level and enhanceData.level > 0) or
        (enhanceData.attributes and #enhanceData.attributes > 0)
    ) then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "追加强化" .. DB.colors.reset)

        if enhanceData.level and enhanceData.level > 0 then
            tooltip:AddLine(string.format("|cff00ff00等级: +%d|r", enhanceData.level))
        end

        if enhanceData.attributes and #enhanceData.attributes > 0 then
            for _, attr in ipairs(enhanceData.attributes) do
                local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
                local leftText

                -- 如果存在幻境倍率，则按“原值 + 倍率xN = 最终值”格式显示
                if hjData and hjData.multiplier and hjData.multiplier > 1 then
                    local mult = hjData.multiplier
                    local baseValue = attr.value or 0
                    local finalValue = math.floor(baseValue * mult + 0.5)

                    leftText = string.format("%d + %s倍率x%d%s %s= %d%s",
                        baseValue,
                        COLOR_PINK, mult, COLOR_RESET,
                        COLOR_RED, finalValue, COLOR_RESET)
                else
                    leftText = string.format("+%d", attr.value)
                end

                tooltip:AddDoubleLine(
                    leftText,
                    name,
                    0, 1, 0,  -- 左列：绿色
                    0, 1, 0   -- 右列：绿色
                )
            end
        end
    end

    -- 渲染成长系统的属性（独立区块）
    -- 只要有成长数据（等级、经验或属性），就显示成长区块
    if growthData and (
        (growthData.level and growthData.level > 0) or
        (growthData.attributes and #growthData.attributes > 0)
    ) then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "追加成长" .. DB.colors.reset)

        if growthData.level and growthData.level > 0 then
            tooltip:AddLine(string.format("|cff00ff00等级: %d|r", growthData.level))
        end

        if growthData.currentExp and growthData.requiredExp and growthData.requiredExp > 0 then
            tooltip:AddLine(string.format("|cff00ff00经验: %d/%d|r",
                growthData.currentExp, growthData.requiredExp))
        end

        if growthData.attributes and #growthData.attributes > 0 then
            for _, attr in ipairs(growthData.attributes) do
                local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
                local leftText

                -- 成长属性同样应用幻境倍率显示
                if hjData and hjData.multiplier and hjData.multiplier > 1 then
                    local mult = hjData.multiplier
                    local baseValue = attr.value or 0
                    local finalValue = math.floor(baseValue * mult + 0.5)

                    leftText = string.format("%d + %s倍率x%d%s %s= %d%s",
                        baseValue,
                        COLOR_PINK, mult, COLOR_RESET,
                        COLOR_RED, finalValue, COLOR_RESET)
                else
                    leftText = string.format("+%d", attr.value)
                end

                tooltip:AddDoubleLine(
                    leftText,
                    name,
                    0, 1, 0,  -- 左列：绿色
                    0, 1, 0   -- 右列：绿色
                )
            end
        end
    end

    -- 标记这些系统已渲染
    meta.rendered.identification = true
    meta.rendered.enhancement = true
    meta.rendered.growth = true

    tooltip:Show()
end

-- 渲染提示框
RenderTooltip = function(tooltip, itemID, guid)
    if not itemID or not guid or guid == 0 then return end

    local key = MakeKey(itemID, guid, nil, nil, false)
    local meta = GetTooltipMeta(tooltip, key)
    local cached = GetCachedData(itemID, guid)

    local now = GetTime()

    -- 幻境系统查询：通过系统消息单独获取倍率和增强后的属性
    HuanJingRequest(itemID, guid, key, now)

    -- 只在没有完整缓存时才发送查询
    local hasCompleteCache = false
    if cached and cached.systems then
        hasCompleteCache = true
        -- 检查是否所有启用的系统都有数据（包括isEmpty标记）
        for systemName, enabled in pairs(DB.systems) do
            if enabled and not cached.systems[systemName] then
                hasCompleteCache = false
                break
            end
        end
    end

    -- 如果没有完整缓存，尝试发送查询（SendQuery内部会检查是否需要查询）
    if not hasCompleteCache then
        SendQuery(itemID, guid)
    end

    -- 检查是否正在查询中
    local isPending = State.pending[key] ~= nil

    if not cached then
        -- 完全没有缓存
        if isPending then
            -- 正在查询中，显示等待时间
            local waitTime = 0
            if State.pending[key] and State.pending[key].queryStartTime then
                waitTime = now - State.pending[key].queryStartTime
            end
            tooltip:AddLine(" ")
            if waitTime > 2 then
                -- 超过2秒，显示警告
                tooltip:AddLine(string.format("%s正在加载属性... (已等待 %.1f秒)%s", 
                    DB.colors.special, waitTime, DB.colors.reset))
            else
                tooltip:AddLine(DB.colors.special .. "正在加载属性..." .. DB.colors.reset)
            end
            tooltip:Show()
            return
        else
            -- 不在查询中，也没有缓存（可能是GUID无效或其他问题）
            -- 不显示任何额外信息，使用默认tooltip
            return
        end
    end

    -- 如果缓存标记为无数据，直接返回不显示任何额外内容
    if cached.noData then
        return
    end

    -- 1. 先统一渲染所有基础属性（鉴定、强化、成长）
    if not meta.rendered.identification and not meta.rendered.enhancement and not meta.rendered.growth then
        RenderUnifiedBaseAttributes(tooltip, cached, meta)
    end

    -- 2. 然后按顺序渲染其他系统
    local renderOrder = {
        "skills",          -- 追加技能
        "runes",           -- 符文系统（宝石插槽）
        "sets",            -- 套装系统
        "magic"            -- 魔次属性
    }

    for _, systemType in ipairs(renderOrder) do
        if DB.systems[systemType] and cached.systems[systemType] then
            -- 跳过标记为isEmpty的空数据
            if not cached.systems[systemType].isEmpty then
                if not meta.rendered[systemType] then
                    local rendererName = systemType:sub(1,1):upper() .. systemType:sub(2)
                    local renderer = Renderers[rendererName]
                    if renderer then
                        renderer(tooltip, cached.systems[systemType])
                        meta.rendered[systemType] = true
                    end
                end
            end
        end
    end

    tooltip:Show()
end


-- ============================================================================
-- 事件处理
-- ============================================================================

local EventFrame = CreateFrame("Frame")

-- 前向声明（函数定义在后面）
local ProcessServerResponse

-- 处理Addon消息
local function OnAddonMessage(self, event, prefix, message, channel, sender)
    local now = GetTime()

    -- 修复：处理服务器可能发送的包含竖线的前缀
    -- 服务器可能发送 "ITEMENHANCE|LOGIN_COMPLETED" 这样的格式
    -- 我们需要分离出实际的前缀和消息内容
    local actualPrefix = prefix
    local actualMessage = message
    
    -- 如果prefix包含竖线，分离出真正的前缀和消息
    if prefix:find("|") then
        local parts = { strsplit("|", prefix) }
        actualPrefix = parts[1]
        -- 将剩余部分重新组合到消息中
        if #parts > 1 then
            local extraContent = table.concat(parts, "|", 2)
            actualMessage = extraContent .. (message or "")
        end
    end

    -- 只处理我们自己的前缀（包括主前缀和备用前缀）
    if actualPrefix ~= ADDON_PREFIX and actualPrefix ~= ADDON_PREFIX_ALT then
        return
    end

    -- 只处理来自自己的消息
    if sender ~= UnitName("player") then
        return
    end
    
    -- 使用分离后的消息继续处理
    message = actualMessage
    
    local receiveTime = GetTime()
    
    -- 修复：忽略自己发出的查询消息（格式：QUERY:itemID:guid）
    if message:match("^QUERY:") then
        return
    end

    -- 过滤掉服务器事件通知（非数据响应）
    if message:match("LOGIN_COMPLETE") or message:match("LOGIN_SUCCESS") or
       message:match("LOGOUT") or message == "" or message:len() < 5 then
        return
    end

    -- 优先处理幻境系统的Addon响应
    if message:match("^HUANJING_DATA:") or message:match("^RESPONSE:HUANJING_DATA:") then
        local dataMessage = message:gsub("^RESPONSE:", "")
        HuanJingHandleAddonMessage(dataMessage)
        return
    end

    -- 检查是否是ALL_MODULE_DATA响应
    if not message:match("ALL_MODULE_DATA:") then
        return
    end

    -- 移除RESPONSE:前缀（如果有）
    local dataMessage = message:gsub("^RESPONSE:", "")
    
    -- 使用pcall捕获错误
    local success, err = pcall(function()
        ProcessServerResponse(dataMessage, receiveTime)
    end)
    
    if not success then
        print("|cffff0000[统一提示框错误]|r ProcessServerResponse失败:", err)
    end
end

-- 处理系统消息（目前用于接收幻境系统的倍率与属性响应）
local function OnSystemMessage(self, event, message)
    HuanJingHandleSystemMessage(message)
end

-- 统一的服务器响应处理函数（实现前面声明的函数）
ProcessServerResponse = function(message, receiveTime)
    -- 优先尝试批量数据解析器
    local batchData = Parsers.BatchQuery(message)

    if not batchData then
        return
    end

    if batchData then
        local key = MakeKey(batchData.itemID, batchData.guid, nil, nil, false)
        
        -- 性能监控：计算从发送到接收的延迟
        local now = GetTime()
        local pending = State.pending[key]
        if pending and pending.queryStartTime then
            local rtt = (now - pending.queryStartTime) * 1000  -- 转换为毫秒
            local queryId = pending.queryId or "?"
            
            -- 更详细的时间信息（已关闭控制台日志输出）
            -- print(string.format("|cff00ffff[统一提示框]|r |cffff8000[Q%s←]|r itemID=%d guid=%d RTT=%.0fms |cffaaaaaa[接收时间 %.3fs]|r", 
            --     queryId, batchData.itemID, batchData.guid, rtt, now))
            
            -- 如果RTT超过3秒，显示详细分析（已关闭）
            if rtt > 3000 then
                -- print(string.format("  |cffff0000[延迟分析]|r 发送时间: %.3fs, 接收时间: %.3fs, 延迟: %.3fs", 
                --     pending.queryStartTime, now, (now - pending.queryStartTime)))
                -- print(string.format("  |cffff0000[警告]|r 这可能是服务器处理慢或网络延迟导致"))
            end
        else
            -- 没有pending记录，说明可能是重复消息或异常情况（已关闭日志）
            -- print(string.format("|cff00ffff[统一提示框]|r |cffff8000[Q?←]|r itemID=%d guid=%d |cffff0000(无pending记录)|r", 
            --     batchData.itemID, batchData.guid))
        end

        local hasAnyData = false

        -- 先确保缓存对象存在
        if not State.cache[key] then
            State.cache[key] = {
                itemID = batchData.itemID,
                guid = batchData.guid,
                systems = {}
            }
            State.cacheTime[key] = GetTime()
        end

        -- 将批量数据拆分并缓存到各个系统
        for systemName, systemData in pairs(batchData.systems) do
            hasAnyData = true
            CacheData(batchData.itemID, batchData.guid, systemData)
        end

        -- 为所有启用但没有数据的系统创建空标记，确保pending可以被清除
        for systemName, enabled in pairs(DB.systems) do
            if enabled and not State.cache[key].systems[systemName] then
                State.cache[key].systems[systemName] = {
                    type = systemName,
                    itemID = batchData.itemID,
                    guid = batchData.guid,
                    isEmpty = true  -- 标记为空数据
                }
            end
        end

        -- 批量查询完成后，强制清除pending状态（因为服务器已经返回了所有数据）
        State.pending[key] = nil
        State.noDataUntil[key] = nil

        -- 如果完全没有数据，标记整个缓存
        if not hasAnyData then
            State.cache[key].noData = true
        end

        -- 更新所有相关的提示框（仅限真正的 GameTooltip/ShoppingTooltip 等）
        local renderCount = 0
        for tooltip, meta in pairs(State.tooltips) do
            if not tooltip.UIT_IsUnifiedFrame and tooltip:IsShown() and meta.key == key then
                RenderTooltip(tooltip, batchData.itemID, batchData.guid)
                renderCount = renderCount + 1
            end
        end

        -- 同步刷新所有统一四联大框（是否显示由 RefreshUnifiedFrame 自己决定）
        local frames = UnifiedFramesByKey[key]
        if frames then
            for _, frame in ipairs(frames) do
                RefreshUnifiedFrame(frame, batchData.itemID, batchData.guid)
                renderCount = renderCount + 1
            end
        end

        -- 如果没有渲染目标，显示警告
        if renderCount == 0 and pending then
            local queryId = pending.queryId or "?"
            DebugPrint(string.format("[警告] Q%s 收到数据但无渲染目标（用户可能已移开鼠标）", queryId))
        end

        return
    end
end

-- 注册Addon消息前缀（兼容WoW 3.3.5）
if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
    RegisterAddonMessagePrefix(ADDON_PREFIX_ALT)  -- 注册备用前缀
    DebugPrint("[初始化] 已注册前缀:", ADDON_PREFIX, "和", ADDON_PREFIX_ALT)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX_ALT)  -- 注册备用前缀
    DebugPrint("[初始化] 已注册前缀:", ADDON_PREFIX, "和", ADDON_PREFIX_ALT)
end

-- 注册Addon消息事件
EventFrame:RegisterEvent("CHAT_MSG_ADDON")
EventFrame:RegisterEvent("CHAT_MSG_SYSTEM")

EventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        OnAddonMessage(self, event, ...)
    elseif event == "CHAT_MSG_SYSTEM" then
        OnSystemMessage(self, event, ...)
    end
end)


-- ============================================================================
-- Tooltip Hook
-- ============================================================================

-- ============================================================================
-- 四联提示框系统函数（必须在OnTooltipSetItem之前定义）
-- ============================================================================

-- 创建一个统一的大框，内部分2列
local function GetUnifiedTooltipFrame(ownerTooltip)
    if not ownerTooltip or not ownerTooltip.GetName then
        print("|cFFFF0000[错误]|r ownerTooltip无效")
        return nil
    end

    local baseName = ownerTooltip:GetName() or "UnifiedItemTooltip"

    -- 如果已经创建过，直接返回
    if ownerTooltip.UIT_UnifiedFrame then
        return ownerTooltip.UIT_UnifiedFrame
    end

    -- 创建主框架
    local mainFrame = CreateFrame("Frame", baseName .. "_UnifiedFrame", UIParent)
    mainFrame:SetFrameStrata("TOOLTIP")
    mainFrame:SetFrameLevel(100)  -- 确保在最上层
    mainFrame.UIT_IsUnifiedFrame = true

    -- 设置背景
    mainFrame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 }
    })
    mainFrame:SetBackdropColor(0, 0, 0, 0.8)  -- 半透明黑色背景，提升可读性
    mainFrame:SetBackdropBorderColor(1, 0.8, 0, 0)  -- 金色边框完全透明

    -- 根据屏幕宽度自适应总宽，以 2560x1440 分辨率下两列布局为基准
    local baselineScreenWidth = 2560
    local baselineTotalWidth = 600

    local screenWidth = GetScreenWidth and GetScreenWidth() or baselineScreenWidth
    if not screenWidth or screenWidth <= 0 then
        screenWidth = baselineScreenWidth
    end

    -- 按屏幕宽度比例缩放总宽，在 1920 下仅轻微缩小，在更高分辨率下略放大，限定缩放范围
    local widthScale = screenWidth / baselineScreenWidth
    if widthScale < 0.95 then
        widthScale = 0.95
    elseif widthScale > 1.25 then
        widthScale = 1.25
    end

    -- 两列布局：先计算基准内部宽度，再在列宽基础上放大 30%
    local columnSpacing = 10
    local sidePadding = 20

    local baseInsideWidth = baselineTotalWidth - sidePadding - columnSpacing
    local baseColumnWidth = baseInsideWidth / 2

    -- 在基准两列宽度基础上再放大 30%，并按分辨率缩放
    local columnWidth = baseColumnWidth * 1.3 * widthScale

    local totalWidth = columnWidth * 2 + sidePadding + columnSpacing

    mainFrame:SetWidth(totalWidth)
    mainFrame:SetHeight(800)

    -- 创建2个内容区域
    local columns = {}

    for i = 1, 2 do
        local column = CreateFrame("Frame", baseName .. "_Column" .. i, mainFrame)
        column:SetWidth(columnWidth)
        column:SetHeight(1150)
        column:SetFrameLevel(mainFrame:GetFrameLevel() + 1)  -- 确保在主框架之上

        -- 设置列的背景，���每列可见
        column:SetBackdrop({
            bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
            edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
            tile = false,
            tileSize = 16,
            edgeSize = 1,
            insets = { left = 1, right = 1, top = 1, bottom = 1 }
        })

        -- 每列不同的背景色
        if i == 1 then
            column:SetPoint("TOPLEFT", mainFrame, "TOPLEFT", 10, -10)
            column:SetBackdropColor(0.1, 0.1, 0.2, 0)  -- 深蓝色，背景完全透明
            column:SetBackdropBorderColor(0.3, 0.3, 0.5, 0)  -- 边框完全透明
        elseif i == 2 then
            column:SetPoint("TOPLEFT", columns[i-1], "TOPRIGHT", columnSpacing, 0)
            column:SetBackdropColor(0.1, 0.2, 0.1, 0)  -- 深绿色，背景完全透明
            column:SetBackdropBorderColor(0.3, 0.5, 0.3, 0)  -- 边框完全透明
        end

        -- 所有列都创建内容容器
        local content = CreateFrame("Frame", nil, column)
        content:SetWidth(columnWidth)
        content:SetAllPoints(column)  -- 填充整个列
        content:SetFrameLevel(column:GetFrameLevel() + 1)
        column.content = content

        -- 创建文本显示区域
        local textFrame = CreateFrame("Frame", nil, content)
        textFrame:SetWidth(columnWidth - 10)
        textFrame:SetPoint("TOPLEFT", content, "TOPLEFT", 5, -5)
        textFrame:SetFrameLevel(content:GetFrameLevel() + 1)
        column.textFrame = textFrame

        -- 存储文本行
        column.lines = {}
        column.currentY = 0

        columns[i] = column
    end

    mainFrame.columns = columns

    -- 为列创建适配器，以便复用现有 tooltip 渲染函数
    local function CreateColumnTooltipAdapter(frame, columnIndex)
        local adapter = {}

        function adapter:AddLine(text, r, g, b)
            frame:AddLineToColumn(columnIndex, text or "", r, g, b)
        end

        function adapter:AddDoubleLine(left, right, lr, lg, lb, rr, rg, rb)
            local combined
            if right and right ~= "" then
                combined = (left or "") .. "  " .. right
            else
                combined = left or ""
            end
            frame:AddLineToColumn(columnIndex, combined, lr, lg, lb)
        end

        function adapter:Show()
            -- 统一大框本身负责显示
        end

        return adapter
    end

    mainFrame.ColumnTooltips = {
        [1] = CreateColumnTooltipAdapter(mainFrame, 1),
        [2] = CreateColumnTooltipAdapter(mainFrame, 2),
    }

    -- 添加辅助方法：向指定列添加文本
    mainFrame.AddLineToColumn = function(self, columnIndex, text, r, g, b)
        if columnIndex < 1 or columnIndex > 2 then return end

        local column = self.columns[columnIndex]

        -- 创建FontString（直接使用column作为父框架）
        local fontString = column:CreateFontString(nil, "OVERLAY")
        if not fontString then
            return
        end

        -- 设置字体（带备用字体列表，按分辨率缩放字号）
        local baselineScreenWidth = 2560
        local baselineFontSize = 14
        local minFontSize = 11
        local maxFontSize = 14

        local screenWidth = GetScreenWidth and GetScreenWidth() or baselineScreenWidth
        if not screenWidth or screenWidth <= 0 then
            screenWidth = baselineScreenWidth
        end

        local fontScale = screenWidth / baselineScreenWidth
        local fontSize = baselineFontSize * fontScale
        if fontSize < minFontSize then
            fontSize = minFontSize
        elseif fontSize > maxFontSize then
            fontSize = maxFontSize
        end

        local fontSet = false
        local fonts = {
            "Fonts\\ARKai_T.ttf",           -- 主字体
            "Fonts\\ARKai_C.ttf",           -- 备用中文字体1
            "Fonts\\ZYKai_T.ttf",           -- 备用中文字体2
            "Fonts\\ZYHei.ttf",             -- 备用中文字体3
            "Fonts\\FRIZQT__.TTF",          -- 游戏默认字体
        }

        for _, fontPath in ipairs(fonts) do
            local ok = fontString:SetFont(fontPath, fontSize, "OUTLINE")
            if ok then
                fontSet = true
                break
            end
        end

        if not fontSet then
            fontString:SetFont("Fonts\\FRIZQT__.TTF", fontSize)
        end

        fontString:SetPoint("TOPLEFT", column, "TOPLEFT", 10, -10 - column.currentY)
        fontString:SetText(text or "")

        if r and g and b then
            fontString:SetTextColor(r, g, b, 1)
        else
            fontString:SetTextColor(1, 1, 1, 1)
        end

        fontString:SetWidth(columnWidth - 20)
        fontString:SetJustifyH("LEFT")
        fontString:SetWordWrap(true)
        fontString:SetDrawLayer("OVERLAY", 7)
        fontString:Show()

        table.insert(column.lines, fontString)

        local height = fontString:GetStringHeight() or fontSize
        column.currentY = column.currentY + height + 2
    end

    -- 清空指定列的内容
    mainFrame.ClearColumn = function(self, columnIndex)
        if columnIndex < 1 or columnIndex > 2 then return end

        local column = self.columns[columnIndex]
        for _, line in ipairs(column.lines) do
            line:Hide()
            line:SetText("")
        end
        column.lines = {}
        column.currentY = 0
    end

    -- 复制官方tooltip内容到第1列
    mainFrame.CopyOfficialTooltip = function(self, officialTooltip)
        self:ClearColumn(1)

        local tooltipName = officialTooltip:GetName()
        if not tooltipName then
            return
        end

        local numLines = officialTooltip:NumLines()

        -- 遍历所有行，复制文本和颜色
        for i = 1, numLines do
            local leftText = _G[tooltipName .. "TextLeft" .. i]
            local rightText = _G[tooltipName .. "TextRight" .. i]

            if leftText then
                local text = leftText:GetText()
                if text and text ~= "" then
                    local r, g, b = leftText:GetTextColor()

                    -- 如果有右侧文本，合并显示
                    if rightText then
                        local rightStr = rightText:GetText()
                        if rightStr and rightStr ~= "" then
                            text = text .. "  " .. rightStr
                        end
                    end

                    self:AddLineToColumn(1, text, r, g, b)
                end
            end
        end
    end

    -- 保存引用
    ownerTooltip.UIT_UnifiedFrame = mainFrame
    mainFrame.UIT_OwnerTooltip = ownerTooltip
    return mainFrame
end

-- 根据官方 tooltip 位置，将统一大框智能停靠在其左右侧，避免超出屏幕
local function AnchorUnifiedFrame(unifiedFrame, officialTooltip)
    if not unifiedFrame then return end

    unifiedFrame:ClearAllPoints()

    -- 如果没有有效的官方 tooltip，则兜底放在屏幕中上方
    if not officialTooltip or not officialTooltip.GetLeft then
        unifiedFrame:SetPoint("TOP", UIParent, "TOP", 0, -80)
        return
    end

    local tipLeft  = officialTooltip:GetLeft()
    local tipRight = officialTooltip:GetRight()
    local tipTop   = officialTooltip:GetTop()

    if not tipLeft or not tipRight or not tipTop then
        unifiedFrame:SetPoint("TOP", UIParent, "TOP", 0, -80)
        return
    end

    local screenWidth  = GetScreenWidth and GetScreenWidth() or 0
    local screenHeight = GetScreenHeight and GetScreenHeight() or 0
    local frameWidth   = unifiedFrame:GetWidth() or 0

    -- 官方提示框与大框之间的水平间距（越小越紧凑）
    local gap = 2

    if unifiedFrame.SetClampedToScreen then
        unifiedFrame:SetClampedToScreen(true)
    end
    if officialTooltip.SetClampedToScreen then
        officialTooltip:SetClampedToScreen(true)
    end

    -- 默认：tooltip 在屏幕左半边 → 大框放右侧；在右半边 → 放左侧
    local placeOnRight = true
    if screenWidth > 0 then
        local tipCenterX = (tipLeft + tipRight) / 2
        if tipCenterX > screenWidth / 2 then
            placeOnRight = false
        end
    end

    -- 如果右侧空间不足，则强制放左侧
    if placeOnRight and screenWidth > 0 then
        if tipRight + gap + frameWidth > screenWidth then
            placeOnRight = false
        end
    end

    -- 如果左侧空间不足，则强制放右侧
    if not placeOnRight and screenWidth > 0 then
        if tipLeft - gap - frameWidth < 0 then
            placeOnRight = true
        end
    end

    if placeOnRight then
        unifiedFrame:SetPoint("TOPLEFT", officialTooltip, "TOPRIGHT", gap, 0)
    else
        unifiedFrame:SetPoint("TOPRIGHT", officialTooltip, "TOPLEFT", -gap, 0)
    end
end

-- 刷新统一四联大框中的自定义属性/技能/符文等数据
RefreshUnifiedFrame = function(unifiedFrame, itemID, guid)
    if not unifiedFrame or not unifiedFrame.columns then return end

    local cached = GetCachedData(itemID, guid)

    -- 没有缓存或被标记无数据：不显示大框，保留官方提示框
    if not cached or not cached.systems or cached.noData then
        unifiedFrame:Hide()
        if unifiedFrame.UIT_OwnerTooltip and unifiedFrame.UIT_OwnerTooltip.SetAlpha then
            unifiedFrame.UIT_OwnerTooltip:SetAlpha(1)
        end
        return
    end

    local systems = cached.systems

    -- 检查是否至少有一个系统有有效数据（不是 isEmpty 占位）
    local function hasNonEmpty(system)
        return system and not system.isEmpty
    end

    local hasData = hasNonEmpty(systems.identification)
        or hasNonEmpty(systems.enhancement)
        or hasNonEmpty(systems.growth)
        or hasNonEmpty(systems.magic)
        or hasNonEmpty(systems.skills)
        or hasNonEmpty(systems.sets)
        or hasNonEmpty(systems.runes)

    if not hasData then
        unifiedFrame:Hide()
        if unifiedFrame.UIT_OwnerTooltip and unifiedFrame.UIT_OwnerTooltip.SetAlpha then
            unifiedFrame.UIT_OwnerTooltip:SetAlpha(1)
        end
        return
    end

    -- 现在两列全部用于自定义内容
    unifiedFrame:ClearColumn(1)
    unifiedFrame:ClearColumn(2)

    unifiedFrame:AddLineToColumn(1, "|cFFFFD700基础/追加/成长/强化|r", 1, 0.84, 0)
    unifiedFrame:AddLineToColumn(2, "|cFFFFD700技能效果（魔次/技能/套装/符文）|r", 1, 0.84, 0)

    -- 为当前大框构造/重置渲染元数据（用于幻境倍率等逻辑）
    local key = MakeKey(itemID, guid, nil, nil, false)
    local meta = State.tooltips[unifiedFrame] or { key = key, rendered = {} }
    meta.key = key
    meta.rendered = {}
    State.tooltips[unifiedFrame] = meta

    -- 第1列：基础属性 / 追加属性 / 成长属性 / 强化属性
    if systems.identification or systems.enhancement or systems.growth then
        RenderUnifiedBaseAttributes(unifiedFrame.ColumnTooltips[1], cached, meta)
    end

    -- 第2列：技能效果（魔次属性 / 追加技能 / 追加套装 / 符文系统）
    if DB.systems.magic and systems.magic and not systems.magic.isEmpty then
        Renderers.Magic(unifiedFrame.ColumnTooltips[2], systems.magic)
    end

    if DB.systems.skills and systems.skills and not systems.skills.isEmpty then
        Renderers.Skills(unifiedFrame.ColumnTooltips[2], systems.skills)
    end

    if DB.systems.sets and systems.sets and not systems.sets.isEmpty then
        Renderers.Sets(unifiedFrame.ColumnTooltips[2], systems.sets)
    end

    if DB.systems.runes and systems.runes and not systems.runes.isEmpty then
        Renderers.Runes(unifiedFrame.ColumnTooltips[2], systems.runes)
    end

    -- 渲染完成后显示大框，官方 tooltip 保持可见，由 AnchorUnifiedFrame 决定左右停靠
    unifiedFrame:Show()

    -- 将官方提示框的高度拉到与大框一致，保持整体视觉统一
    if unifiedFrame.UIT_OwnerTooltip and unifiedFrame.UIT_OwnerTooltip.SetHeight then
        local owner = unifiedFrame.UIT_OwnerTooltip
        local frameHeight = unifiedFrame:GetHeight() or 0
        local tipHeight   = owner:GetHeight() or 0

        if frameHeight > 0 and tipHeight < frameHeight then
            owner:SetHeight(frameHeight)
        end
    end
end

-- ============================================================================
-- 旧的tooltip辅助函数（将被逐步替换）
-- ============================================================================

-- 为任意物品提示框创建/获取一个"自定义属性提示框"
local function GetExtraTooltip(ownerTooltip)
    if not ownerTooltip or not ownerTooltip.GetName then
        return nil
    end

    if ownertooltip.UIT_Tooltip2 then
        return ownertooltip.UIT_Tooltip2
    end

    local baseName = ownerTooltip:GetName() or "UnifiedItemTooltip"
    local extraName = baseName .. "_UnifiedExtra"

    local extra = CreateFrame("GameTooltip", extraName, UIParent, "GameTooltipTemplate")
    extra:SetFrameStrata(ownerTooltip:GetFrameStrata())
    extra:SetScale(ownerTooltip:GetScale())

    ownertooltip.UIT_Tooltip2 = extra
    return extra
end

-- 判断一个提示框是否源自聊天框/聊天链接
local function IsTooltipFromChat(tooltip)
	if not tooltip or not tooltip.GetOwner then return false end

	local current = tooltip
	local depth = 0
	while current and depth < 5 do
		local name = current.GetName and current:GetName()
		if name and name:match("^ChatFrame%d+") then
			return true
		end
		if _G.ItemRefTooltip and current == _G.ItemRefTooltip then
			return true
		end
		if not current.GetOwner then break end
		current = current:GetOwner()
		depth = depth + 1
	end

	return false
end

-- 根据提示框类型决定自定义属性提示框的停靠方向
local function AnchorExtraTooltip(extra, ownerTooltip)
	extra:SetOwner(ownerTooltip, "ANCHOR_NONE")
	extra:ClearAllPoints()

	local name = ownerTooltip:GetName() or ""
	local isChatTooltip = IsTooltipFromChat(ownerTooltip)

	-- 通过 owner 链判断是否为"角色/检查装备栏"环境
	local isCharacterEquip = false
	local current = ownerTooltip
	local depth = 0
	while current and depth < 5 do
		local ownerName = current.GetName and current:GetName() or ""
		if ownerName ~= "" and (ownerName:match("^Character") or ownerName:match("PaperDoll") or ownerName:match("^Inspect")) then
			isCharacterEquip = true
			break
		end
		if not current.GetOwner then break end
		current = current:GetOwner()
		depth = depth + 1
	end

	-- 统一使用紧贴模式，间距设为2像素（避免完全重叠但又不会有明显缝隙）
	local gap = 2

	extra.UIT_IsCharacterEquip = isCharacterEquip
	extra.UIT_FixedCenter = false
	extra.UIT_Gap = gap

	DebugPrint("AnchorExtraTooltip", "name=", name, "isCharacterEquip=", isCharacterEquip, "gap=", gap)

	if name == "ShoppingTooltip1" then
		-- 第一个对比框：自定义提示框放在其左侧
		extra:SetPoint("TOPRIGHT", ownerTooltip, "TOPLEFT", -gap, 0)
	elseif name == "ShoppingTooltip2" then
		-- 第二个对比框：自定义提示框放在其右侧
		extra:SetPoint("TOPLEFT", ownerTooltip, "TOPRIGHT", gap, 0)
	else
		-- 所有其他情况（背包、角色装备、聊天等）：自定义提示框挂在官方提示框右侧
		extra:SetPoint("TOPLEFT", ownerTooltip, "TOPRIGHT", gap, 0)
	end
end

local function FixExtraTooltipOffscreen(extra, ownerTooltip)
	if not extra or not extra:IsShown() or not ownerTooltip or not ownerTooltip:IsShown() then 
		return 
	end

	local ownerLeft = ownerTooltip:GetLeft()
	local ownerRight = ownerTooltip:GetRight()
	local ownerTop = ownerTooltip:GetTop()
	local ownerWidth = ownerTooltip:GetWidth()
	
	if not ownerLeft or not ownerRight or not ownerTop or not ownerWidth then 
		return 
	end

	local screenWidth = GetScreenWidth() or 0
	local screenHeight = GetScreenHeight() or 0
	if screenWidth <= 0 or screenHeight <= 0 then 
		return 
	end

	local extraWidth = extra:GetWidth() or 0
	local gap = extra.UIT_Gap or 2
	
	-- 启用边界限制
	if extra.SetClampedToScreen then
		extra:SetClampedToScreen(true)
	end
	if ownerTooltip.SetClampedToScreen then
		ownerTooltip:SetClampedToScreen(true)
	end

	-- 计算自定义tooltip在官方tooltip右侧时的总宽度
	local totalWidth = ownerWidth + gap + extraWidth
	
	-- 如果总宽度超出屏幕，需要调整
	if totalWidth > screenWidth then
		DebugPrint("FixExtraTooltipOffscreen", "总宽度超出屏幕", "totalWidth=", totalWidth, "screenWidth=", screenWidth)
		
		-- 计算屏幕中央位置，使两个tooltip居中显示
		local centerX = screenWidth / 2
		local startX = centerX - totalWidth / 2
		
		-- 确保不会超出左边界
		if startX < 0 then
			startX = 10  -- 留出一点边距
		end
		
		-- 移动官方tooltip到新位置
		ownerTooltip:ClearAllPoints()
		ownerTooltip:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", startX, ownerTop)
		
		-- 自定义tooltip挂在官方tooltip右侧
		extra:ClearAllPoints()
		extra:SetPoint("TOPLEFT", ownerTooltip, "TOPRIGHT", gap, 0)
		
		DebugPrint("FixExtraTooltipOffscreen", "已调整到中央", "startX=", startX)
		return
	end
	
	-- 如果官方tooltip在右侧会导致自定义tooltip超出屏幕
	if ownerRight + gap + extraWidth > screenWidth then
		DebugPrint("FixExtraTooltipOffscreen", "右侧超出", "ownerRight=", ownerRight, "需要=", ownerRight + gap + extraWidth)
		
		-- 计算需要向左移动的距离
		local overflow = (ownerRight + gap + extraWidth) - screenWidth + 10  -- 多留10像素边距
		local newLeft = ownerLeft - overflow
		
		-- 确保不会超出左边界
		if newLeft < 10 then
			-- 如果向左移动还是不够，那就居中显示
			local centerX = screenWidth / 2
			newLeft = centerX - totalWidth / 2
			if newLeft < 10 then
				newLeft = 10
			end
		end
		
		-- 移动官方tooltip
		ownerTooltip:ClearAllPoints()
		ownerTooltip:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", newLeft, ownerTop)
		
		-- 自定义tooltip挂在官方tooltip右侧
		extra:ClearAllPoints()
		extra:SetPoint("TOPLEFT", ownerTooltip, "TOPRIGHT", gap, 0)
		
		DebugPrint("FixExtraTooltipOffscreen", "已向左移动", "newLeft=", newLeft, "overflow=", overflow)
		return
	end
	
	-- 如果官方tooltip太靠左，确保有足够空间
	if ownerLeft < 10 then
		ownerTooltip:ClearAllPoints()
		ownerTooltip:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", 10, ownerTop)
		
		extra:ClearAllPoints()
		extra:SetPoint("TOPLEFT", ownerTooltip, "TOPRIGHT", gap, 0)
		
		DebugPrint("FixExtraTooltipOffscreen", "调整左边距")
	end
end

local function OnTooltipSetItem(tooltip)
	-- 从聊天框悬停/点击时，屏蔽"当前装备"对比
	local tName = tooltip.GetName and tooltip:GetName() or ""

	-- 1) 如果是聊天里点击出来的 ItemRefTooltip：
	--    强制隐藏官方对比框 + 清理 tooltip 自身可能残留的"当前装备"文字
	if tName == "ItemRefTooltip" and IsTooltipFromChat(tooltip) then
		if ShoppingTooltip1 then
			ShoppingTooltip1:Hide()
			if ShoppingTooltip1.UIT_Tooltip2 then
				ShoppingTooltip1.UIT_Tooltip2:Hide()
				ClearTooltipMeta(ShoppingTooltip1.UIT_Tooltip2)
			end
			if ShoppingTooltip1.UIT_Tooltip3 then
				ShoppingTooltip1.UIT_Tooltip3:Hide()
				ClearTooltipMeta(ShoppingTooltip1.UIT_Tooltip3)
			end
			if ShoppingTooltip1.UIT_Tooltip4 then
				ShoppingTooltip1.UIT_Tooltip4:Hide()
				ClearTooltipMeta(ShoppingTooltip1.UIT_Tooltip4)
			end
		end
		if ShoppingTooltip2 then
			ShoppingTooltip2:Hide()
			if ShoppingTooltip2.UIT_Tooltip2 then
				ShoppingTooltip2.UIT_Tooltip2:Hide()
				ClearTooltipMeta(ShoppingTooltip2.UIT_Tooltip2)
			end
			if ShoppingTooltip2.UIT_Tooltip3 then
				ShoppingTooltip2.UIT_Tooltip3:Hide()
				ClearTooltipMeta(ShoppingTooltip2.UIT_Tooltip3)
			end
			if ShoppingTooltip2.UIT_Tooltip4 then
				ShoppingTooltip2.UIT_Tooltip4:Hide()
				ClearTooltipMeta(ShoppingTooltip2.UIT_Tooltip4)
			end
		end

		local compareText = _G.ITEM_COMPARE_TOOLTIP_TEXT
		for i = 1, tooltip:NumLines() do
			local left = _G[tName .. "TextLeft" .. i]
			if left then
				local text = left:GetText()
				if text and ((compareText and text == compareText) or text:find("当前装备")) then
					left:SetText("")
				end
			end
		end
	end

	-- 2) 如果是 ShoppingTooltip1/2：完全不显示官方装备对比框（无论是否来自聊天）
	if (tName == "ShoppingTooltip1" or tName == "ShoppingTooltip2") then
		tooltip:Hide()
		if tooltip.UIT_Tooltip2 then
			tooltip.UIT_Tooltip2:Hide()
			ClearTooltipMeta(tooltip.UIT_Tooltip2)
		end
		if tooltip.UIT_Tooltip3 then
			tooltip.UIT_Tooltip3:Hide()
			ClearTooltipMeta(tooltip.UIT_Tooltip3)
		end
		if tooltip.UIT_Tooltip4 then
			tooltip.UIT_Tooltip4:Hide()
			ClearTooltipMeta(tooltip.UIT_Tooltip4)
		end
		return
	end

	local _, itemLink = tooltip:GetItem()
	if not itemLink then
		return
	end

    -- 使用增强版提取逻辑，优先从链接中取GUID，不存在时再从装备栏/背包扫描
    local itemID, guid = ExtractItemInfoEnhanced(itemLink)
    if not itemID then
        return
    end

    -- 没有有效 GUID 说明没有自定义属性，仅保持官方提示框原样
    if not (guid and guid > 0) then
        if tooltip.UIT_Tooltip2 then
            tooltip.UIT_Tooltip2:Hide()
            ClearTooltipMeta(tooltip.UIT_Tooltip2)
        end
        if tooltip.UIT_Tooltip3 then
            tooltip.UIT_Tooltip3:Hide()
            ClearTooltipMeta(tooltip.UIT_Tooltip3)
        end
        if tooltip.UIT_Tooltip4 then
            tooltip.UIT_Tooltip4:Hide()
            ClearTooltipMeta(tooltip.UIT_Tooltip4)
        end
        return
    end

    local key = MakeKey(itemID, guid, nil, nil, false)

    -- 先触发幻境/批量查询，只利用官方tooltip作为事件入口，不在其上追加自定义属性
    local now = GetTime()
    HuanJingRequest(itemID, guid, key, now)

    local cached = GetCachedData(itemID, guid)
    local hasCompleteCache = false
    if cached and cached.systems then
        hasCompleteCache = true
        for systemName, enabled in pairs(DB.systems) do
            if enabled and not cached.systems[systemName] then
                hasCompleteCache = false
                break
            end
        end
    end
    if not hasCompleteCache then
        SendQuery(itemID, guid)
    end

    -- 记录该官方提示框当前对应的装备，用于在收到幻境倍率时回写左侧属性
    HuanJingOfficialTooltips[tooltip] = {
        key = key,
        itemID = itemID,
        guid = guid,
        applied = false
    }

    -- 如果缓存中已经有幻境数据,立即更新官方提示框
    ApplyHuanJingToOfficialTooltip(tooltip)

    -- 在任何情况下都强制隐藏官方比较框，只保留我们的大框
    if ShoppingTooltip1 then
        ShoppingTooltip1:Hide()
    end
    if ShoppingTooltip2 then
        ShoppingTooltip2:Hide()
    end

    -- 创建并显示统一的四列框架
    local unifiedFrame = GetUnifiedTooltipFrame(tooltip)

    if not unifiedFrame then
        print("|cFFFF0000[错误]|r 统一框架创建失败")
        return
    end

    -- 不再复制官方tooltip内容到第1列，四列全部显示自定义属性

    -- 定位统一大框到固定位置
    AnchorUnifiedFrame(unifiedFrame, tooltip)

    -- 记录物品键与统一大框的对应关系，便于服务器返回数据后刷新
    unifiedFrame.UIT_ItemID = itemID
    unifiedFrame.UIT_GUID = guid
    unifiedFrame.UIT_Key = key

    UnifiedFramesByKey[key] = UnifiedFramesByKey[key] or {}
    table.insert(UnifiedFramesByKey[key], unifiedFrame)

    -- 初次渲染所有自定义数据（内部会根据是否有数据决定是否显示大框）
    RefreshUnifiedFrame(unifiedFrame, itemID, guid)

end

local function OnTooltipCleared(tooltip)
    -- 清理统一框架
    if tooltip.UIT_UnifiedFrame then
        local frame = tooltip.UIT_UnifiedFrame
        frame:Hide()

        -- 从映射表中移除
        if frame.UIT_Key and UnifiedFramesByKey[frame.UIT_Key] then
            local list = UnifiedFramesByKey[frame.UIT_Key]
            for i = #list, 1, -1 do
                if list[i] == frame then
                    table.remove(list, i)
                end
            end
            if #list == 0 then
                UnifiedFramesByKey[frame.UIT_Key] = nil
            end
        end

        -- 清理该大框对应的渲染元数据
        State.tooltips[frame] = nil
    end

    -- 恢复官方tooltip的透明度（下次显示时正常使用），但不强制重新显示
    tooltip:SetAlpha(1)
    tooltip.UIT_ForceHidden = nil

    HuanJingOfficialTooltips[tooltip] = nil
end

-- Hook游戏提示框

GameTooltip:HookScript("OnTooltipSetItem", OnTooltipSetItem)
GameTooltip:HookScript("OnTooltipCleared", OnTooltipCleared)

-- 当需要时强制隐藏官方tooltip（仅作为事件源使用）
local function UIT_ForceHideTooltip(self)
    if self.UIT_ForceHidden then
        -- 保留位置用于统一大框对齐，仅隐藏官方提示框内容
        self:SetAlpha(0)
    end
end

GameTooltip:HookScript("OnShow", UIT_ForceHideTooltip)


-- Hook物品对比提示框
if ShoppingTooltip1 then
    ShoppingTooltip1:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ShoppingTooltip1:HookScript("OnTooltipCleared", OnTooltipCleared)
    ShoppingTooltip1:HookScript("OnShow", function(self) self:Hide() end)
end
if ShoppingTooltip2 then
    ShoppingTooltip2:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ShoppingTooltip2:HookScript("OnTooltipCleared", OnTooltipCleared)
    ShoppingTooltip2:HookScript("OnShow", function(self) self:Hide() end)
end

-- Hook物品引用提示框（聊天框链接点击）
if ItemRefTooltip then
    ItemRefTooltip:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ItemRefTooltip:HookScript("OnTooltipCleared", OnTooltipCleared)
end

-- ============================================================================
-- 斜杠命令
-- ============================================================================


SLASH_UNIFIEDTOOLTIP1 = "/提示框"
SLASH_UNIFIEDTOOLTIP2 = "/属性提示"
SLASH_UNIFIEDTOOLTIP3 = "/物品提示"
SLASH_UNIFIEDTOOLTIP4 = "/utt"  -- 保留英文别名

SlashCmdList["UNIFIEDTOOLTIP"] = function(msg)
    msg = msg:lower():trim()

    if msg == "调试" or msg == "debug" then
        DB.debug = not DB.debug
        print("|cff00ff00[统一提示框]|r 调试模式: " .. (DB.debug and "开启" or "关闭"))

    elseif msg:match("^设置超时%s+") or msg:match("^timeout%s+") then
        local time = tonumber(msg:match("%d+"))
        if time and time > 0 and time <= 60 then
            DB.timeout = time
            print("|cff00ff00[统一提示框]|r 查询超时已设置为: " .. time .. " 秒")
        else
            print("|cffff0000[统一提示框]|r 无效的时间值，请输入1-60之间的整数")
            print("|cff888888示例: /提示框 设置超时 15|r （15秒）")
        end

    elseif msg:match("^设置有效期%s+") or msg:match("^缓存时间%s+") or msg:match("^expire%s+") then
        local time = tonumber(msg:match("%d+"))
        if time and time > 0 then
            DB.cacheExpiration = time
            print("|cff00ff00[统一提示框]|r 缓存有效期已设置为: " .. time .. " 秒")
        else
            print("|cffff0000[统一提示框]|r 无效的时间值，请输入正整数")
            print("|cff888888示例: /提示框 设置有效期 300|r （设置为5分钟）")
        end

    elseif msg == "清除" or msg == "清除缓存" or msg == "clear" then
        State.cache = {}
        State.pending = {}
        State.lastQuery = {}
        State.noDataUntil = {}
        State.cacheTime = {}
        print("|cff00ff00[统一提示框]|r 已清除所有缓存")

    elseif msg == "刷新" or msg == "重载" or msg == "refresh" then
        -- 获取当前鼠标悬停的物品
        local _, itemLink = GameTooltip:GetItem()
        if itemLink then
            local itemID, guid = ExtractItemInfo(itemLink)
            if itemID and guid and guid > 0 then
                local key = MakeKey(itemID, guid, nil, nil, false)
                -- 清除该装备的缓存
                State.cache[key] = nil
                State.pending[key] = nil
                State.lastQuery[key] = nil
                State.noDataUntil[key] = nil
                State.cacheTime[key] = nil
                print("|cff00ff00[统一提示框]|r 已清除物品缓存: " .. itemID .. " (GUID: " .. guid .. ")")
                print("  请移开鼠标后重新悬停以重新查询")
            else
                print("|cffff0000[统一提示框]|r 物品没有有效的GUID")
            end
        else
            print("|cffff0000[统一提示框]|r 请先将鼠标悬停在装备上")
        end

    elseif msg == "状态" or msg == "查看" or msg == "status" then
        -- 统计数量
        local cacheCount = 0
        for _ in pairs(State.cache or {}) do cacheCount = cacheCount + 1 end
        
        local pendingCount = 0
        for _ in pairs(State.pending or {}) do pendingCount = pendingCount + 1 end
        
        local cooldownCount = 0
        for _ in pairs(State.noDataUntil or {}) do cooldownCount = cooldownCount + 1 end
        
        print("|cff00ff00[统一提示框]|r 缓存状态:")
        print("  总缓存数: " .. cacheCount)
        print("  查询中: " .. pendingCount)
        print("  冷却中: " .. cooldownCount)
        print("\n|cff00ff00[统一提示框]|r 性能配置:")
        print("  缓存有效期: " .. DB.cacheExpiration .. " 秒 (" .. math.floor(DB.cacheExpiration / 60) .. " 分钟)")
        print("  查询超时: " .. DB.timeout .. " 秒")
        print("  通信方式: Addon消息（不受聊天速率限制）")
        print("  已注册前缀: " .. ADDON_PREFIX .. ", " .. ADDON_PREFIX_ALT)
        print("  调试模式: " .. (DB.debug and "开启" or "关闭"))

        -- 显示当前装备的详细信息
        local _, itemLink = GameTooltip:GetItem()
        if itemLink then
            local itemID, guid = ExtractItemInfo(itemLink)
            if itemID and guid and guid > 0 then
                local key = MakeKey(itemID, guid, nil, nil, false)
                local cached = State.cache[key]
                print("\n  当前装备 [" .. itemID .. " GUID:" .. guid .. "]:")
                if cached then
                    print("  已缓存系统:")
                    for systemName, _ in pairs(cached.systems) do
                        print("    - " .. systemName)
                    end
                else
                    print("  无缓存")
                end

                -- 显示pending状态
                local pending = State.pending[key]
                if pending then
                    local now = GetTime()
                    print("\n  查询中的系统:")
                    for systemName, sentTime in pairs(pending.systems) do
                        local elapsed = now - sentTime
                        print(string.format("    - %s: 已等待 %.2f 秒", systemName, elapsed))
                    end
                end
            end
        end
        
        -- 如果有pending查询，显示详细列表
        if pendingCount > 0 then
            print("\n|cffffcc00所有等待中的查询:|r")
            local now = GetTime()
            local pendingList = {}
            
            for pKey, pData in pairs(State.pending) do
                if pData.queryStartTime then
                    table.insert(pendingList, {
                        key = pKey,
                        queryId = pData.queryId,
                        startTime = pData.queryStartTime,
                        waitTime = now - pData.queryStartTime
                    })
                end
            end
            
            -- 按queryId排序
            table.sort(pendingList, function(a, b) return (a.queryId or 0) < (b.queryId or 0) end)
            
            for _, item in ipairs(pendingList) do
                local color = item.waitTime > 5 and "|cffff0000" or (item.waitTime > 3 and "|cffffcc00" or "|cff00ff00")
                print(string.format("  Q%s: %s 已等待 %s%.1fs|r", 
                    item.queryId or "?", item.key, color, item.waitTime))
            end
        end

    elseif msg == "pending" or msg == "等待" then
        -- 新命令：专门查看pending状态
        local pendingCount = 0
        for _ in pairs(State.pending or {}) do pendingCount = pendingCount + 1 end
        
        if pendingCount == 0 then
            print("|cff00ff00[统一提示框]|r 当前没有等待中的查询")
            return
        end
        
        print("|cff00ff00[统一提示框]|r 等待中的查询 (共 " .. pendingCount .. " 个):")
        
        local now = GetTime()
        local pendingList = {}
        
        for pKey, pData in pairs(State.pending) do
            if pData.queryStartTime then
                table.insert(pendingList, {
                    key = pKey,
                    queryId = pData.queryId,
                    startTime = pData.queryStartTime,
                    waitTime = now - pData.queryStartTime
                })
            end
        end
        
        -- 按等待时间排序（最长的在前）
        table.sort(pendingList, function(a, b) return a.waitTime > b.waitTime end)
        
        for _, item in ipairs(pendingList) do
            local color = item.waitTime > 5 and "|cffff0000" or (item.waitTime > 3 and "|cffffcc00" or "|cff00ff00")
            print(string.format("  Q%s: %s 已等待 %s%.1fs|r", 
                item.queryId or "?", item.key, color, item.waitTime))
        end

    elseif msg == "性能" or msg == "统计" or msg == "perf" or msg == "performance" then
        print("|cff00ff00[统一提示框]|r 性能统计:")
        if not State.stats or not next(State.stats) then
            print("  暂无统计数据")
            return
        end

        local systems = {"identification", "growth", "enhancement", "magic", "skills", "runes", "sets"}
        for _, systemName in ipairs(systems) do
            local stats = State.stats[systemName]
            if stats and stats.totalQueries > 0 then
                local avgTime = stats.totalTime / stats.totalQueries
                print(string.format("\n  |cffffcc00%s|r 系统:", systemName))
                print(string.format("    查询次数: %d", stats.totalQueries))
                print(string.format("    平均响应: |cff00ff00%.3f|r 秒", avgTime))
                print(string.format("    最快响应: |cff00ff00%.3f|r 秒", stats.minTime))
                print(string.format("    最慢响应: |cffff0000%.3f|r 秒", stats.maxTime))
            end
        end

    elseif msg == "清除统计" or msg == "重置统计" or msg == "clearstats" then
        State.stats = {}
        print("|cff00ff00[统一提示框]|r 已清除所有性能统计数据")

    elseif msg == "优化延迟" or msg == "慢速模式" or msg == "slow" then
        DB.timeout = 20
        print("|cff00ff00[统一提示框]|r 已应用慢速服务器优化:")
        print("  查询超时: 20 秒")
        print("|cff888888此配置适用于响应慢的服务器|r")

    elseif msg == "优化速度" or msg == "快速模式" or msg == "fast" then
        DB.timeout = 10
        print("|cff00ff00[统一提示框]|r 已应用快速服务器优化:")
        print("  查询超时: 10 秒")
        print("|cff888888此配置适用于响应快的服务器|r")

    elseif msg == "链接" or msg == "调试链接" or msg == "link" then
        -- 显示当前鼠标悬停物品的完整链接格式
        local _, itemLink = GameTooltip:GetItem()
        if not itemLink then
            _, itemLink = ItemRefTooltip:GetItem()
        end

        if itemLink then
            print("|cff00ff00[统一提示框]|r 物品链接完整格式:")
            print(itemLink)

            local itemString = string.match(itemLink, "item[%-?%d:]+")
            if itemString then
                print("\n物品字符串:")
                print(itemString)

                local parts = { strsplit(":", itemString) }
                print("\n参数详情:")
                print("位置1 (item): " .. (parts[1] or "无"))
                print("位置2 (itemID): " .. (parts[2] or "无"))
                print("位置3 (enchantID): " .. (parts[3] or "无"))
                print("位置4 (gem1): " .. (parts[4] or "无"))
                print("位置5 (gem2): " .. (parts[5] or "无"))
                print("位置6 (gem3): " .. (parts[6] or "无"))
                print("位置7 (gem4/suffixID): " .. (parts[7] or "无"))
                print("位置8 (uniqueID/GUID): " .. (parts[8] or "无"))
                print("位置9 (level): " .. (parts[9] or "无"))
                print("位置10-N (bonus等): ")
                for i = 10, #parts do
                    print("  位置" .. i .. ": " .. (parts[i] or "无"))
                end
            end

            local itemID, guid = ExtractItemInfo(itemLink)
            print("\n提取结果:")
            print("itemID: " .. (itemID or "无"))
            print("GUID: " .. (guid or "无"))
        else
            print("|cffff0000[统一提示框]|r 请先将鼠标悬停在物品上或点击物品链接")
        end

    elseif msg == "帮助" or msg == "help" or msg == "" then
        print("|cff00ff00[统一提示框]|r 命令列表:")
        print("\n|cffffcc00基础命令:|r")
        print("  |cffffcc00/提示框 调试|r - 切换调试模式")
        print("  |cffffcc00/提示框 清除|r - 清除所有缓存")
        print("  |cffffcc00/提示框 刷新|r - 清除当前装备缓存并重新查询")
        print("  |cffffcc00/提示框 状态|r - 显示缓存状态和配置")
        print("  |cffffcc00/提示框 pending|r - 显示所有等待中的查询")
        print("\n|cffffcc00性能优化:|r")
        print("  |cffffcc00/提示框 性能|r - 显示各系统响应时间统计")
        print("  |cffffcc00/提示框 优化延迟|r - 应用慢速服务器预设（推荐⭐）")
        print("  |cffffcc00/提示框 优化速度|r - 应用快速服务器预设")
        print("  |cffffcc00/提示框 设置超时 <秒数>|r - 设置查询超时时间")
        print("    |cff888888示例: /提示框 设置超时 20|r （20秒）")
        print("  |cffffcc00/提示框 设置有效期 <秒数>|r - 设置缓存有效期")
        print("    |cff888888示例: /提示框 设置有效期 600|r （10分钟）")
        print("\n|cffffcc00其他:|r")
        print("  |cffffcc00/提示框 清除统计|r - 清除性能统计数据")
        print("  |cffffcc00/提示框 链接|r - 显示当前物品的完整链接格式（调试用）")
        print("  |cffffcc00/提示框 帮助|r - 显示此帮助信息")
        print(" ")
        print("|cff888888其他可用命令：/属性提示 /物品提示 /utt|r")
        print(" ")
        print("|cff00ff00提示：插件使用Addon消息通道，不受聊天速率限制！|r")
        print("|cff888888快速悬停多个装备时，查询将以最快速度响应（RTT<100ms）。|r")

    else
        print("|cffff0000[统一提示框]|r 未知命令: " .. msg)
        print("|cff888888输入 |cffffcc00/提示框 帮助|r |cff888888查看命令列表|r")
    end
end

-- ============================================================================
-- 插件加载完成
-- ============================================================================

-- 插件加载完成
print("|cFF00FF00[统一提示框]|r 四联提示框系统已加载")
