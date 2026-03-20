-- UnifiedItemTooltip.lua
-- 统一的物品提示框插件 - 根据服务器数据动态显示所有属性

local ADDON_NAME = "UnifiedItemTooltip"
local ADDON_PREFIX = "UITQ"  -- Addon消息前缀（最多16字符）- UnifiedItemTooltipQuery
local ADDON_PREFIX_ALT = "ITEMENHANCE"  -- 备用前缀（兼容服务器可能使用的其他前缀）

-- ============================================================================
-- 配置
-- ============================================================================

local DEFAULTS = {
    debug = false,  -- 调试模式（默认关闭，可通过 /提示框 调试 开启）
    traceEnabled = false,
    traceItemID = 0,
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
local IsTooltipFromChat

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

local TRACE_PREFIX = "|cffff8800[UITQ-TRACE]|r"

local function GetTraceItemID()
    local itemID = tonumber(DB.traceItemID or 0)
    if not itemID or itemID < 0 then
        itemID = 0
    end
    return itemID
end

local function ShouldTrace(itemID)
    if not DB.traceEnabled then
        return false
    end

    local traceItemID = GetTraceItemID()
    if traceItemID <= 0 then
        return true
    end

    return tonumber(itemID) == traceItemID
end

local function TraceLog(itemID, fmt, ...)
    if not ShouldTrace(itemID) then
        return
    end

    local ok, msg = pcall(string.format, fmt, ...)
    if not ok then
        msg = tostring(fmt)
    end

    print(string.format("%s [itemID=%s] %s", TRACE_PREFIX, tostring(itemID), msg))
end

local function TraceLogDedup(traceKey, itemID, signature, fmt, ...)
    if not ShouldTrace(itemID) then
        return
    end

    State.traceLast = State.traceLast or {}

    local now = GetTime()
    local last = State.traceLast[traceKey]
    if last and last.signature == signature and (now - last.time) < 0.2 then
        return
    end

    State.traceLast[traceKey] = {
        signature = signature,
        time = now
    }

    TraceLog(itemID, fmt, ...)
end

local function CollectTraceKeys(list)
    local names = {}
    if list then
        for name, value in pairs(list) do
            if value ~= nil then
                table.insert(names, tostring(name))
            end
        end
    end

    table.sort(names)
    return #names > 0 and table.concat(names, ",") or "-"
end

local function FormatTracePositions(positions)
    if not positions or #positions == 0 then
        return "-"
    end

    local parts = {}
    for _, pos in ipairs(positions) do
        table.insert(parts, string.format("%s:%s", tostring(pos.bag), tostring(pos.slot)))
    end
    return table.concat(parts, ",")
end

local function UrlDecode(text)
    if not text or text == "" then
        return ""
    end

    return (text:gsub("%%(%x%x)", function(hex)
        return string.char(tonumber(hex, 16) or 0)
    end))
end

local function StripColorCodes(text)
    if not text or text == "" then
        return ""
    end

    return (text:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", ""))
end

local function SanitizeDisplayNameText(text)
    if not text or text == "" then
        return ""
    end

    return StripColorCodes(text)
end

local function NormalizeColorCode(color)
    if not color or color == "" then
        return nil
    end

    color = tostring(color)
    color = color:gsub("^%s+", ""):gsub("%s+$", "")

    local directCode = color:match("^(|c%x%x%x%x%x%x%x%x)")
    if directCode then
        return directCode
    end

    local directCodeShort = color:match("^(|cff%x%x%x%x%x%x)")
    if directCodeShort then
        return directCodeShort
    end

    local hex8 = color:match("^(%x%x%x%x%x%x%x%x)")
    if hex8 then
        return "|c" .. hex8
    end

    local hex6 = color:match("^(%x%x%x%x%x%x)")
    if hex6 then
        return "|cff" .. hex6
    end

    if color:match("^|c%x%x%x%x%x%x%x%x$") then
        return color
    end

    if color:match("^|cff%x%x%x%x%x%x$") then
        return color
    end

    if color:match("^%x%x%x%x%x%x%x%x$") then
        return "|c" .. color
    end

    if color:match("^%x%x%x%x%x%x$") then
        return "|cff" .. color
    end

    return nil
end

local function ParseItemNameColors(colorsText)
    local colors = {}

    if colorsText and colorsText ~= "" then
        for token in string.gmatch(colorsText, "([^,]+)") do
            local trimmed = token:gsub("^%s+", ""):gsub("%s+$", "")
            if trimmed ~= "" then
                table.insert(colors, NormalizeColorCode(trimmed))
            end
        end
    end

    return colors
end

local function SplitUtf8Chars(text)
    local chars = {}
    if not text or text == "" then
        return chars
    end

    local index = 1
    local length = string.len(text)
    while index <= length do
        local currentByte = string.byte(text, index)
        local charLength = 1

        if currentByte >= 240 then
            charLength = 4
        elseif currentByte >= 224 then
            charLength = 3
        elseif currentByte >= 192 then
            charLength = 2
        end

        table.insert(chars, string.sub(text, index, index + charLength - 1))
        index = index + charLength
    end

    return chars
end

local function ApplyColorToFullName(text, color)
    if not text or text == "" then
        return ""
    end

    local normalizedColor = NormalizeColorCode(color)
    if not normalizedColor then
        return text
    end

    return normalizedColor .. text .. "|r"
end

local function ApplyPerCharacterColors(text, colors, fallbackColor)
    if not text or text == "" then
        return ""
    end

    local normalizedFallback = NormalizeColorCode(fallbackColor)
    if not colors or #colors == 0 then
        return ApplyColorToFullName(text, normalizedFallback)
    end

    if #colors == 1 then
        return ApplyColorToFullName(text, colors[1] or normalizedFallback)
    end

    local chars = SplitUtf8Chars(text)
    local coloredText = ""
    local lastColor = colors[#colors] or normalizedFallback

    for index, char in ipairs(chars) do
        local color = colors[index] or lastColor
        color = NormalizeColorCode(color) or normalizedFallback
        if color then
            coloredText = coloredText .. color .. char .. "|r"
        else
            coloredText = coloredText .. char
        end
    end

    return coloredText
end

local function ParseIdentificationDisplayData(displayData)
    if not displayData or displayData == "" then
        return nil
    end

    local parts = { strsplit("|", displayData) }
    if #parts < 1 or parts[1] ~= "IDDISP" then
        return nil
    end

    return {
        qualityColor = UrlDecode(parts[2] or ""),
        itemNamePrefix = UrlDecode(parts[3] or ""),
        itemNameSuffix = UrlDecode(parts[4] or ""),
        itemNameColors = UrlDecode(parts[5] or ""),
        itemBottomDescription = UrlDecode(parts[6] or "")
    }
end

local MakeKey       -- 提前声明，供幻境相关函数使用
local RenderTooltip -- 提前声明，供幻境相关更新调用
local RefreshUnifiedFrame -- 提前声明，供四联大框刷新使用
local ExtractItemInfo -- 提前声明，供对比提示框定位使用
local GetTooltipBagSlot -- 提前声明，供待鉴定提示刷新使用
local NormalizeHuanJingMode -- 提前声明，供批量解析/待鉴定逻辑使用
local HasHuanJingEffect -- 提前声明，供批量解析/渲染逻辑使用
local CalculateHuanJingEnhancedValue -- 提前声明，供官方提示框幻境换算使用
local FormatHuanJingModeText -- 提前声明，供属性文本格式化使用
local FormatTooltipStatLine -- 提前声明，供幻境属性文本格式化使用

-- 记录当前按物品键关联的统一四联大框，用于异步刷新
local UnifiedFramesByKey = {}

-- 幻境系统数据缓存（独立于服务器批量查询）
local HuanJingState = {
    cache = {},   -- key -> { itemID, guid, multiplier, attributeData, identificationData, timestamp }
    pending = {}, -- key -> lastQueryTime
    COOLDOWN = 1, -- 查询冷却（秒）- 降低到1秒加快响应
    EXPIRE = 300  -- 缓存有效期（秒）- 增加到5分钟
}

-- 【新增】待鉴定物品状态缓存
-- 存储玩家所有待鉴定物品的信息，用于在提示框底部显示"未鉴定 x倍率"
-- 【修改】使用 "bag:slot:itemId" 作为缓存键，而不是GUID
local PendingIdentifyState = {
    cache = {},       -- "bag:slot:itemId" -> { multiplier, groupId, timestamp }
    suppressed = {},  -- "bag:slot:itemId" -> expireTime???????????????????
    lastQuery = 0,    -- ??????
    COOLDOWN = 5,     -- ???????
    EXPIRE = 60,      -- ????????
    SUPPRESS_EXPIRE = 8, -- ?????????????????????
    querying = false, -- ???????
    refreshing = false,  -- ????????????????????????
    refreshQueued = false
}

-- 【新增】查询待鉴定物品列表
-- forceRefresh: 是否强制刷新（忽略冷却时间）
local function QueryPendingIdentifyList(forceRefresh)
    local now = GetTime()

    -- 检查冷却时间（强制刷新时忽略）
    if PendingIdentifyState.querying then
        return
    end
    if not forceRefresh and (now - PendingIdentifyState.lastQuery) < PendingIdentifyState.COOLDOWN then
        return
    end

    PendingIdentifyState.querying = true

    -- 发送查询请求到服务器
    local addonMessage = "LIST_PENDING"
    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    end
end

local function RefreshPendingIdentifyTooltip(tooltip)
    if not tooltip or not tooltip.IsShown or not tooltip:IsShown() then
        return
    end

    local bag, slot = GetTooltipBagSlot(tooltip)
    if bag == nil or slot == nil then
        return
    end

    if bag == 255 then
        if tooltip.SetInventoryItem then
            tooltip:SetInventoryItem("player", slot)
        end
    elseif tooltip.SetBagItem then
        tooltip:SetBagItem(bag, slot)
    end
end

local function RefreshVisiblePendingIdentifyTooltips(itemId)
    C_Timer.After(0.05, function()
        for tooltip, _ in pairs(State.tooltips) do
            if tooltip and tooltip:IsShown() then
                local tooltipName = tooltip:GetName() or ""
                if tooltipName == "GameTooltip" then
                    local _, itemLink = tooltip:GetItem()
                    local tooltipItemId = itemLink and tonumber(string.match(itemLink, "item:(%d+)")) or nil
                    if not itemId or tooltipItemId == itemId then
                        RefreshPendingIdentifyTooltip(tooltip)
                    end
                end
            end
        end
    end)
end

local function SchedulePendingIdentifyRefresh(reason, itemId)
    PendingIdentifyState.lastQuery = 0

    if PendingIdentifyState.querying or PendingIdentifyState.refreshing then
        PendingIdentifyState.refreshQueued = true
        TraceLog(itemId or 0, "PendingIdentify refresh queued reason=%s querying=%s refreshing=%s",
            tostring(reason), tostring(PendingIdentifyState.querying), tostring(PendingIdentifyState.refreshing))
        return
    end

    PendingIdentifyState.refreshing = true
    C_Timer.After(0.1, function()
        PendingIdentifyState.refreshing = false

        if PendingIdentifyState.querying then
            PendingIdentifyState.refreshQueued = true
            TraceLog(itemId or 0, "PendingIdentify refresh deferred reason=%s querying=%s",
                tostring(reason), tostring(PendingIdentifyState.querying))
            return
        end

        TraceLog(itemId or 0, "PendingIdentify refresh now reason=%s", tostring(reason))
        QueryPendingIdentifyList(true)
    end)
end

-- 【新增】检查物品是否待鉴定，返回 { isPending, multiplier } 或 nil
-- 【修改】使用背包号+槽位+物品ID来查询，而不是GUID
local function GetPendingIdentifyCacheKey(bag, slot, itemId)
    if bag == nil or slot == nil or not itemId then
        return nil
    end
    return string.format("%d:%d:%d", bag, slot, itemId)
end

local function CleanupPendingIdentifySuppressed(now)
    now = now or GetTime()
    for cacheKey, expireTime in pairs(PendingIdentifyState.suppressed) do
        if not expireTime or now >= expireTime then
            PendingIdentifyState.suppressed[cacheKey] = nil
        end
    end
end

local function SuppressPendingIdentify(bag, slot, itemId, duration)
    local cacheKey = GetPendingIdentifyCacheKey(bag, slot, itemId)
    if not cacheKey then
        return
    end

    PendingIdentifyState.cache[cacheKey] = nil
    PendingIdentifyState.suppressed[cacheKey] = GetTime() + (duration or PendingIdentifyState.SUPPRESS_EXPIRE)
end

local function IsPendingIdentifySuppressed(bag, slot, itemId)
    local cacheKey = GetPendingIdentifyCacheKey(bag, slot, itemId)
    if not cacheKey then
        return false
    end

    local now = GetTime()
    local expireTime = PendingIdentifyState.suppressed[cacheKey]
    if not expireTime then
        return false
    end

    if now >= expireTime then
        PendingIdentifyState.suppressed[cacheKey] = nil
        return false
    end

    return true
end

local function GetPendingIdentifyInfo(bag, slot, itemId)
    local cacheKey = GetPendingIdentifyCacheKey(bag, slot, itemId)
    if not cacheKey then
        return nil
    end

    CleanupPendingIdentifySuppressed()
    if IsPendingIdentifySuppressed(bag, slot, itemId) then
        PendingIdentifyState.cache[cacheKey] = nil
        return nil
    end

    local data = PendingIdentifyState.cache[cacheKey]
    if not data then
        return nil
    end

    -- ??????
    local now = GetTime()
    if (now - data.timestamp) > PendingIdentifyState.EXPIRE then
        PendingIdentifyState.cache[cacheKey] = nil
        return nil
    end

    return {
        isPending = true,
        multiplier = data.multiplier or 1,
        mode = NormalizeHuanJingMode(data.mode),
        groupId = data.groupId or 0
    }
end

-- 【新增】通过itemID查找所有待鉴定物品的正确位置
-- 返回所有匹配该itemID的待鉴定物品位置列表 { {bag, slot, multiplier, groupId}, ... }
-- 这用于解决Combuctor等第三方背包插件返回错误槽位的问题
local function FindPendingItemPositions(itemId)
    if not itemId then
        return {}
    end

    local positions = {}
    local now = GetTime()

    for cacheKey, data in pairs(PendingIdentifyState.cache) do
        -- 检查是否过期
        if (now - data.timestamp) <= PendingIdentifyState.EXPIRE then
            if data.itemId == itemId then
                table.insert(positions, {
                    bag = data.bag,
                    slot = data.slot,
                    multiplier = data.multiplier or 1,
                    mode = NormalizeHuanJingMode(data.mode),
                    groupId = data.groupId or 0
                })
            end
        end
    end

    return positions
end

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

-- 发送幻境倍率查询
-- 【优化】幻境数据现在已整合到ALL_MODULE_DATA批量查询中，不再单独发送HUANJING_QUERY
-- 此函数保留用于兼容性，但不再发送网络请求
local function HuanJingRequest(itemID, guid, key, now)
    if not itemID or not guid or guid == 0 then return end

    -- 已有有效缓存则不再查询
    if HuanJingGetData(key) then
        return
    end

    -- 【优化】幻境数据现在通过ALL_MODULE_DATA批量查询获取
    -- 不再单独发送HUANJING_QUERY请求，减少网络负担
    -- 如果缓存中没有幻境数据，说明QUERY请求尚未返回或该物品没有幻境倍率
    -- 此处不做任何操作，等待QUERY响应中的huanjingData字段

    -- 注释掉原有的单独查询代码：
    -- local addonMessage = string.format("HUANJING_QUERY:%d:%d", itemID, guid)
    -- if SendAddonMessage then
    --     SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    -- elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
    --     C_ChatInfo.SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    -- end

    HuanJingState.pending[key] = now
end

local SEARCH_CONTAINER_BAGS = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }

local function ForEachSearchBag(callback)
    for _, bagIndex in ipairs(SEARCH_CONTAINER_BAGS) do
        local numSlots = GetContainerNumSlots(bagIndex)
        if numSlots and numSlots > 0 then
            if callback(bagIndex, numSlots) then
                return true
            end
        end
    end

    return false
end

-- 查询其他玩家装备的GUID（通过玩家名和装备槽位）
-- 这个函数向服务器请求指定玩家指定槽位装备的真实GUID
local InspectGuidCache = {}  -- 缓存查询到的GUID: "playerName:slot:itemID" -> guid
local InspectGuidPendingRequests = {}  -- 正在进行中的请求: "playerName:slot:itemID" -> timestamp
local INSPECT_REQUEST_COOLDOWN = 2  -- 同一个请求的冷却时间（秒）

local function RequestInspectItemGuid(playerName, slot, itemID)
    if not playerName or not slot or not itemID then
        return nil
    end

    local cacheKey = string.format("%s:%d:%d", playerName, slot, itemID)

    -- 只有当缓存的GUID大于0时才使用
    local cachedGuid = InspectGuidCache[cacheKey]
    if cachedGuid and cachedGuid > 0 then
        return cachedGuid
    elseif cachedGuid == 0 then
        -- 清除无效缓存
        InspectGuidCache[cacheKey] = nil
    end

    -- 节流控制：检查是否有正在进行中的请求
    local now = GetTime()
    local lastRequestTime = InspectGuidPendingRequests[cacheKey]
    if lastRequestTime and (now - lastRequestTime) < INSPECT_REQUEST_COOLDOWN then
        return nil
    end

    -- 记录请求时间
    InspectGuidPendingRequests[cacheKey] = now

    -- 向服务器发送查询请求
    local addonMessage = string.format("INSPECT_ITEM_GUID:%s:%d:%d", playerName, slot, itemID)
    local myName = UnitName("player")

    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", myName)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", myName)
    end

    return nil
end

-- 将幻境倍率应用到“官方”物品提示框（左侧原版 GameTooltip）
local function ApplyHuanJingToOfficialTooltip(tooltip)
    if not tooltip or not tooltip:IsShown() then return end

    local state = HuanJingOfficialTooltips[tooltip]
    if not state or state.applied then return end

    -- 【关键修复】幻境缓存使用GUID格式的key，但state.key可能是位置格式
    -- 需要同时尝试两种格式的key来查找幻境数据
    local hjData = HuanJingGetData(state.key)

    -- 如果用state.key找不到，尝试用GUID格式的key
    if not hjData and state.itemID and state.guid and state.guid > 0 then
        local guidKey = "G:" .. state.itemID .. ":" .. state.guid
        hjData = HuanJingGetData(guidKey)
    end

    if not hjData or not HasHuanJingEffect(hjData.multiplier, hjData.mode) then
        return
    end

    local mult = hjData.multiplier
    local mode = hjData.mode

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
                        local enhanced = info.enhanced or CalculateHuanJingEnhancedValue(original, mult, mode)

                        -- 与右侧“基础/追加属性”保持一致的配色：倍率为粉色，最终值为红色
                        local newCoreText = FormatTooltipStatLine(name, original, mult, mode)

                        leftText:SetText(newCoreText)
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

local function ApplyIdentificationDisplay(tooltip, cached, meta)
    if not tooltip or not cached or not cached.systems then
        return
    end

    local identData = cached.systems.identification
    if not identData or identData.isEmpty then
        return
    end

    local hasDisplayConfig = (identData.qualityColor and identData.qualityColor ~= "")
        or (identData.itemNamePrefix and identData.itemNamePrefix ~= "")
        or (identData.itemNameSuffix and identData.itemNameSuffix ~= "")
        or (identData.itemNameColors and identData.itemNameColors ~= "")
        or (identData.itemBottomDescription and identData.itemBottomDescription ~= "")

    if not hasDisplayConfig then
        return
    end

    meta.rendered = meta.rendered or {}

    local titleLine = nil
    if tooltip.GetName then
        local tooltipName = tooltip:GetName()
        if tooltipName == "ShoppingTooltip1" or tooltipName == "ShoppingTooltip2" then
            titleLine = _G[tooltipName .. "TextLeft2"] or _G[tooltipName .. "TextLeft1"]
        else
            titleLine = _G[tooltipName .. "TextLeft1"]
        end
    end

    if titleLine and not meta.rendered.identificationTitle then
        if not meta.originalTitle or meta.originalTitle == "" then
            meta.originalTitle = StripColorCodes(titleLine:GetText() or "")
            if meta.originalTitle == "" then
                local itemName = tooltip:GetItem()
                meta.originalTitle = StripColorCodes(itemName or "")
            end
        end

        local originalTitle = meta.originalTitle or ""
        local fullTitle = string.format("%s%s%s",
            SanitizeDisplayNameText(identData.itemNamePrefix or ""),
            originalTitle,
            SanitizeDisplayNameText(identData.itemNameSuffix or ""))

        local fallbackColor = NormalizeColorCode(identData.qualityColor)
        local colors = ParseItemNameColors(identData.itemNameColors)
        local newTitle = ApplyPerCharacterColors(fullTitle, colors, fallbackColor)

        if newTitle ~= "" then
            titleLine:SetText(newTitle)
            meta.rendered.identificationTitle = true
        end
    end

    tooltip:Show()
end

local function ApplyIdentificationBottomDescription(tooltip, cached, meta)
    if not tooltip or not cached or not cached.systems then
        return
    end

    local identData = cached.systems.identification
    if not identData or identData.isEmpty then
        return
    end

    if not identData.itemBottomDescription or identData.itemBottomDescription == "" then
        return
    end

    meta.rendered = meta.rendered or {}
    if meta.rendered.identificationBottomDescription then
        return
    end

    tooltip:AddLine(" ")
    tooltip:AddLine(identData.itemBottomDescription)
    meta.rendered.identificationBottomDescription = true
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
    local itemId, itemGuid, mode, multiplier, attributeData, identificationData =
        message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性模式:([x%+%-]) 属性倍率:([%d%.]+) 属性数据:(.+) 鉴定数据:(.+)")

    if not itemId then
        itemId, itemGuid, mode, multiplier, attributeData =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性模式:([x%+%-]) 属性倍率:([%d%.]+) 属性数据:(.+)")
        identificationData = nil
    end

    if not itemId then
        itemId, itemGuid, mode, multiplier =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性模式:([x%+%-]) 属性倍率:([%d%.]+)")
        attributeData = nil
        identificationData = nil
    end

    if not itemId then
        itemId, itemGuid, multiplier, attributeData, identificationData =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性倍率:([%d%.]+) 属性数据:(.+) 鉴定数据:(.+)")
        mode = "x"
    end

    if not itemId then
        itemId, itemGuid, multiplier, attributeData =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性倍率:([%d%.]+) 属性数据:(.+)")
        identificationData = nil
        mode = "x"
    end

    if not itemId then
        itemId, itemGuid, multiplier =
            message:match("%[幻境系统%] 装备ID:(%d+) GUID:(%d+) 属性倍率:([%d%.]+)")
        attributeData = nil
        identificationData = nil
        mode = "x"
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
        mode = NormalizeHuanJingMode(mode),
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

    local itemId, itemGuid, mode, multiplier, rest =
        message:match("^HUANJING_DATA:(%d+):(%d+):([x%+%-]):([%d%.]+):?(.*)$")

    if not itemId then
        itemId, itemGuid, multiplier, rest =
            message:match("^HUANJING_DATA:(%d+):(%d+):([%d%.]+):?(.*)$")
        mode = "x"
    end

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
        mode = NormalizeHuanJingMode(mode),
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
-- 【重要修复】WoW 3.3.5客户端物品链接不包含物品GUID！
-- 必须使用 bag:slot:itemID 作为唯一标识符
-- bag=255 表示装备栏物品
MakeKey = function(itemID, guid, bag, slot, equipFlag)
    -- 【优先】如果有背包位置信息，使用 bag:slot:itemID 作为缓存键
    -- 这是WoW 3.3.5中唯一可靠的物品实例标识方式
    if bag ~= nil and slot ~= nil then
        return string.format("P:%d:%d:%d", bag, slot, itemID)
    end

    -- 【装备栏】使用 255:slot:itemID 格式
    if equipFlag and slot then
        return string.format("P:255:%d:%d", slot, itemID)
    end

    -- 【备用】如果有有效GUID（某些服务器可能支持），使用GUID
    if guid and guid > 0 then
        return string.format("G:%d:%d", itemID, guid)
    end

    -- 【最后手段】没有位置信息也没有GUID，使用时间戳避免缓存污染
    -- 这种情况不应该发生，记录警告
    DebugPrint(string.format("[MakeKey警告] 无法生成有效缓存键: itemID=%d, bag=%s, slot=%s, guid=%s",
        itemID, tostring(bag), tostring(slot), tostring(guid)))
    return string.format("U:%d:%.0f", itemID, GetTime() * 1000)
end

-- 从物品链接字符串中提取GUID的通用函数
-- 【重要】WoW 3.3.5客户端物品链接中不包含物品实例GUID！
-- 位置8是suffixId（随机附魔），只有鉴定过的物品才有值，不是物品GUID
-- 位置9是uniqueId，通常为0
-- 因此我们必须依赖服务器返回真实GUID
local function ExtractGuidFromItemString(itemString)
    if not itemString then return nil end

    local parts = { strsplit(":", itemString) }

    -- WoW 3.3.5 物品链接格式: item:itemId:enchant:gem1:gem2:gem3:gem4:suffixId:uniqueId:level
    -- 【修复】聊天框链接中GUID可能在位置7（suffixId）或位置8（uniqueId）
    -- 例如: item:2651:0:0:0:0:0:240:0:80 中GUID=240在位置7

    -- 先检查位置8（uniqueId）
    if #parts >= 9 then
        local val9 = tonumber(parts[9])
        if val9 and val9 ~= 0 then
            return math.abs(val9)
        end
    end

    -- 再检查位置7（suffixId），聊天框链接的GUID可能在这里
    if #parts >= 8 then
        local val8 = tonumber(parts[8])
        if val8 and val8 ~= 0 then
            return math.abs(val8)
        end
    end

    -- WoW 3.3.5客户端物品链接不包含物品实例GUID
    -- 返回nil，依赖服务器通过bag:slot查询返回真实GUID
    return nil
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
                local bagItemID = tonumber(string.match(link, "item:(%d+)"))

                if bagItemID == itemID then
                    -- 尝试从链接中提取 GUID
                    local itemString = string.match(link, "item[%-?%d:]+")
                    local bagGUID = ExtractGuidFromItemString(itemString)

                    if bagGUID and bagGUID > 0 then
                        return bagGUID, bag, slot
                    end
                end
            end
        end
    end

    return nil
end

-- 扫描装备栏查找物品的 GUID（支持扫描其他玩家）
-- unit: 要扫描的单位，默认为 "player"，可传入 "target" 或检查目标单位
-- 返回: guid, bag(nil), slot, isEquipped
-- 注意：对于其他玩家，只返回槽位信息，guid为nil（需要向服务器查询）
local function ScanEquipmentForItem(itemID, unit)
    if not itemID then return nil end

    unit = unit or "player"

    -- 遍历所有装备槽位
    for slot = 0, 19 do
        local link = GetInventoryItemLink(unit, slot)
        if link then
            local equipItemID = tonumber(string.match(link, "item:(%d+)"))

            if equipItemID == itemID then
                -- 尝试从链接中提取 GUID
                local itemString = string.match(link, "item[%-?%d:]+")
                local equipGUID = ExtractGuidFromItemString(itemString)

                -- 对于其他玩家，链接中的GUID不可信，返回nil
                -- 只有自己的装备才返回GUID
                if unit == "player" and equipGUID and equipGUID > 0 then
                    return equipGUID, nil, slot, true
                else
                    -- 其他玩家或没有GUID，只返回槽位信息
                    return nil, nil, slot, true
                end
            end
        end
    end

	return nil
end

local COMPARE_TOOLTIP_EQUIP_SLOTS = {
    INVTYPE_HEAD = {"HeadSlot"},
    INVTYPE_NECK = {"NeckSlot"},
    INVTYPE_SHOULDER = {"ShoulderSlot"},
    INVTYPE_BODY = {"ShirtSlot"},
    INVTYPE_CHEST = {"ChestSlot"},
    INVTYPE_ROBE = {"ChestSlot"},
    INVTYPE_WAIST = {"WaistSlot"},
    INVTYPE_LEGS = {"LegsSlot"},
    INVTYPE_FEET = {"FeetSlot"},
    INVTYPE_WRIST = {"WristSlot"},
    INVTYPE_HAND = {"HandsSlot"},
    INVTYPE_FINGER = {"Finger0Slot", "Finger1Slot"},
    INVTYPE_TRINKET = {"Trinket0Slot", "Trinket1Slot"},
    INVTYPE_CLOAK = {"BackSlot"},
    INVTYPE_WEAPON = {"MainHandSlot", "SecondaryHandSlot"},
    INVTYPE_2HWEAPON = {"MainHandSlot"},
    INVTYPE_WEAPONMAINHAND = {"MainHandSlot"},
    INVTYPE_WEAPONOFFHAND = {"SecondaryHandSlot"},
    INVTYPE_SHIELD = {"SecondaryHandSlot"},
    INVTYPE_HOLDABLE = {"SecondaryHandSlot"},
    INVTYPE_RANGED = {"RangedSlot"},
    INVTYPE_RANGEDRIGHT = {"RangedSlot"},
    INVTYPE_THROWN = {"RangedSlot"},
    INVTYPE_RELIC = {"RangedSlot"},
    INVTYPE_TABARD = {"TabardSlot"}
}

local function GetCompareTooltipIndex(tooltipName)
    if tooltipName == "ShoppingTooltip1" then
        return 1
    elseif tooltipName == "ShoppingTooltip2" then
        return 2
    end

    return nil
end

local function ResolveCompareTooltipEquipSlot(itemLink, itemID, tooltipName)
    local compareIndex = GetCompareTooltipIndex(tooltipName)
    if not compareIndex or not itemLink or not itemID then
        return nil
    end

    local tooltipItemString = string.match(itemLink, "item[%-?%d:]+") or itemLink
    local _, _, _, _, _, _, _, _, equipLoc = GetItemInfo(itemLink)
    local slotNames = equipLoc and COMPARE_TOOLTIP_EQUIP_SLOTS[equipLoc]
    if not slotNames then
        return nil, nil, "equipLocMissing", equipLoc, 0
    end

    local exactMatches = {}
    local itemIDMatches = {}

    for _, slotName in ipairs(slotNames) do
        local slotId = GetInventorySlotInfo(slotName)
        if slotId then
            local equipLink = GetInventoryItemLink("player", slotId)
            if equipLink then
                local equipItemString = string.match(equipLink, "item[%-?%d:]+") or equipLink
                local equipItemID, equipGuid = ExtractItemInfo(equipLink)

                if equipItemString == tooltipItemString then
                    table.insert(exactMatches, { slot = slotId, guid = equipGuid, source = "itemString" })
                elseif equipItemID == itemID then
                    table.insert(itemIDMatches, { slot = slotId, guid = equipGuid, source = "itemID" })
                end
            end
        end
    end

    local matches = (#exactMatches > 0) and exactMatches or itemIDMatches
    if #matches == 0 then
        return nil, nil, "noMatch", equipLoc, 0
    end

    local matchIndex = compareIndex
    if matchIndex > #matches then
        matchIndex = 1
    end

    local match = matches[matchIndex]
    return match.slot, match.guid, match.source, equipLoc, #matches
end

-- 获取当前正在检查的玩家单位（如果正在检查其他玩家）
local function GetInspectUnit()
    -- 检查 InspectFrame 是否打开
    if InspectFrame and InspectFrame:IsShown() then
        local inspectUnit = InspectFrame.unit
        if inspectUnit and UnitExists(inspectUnit) and not UnitIsUnit(inspectUnit, "player") then
            return inspectUnit
        end
    end
    return nil
end

-- 从物品链接提取ID和GUID
ExtractItemInfo = function(itemLink)
    if not itemLink then return nil end

    local itemID = tonumber(string.match(itemLink, "item:(%d+)"))
    if not itemID then return nil end

    local itemString = string.match(itemLink, "item[%-?%d:]+")
    local guid = ExtractGuidFromItemString(itemString)

    return itemID, guid
end

-- 增强版物品信息提取 - 如果链接中没有 GUID，尝试从背包/装备栏查找
-- inspectUnit: 可选参数，如果正在检查其他玩家，传入该玩家的单位ID
local function ExtractItemInfoEnhanced(itemLink, inspectUnit, preferEquipped)
    if not itemLink then return nil end

    local itemID, guid = ExtractItemInfo(itemLink)
    if not itemID then return nil end

    -- 【关键修复】如果正在观察其他玩家，链接中的GUID是无效的临时值
    -- 需要强制向服务器查询真实GUID，即使链接中有值也要忽略
    if inspectUnit and UnitExists(inspectUnit) and not UnitIsUnit(inspectUnit, "player") then
        -- 扫描其他玩家的装备栏找到槽位
        local _, _, equipSlot = ScanEquipmentForItem(itemID, inspectUnit)

        if equipSlot then
            -- 找到了装备槽位
            local playerName = UnitName(inspectUnit)

            -- 先检查缓存
            local cacheKey = string.format("%s:%d:%d", playerName, equipSlot, itemID)
            local cachedGuid = InspectGuidCache[cacheKey]

            -- 【修复】只有当缓存的GUID大于0时才使用，GUID=0表示之前查询失败
            if cachedGuid and cachedGuid > 0 then
                return itemID, cachedGuid, nil, equipSlot, true
            else
                -- 向服务器请求真实GUID（缓存为空或GUID=0都重新请求）
                if cachedGuid == 0 then
                    InspectGuidCache[cacheKey] = nil  -- 清除无效缓存
                end
                RequestInspectItemGuid(playerName, equipSlot, itemID)
                -- 暂时返回nil GUID，等待服务器响应后刷新
                return itemID, nil, nil, equipSlot, false
            end
        end

        -- 没找到装备槽位，返回nil
        return itemID, nil, nil, nil, false
    end

    -- 以下是自己的装备或背包物品。
    -- 注意：WoW 3.3.5 链接里的 suffixId/uniqueId 不是实例 GUID，不能再拿它反查背包位置，
    -- 否则同 itemID 物品会互相命中缓存。对比 tooltip 必须只走装备栏定位。
    if preferEquipped then
        local equipGUID, _, equipSlot, isEquipped = ScanEquipmentForItem(itemID, "player")
        if isEquipped and equipSlot then
            if equipGUID and equipGUID > 0 then
                return itemID, equipGUID, nil, equipSlot, true
            end
            return itemID, guid, nil, equipSlot, true
        end

        return itemID, guid, nil, nil, false
    end

    if guid and guid > 0 then
        -- 背包 tooltip 优先命中背包
        for bag = 0, 4 do
            local numSlots = GetContainerNumSlots(bag)
            for slot = 1, numSlots do
                local link = GetContainerItemLink(bag, slot)
                if link then
                    local bagItemID, bagGUID = ExtractItemInfo(link)
                    if bagItemID == itemID and bagGUID == guid then
                        return itemID, guid, bag, slot, false
                    end
                end
            end
        end

        local _, _, equipSlot, isEquipped = ScanEquipmentForItem(itemID, "player")
        if isEquipped and equipSlot then
            return itemID, guid, nil, equipSlot, true
        end

        return itemID, guid, nil, nil, false
    end

    -- 没有 GUID 时，背包 tooltip 优先扫描背包
    local bagGUID, bag, slot = ScanBagsForItem(itemID)
    if bagGUID and bagGUID > 0 then
        return itemID, bagGUID, bag, slot, false
    end

    if not preferEquipped then
        local equipGUID, _, equipSlot, isEquipped = ScanEquipmentForItem(itemID, "player")
        if equipGUID and equipGUID > 0 then
            return itemID, equipGUID, nil, equipSlot, true
        end
    end

    -- 如果都找不到，返回 itemID 和 nil GUID
    return itemID, nil, nil, nil, false
end

local function WrapText(text, maxCharsPerLine)
    if not text or text == "" then return {text} end

    -- 默认每行200个“显示字节”：ASCII算1，中文算2，大约可以放下100个汉字
    maxCharsPerLine = maxCharsPerLine or 220

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
    -- 【修改】新格式：ALL_MODULE_DATA:bag:slot:itemID:guid:base:additional:identDisplay:growth:enhancement:skills:magic:rune:set:huanjing
    -- 旧格式（兼容）：ALL_MODULE_DATA:itemID:guid:base:additional:growth:enhancement:skills:magic:rune:set:huanjing
    if not message:match("^ALL_MODULE_DATA:") then
        return nil
    end

    -- 性能监控：开始解析
    local parseStartTime = GetTime()

    local parts = { strsplit(":", message) }
    if #parts < 3 then
        return nil
    end

    -- 【修改】检测新格式还是旧格式
    -- 新格式：parts[2]是bag（-1/0-11/255），parts[3]是slot（0-36），parts[4]是itemID，parts[5]是guid
    -- 旧格式：parts[2]是itemID（大数字），parts[3]是guid
    local bag, slot, itemID, guid
    local dataStartIndex  -- 数据字段开始的索引

    local firstNum = tonumber(parts[2])
    local secondNum = tonumber(parts[3])
    local thirdNum = tonumber(parts[4])
    local fourthNum = tonumber(parts[5])

    -- 【关键修复】改进格式判断逻辑：
    -- 新格式特征：bag是-1/0-11或255，slot是0-36左右，且有第5个字段（guid）
    -- 旧格式特征：第一个数字是itemID（通常>100），第二个是guid
    -- 判断条件：如果第一个数字<=255且第二个数字<=50且有第5个字段，则是新格式
    local isNewFormat = false
    if firstNum and secondNum and thirdNum and fourthNum then
        -- 新格式有5个数值字段：bag, slot, itemID, guid, 然后是数据
        -- bag范围：-1（银行主仓）、0-11（背包/银行背包）或255（装备栏）
        -- slot范围：0-36（背包槽位）或1-19（装备槽位）
        if (firstNum >= -1 and firstNum <= 255) and (secondNum >= 0 and secondNum <= 50) then
            isNewFormat = true
        end
    end

    if isNewFormat then
        -- 新格式：bag:slot:itemID:guid:...
        bag = firstNum
        slot = secondNum
        itemID = thirdNum
        guid = fourthNum
        dataStartIndex = 6
    else
        -- 旧格式：itemID:guid:...
        bag = nil
        slot = nil
        itemID = firstNum
        guid = secondNum
        dataStartIndex = 4
    end

    if not itemID or not guid or guid == 0 then
        return nil
    end

    -- 解析所有系统数据
    local baseAttributes = parts[dataStartIndex] or ""
    local additionalAttributes = parts[dataStartIndex + 1] or ""
    local maybeDisplayData = parts[dataStartIndex + 2] or ""
    local hasDisplayData = maybeDisplayData:match("^IDDISP|") ~= nil
    local identificationDisplayData = hasDisplayData and maybeDisplayData or ""
    local growthData = parts[dataStartIndex + (hasDisplayData and 3 or 2)] or ""
    local enhancementData = parts[dataStartIndex + (hasDisplayData and 4 or 3)] or ""
    local skillsData = parts[dataStartIndex + (hasDisplayData and 5 or 4)] or ""
    local magicData = parts[dataStartIndex + (hasDisplayData and 6 or 5)] or ""
    local runeData = parts[dataStartIndex + (hasDisplayData and 7 or 6)] or ""

    -- 套装字段可能包含多个":"，幻境字段(最后一个)不包含":"
    -- 策略：先提取最后一个字段作为幻境数据（不包含":"），剩余的拼接为套装数据
    local setDataStartIndex = dataStartIndex + (hasDisplayData and 8 or 7)
    local setData = ""
    local huanjingData = ""
    local tailEndIndex = #parts

    if #parts >= setDataStartIndex and parts[#parts] and parts[#parts]:match("^IDDISP|") then
        identificationDisplayData = parts[#parts]
        tailEndIndex = #parts - 1
    end

    if tailEndIndex >= setDataStartIndex then
        -- 检查最后一个字段是否是幻境数据格式
        -- 兼容旧格式："倍率" / "倍率|属性"
        -- 兼容新格式："模式,倍率" / "模式,倍率|属性"（如 x,10 / +,10 / -,0）
        local lastPart = parts[tailEndIndex]
        local isLegacyHuanjingFormat = lastPart:match("^%d+") and not lastPart:match(":")
        local isModeHuanjingFormat = lastPart:match("^[x%+%-],[%d%.]+") and not lastPart:match(":")
        local isHuanjingFormat = isLegacyHuanjingFormat or isModeHuanjingFormat

        if tailEndIndex >= setDataStartIndex + 1 and isHuanjingFormat then
            -- 有幻境数据：最后一个是幻境，之前的是套装
            huanjingData = lastPart
            if tailEndIndex > setDataStartIndex + 1 then
                setData = table.concat(parts, ":", setDataStartIndex, tailEndIndex - 1)
            else
                setData = parts[setDataStartIndex] or ""
            end
        else
            -- 没有幻境数据或旧格式：setDataStartIndex之后全是套装
            setData = table.concat(parts, ":", setDataStartIndex, tailEndIndex)
        end
    end

    local result = {
        type = "batch",
        itemID = itemID,
        guid = guid,
        bag = bag,      -- 【新增】用于精确匹配pending记录
        slot = slot,    -- 【新增】用于精确匹配pending记录
        systems = {}
    }

    -- 【临时调试 - 已关闭】输出解析的各部分数据
    -- print(string.format("|cffff8800[解析调试]|r 字段数量: %d", #parts))
    -- print(string.format("|cffff8800[解析调试]|r base='%s' additional='%s' growth='%s'",
    --     baseAttributes, additionalAttributes, growthData))
    -- print(string.format("|cffff8800[解析调试]|r enhance='%s' skills='%s' magic='%s'",
    --     enhancementData, skillsData, magicData))
    -- print(string.format("|cffff8800[解析调试]|r rune='%s' set='%s' huanjing='%s'",
    --     runeData, setData, huanjingData))

    -- 解析鉴定系统数据（基础属性、追加属性和名称显示）
    if baseAttributes ~= "" or additionalAttributes ~= "" or identificationDisplayData ~= "" then
        local baseAttrs = {}
        local additionalAttrs = {}
        local displayData = ParseIdentificationDisplayData(identificationDisplayData)
        local hasDisplayConfig = displayData and (
            (displayData.qualityColor and displayData.qualityColor ~= "")
            or (displayData.itemNamePrefix and displayData.itemNamePrefix ~= "")
            or (displayData.itemNameSuffix and displayData.itemNameSuffix ~= "")
            or (displayData.itemNameColors and displayData.itemNameColors ~= "")
            or (displayData.itemBottomDescription and displayData.itemBottomDescription ~= "")
        )

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

        -- 只要有基础属性、追加属性或显示配置，就创建鉴定系统数据
        if #baseAttrs > 0 or #additionalAttrs > 0 or hasDisplayConfig then
            result.systems.identification = {
                type = "identification",
                itemID = itemID,
                guid = guid,
                baseAttributes = baseAttrs,
                additionalAttributes = additionalAttrs,
                qualityColor = displayData and displayData.qualityColor or "",
                itemNamePrefix = displayData and displayData.itemNamePrefix or "",
                itemNameSuffix = displayData and displayData.itemNameSuffix or "",
                itemNameColors = displayData and displayData.itemNameColors or "",
                itemBottomDescription = displayData and displayData.itemBottomDescription or ""
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

            local setId = tonumber(sParts[1])
            if setId and setId > 0 then
                result.systems.sets = {
                    type = "sets",
                    itemID = itemID,
                    guid = guid,
                    setId = setId,
                    setName = sParts[2] or "套装",  -- 如果没有setName，使用默认值
                    attributes = attrs,
                    effects = effects
                }
            end
        end
    end

    -- 解析幻境系统数据（格式：multiplier 或 multiplier|enhancedAttrs）
    -- enhancedAttrs格式："attrType value enhanced,attrType value enhanced"
    if huanjingData ~= "" then
        local hjParts = { strsplit("|", huanjingData) }
        local mode = "x"
        local multiplier = tonumber(hjParts[1])

        if not multiplier and hjParts[1] then
            local modeText, multiplierText = string.match(hjParts[1], "^([x%+%-]),([%d%.]+)$")
            if modeText and multiplierText then
                mode = NormalizeHuanJingMode(modeText)
                multiplier = tonumber(multiplierText) or 1
            end
        end

        multiplier = multiplier or 1

        if HasHuanJingEffect(multiplier, mode) then
            local enhancedAttrs = {}

            -- 如果有增强属性数据
            if #hjParts >= 2 and hjParts[2] ~= "" then
                for pair in string.gmatch(hjParts[2], "([^,]+)") do
                    -- 格式：attrType value enhancedValue（三个数字）
                    local attrType, value, enhanced = pair:match("(%d+)%s+([%-]?%d+)%s+([%-]?%d+)")
                    if attrType and value and enhanced then
                        table.insert(enhancedAttrs, {
                            type = tonumber(attrType),
                            value = tonumber(value),
                            enhanced = tonumber(enhanced)
                        })
                    end
                end
            end

            result.systems.huanjing = {
                type = "huanjing",
                itemID = itemID,
                guid = guid,
                multiplier = multiplier,
                mode = mode,
                enhancedAttributes = enhancedAttrs
            }

            -- 【关键】同时更新幻境缓存，保持与原有幻境查询系统的兼容性
            local key = "G:" .. itemID .. ":" .. guid
            HuanJingState.cache[key] = {
                itemID = itemID,
                guid = guid,
                multiplier = multiplier,
                mode = mode,
                attributeData = hjParts[2] or "",  -- 原始属性数据字符串
                identificationData = "",
                timestamp = GetTime()
            }
        end
    end

    -- 【临时调试 - 已关闭】统计解析的系统数量
    -- local sysCount = 0
    -- for sysName, _ in pairs(result.systems) do
    --     sysCount = sysCount + 1
    --     print(string.format("|cff00ff00[解析调试]|r 系统 '%s' 解析成功", sysName))
    -- end
    -- print(string.format("|cff00ff00[解析调试]|r 总共解析 %d 个系统", sysCount))

    return result
end


-- ============================================================================
-- 渲染器 - 统一的渲染逻辑
-- ============================================================================

local function FormatSignedValue(value)
    local numericValue = tonumber(value) or 0
    local absValue = math.abs(numericValue)
    if numericValue < 0 then
        return string.format("- %d", absValue)
    end
    return string.format("+ %d", absValue)
end

NormalizeHuanJingMode = function(mode)
    if mode == "+" then
        return "+"
    end
    if mode == "-" or mode == "n" then
        return "-"
    end
    return "x"
end

HasHuanJingEffect = function(multiplier, mode)
    mode = NormalizeHuanJingMode(mode)
    multiplier = tonumber(multiplier) or 0
    if mode == "-" then
        return false
    end
    if mode == "+" then
        return multiplier > 0
    end
    return multiplier > 1
end

CalculateHuanJingEnhancedValue = function(baseValue, multiplier, mode)
    baseValue = tonumber(baseValue) or 0
    multiplier = tonumber(multiplier) or 0
    mode = NormalizeHuanJingMode(mode)
    if mode == "-" then
        return baseValue
    end
    if mode == "+" then
        return baseValue + multiplier
    end
    return baseValue * multiplier
end

FormatHuanJingModeText = function(multiplier, mode)
    mode = NormalizeHuanJingMode(mode)
    multiplier = tonumber(multiplier) or 0
    if mode == "+" then
        return string.format("%s+%d|r", COLOR_PINK, multiplier)
    end
    return string.format("%s倍率x%d|r", COLOR_PINK, multiplier)
end

FormatTooltipStatLine = function(name, value, multiplier, mode)
    local label = name or "属性"
    local baseText = string.format("%s %s", FormatSignedValue(value), label)
    if HasHuanJingEffect(multiplier, mode) then
        return string.format("|cff00ff00%s  |r%s", baseText, FormatHuanJingModeText(multiplier, mode))
    end
    return string.format("|cff00ff00%s|r", baseText)
end

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
            tooltip:AddLine(FormatTooltipStatLine(name, attr.value))
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
    local hasBaseAttributes = data.baseAttributes and #data.baseAttributes > 0
    local hasAdditionalAttributes = data.additionalAttributes and #data.additionalAttributes > 0
    if not hasBaseAttributes and not hasAdditionalAttributes then return end

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
                local displayText = FormatTooltipStatLine(name, attr.value)
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
                local displayText = FormatTooltipStatLine(name, attr.value)
                tooltip:AddLine(displayText)
            end
        end
    else
        -- 如果没找到官方属性位置，直接在底部添加
        tooltip:AddLine(" ")
        tooltip:AddLine("|cFFFFFF00基础属性|r")

        for _, attr in ipairs(data.baseAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local displayText = FormatTooltipStatLine(name, attr.value)
            tooltip:AddLine(displayText)
        end
    end

    -- 显示追加属性（如果有）
    if hasAdditionalAttributes then
        tooltip:AddLine(" ")
        tooltip:AddLine("|cff00ff00追加属性|r")

        for _, attr in ipairs(data.additionalAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local displayText = FormatTooltipStatLine(name, attr.value)
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
            tooltip:AddLine(FormatTooltipStatLine(name, attr.value))
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
        local lines = WrapText(text, 220)
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
            tooltip:AddLine("  " .. FormatTooltipStatLine(attrName, attr.value))
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
-- 【修改】添加可选的key参数，如果提供则直接使用，否则使用GUID生成（兼容性）
local function CacheData(itemID, guid, data, existingKey)
    if not itemID then return end

    -- 如果提供了existingKey则直接使用，否则使用GUID生成（兼容旧逻辑）
    local key
    if existingKey then
        key = existingKey
    elseif guid and guid > 0 then
        key = MakeKey(itemID, guid, nil, nil, false)
    else
        -- 没有key也没有GUID，无法缓存
        DebugPrint(string.format("[CacheData] 无法缓存: itemID=%d, 无有效key或GUID", itemID))
        return
    end

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

    -- 正在查询中，但如果超过1秒仍未返回，允许重新查询（避免用户感觉无响应）
    if State.pending[key] then
        local pending = State.pending[key]
        if pending.queryStartTime then
            local waitTime = now - pending.queryStartTime
            local staleThreshold = math.max((DB.timeout or 15), 5)
            if waitTime < staleThreshold then
                return true, "pending_active"
            else
                State.pending[key] = nil
                return false
            end
        end
        return true, "pending_no_time"
    end

    -- 已移除最近查询时间限制，允许立即重新查询

    -- 检查是否所有启用的系统都已有缓存数据
    if State.cache[key] then
        local cached = State.cache[key]

        -- 【修改】不再因为isEmpty而清除缓存
        -- 原因：我们现在使用bag:slot:itemID作为缓存键，已经足够唯一标识物品
        -- isEmpty表示物品没有自定义属性，这是有效的缓存数据，不需要重新查询
        if cached.isEmpty then
            return true, "cache_isEmpty"
        end

        local allSystemsCached = true
        local missingCount = 0
        local hasOnlyEmpty = true  -- 检查是否所有系统都是isEmpty

        -- 检查每个启用的系统是否都有数据
        for systemName, enabled in pairs(DB.systems) do
            if enabled then
                local systemData = cached.systems and cached.systems[systemName]
                if not systemData then
                    allSystemsCached = false
                    missingCount = missingCount + 1
                else
                    -- 如果至少有一个系统不是isEmpty，说明有真实数据
                    if not systemData.isEmpty then
                        hasOnlyEmpty = false
                    end
                end
            end
        end

        -- 【调试日志】
        if DB.debug then
            print(string.format("|cff00ffff[ShouldSkipQuery]|r %s allSystemsCached=%s, missingCount=%d, hasOnlyEmpty=%s",
                key, tostring(allSystemsCached), missingCount, tostring(hasOnlyEmpty)))
        end

        -- 只有当所有启用的系统都有数据时才检查缓存时间
        if allSystemsCached then
            -- 【修改】不再因为isEmpty而清除缓存
            -- 原因：我们现在使用bag:slot:itemID作为缓存键，已经足够唯一标识物品
            -- isEmpty表示物品没有自定义属性，这是有效的缓存数据
            if hasOnlyEmpty then
                -- 所有系统都是isEmpty，但这是有效的缓存，跳过查询
                if DB.debug then
                    print(string.format("|cffff8800[ShouldSkipQuery]|r %s 所有系统isEmpty，跳过", key))
                end
                return true, "all_systems_empty"
            end

            local cacheAge = State.cacheTime[key] and (now - State.cacheTime[key]) or 999

            -- 如果缓存超过配置的有效期，清除缓存并允许重新查询
            if cacheAge >= DB.cacheExpiration then
                if DB.debug then
                    print(string.format("|cff00ff00[ShouldSkipQuery]|r %s 缓存过期(%.0fs)，清除并允许查询", key, cacheAge))
                end
                State.cache[key] = nil
                State.cacheTime[key] = nil
                return false  -- 允许查询
            else
                if DB.debug then
                    print(string.format("|cffff8800[ShouldSkipQuery]|r %s 缓存完整有效(%.0fs)，跳过", key, cacheAge))
                end
                return true, "cache_complete_valid"
            end
        else
            -- 缓存不完整，允许查询
            if DB.debug then
                print(string.format("|cff00ff00[ShouldSkipQuery]|r %s 缺少%d个系统，允许查询", key, missingCount))
            end
            -- 【关键修复】不完整缓存应该返回false允许查询
            -- 继续检查冷却期
        end
    else
        if DB.debug then
            print(string.format("|cff00ff00[ShouldSkipQuery]|r %s 无缓存，允许查询", key))
        end
    end

    -- 在冷却期
    if State.noDataUntil[key] and now < State.noDataUntil[key] then
        local cooldown = State.noDataUntil[key] - now
        if DB.debug then
            print(string.format("|cffff8800[ShouldSkipQuery]|r %s 在冷却期(%.1fs)，跳过", key, cooldown))
        end
        return true, "cooldown"
    end

    return false
end

-- ============================================================================
-- 查询管理
-- ============================================================================

-- 发送查询（使用Addon消息）
-- 【修改】使用 bag:slot:itemID 作为查询参数，而不是GUID
-- 【新增】guid参数用于在响应匹配时精确识别物品
-- 【新增】isInspectOther参数：观察其他玩家时使用GUID格式查询（服务器无法访问其他玩家的背包）
local function DoSendQuery(itemID, bag, slot, key, guid, fingerprint, isInspectOther)
    local now = GetTime()

    -- 立即标记为查询中，防止重复发送
    State.queryId = State.queryId + 1
    local queryId = State.queryId

    State.lastQuery[key] = now
    State.pending[key] = {
        started = now,
        systems = {},
        queryId = queryId,
        queryStartTime = now,
        -- 【新增】保存位置信息，用于响应时匹配
        bag = bag,
        slot = slot,
        itemID = itemID,
        -- 【关键】保存GUID，用于服务器响应时精确匹配正确的pending记录
        guid = guid,
        -- 【新增】保存物品链接指纹，避免同槽位换装后串缓存
        fingerprint = fingerprint,
        -- 【新增】标记是否为观察其他玩家
        isInspectOther = isInspectOther
    }

    -- 【新增】建立itemID到缓存键的映射，用于服务器响应时找到正确的pending记录
    -- 注意：同一个itemID可能有多个实例（在不同槽位），所以使用列表
    if not State.itemIdToKeys then
        State.itemIdToKeys = {}
    end
    if not State.itemIdToKeys[itemID] then
        State.itemIdToKeys[itemID] = {}
    end
    -- 添加映射（如果不存在）
    local found = false
    for _, k in ipairs(State.itemIdToKeys[itemID]) do
        if k == key then
            found = true
            break
        end
    end
    if not found then
        table.insert(State.itemIdToKeys[itemID], key)
    end

    -- 获取已缓存的系统
    local cached = State.cache[key]
    local cachedSystems = {}
    if cached and cached.systems then
        for systemName, _ in pairs(cached.systems) do
            cachedSystems[systemName] = true
        end
    end

    -- 使用Addon消息格式发送查询（避免聊天速率限制）
    -- 【关键修改】根据场景选择查询格式：
    -- 1. 观察其他玩家装备时：使用 QUERY:itemID:guid 格式（服务器无法访问其他玩家的背包）
    -- 2. 查询自己的装备/背包时：使用 QUERY:bag:slot:itemID 格式
    local addonMessage
    if isInspectOther and guid and guid > 0 then
        -- 观察其他玩家：使用旧的GUID格式查询
        addonMessage = string.format("QUERY:%d:%d", itemID, guid)
        if DB.debug then
            print(string.format("|cff00ffff[DoSendQuery]|r 观察其他玩家，使用GUID格式: %s", addonMessage))
        end
    else
        -- 自己的装备/背包：使用位置格式查询
        addonMessage = string.format("QUERY:%d:%d:%d", bag, slot, itemID)
        if DB.debug then
            print(string.format("|cff00ffff[DoSendQuery]|r 自己的物品，使用位置格式: %s", addonMessage))
        end
    end

    -- 标记所有系统为查询中
    TraceLog(itemID, "DoSendQuery queryId=%s key=%s bag=%s slot=%s guid=%s inspect=%s msg=%s",
        tostring(queryId), tostring(key), tostring(bag), tostring(slot), tostring(guid), tostring(isInspectOther), tostring(addonMessage))

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

    -- 【临时调试 - 已关闭】强制输出日志，不受debug开关控制
    -- print(string.format("|cff00ffff[查询发送]|r Q%d 消息: %s (pending=%d)", queryId, addonMessage, pendingCount))

    -- 记录发送前的时间
    local sendBeforeTime = GetTime()

    -- 【临时调试 - 已关闭】
    -- if SendAddonMessage then
    --     print(string.format("|cff00ff00[查询发送]|r Q%d SendAddonMessage成功", queryId))
    -- elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
    --     print(string.format("|cff00ff00[查询发送]|r Q%d C_ChatInfo.SendAddonMessage成功", queryId))
    -- else
    --     print(string.format("|cffff0000[查询发送]|r Q%d 无可用的SendAddonMessage API！", queryId))
    -- end

    -- 使用SendAddonMessage发送（不受聊天速率限制）
    -- 兼容WoW 3.3.5和零售版API
    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, addonMessage, "WHISPER", UnitName("player"))
    end

    -- 记录发送后的时间（仅用于调试，不输出）
    local sendAfterTime = GetTime()
    local sendDuration = (sendAfterTime - sendBeforeTime) * 1000  -- 转换为毫秒
    
    -- 已移除发送耗时和pending查询警告输出，避免干扰用户

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

-- 发送查询（已移除限流机制）
-- 【修改】使用 bag:slot:itemID 作为查询参数，而不是GUID
-- 【新增】guid参数用于精确匹配服务器响应
-- 【新增】isInspectOther参数：观察其他玩家时使用GUID格式查询
-- 【新增】isChatLink参数：聊天框链接时允许使用uniqueId查询
-- bag=255 表示装备栏物品
local function SendQuery(itemID, bag, slot, guid, fingerprint, isInspectOther, isChatLink)
    -- 【排查日志2】SendQuery入口
    -- 【关键检查】必须有有效的背包位置才能发送查询
    if not itemID then
        return
    end

    -- 【修复】聊天框链接：当物品不在玩家身上时，允许使用uniqueId查询
    if bag == nil or slot == nil then
        -- 检查是否可以使用GUID格式查询（用于聊天框链接中不在玩家身上的物品）
        if isChatLink and guid and guid > 0 then
            -- 使用GUID格式查询
            local key = string.format("G:%d:%d", itemID, guid)

            local shouldSkip = ShouldSkipQuery(key)
            if shouldSkip then
                return
            end

            if DB.debug then
                print(string.format("|cff00ff00[SendQuery]|r 聊天框链接使用GUID查询 itemID=%d guid=%d key=%s",
                    itemID, guid, key))
            end

            -- 使用GUID格式发送查询（设置isInspectOther=true以使用GUID格式）
            DoSendQuery(itemID, 0, 0, key, guid, fingerprint, true)
            return
        end

        -- 【调试日志】
        if DB.debug then
            print(string.format("|cffff0000[SendQuery]|r 背包位置无效 itemID=%d bag=%s slot=%s，跳过",
                itemID, tostring(bag), tostring(slot)))
        end
        return
    end

    local key = MakeKey(itemID, nil, bag, slot, bag == 255)
    local now = GetTime()

    -- 先检查是否应该跳过（缓存命中等）
    local shouldSkip = ShouldSkipQuery(key)
    if shouldSkip then
        -- ShouldSkipQuery已经输出了详细日志，这里不再重复
        -- 如果是因为pending而跳过，且等待时间过长，显示警告
        if State.pending[key] and State.pending[key].queryStartTime then
            local waitTime = now - State.pending[key].queryStartTime
            if waitTime > 3 and (not State.pending[key].warnShown or now - State.pending[key].warnShown > 5) then
                State.pending[key].warnShown = now
                DebugPrint(string.format("[警告] itemID=%d bag=%d slot=%d 已等待%.1f秒，服务器响应缓慢",
                    itemID, bag, slot, waitTime))
            end
        end
        return
    end

    -- 【调试日志】实际发送
    if DB.debug then
        print(string.format("|cff00ff00[SendQuery执行]|r itemID=%d bag=%d slot=%d guid=%s key=%s isInspectOther=%s",
            itemID, bag, slot, tostring(guid), key, tostring(isInspectOther)))
    end

    -- 发送查询，传递GUID和isInspectOther用于选择正确的查询格式
    DoSendQuery(itemID, bag, slot, key, guid, fingerprint, isInspectOther)
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
    if meta and meta.officialStats and hjData and HasHuanJingEffect(hjData.multiplier, hjData.mode) then
        local stats = meta.officialStats
        local mult = hjData.multiplier

        local baseStatConfig = {
            { key = "ITEM_MOD_STRENGTH_SHORT",  name = ITEM_MOD_STRENGTH_SHORT or "力量" },
            { key = "ITEM_MOD_AGILITY_SHORT",   name = ITEM_MOD_AGILITY_SHORT or "敏捷" },
            { key = "ITEM_MOD_INTELLECT_SHORT", name = ITEM_MOD_INTELLECT_SHORT or "智力" },
            { key = "ITEM_MOD_STAMINA_SHORT",   name = ITEM_MOD_STAMINA_SHORT or "耐力" },
            { key = "ITEM_MOD_SPIRIT_SHORT",    name = ITEM_MOD_SPIRIT_SHORT or "精神" },
        }

        for _, conf in ipairs(baseStatConfig) do
            local amount = stats[conf.key]
            if amount and amount ~= 0 then
                table.insert(officialBaseAttributes, {
                    name = conf.name,
                    value = amount,
                    multiplier = mult,
                    mode = hjData.mode
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

        for _, attr in ipairs(officialBaseAttributes) do
            local baseValue = attr.value
            local mult = attr.multiplier
            local mode = attr.mode
            tooltip:AddLine(FormatTooltipStatLine(attr.name, baseValue, mult, mode), 0, 1, 0)
        end

        for index, attr in ipairs(baseAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local hjInfo = hjBaseInfo[index]
            local leftText

            if hjData and HasHuanJingEffect(hjData.multiplier, hjData.mode) then
                local mult = hjData.multiplier
                local original = hjInfo and (hjInfo.original or attr.value) or attr.value
                leftText = FormatTooltipStatLine(name, original, mult, hjData.mode)
            else
                leftText = FormatTooltipStatLine(name, attr.value)
            end

            tooltip:AddLine(leftText, 0, 1, 0)
        end
    end

    if #additionalAttributes > 0 then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "追加属性" .. DB.colors.reset)

        for index, attr in ipairs(additionalAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            local hjInfo = hjAdditionalInfo[index]
            local leftText

            if hjData and HasHuanJingEffect(hjData.multiplier, hjData.mode) then
                local mult = hjData.multiplier
                local original = hjInfo and (hjInfo.original or attr.value) or attr.value
                leftText = FormatTooltipStatLine(name, original, mult, hjData.mode)
            else
                leftText = FormatTooltipStatLine(name, attr.value)
            end

            tooltip:AddLine(leftText, 0, 1, 0)
        end
    end

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

                if hjData and HasHuanJingEffect(hjData.multiplier, hjData.mode) then
                    local mult = hjData.multiplier
                    local baseValue = attr.value or 0
                    leftText = FormatTooltipStatLine(name, baseValue, mult, hjData.mode)
                else
                    leftText = FormatTooltipStatLine(name, attr.value)
                end

                tooltip:AddLine(leftText, 0, 1, 0)
            end
        end
    end

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

                if hjData and HasHuanJingEffect(hjData.multiplier, hjData.mode) then
                    local mult = hjData.multiplier
                    local baseValue = attr.value or 0
                    leftText = FormatTooltipStatLine(name, baseValue, mult, hjData.mode)
                else
                    leftText = FormatTooltipStatLine(name, attr.value)
                end

                tooltip:AddLine(leftText, 0, 1, 0)
            end
        end
    end

    meta.rendered.identification = true
    meta.rendered.enhancement = true
    meta.rendered.growth = true

    tooltip:Show()
end

-- 渲染提示框
-- 注意：此函数仅负责渲染缓存中已有的数据，不发起查询
-- 查询应该在OnTooltipSetItem中通过SendQuery发起
RenderTooltip = function(tooltip, itemID, guid, bag, slot)
    if not itemID then return end

    -- 优先使用位置键（bag/slot），其次使用GUID键
    local key
    if bag ~= nil and slot ~= nil then
        key = MakeKey(itemID, nil, bag, slot, bag == 255)
    elseif guid and guid > 0 then
        if IsTooltipFromChat and IsTooltipFromChat(tooltip) then
            key = MakeKey(itemID, guid, nil, nil, false)
        else
            local meta = State.tooltips[tooltip]
            if meta and meta.key then
                key = meta.key
            else
                TraceLog(itemID, "Render skip guid-fallback tooltip=%s guid=%s", tostring(tooltip and tooltip.GetName and tooltip:GetName() or tooltip), tostring(guid))
                return
            end
        end
    else
        -- 没有位置也没有GUID时，尝试从tooltip元数据获取缓存键
        local meta = State.tooltips[tooltip]
        if meta and meta.key then
            key = meta.key
        else
            -- 无法确定缓存键，跳过渲染
            return
        end
    end

    local meta = GetTooltipMeta(tooltip, key)

    -- 尝试多种缓存键格式查找缓存数据
    local cached = State.cache[key]

    local now = GetTime()

    -- 幻境系统查询：通过系统消息单独获取倍率和增强后的属性
    if guid and guid > 0 then
        HuanJingRequest(itemID, guid, key, now)
    end

    -- 【移除】不再在RenderTooltip中发起查询
    -- 查询应该在OnTooltipSetItem中通过SendQuery(itemID, bag, slot)发起

    -- 检查缓存是否完整（用于显示"正在加载"提示）
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

    -- 检查是否正在查询中
    local isPending = State.pending[key] ~= nil
    local hasRealCustomData = false

    if cached and cached.systems then
        for _, systemData in pairs(cached.systems) do
            if systemData and not systemData.isEmpty then
                hasRealCustomData = true
                break
            end
        end
    end


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

    -- 如果正在查询中且缓存不完整，在底部显示提示
    if isPending and not hasCompleteCache and not hasRealCustomData then
        local waitTime = 0
        if State.pending[key] and State.pending[key].queryStartTime then
            waitTime = now - State.pending[key].queryStartTime
        end
        if waitTime > 0.5 then
            -- 超过0.5秒才显示提示，避免闪烁
            tooltip:AddLine(" ")
            tooltip:AddLine(string.format("%s正在加载更多属性...%s", 
                DB.colors.special, DB.colors.reset))
        end
    end

    -- 如果缓存标记为无数据，直接返回不显示任何额外内容

    if hasRealCustomData and isPending then
        TraceLog(itemID, "Render clearPending key=%s because real custom data already exists", tostring(key))
        State.pending[key] = nil
        State.noDataUntil[key] = nil
        isPending = false
    end

    local renderSystems = CollectTraceKeys(cached and cached.systems)
    local renderSignature = string.format("%s|%s|%s|%s|%s|%s", tostring(key), tostring(isPending), tostring(hasCompleteCache), tostring(hasRealCustomData), tostring(cached and cached.noData), renderSystems)
    TraceLogDedup("RenderTooltip:" .. tostring(key), itemID, renderSignature,
        "Render key=%s bag=%s slot=%s pending=%s complete=%s real=%s noData=%s systems=%s",
        tostring(key), tostring(bag), tostring(slot), tostring(isPending), tostring(hasCompleteCache), tostring(hasRealCustomData), tostring(cached and cached.noData), renderSystems)

    if cached.noData then
        return
    end

    ApplyIdentificationDisplay(tooltip, cached, meta)

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

    ApplyIdentificationBottomDescription(tooltip, cached, meta)

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
    if message:match("^QUERY:") or message:match("^INSPECT_ITEM_GUID:") then
        return
    end

    -- 处理检查装备GUID的响应
    if message:match("^INSPECT_ITEM_GUID_RESPONSE:") then
        local playerName, slot, itemID, guid = message:match("^INSPECT_ITEM_GUID_RESPONSE:([^:]+):(%d+):(%d+):(%d+)$")

        -- 【调试日志】INSPECT_ITEM_GUID_RESPONSE收到
        if DB.debug then
            print(string.format("|cff00ff00[INSPECT_GUID响应]|r playerName=%s, slot=%s, itemID=%s, guid=%s",
                tostring(playerName), tostring(slot), tostring(itemID), tostring(guid)))
        end

        if playerName and slot and itemID and guid then
            slot = tonumber(slot)
            itemID = tonumber(itemID)
            guid = tonumber(guid)

            local cacheKey = string.format("%s:%d:%d", playerName, slot, itemID)

            -- 清除请求节流记录
            InspectGuidPendingRequests[cacheKey] = nil

            -- 只缓存有效的GUID（大于0）
            if guid and guid > 0 then
                InspectGuidCache[cacheKey] = guid

                -- 使用获取到的GUID重新渲染提示框
                -- 注意：其他玩家装备使用 bag=255, slot=装备槽位
                -- 【关键】isInspectOther=true，使用GUID格式查询（服务器无法访问其他玩家背包）
                if State.pendingInspectTooltips then
                    for tooltip, pendingInfo in pairs(State.pendingInspectTooltips) do
                        if tooltip:IsShown() and pendingInfo.itemID == itemID then
                            SendQuery(itemID, 255, slot, guid, nil, true)  -- isInspectOther=true

                            C_Timer.After(0.1, function()
                                if tooltip:IsShown() then
                                    RenderTooltip(tooltip, itemID, guid, 255, slot)  -- 传递bag和slot参数
                                end
                            end)

                            State.pendingInspectTooltips[tooltip] = nil
                        end
                    end
                end

                -- 检查State.tooltips中的提示框
                for tooltip, meta in pairs(State.tooltips) do
                    if tooltip:IsShown() then
                        local _, itemLink = tooltip:GetItem()
                        if itemLink then
                            local tooltipItemID = ExtractItemInfo(itemLink)
                            if tooltipItemID == itemID then
                                SendQuery(itemID, 255, slot, guid, nil, true)  -- isInspectOther=true

                                C_Timer.After(0.1, function()
                                    if tooltip:IsShown() then
                                        RenderTooltip(tooltip, itemID, guid, 255, slot)  -- 传递bag和slot参数
                                    end
                                end)
                            end
                        end
                    end
                end
            end
        end
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

    -- 【新增】处理待鉴定物品列表响应
    -- 新格式(分片): PENDING_LIST:总数:包序号:总包数:数据
    -- 旧格式(兼容): PENDING_LIST:count:数据
    -- 【修改】使用背包号+槽位+物品ID作为缓存键，而不是GUID
    if message:match("^PENDING_LIST:") or message:match("^RESPONSE:PENDING_LIST:") then
        local dataMessage = message:gsub("^RESPONSE:", "")

        -- 尝试解析新的分片格式: PENDING_LIST:总数:包序号:总包数:数据
        local totalCount, packetNum, totalPackets, itemsData = dataMessage:match("^PENDING_LIST:(%d+):(%d+):(%d+):(.*)$")

        if totalCount and packetNum and totalPackets then
            -- 分片格式
            totalCount = tonumber(totalCount)
            packetNum = tonumber(packetNum)
            totalPackets = tonumber(totalPackets)

            -- 初始化分片缓存
            if not PendingIdentifyState.packetBuffer then
                PendingIdentifyState.packetBuffer = {}
            end

            -- 如果是第一个包，清空缓存和分片缓存
            if packetNum == 1 then
                PendingIdentifyState.cache = {}
                PendingIdentifyState.packetBuffer = {}
                PendingIdentifyState.expectedPackets = totalPackets
                PendingIdentifyState.receivedPackets = 0
            end

            -- 保存当前包的数据
            local packetBuffer = PendingIdentifyState.packetBuffer or {}
            PendingIdentifyState.packetBuffer = packetBuffer
            if not packetNum then
                return
            end
            packetBuffer[packetNum] = itemsData or ""
            PendingIdentifyState.receivedPackets = (PendingIdentifyState.receivedPackets or 0) + 1

            -- 检查是否收到所有包
            if PendingIdentifyState.receivedPackets >= totalPackets then
                -- 合并所有分片数据
                local allItemsData = ""
                for i = 1, totalPackets do
                    if PendingIdentifyState.packetBuffer[i] then
                        if allItemsData ~= "" then
                            allItemsData = allItemsData .. ";"
                        end
                        allItemsData = allItemsData .. PendingIdentifyState.packetBuffer[i]
                    end
                end

                -- 解析合并后的物品列表
                if allItemsData ~= "" then
                    for itemStr in string.gmatch(allItemsData, "([^;]+)") do
                        local bag, slot, itemId, mode, multiplier, groupId = itemStr:match("(%d+),(%d+),(%d+),([x%+%-]),(%d+),(%d+)")
                        if not bag then
                            bag, slot, itemId, multiplier, groupId = itemStr:match("(%d+),(%d+),(%d+),(%d+),(%d+)")
                            mode = "x"
                        end
                        if bag and slot and itemId then
                            local cacheKey = string.format("%s:%s:%s", bag, slot, itemId)
                            if not IsPendingIdentifySuppressed(tonumber(bag), tonumber(slot), tonumber(itemId)) then
                                PendingIdentifyState.cache[cacheKey] = {
                                    bag = tonumber(bag),
                                    slot = tonumber(slot),
                                    itemId = tonumber(itemId),
                                    multiplier = tonumber(multiplier) or 1,
                                    mode = NormalizeHuanJingMode(mode),
                                    groupId = tonumber(groupId) or 0,
                                    timestamp = GetTime()
                                }
                            end
                        end
                    end
                end

                -- 清理分片缓存
                PendingIdentifyState.packetBuffer = nil
                PendingIdentifyState.expectedPackets = nil
                PendingIdentifyState.receivedPackets = nil
                PendingIdentifyState.querying = false
                PendingIdentifyState.refreshing = false
                PendingIdentifyState.lastQuery = GetTime()

                RefreshVisiblePendingIdentifyTooltips()

                if PendingIdentifyState.refreshQueued then
                    PendingIdentifyState.refreshQueued = false
                    SchedulePendingIdentifyRefresh("afterPendingListMultipart", 0)
                end
            end
        else
            -- 旧格式（单包）或空列表: PENDING_LIST:count:数据 或 PENDING_LIST:0:
            local countStr, itemsData2 = dataMessage:match("^PENDING_LIST:(%d+):(.*)$")
            local count = tonumber(countStr) or 0

            -- 清空旧缓存
            PendingIdentifyState.cache = {}
            PendingIdentifyState.querying = false
            PendingIdentifyState.refreshing = false  -- 【新增】刷新完成

            if count > 0 and itemsData2 and itemsData2 ~= "" then
                -- 解析物品列表，新格式：bag,slot,itemId,mode,multiplier,groupId
                for itemStr in string.gmatch(itemsData2, "([^;]+)") do
                    local bag, slot, itemId, mode, multiplier, groupId = itemStr:match("(%d+),(%d+),(%d+),([x%+%-]),(%d+),(%d+)")
                    if not bag then
                        bag, slot, itemId, multiplier, groupId = itemStr:match("(%d+),(%d+),(%d+),(%d+),(%d+)")
                        mode = "x"
                    end
                    if bag and slot and itemId then
                        local cacheKey = string.format("%s:%s:%s", bag, slot, itemId)
                        if not IsPendingIdentifySuppressed(tonumber(bag), tonumber(slot), tonumber(itemId)) then
                            PendingIdentifyState.cache[cacheKey] = {
                                bag = tonumber(bag),
                                slot = tonumber(slot),
                                itemId = tonumber(itemId),
                                multiplier = tonumber(multiplier) or 1,
                                mode = NormalizeHuanJingMode(mode),
                                groupId = tonumber(groupId) or 0,
                                timestamp = GetTime()
                            }
                        end
                    end
                end
            end

            PendingIdentifyState.lastQuery = GetTime()

            RefreshVisiblePendingIdentifyTooltips()

            if PendingIdentifyState.refreshQueued then
                PendingIdentifyState.refreshQueued = false
                SchedulePendingIdentifyRefresh("afterPendingListSingle", 0)
            end
        end

        return
    end

    -- 【新增】处理鉴定结果响应 - 鉴定成功后从缓存中移除该物品
    -- 新格式: IDENTIFY_RESULT:SUCCESS:bag:slot:itemId:multiplier
    if message:match("^IDENTIFY_RESULT:SUCCESS:") or message:match("^RESPONSE:IDENTIFY_RESULT:SUCCESS:") then
        local dataMessage = message:gsub("^RESPONSE:", "")
        local bag, slot, itemId = dataMessage:match("^IDENTIFY_RESULT:SUCCESS:(%d+):(%d+):(%d+)")
        if bag and slot and itemId then
            local cacheKey = string.format("%s:%s:%s", bag, slot, itemId)
            SuppressPendingIdentify(tonumber(bag), tonumber(slot), tonumber(itemId))
            -- 清除待鉴定状态缓存
            if PendingIdentifyState.cache[cacheKey] then
                PendingIdentifyState.cache[cacheKey] = nil
            end

            -- 【关键修复】同时清除主缓存State.cache，这样重新悬停时会从服务器查询新的鉴定数据
            local stateCacheKey = string.format("P:%s:%s:%s", bag, slot, itemId)
            if State.cache[stateCacheKey] then
                State.cache[stateCacheKey] = nil
                State.cacheTime[stateCacheKey] = nil
                State.pending[stateCacheKey] = nil
            end
            State.lastQuery[stateCacheKey] = nil
            State.noDataUntil[stateCacheKey] = nil
            if State.itemIdToKeys and State.itemIdToKeys[tonumber(itemId)] then
                local mappedKeys = State.itemIdToKeys[tonumber(itemId)]
                for idx = #mappedKeys, 1, -1 do
                    if mappedKeys[idx] == stateCacheKey then
                        table.remove(mappedKeys, idx)
                    end
                end
                if #mappedKeys == 0 then
                    State.itemIdToKeys[tonumber(itemId)] = nil
                end
            end
        end

        -- 【关键】延迟3秒后重新查询待鉴定列表，等待服务器清理数据库记录
        -- 使用OnUpdate延迟（兼容WoW 3.3.5）
        local delayFrame = CreateFrame("Frame")
        delayFrame.elapsed = 0
        delayFrame:SetScript("OnUpdate", function(self, elapsed)
            self.elapsed = self.elapsed + elapsed
            if self.elapsed >= 3 then
                self:SetScript("OnUpdate", nil)
                -- 重置查询状态，强制重新查询
                PendingIdentifyState.lastQuery = 0
                PendingIdentifyState.querying = false
                QueryPendingIdentifyList()
            end
        end)

        -- 不return，让后续处理继续
    end

    -- 【新增】处理强化成功响应 - 强化成功后清除缓存以便显示最新强化等级
    -- 服务器发送格式: ITEMENHANCE|ENHANCED|itemId|itemGuid|level|statValues
    -- 注意：消息可能在prefix中（已在上方处理分离）

    -- 匹配多种可能的格式
    if message:match("^ENHANCED|") or message:match("^ENHANCED:") or message:match("^ITEMENHANCE|ENHANCED|") then
        local parts = { strsplit("|", message) }
        -- 格式可能是: ENHANCED|itemId|itemGuid|level|statValues
        -- 或者: ITEMENHANCE|ENHANCED|itemId|itemGuid|level|statValues

        -- 找到ENHANCED的位置
        local startIndex = 1
        for i, part in ipairs(parts) do
            if part == "ENHANCED" then
                startIndex = i
                break
            end
        end

        -- 从ENHANCED后面开始解析
        local itemId = tonumber(parts[startIndex + 1])
        local itemGuid = tonumber(parts[startIndex + 2])
        -- parts[startIndex + 3] 是强化等级，暂不使用

        if itemId and itemGuid then
            -- 清除该物品的所有相关缓存
            -- 1. 清除GUID格式的缓存
            local guidKey = string.format("G:%d:%d", itemId, itemGuid)
            if State.cache[guidKey] then
                State.cache[guidKey] = nil
                State.cacheTime[guidKey] = nil
                State.pending[guidKey] = nil
            end

            -- 2. 清除所有位置格式的缓存（遍历查找同itemId的缓存）
            local keysToRemove = {}
            for key, cached in pairs(State.cache) do
                if cached.itemID == itemId then
                    -- 检查GUID是否匹配（如果缓存有GUID）
                    if cached.guid == nil or cached.guid == itemGuid then
                        table.insert(keysToRemove, key)
                    end
                end
            end

            for _, key in ipairs(keysToRemove) do
                State.cache[key] = nil
                State.cacheTime[key] = nil
                State.pending[key] = nil
            end

            -- 3. 清除幻境系统缓存（因为强化后属性变化会影响幻境倍率计算）
            if HuanJingState.cache[guidKey] then
                HuanJingState.cache[guidKey] = nil
            end

            -- 4. 刷新当前显示的tooltip（如果正在显示该物品）
            for tooltip, meta in pairs(State.tooltips) do
                if tooltip:IsShown() then
                    local _, itemLink = tooltip:GetItem()
                    if itemLink then
                        local tooltipItemId = tonumber(string.match(itemLink, "item:(%d+)"))
                        if tooltipItemId == itemId then
                            -- 标记需要重新渲染
                            meta.rendered = {}
                        end
                    end
                end
            end

            -- 5. 刷新统一大框（如果正在显示该物品）
            for key, frames in pairs(UnifiedFramesByKey) do
                for _, frame in ipairs(frames) do
                    if frame:IsShown() and frame.UIT_ItemID == itemId then
                        -- 清除该大框的缓存键关联，强制重新查询
                        frame.UIT_Key = nil
                    end
                end
            end
        end

        -- 不return，让后续处理继续（以防还有其他处理逻辑）
    end

    -- 检查是否是ALL_MODULE_DATA响应
    if not message:match("ALL_MODULE_DATA:") then
        return
    end

    -- 【临时调试 - 已关闭】强制输出日志
    -- print(string.format("|cff00ff00[收到响应]|r %s", string.sub(message, 1, 400)))

    -- 移除RESPONSE:前缀（如果有）
    local dataMessage = message:gsub("^RESPONSE:", "")
    
    -- 使用pcall捕获错误
    local success, err = pcall(function()
        ProcessServerResponse(dataMessage, receiveTime)
    end)
    
    if not success then
        DebugPrint("[统一提示框错误] ProcessServerResponse失败:", err)
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
        -- 【关键修改】使用itemID到缓存键的映射找到正确的pending记录
        -- 服务器返回的是itemID和guid，但客户端使用bag:slot:itemID作为缓存键
        local key = nil
        local pending = nil
        local matchSource = "unresolved"

        TraceLog(batchData.itemID, "Response recv guid=%s bag=%s slot=%s systems=%s",
            tostring(batchData.guid), tostring(batchData.bag), tostring(batchData.slot), CollectTraceKeys(batchData.systems))

        -- 【临时调试 - 已关闭】输出映射状态
        -- local mappingInfo = "无映射"
        -- if State.itemIdToKeys and State.itemIdToKeys[batchData.itemID] then
        --     local keys = State.itemIdToKeys[batchData.itemID]
        --     mappingInfo = string.format("有%d个映射键", #keys)
        --     for i, k in ipairs(keys) do
        --         local hasPending = State.pending[k] and "有pending" or "无pending"
        --         print(string.format("|cffffcc00[映射]|r 键%d: %s (%s)", i, k, hasPending))
        --     end
        -- end
        -- print(string.format("|cff00ff00[响应处理]|r itemID=%d的映射状态: %s", batchData.itemID, mappingInfo))

        -- 优先按精确槽位匹配当前 pending，避免同 itemID 物品互串缓存
        local function PendingSlotMatches(candidatePending)
            if not candidatePending then return false end

            if batchData.bag ~= nil and batchData.slot ~= nil then
                if candidatePending.bag ~= batchData.bag or candidatePending.slot ~= batchData.slot then
                    return false
                end
            end

            -- ???????????????????
            if candidatePending.bag == nil or candidatePending.slot == nil then
                return true
            end

            local liveLink = nil
            if candidatePending.bag == 255 then
                liveLink = GetInventoryItemLink("player", candidatePending.slot)
            else
                liveLink = GetContainerItemLink(candidatePending.bag, candidatePending.slot)
            end

            if not liveLink then
                return false
            end

            local liveItemID = tonumber(string.match(liveLink, "item:(%d+)"))
            if not liveItemID or liveItemID ~= batchData.itemID then
                return false
            end

            local liveFingerprint = string.match(liveLink, "item[%-?%d:]+") or liveLink
            if candidatePending.fingerprint and liveFingerprint ~= candidatePending.fingerprint then
                return false
            end

            return true
        end

        -- ?????????itemID -> pending?????????
        -- ????????bag:slot??????????????????????????
        if State.itemIdToKeys and State.itemIdToKeys[batchData.itemID] then
            local keys = State.itemIdToKeys[batchData.itemID]

            -- ???????????????guid??
            for _, candidateKey in ipairs(keys) do
                local candidatePending = State.pending[candidateKey]
                if candidatePending and PendingSlotMatches(candidatePending) then
                    if candidatePending.guid and batchData.guid and candidatePending.guid == batchData.guid then
                        key = candidateKey
                        pending = candidatePending
                        matchSource = "itemIdToKeys:guid"
                        break
                    end
                end
            end

            -- 如果精确 GUID 没命中，再从相同 itemID 的 pending 中选最新的一条
            if not key then
                local bestKey = nil
                local bestPending = nil
                for _, candidateKey in ipairs(keys) do
                    local candidatePending = State.pending[candidateKey]
                    if candidatePending and PendingSlotMatches(candidatePending) then
                        if not bestPending or (candidatePending.queryId or 0) > (bestPending.queryId or 0) then
                            bestKey = candidateKey
                            bestPending = candidatePending
                        end
                    end
                end
                if bestKey and bestPending then
                    key = bestKey
                    pending = bestPending
                    matchSource = "itemIdToKeys:latestPending"
                end
            end
        end

        -- ?????????????????bag:slot???????
        if not key and batchData.bag ~= nil and batchData.slot ~= nil then
            local directKey = MakeKey(batchData.itemID, nil, batchData.bag, batchData.slot, batchData.bag == 255)
            local directPending = State.pending[directKey]
            if directPending and PendingSlotMatches(directPending) then
                key = directKey
                pending = directPending
                matchSource = "directBagSlot:pending"
            else
                key = directKey
                matchSource = "directBagSlot:fallback"
            end
        end

        -- ????GUID????????
        if not key then
            key = MakeKey(batchData.itemID, batchData.guid, nil, nil, false)
            pending = State.pending[key]
            matchSource = "guidKey"
        end

        -- ?????????pending????????????????
        if not key or not pending then
            local bestKey = nil
            local bestPending = nil
            for pendingKey, pendingData in pairs(State.pending) do
                if pendingData.itemID == batchData.itemID and PendingSlotMatches(pendingData) then
                    if not bestPending or (pendingData.queryId or 0) > (bestPending.queryId or 0) then
                        bestKey = pendingKey
                        bestPending = pendingData
                    end
                end
            end
            if bestKey and bestPending then
                key = bestKey
                pending = bestPending
                matchSource = "pendingScan"
            end
        end

        -- 性能监控：计算从发送到接收的延迟
        local now = GetTime()
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

        -- 如果没有找到对应的pending记录，且服务器没有返回bag:slot（旧格式响应），
        -- 【修复】即使没有pending，只要有key就应该写入缓存
        if not key then
            TraceLog(batchData.itemID, "Response drop source=%s reason=no-key guid=%s bag=%s slot=%s",
                tostring(matchSource), tostring(batchData.guid), tostring(batchData.bag), tostring(batchData.slot))
            return
        end

        -- 【临时调试 - 已关闭】输出最终使用的缓存键
        -- print(string.format("|cff00ffff[响应处理]|r 使用缓存键: %s, pending=%s", key, tostring(pending ~= nil)))

        -- 先确保缓存对象存在
        if not State.cache[key] then
            State.cache[key] = {
                itemID = batchData.itemID,
                guid = batchData.guid,
                systems = {}
            }
            State.cacheTime[key] = GetTime()
        end

        -- 记录物品指纹，避免同槽位换装后继续沿用旧数据
        if pending and pending.fingerprint then
            State.cache[key].fingerprint = pending.fingerprint
        end

        -- 将批量数据拆分并缓存到各个系统
        -- 【修改】传入已计算好的key，确保缓存键一致
        for systemName, systemData in pairs(batchData.systems) do
            hasAnyData = true
            CacheData(batchData.itemID, batchData.guid, systemData, key)
        end

        -- 【临时调试 - 已关闭】输出缓存数据统计
        -- print(string.format("|cff00ffff[响应处理]|r hasAnyData=%s", tostring(hasAnyData)))

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

        TraceLog(batchData.itemID, "Response apply source=%s key=%s pending=%s hasAnyData=%s noData=%s systems=%s",
            tostring(matchSource), tostring(key), tostring(pending ~= nil), tostring(hasAnyData), tostring(not hasAnyData), CollectTraceKeys(State.cache[key] and State.cache[key].systems))

        -- 【新增】清理itemID到缓存键的映射中已处理的条目
        if State.itemIdToKeys and State.itemIdToKeys[batchData.itemID] then
            local keys = State.itemIdToKeys[batchData.itemID]
            for i = #keys, 1, -1 do
                if keys[i] == key then
                    table.remove(keys, i)
                end
            end
            -- 如果该itemID没有更多映射，清除整个条目
            if #keys == 0 then
                State.itemIdToKeys[batchData.itemID] = nil
            end
        end

        -- 如果完全没有数据，标记整个缓存
        if not hasAnyData then
            State.cache[key].noData = true
        end

        -- 更新所有相关的提示框（仅限真正的 GameTooltip/ShoppingTooltip 等）
        local renderCount = 0

        -- 【临时调试 - 已关闭】输出当前State.tooltips的状态
        -- local tooltipCount = 0
        -- for t, m in pairs(State.tooltips) do
        --     tooltipCount = tooltipCount + 1
        --     print(string.format("|cff00ffff[渲染调试]|r tooltip #%d: shown=%s, meta.key=%s, 匹配=%s",
        --         tooltipCount, tostring(t:IsShown()), tostring(m.key), tostring(m.key == key)))
        -- end
        -- print(string.format("|cff00ffff[渲染调试]|r 目标缓存键: %s, State.tooltips数量: %d", key, tooltipCount))

        for tooltip, meta in pairs(State.tooltips) do
            if not tooltip.UIT_IsUnifiedFrame and tooltip:IsShown() and meta.key == key then
                RenderTooltip(tooltip, batchData.itemID, batchData.guid)
                renderCount = renderCount + 1
            end
        end

        -- 同步刷新所有统一四联大框（是否显示由 RefreshUnifiedFrame 自己决定）
        local frames = UnifiedFramesByKey[key]

        -- 【临时调试 - 已关闭】输出UnifiedFramesByKey的状态
        -- local frameKeyCount = 0
        -- for k, fs in pairs(UnifiedFramesByKey) do
        --     frameKeyCount = frameKeyCount + 1
        --     print(string.format("|cff00ffff[渲染调试]|r UnifiedFramesByKey[%s] = %d frames, 匹配=%s", k, #fs, tostring(k == key)))
        -- end
        -- print(string.format("|cff00ffff[渲染调试]|r UnifiedFramesByKey总数: %d, 查找key=%s, found=%s",
        --     frameKeyCount, key, tostring(frames ~= nil)))

        if frames then
            for _, frame in ipairs(frames) do
                RefreshUnifiedFrame(frame, batchData.itemID, batchData.guid)
                renderCount = renderCount + 1
            end
        end

        TraceLog(batchData.itemID, "Response render key=%s renderCount=%s", tostring(key), tostring(renderCount))

        return
    end
end

-- 注册Addon消息前缀（兼容WoW 3.3.5）
if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
    RegisterAddonMessagePrefix(ADDON_PREFIX_ALT)
elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
    C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX_ALT)
end

-- 处理玩家登录/重新进入世界事件 - 清除缓存确保数据刷新
local function OnPlayerEnteringWorld(self, event, isLogin, isReload)
    -- 清除所有缓存，确保重新登录后能正确获取最新数据
    State.cache = {}
    State.pending = {}
    State.lastQuery = {}
    State.noDataUntil = {}
    State.cacheTime = {}
    State.tooltips = {}
    State.itemIdToKeys = {}  -- 【新增】清除itemID到缓存键的映射

    -- 清除幻境系统缓存
    HuanJingState.cache = {}
    HuanJingState.pending = {}

    -- 【新增】清除待鉴定状态缓存并重新查询
    PendingIdentifyState.cache = {}
    PendingIdentifyState.suppressed = {}
    PendingIdentifyState.lastQuery = 0
    PendingIdentifyState.querying = false

    -- 延迟1秒后查询待鉴定列表（等待服务器准备好）
    -- 使用OnUpdate延迟（兼容WoW 3.3.5）
    local delayFrame = CreateFrame("Frame")
    delayFrame.elapsed = 0
    delayFrame:SetScript("OnUpdate", function(self, elapsed)
        self.elapsed = self.elapsed + elapsed
        if self.elapsed >= 1 then
            self:SetScript("OnUpdate", nil)
            QueryPendingIdentifyList()
        end
    end)
    
    UnifiedFramesByKey = {}
end

-- 【新增】背包物品变化时清理相关槽位缓存，避免移动物品后显示错误数据
-- 记录每个槽位的物品指纹，用于检测物品变化
local SlotFingerprints = {}

local function ClearSlotCache(bag, slot)
    -- 收集要删除的键（避免遍历时删除）
    local keysToRemove = {}
    for key, _ in pairs(State.cache) do
        -- 检查缓存键是否属于这个槽位
        -- 缓存键格式: P:bag:slot:itemID
        local keyBag, keySlot = key:match("^P:(%d+):(%d+):")
        if keyBag and keySlot then
            if tonumber(keyBag) == bag and tonumber(keySlot) == slot then
                table.insert(keysToRemove, key)
            end
        end
    end

    -- 删除收集到的键
    for _, key in ipairs(keysToRemove) do
        local cached = State.cache[key]
        if cached then
            TraceLog(cached.itemID, "ClearSlotCache bag=%s slot=%s key=%s", tostring(bag), tostring(slot), tostring(key))
        end
        State.cache[key] = nil
        State.cacheTime[key] = nil
        State.pending[key] = nil
        State.lastQuery[key] = nil
    end
end

local function OnBagUpdate(self, event, bagID)
    if bagID == nil then return end

    -- 只处理背包0-4
    if bagID < 0 or bagID > 4 then return end

    local numSlots = GetContainerNumSlots(bagID)
    local changedItemIDs = {}  -- 记录发生变化的物品ID

    for slot = 1, numSlots do
        local slotKey = string.format("%d:%d", bagID, slot)
        local link = GetContainerItemLink(bagID, slot)
        local currentFingerprint = link and (string.match(link, "item[%-?%d:]+") or link) or nil
        local previousFingerprint = SlotFingerprints[slotKey]

        -- 如果槽位物品发生变化（包括：物品移走、物品移入、物品替换）
        if currentFingerprint ~= previousFingerprint then
            -- 更新记录
            SlotFingerprints[slotKey] = currentFingerprint

            local currentItemID = link and tonumber(link:match("item:(%d+)")) or nil
            local previousItemID = previousFingerprint and tonumber(previousFingerprint:match("item:(%d+)")) or nil
            TraceLog(currentItemID or previousItemID, "BAG_UPDATE bag=%s slot=%s prev=%s curr=%s",
                tostring(bagID), tostring(slot), tostring(previousFingerprint), tostring(currentFingerprint))
            -- 清理该槽位的缓存
            ClearSlotCache(bagID, slot)

            -- 【关键修复】提取物品ID，用于清理同ID的其他缓存
            -- 这是为了解决待鉴定物品（物品链接相同）移动后串缓存的问题
            if link then
                local itemID = tonumber(link:match("item:(%d+)"))
                if itemID then
                    changedItemIDs[itemID] = true
                end
            end
            -- 也检查之前的fingerprint
            if previousFingerprint then
                local prevItemID = tonumber(previousFingerprint:match("item:(%d+)"))
                if prevItemID then
                    changedItemIDs[prevItemID] = true
                end
            end
        end
    end

    -- 【关键修复】清理所有发生变化的物品ID的缓存
    -- 对于待鉴定物品（没有GUID区分），移动后应该清理所有同ID物品的缓存
    -- 这样下次悬停时会重新查询，获取正确的数据
    for itemID, _ in pairs(changedItemIDs) do
        -- 检查这个itemID是否有待鉴定物品
        local pendingPositions = FindPendingItemPositions(itemID)
        if #pendingPositions > 0 then
            TraceLog(itemID, "BAG_UPDATE pendingPositions=%s", FormatTracePositions(pendingPositions))
            -- 清理所有同itemID的缓存
            local keysToRemove = {}
            for key, cached in pairs(State.cache) do
                if cached.itemID == itemID then
                    table.insert(keysToRemove, key)
                end
            end
            TraceLog(itemID, "BAG_UPDATE purgeSameItemCaches itemID=%s count=%s", tostring(itemID), tostring(#keysToRemove))
            for _, key in ipairs(keysToRemove) do
                TraceLog(itemID, "BAG_UPDATE removeKey=%s", tostring(key))
                State.cache[key] = nil
                State.cacheTime[key] = nil
                State.pending[key] = nil
                State.lastQuery[key] = nil
            end
        end

        SchedulePendingIdentifyRefresh("bagUpdate", itemID)
    end

end

-- 【新增】装备栏物品变化时清理缓存
local function OnPlayerEquipmentChanged(self, event, equipSlot, hasCurrent)
    if equipSlot == nil then return end

    local slotKey = string.format("255:%d", equipSlot)
    local link = GetInventoryItemLink("player", equipSlot)
    local currentFingerprint = link and (string.match(link, "item[%-?%d:]+") or link) or nil
    local previousFingerprint = SlotFingerprints[slotKey]

    if currentFingerprint ~= previousFingerprint then
        local currentItemID = link and tonumber(link:match("item:(%d+)")) or nil
        local previousItemID = previousFingerprint and tonumber(previousFingerprint:match("item:(%d+)")) or nil
        TraceLog(currentItemID or previousItemID, "EQUIP_CHANGED slot=%s prev=%s curr=%s",
            tostring(equipSlot), tostring(previousFingerprint), tostring(currentFingerprint))

        SlotFingerprints[slotKey] = currentFingerprint
        -- 装备栏使用 bag=255
        ClearSlotCache(255, equipSlot)

        SchedulePendingIdentifyRefresh("equipmentChanged", currentItemID or previousItemID)
    end
end

-- 注册Addon消息事件
EventFrame:RegisterEvent("CHAT_MSG_ADDON")
EventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
EventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
EventFrame:RegisterEvent("BAG_UPDATE")  -- 【新增】监听背包更新，物品移动时清理缓存
EventFrame:RegisterEvent("PLAYER_EQUIPMENT_CHANGED")  -- 【新增】监听装备栏变化

EventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        OnAddonMessage(self, event, ...)
    elseif event == "CHAT_MSG_SYSTEM" then
        OnSystemMessage(self, event, ...)
    elseif event == "PLAYER_ENTERING_WORLD" then
        OnPlayerEnteringWorld(self, event, ...)
    elseif event == "BAG_UPDATE" then
        OnBagUpdate(self, event, ...)
    elseif event == "PLAYER_EQUIPMENT_CHANGED" then
        OnPlayerEquipmentChanged(self, event, ...)
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
        DebugPrint("[错误] ownerTooltip无效")
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
    mainFrame:SetHeight(350)

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

    -- 【关键修复】优先使用位置 key，禁止本地物品回退到 GUID key
    -- 3.3.5 物品链接里的 guid/uniqueId 并不可靠，回退会把同 itemID 的其他物品缓存串过来
    local cached = nil
    local usedKey = nil

    if unifiedFrame.UIT_Key then
        usedKey = unifiedFrame.UIT_Key
        cached = State.cache[usedKey]
    end

    if not usedKey and unifiedFrame.UIT_OwnerTooltip then
        local ownerMeta = State.tooltips[unifiedFrame.UIT_OwnerTooltip]
        if ownerMeta and ownerMeta.key then
            usedKey = ownerMeta.key
            cached = State.cache[usedKey]
            unifiedFrame.UIT_Key = usedKey
        end
    end

    -- 如果缓存存在，检查itemID是否匹配
    if cached and cached.itemID and cached.itemID ~= itemID then
        -- 缓存与请求的itemID不匹配，视为无效缓存
        cached = nil
    end

    -- 没有缓存或被标记无数据：不显示大框，保留官方提示框
    if not cached or not cached.systems or cached.noData then
        unifiedFrame:Hide()
        if unifiedFrame.UIT_OwnerTooltip and unifiedFrame.UIT_OwnerTooltip.SetAlpha then
            unifiedFrame.UIT_OwnerTooltip:SetAlpha(1)
        end
        return
    end

    local systems = cached.systems

    -- 检查是否至少有一个系统有有效数据（不是 isEmpty 占位，且确实有内容）
    local function hasNonEmpty(system)
        if not system or system.isEmpty then
            return false
        end
        return true
    end

    -- 检查鉴定系统是否有实际属性数据
    local function hasIdentificationData(system)
        if not system or system.isEmpty then
            return false
        end
        -- 检查是否有基础属性或追加属性
        local hasBase = system.baseAttributes and #system.baseAttributes > 0
        local hasAdditional = system.additionalAttributes and #system.additionalAttributes > 0
        return hasBase or hasAdditional
    end

    -- 检查成长系统是否有实际数据
    local function hasGrowthData(system)
        if not system or system.isEmpty then
            return false
        end
        -- 检查是否有等级或属性
        local hasLevel = system.level and system.level > 0
        local hasAttrs = system.attributes and #system.attributes > 0
        return hasLevel or hasAttrs
    end

    -- 检查强化系统是否有实际数据
    local function hasEnhancementData(system)
        if not system or system.isEmpty then
            return false
        end
        -- 检查是否有等级或属性
        local hasLevel = system.level and system.level > 0
        local hasAttrs = system.attributes and #system.attributes > 0
        return hasLevel or hasAttrs
    end

    -- 检查魔次系统是否有实际数据
    local function hasMagicData(system)
        if not system or system.isEmpty then
            return false
        end
        return system.configs and #system.configs > 0
    end

    -- 检查技能系统是否有实际数据
    local function hasSkillsData(system)
        if not system or system.isEmpty then
            return false
        end
        return system.skills and #system.skills > 0
    end

    -- 检查套装系统是否有实际数据
    local function hasSetsData(system)
        if not system or system.isEmpty then
            return false
        end
        return system.setId and system.setId > 0
    end

    -- 检查符文系统是否有实际数据
    local function hasRunesData(system)
        if not system or system.isEmpty then
            return false
        end
        return system.totalSlots and system.totalSlots > 0
    end

    local hasData = hasIdentificationData(systems.identification)
        or hasEnhancementData(systems.enhancement)
        or hasGrowthData(systems.growth)
        or hasMagicData(systems.magic)
        or hasSkillsData(systems.skills)
        or hasSetsData(systems.sets)
        or hasRunesData(systems.runes)

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

    -- 为当前大框构造/重置渲染元数据（沿用当前绑定的缓存 key）
    local key = usedKey
    local meta = State.tooltips[unifiedFrame] or { key = key, rendered = {} }
    meta.key = key
    meta.rendered = {}
    State.tooltips[unifiedFrame] = meta

    -- 检查第1列是否有内容
    local hasColumn1Data = hasIdentificationData(systems.identification)
        or hasEnhancementData(systems.enhancement)
        or hasGrowthData(systems.growth)

    -- 检查第2列是否有内容
    local hasColumn2Data = hasMagicData(systems.magic)
        or hasSkillsData(systems.skills)
        or hasSetsData(systems.sets)
        or hasRunesData(systems.runes)

    -- 只有有内容时才显示对应列的标题
    if hasColumn1Data then
        unifiedFrame:AddLineToColumn(1, "|cFFFFD700基础/追加/成长/强化|r", 1, 0.84, 0)
        -- 第1列：基础属性 / 追加属性 / 成长属性 / 强化属性
        RenderUnifiedBaseAttributes(unifiedFrame.ColumnTooltips[1], cached, meta)
    end

    if hasColumn2Data then
        unifiedFrame:AddLineToColumn(2, "|cFFFFD700技能效果（魔次/技能/套装/符文）|r", 1, 0.84, 0)

        -- 第2列：技能效果（魔次属性 / 追加技能 / 追加套装 / 符文系统）
        if DB.systems.magic and hasMagicData(systems.magic) then
            Renderers.Magic(unifiedFrame.ColumnTooltips[2], systems.magic)
        end

        if DB.systems.skills and hasSkillsData(systems.skills) then
            Renderers.Skills(unifiedFrame.ColumnTooltips[2], systems.skills)
        end

        if DB.systems.sets and hasSetsData(systems.sets) then
            Renderers.Sets(unifiedFrame.ColumnTooltips[2], systems.sets)
        end

        if DB.systems.runes and hasRunesData(systems.runes) then
            Renderers.Runes(unifiedFrame.ColumnTooltips[2], systems.runes)
        end
    end

    -- 【关键修复】根据两列内容的实际高度，动态调整主框架高度
    local column1Height = unifiedFrame.columns[1] and unifiedFrame.columns[1].currentY or 0
    local column2Height = unifiedFrame.columns[2] and unifiedFrame.columns[2].currentY or 0
    local maxColumnHeight = math.max(column1Height, column2Height)

    -- 添加顶部和底部的padding（上下各10像素）
    local minHeight = 50  -- 最小高度
    local paddingTop = 10
    local paddingBottom = 20
    local calculatedHeight = maxColumnHeight + paddingTop + paddingBottom

    -- 确保高度不会小于最小值
    if calculatedHeight < minHeight then
        calculatedHeight = minHeight
    end

    -- 设置主框架高度
    unifiedFrame:SetHeight(calculatedHeight)

    -- 同时更新两列的高度，使背景框覆盖所有内容
    if unifiedFrame.columns[1] then
        unifiedFrame.columns[1]:SetHeight(calculatedHeight - 20)  -- 减去上下边距
    end
    if unifiedFrame.columns[2] then
        unifiedFrame.columns[2]:SetHeight(calculatedHeight - 20)
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

    if ownerTooltip.UIT_Tooltip2 then
        return ownerTooltip.UIT_Tooltip2
    end

    local baseName = ownerTooltip:GetName() or "UnifiedItemTooltip"
    local extraName = baseName .. "_UnifiedExtra"

    local extra = CreateFrame("GameTooltip", extraName, UIParent, "GameTooltipTemplate")
    extra:SetFrameStrata(ownerTooltip:GetFrameStrata())
    extra:SetScale(ownerTooltip:GetScale())

    ownerTooltip.UIT_Tooltip2 = extra
    return extra
end

-- 判断一个提示框是否源自聊天框/聊天链接
IsTooltipFromChat = function(tooltip)
	if not tooltip or not tooltip.GetOwner then return false end

	local current = tooltip
	local depth = 0
	while current and depth < 5 do
		local name = current.GetName and current:GetName()
		if name and name:match("^ChatFrame%d+") then
			return true
		end
		if ItemRefTooltip and current == ItemRefTooltip then
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

	local screenWidth = GetScreenWidth and GetScreenWidth() or 0
	local screenHeight = GetScreenHeight and GetScreenHeight() or 0
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
		return
	end

	-- 如果官方tooltip在右侧会导致自定义tooltip超出屏幕
	if ownerRight + gap + extraWidth > screenWidth then
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
		return
	end

	-- 如果官方tooltip太靠左，确保有足够空间
	if ownerLeft < 10 then
		ownerTooltip:ClearAllPoints()
		ownerTooltip:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", 10, ownerTop)

		extra:ClearAllPoints()
		extra:SetPoint("TOPLEFT", ownerTooltip, "TOPRIGHT", gap, 0)
	end
end

-- 获取当前鼠标悬停的背包位置
GetTooltipBagSlot = function(tooltip)
    local owner = tooltip:GetOwner()
    if not owner then
        -- 【日志精简】常规日志已注释
        -- DebugPrint("[GetTooltipBagSlot] tooltip没有owner")
        return nil, nil
    end

    local ownerName = owner.GetName and owner:GetName() or ""
    -- 【日志精简】常规日志已注释
    -- DebugPrint(string.format("[GetTooltipBagSlot] owner名称: '%s'", ownerName))

    -- 【Combuctor专用诊断】输出按钮的所有关键属性（仅调试模式）
    if DB.debug and ownerName:match("^Combuctor") then
        local diagInfo = {}
        -- 检查常见属性
        if owner.bag ~= nil then table.insert(diagInfo, "bag=" .. tostring(owner.bag)) end
        if owner.slot ~= nil then table.insert(diagInfo, "slot=" .. tostring(owner.slot)) end
        if owner.bagID ~= nil then table.insert(diagInfo, "bagID=" .. tostring(owner.bagID)) end
        if owner.slotID ~= nil then table.insert(diagInfo, "slotID=" .. tostring(owner.slotID)) end
        if owner.GetID then table.insert(diagInfo, "GetID()=" .. tostring(owner:GetID())) end
        if owner.GetBag then
            local ok, result = pcall(function() return owner:GetBag() end)
            table.insert(diagInfo, "GetBag()=" .. tostring(ok and result or "error"))
        end
        if owner.GetSlot then
            local ok, result = pcall(function() return owner:GetSlot() end)
            table.insert(diagInfo, "GetSlot()=" .. tostring(ok and result or "error"))
        end
        -- 检查父框架
        local parent = owner:GetParent()
        if parent then
            local parentName = parent.GetName and parent:GetName() or "unnamed"
            table.insert(diagInfo, "parent=" .. parentName)
            if parent.bag ~= nil then table.insert(diagInfo, "parent.bag=" .. tostring(parent.bag)) end
            if parent.bagID ~= nil then table.insert(diagInfo, "parent.bagID=" .. tostring(parent.bagID)) end
            if parent.GetBag then
                local ok, result = pcall(function() return parent:GetBag() end)
                table.insert(diagInfo, "parent.GetBag()=" .. tostring(ok and result or "error"))
            end
        end
        -- 【日志精简】Combuctor诊断日志只在调试时按需开启
        -- DebugPrint(string.format("[Combuctor诊断] %s: %s", ownerName, table.concat(diagInfo, ", ")))
    end

    -- 检查是否是背包格子
    -- ContainerFrameItemButton格式: ContainerFrame{bag}Item{slot}
    local bag, slot = ownerName:match("^ContainerFrame(%d+)Item(%d+)$")
    if bag and slot then
        bag = tonumber(bag)
        slot = tonumber(slot)

        local parent = owner:GetParent()
        if parent and parent.GetID then
            local ok, actualBag = pcall(function() return parent:GetID() end)
            if ok and actualBag ~= nil then
                return actualBag, slot
            end
        end

        -- bag索引转换：UI上的bag1=背包4, bag2=背包3, bag3=背包2, bag4=背包1, bag5=背包0(主背包)
        local actualBag = (5 - bag)
        -- 【日志精简】常规日志已注释
        -- DebugPrint(string.format("[GetTooltipBagSlot] 背包物品: UIbag=%d -> actualBag=%d, slot=%d", bag, actualBag, slot))
        return actualBag, slot
    end

    -- 检查是否是装备栏物品
    -- 格式: Character{SlotName}Slot 例如 CharacterHeadSlot
    local equipSlot = ownerName:match("^Character(.+Slot)$")
    if equipSlot then
        local slotId = GetInventorySlotInfo(equipSlot)
        if slotId then
            return 255, slotId  -- bag=255 表示装备栏
        end
    end

    -- 检查是否是检查其他玩家的装备栏
    -- 格式: Inspect{SlotName}Slot 例如 InspectHeadSlot
    local inspectSlot = ownerName:match("^Inspect(.+Slot)$")
    if inspectSlot then
        local slotId = GetInventorySlotInfo(inspectSlot)
        if slotId then
            return 255, slotId  -- bag=255 表示装备栏
        end
    end

    -- 【新增】支持第三方背包插件（Combuctor, Bagnon, ArkInventory等）
    if owner.GetBag and owner.GetSlot then
        local bagId = owner:GetBag()
        local slotId = owner:GetSlot()
        if bagId and slotId then
            return bagId, slotId
        end
    end

    -- 尝试读取owner的bag和slot属性
    if owner.bag ~= nil and owner.slot ~= nil then
        return owner.bag, owner.slot
    end

    -- 【Combuctor/Bagnon专用】尝试通过GetBag和GetID获取背包位置
    if owner.GetBag and owner.GetID then
        local ok1, bagId = pcall(function() return owner:GetBag() end)
        local ok2, slotId = pcall(function() return owner:GetID() end)
        if ok1 and ok2 and bagId ~= nil and slotId ~= nil then
            -- 验证Combuctor返回的槽位是否正确
            local _, itemLink = tooltip:GetItem()
            if itemLink then
                local tooltipItemString = string.match(itemLink, "item[%-?%d:]+")
                local slotLink = GetContainerItemLink(bagId, slotId)
                local slotItemString = slotLink and string.match(slotLink, "item[%-?%d:]+") or nil

                -- 检查Combuctor返回的槽位是否与tooltip物品匹配
                if tooltipItemString ~= slotItemString then
                    -- 槽位不匹配！搜索正确的槽位
                    local allMatches = {}
                    ForEachSearchBag(function(searchBag, numSlots)
                        for searchSlot = 1, numSlots do
                            local searchLink = GetContainerItemLink(searchBag, searchSlot)
                            if searchLink then
                                local searchItemString = string.match(searchLink, "item[%-?%d:]+")
                                if searchItemString == tooltipItemString then
                                    table.insert(allMatches, {bag = searchBag, slot = searchSlot})
                                end
                            end
                        end
                    end)

                    if #allMatches > 0 then
                        return allMatches[1].bag, allMatches[1].slot
                    end
                end
            end

            return bagId, slotId
        end
    end

    -- 备用：尝试通过GetID获取槽位，并查找父框架获取背包ID
    if owner.GetID then
        local slotId = owner:GetID()
        local parent = owner:GetParent()

        if parent then
            local parentBag = nil
            if parent.GetBag then
                local ok, result = pcall(function() return parent:GetBag() end)
                if ok then parentBag = result end
            elseif parent.bag ~= nil then
                parentBag = parent.bag
            elseif parent.bagID ~= nil then
                parentBag = parent.bagID
            end

            if parentBag ~= nil and slotId ~= nil then
                return parentBag, slotId
            end
        end
    end

    -- 【通用方案】遍历owner的所有已知属性
    local possibleBagKeys = {"bag", "bagId", "bagID", "BagID", "containerID"}
    local possibleSlotKeys = {"slot", "slotId", "slotID", "SlotID", "itemSlot"}

    local foundBag, foundSlot = nil, nil

    for _, key in ipairs(possibleBagKeys) do
        if owner[key] ~= nil then
            foundBag = owner[key]
            break
        end
    end

    for _, key in ipairs(possibleSlotKeys) do
        if owner[key] ~= nil then
            foundSlot = owner[key]
            break
        end
    end

    if foundBag ~= nil and foundSlot ~= nil then
        return foundBag, foundSlot
    end

    -- 【Combuctor/Bagnon专用】尝试从按钮的info表或GetItemSlot方法获取
    if ownerName:match("^Combuctor") or ownerName:match("^Bagnon") or ownerName:match("^ArkInventory") then
        if owner.info then
            local info = owner.info
            if info.bag ~= nil and info.slot ~= nil then
                return info.bag, info.slot
            end
        end

        if owner.hasItem then
            local hasItem = owner.hasItem
            if type(hasItem) == "table" and hasItem.bag ~= nil and hasItem.slot ~= nil then
                return hasItem.bag, hasItem.slot
            end
        end

        if owner.GetItemSlot then
            local ok, bagId, slotId = pcall(function() return owner:GetItemSlot() end)
            if ok and bagId and slotId then
                return bagId, slotId
            end
        end

        for k, v in pairs(owner) do
            if type(k) == "string" and type(v) == "table" then
                if v.bag ~= nil and v.slot ~= nil then
                    return v.bag, v.slot
                end
            end
        end

        -- 方法5: 通过物品链接反向查找背包位置（最后手段）
        local _, itemLink = tooltip:GetItem()
        if itemLink then
            local targetItemID = tonumber(string.match(itemLink, "item:(%d+)"))
            if targetItemID then
                local targetItemString = string.match(itemLink, "item[%-?%d:]+")

                local matchedBag, matchedSlot = nil, nil
                ForEachSearchBag(function(bagIndex, numSlots)
                    for slotIndex = 1, numSlots do
                        local bagItemLink = GetContainerItemLink(bagIndex, slotIndex)
                        if bagItemLink then
                            local bagItemString = string.match(bagItemLink, "item[%-?%d:]+")
                            if targetItemString == bagItemString then
                                matchedBag = bagIndex
                                matchedSlot = slotIndex
                                return true
                            end
                        end
                    end
                end)

                if matchedBag ~= nil and matchedSlot ~= nil then
                    return matchedBag, matchedSlot
                end

                -- 如果完全匹配失败，按itemID匹配
                local matches = {}
                ForEachSearchBag(function(bagIndex, numSlots)
                    for slotIndex = 1, numSlots do
                        local bagItemLink = GetContainerItemLink(bagIndex, slotIndex)
                        if bagItemLink then
                            local bagItemID = tonumber(string.match(bagItemLink, "item:(%d+)"))
                            if bagItemID == targetItemID then
                                table.insert(matches, {bag = bagIndex, slot = slotIndex})
                            end
                        end
                    end
                end)

                if #matches > 0 then
                    return matches[1].bag, matches[1].slot
                end
            end
        end
    end

    return nil, nil
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

		local compareText = ITEM_COMPARE_TOOLTIP_TEXT
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
	end

	local _, itemLink = tooltip:GetItem()
	if not itemLink then
		return
	end

    -- 提取完整物品字符串作为指纹，用于缓存一致性校验
    local tooltipItemString = string.match(itemLink, "item[%-?%d:]+") or itemLink

    -- 检测是否正在检查其他玩家的装备
    local inspectUnit = GetInspectUnit()

    -- 【调试日志】观察/聊天框链接排查
    if DB.debug then
        local tooltipName = tooltip:GetName() or "unknown"
        print(string.format("|cff00ffff[OnTooltipSetItem入口]|r tooltip=%s, inspectUnit=%s",
            tooltipName, tostring(inspectUnit)))
    end

    local tooltipName = tooltip:GetName() or "unknown"
    local preferEquipped = (tooltipName == "ShoppingTooltip1" or tooltipName == "ShoppingTooltip2")

    -- 使用增强版提取逻辑
    local itemID, guid, extractedBag, extractedSlot, isEquipped = ExtractItemInfoEnhanced(itemLink, inspectUnit, preferEquipped)
    if not itemID then
        return
    end

    -- 【飞升系统支持】检查是否是飞升系统的物品
    -- 飞升系统会设置全局变量 ASCENSION_TOOLTIP_ITEM_ID 和 ASCENSION_TOOLTIP_ITEM_GUID
    local isAscensionItem = false
    if ASCENSION_TOOLTIP_ITEM_ID and ASCENSION_TOOLTIP_ITEM_GUID then
        if ASCENSION_TOOLTIP_ITEM_ID == itemID and ASCENSION_TOOLTIP_ITEM_GUID > 0 then
            guid = ASCENSION_TOOLTIP_ITEM_GUID
            isAscensionItem = true
            if DB.debug then
                print(string.format("|cff00ff00[飞升系统]|r 使用飞升系统提供的GUID: itemID=%d, guid=%d", itemID, guid))
            end
        end
    end

    -- 【调试日志】ExtractItemInfoEnhanced结果
    if DB.debug then
        print(string.format("|cff00ffff[ExtractItemInfoEnhanced]|r itemID=%d, guid=%s, bag=%s, slot=%s, isEquipped=%s",
            itemID, tostring(guid), tostring(extractedBag), tostring(extractedSlot), tostring(isEquipped)))
    end

	-- 优先从tooltip获取实际悬停的槽位
    local bagNum, slotNum = GetTooltipBagSlot(tooltip)
    local originalBag, originalSlot = bagNum, slotNum
    local isEquipmentSlotTooltip = (bagNum == 255) or (extractedBag == 255) or (isEquipped == true)

    if preferEquipped and (not inspectUnit or UnitIsUnit(inspectUnit, "player")) then
        local compareSlot, compareGuid, compareSource, compareEquipLoc, compareMatchCount = ResolveCompareTooltipEquipSlot(itemLink, itemID, tooltipName)

        if not compareSlot and isEquipped and extractedSlot then
            compareSlot = extractedSlot
            compareGuid = guid
            compareSource = "extractFallback"
            compareMatchCount = 1
        end

        if compareSlot then
            bagNum = 255
            slotNum = compareSlot
            extractedBag = nil
            extractedSlot = compareSlot
            isEquipped = true
            isEquipmentSlotTooltip = true
            if compareGuid and compareGuid > 0 then
                guid = compareGuid
            end

            TraceLog(itemID, "CompareTooltip resolved tooltip=%s slot=%s source=%s equipLoc=%s matches=%s guid=%s",
                tostring(tooltipName), tostring(compareSlot), tostring(compareSource), tostring(compareEquipLoc), tostring(compareMatchCount), tostring(guid))
        else
            local owner = tooltip.GetOwner and tooltip:GetOwner() or nil
            local ownerName = owner and owner.GetName and owner:GetName() or nil
            TraceLog(itemID, "CompareTooltip unresolved tooltip=%s equipLoc=%s owner=%s extracted=%s/%s guid=%s",
                tostring(tooltipName), tostring(compareEquipLoc), tostring(ownerName), tostring(extractedBag), tostring(extractedSlot), tostring(guid))
        end
    end

    -- 【临时调试 - 已关闭】输出槽位识别结果和待鉴定缓存
    -- print(string.format("|cff00ffff[槽位调试]|r itemID=%d, GetTooltipBagSlot返回: bag=%s, slot=%s",
    --     itemID, tostring(bagNum), tostring(slotNum)))
    -- DebugPrintPendingCache()

    -- 【关键修复】首先检查当前位置是否是待鉴定物品
    -- 如果是待鉴定物品，直接使用当前位置，不要再搜索其他位置
    local isPendingIdentifyEarly = false
    if bagNum ~= nil and slotNum ~= nil and bagNum ~= 255 then
        local earlyPendingInfo = GetPendingIdentifyInfo(bagNum, slotNum, itemID)
        if earlyPendingInfo and earlyPendingInfo.isPending then
            isPendingIdentifyEarly = true
            -- 待鉴定物品：直接使用当前位置，跳过所有链接匹配逻辑
        end
    end

    -- 【飞升系统支持】如果是飞升系统的物品，跳过槽位搜索逻辑
    -- 飞升系统的物品不在背包中，直接使用已设置的GUID
    if not isAscensionItem and not isPendingIdentifyEarly and bagNum ~= nil and slotNum ~= nil and bagNum ~= 255 then
        local verifyLink = GetContainerItemLink(bagNum, slotNum)

        -- ??????????????/????tooltip???????????????????
        if verifyLink then
            local verifyItemString = string.match(verifyLink, "item[%-?%d:]+")
            local verifyItemID = tonumber(string.match(verifyLink, "item:(%d+)"))

            if verifyItemString == tooltipItemString then
                -- ????????????
                local pendingInfo = GetPendingIdentifyInfo(bagNum, slotNum, itemID)

                if pendingInfo then
                    local _, verifyGUID = ExtractItemInfo(verifyLink)
                    if verifyGUID and verifyGUID > 0 then
                        guid = verifyGUID
                    end
                else
                    local _, verifyGUID = ExtractItemInfo(verifyLink)
                    if verifyGUID and verifyGUID > 0 then
                        guid = verifyGUID
                    end
                end
            elseif verifyItemID and verifyItemID == itemID then
                -- ??ID????????????????????????????????
                tooltipItemString = verifyItemString or tooltipItemString
                local _, verifyGUID = ExtractItemInfo(verifyLink)
                if verifyGUID and verifyGUID > 0 then
                    guid = verifyGUID
                end
            else
                -- ?????itemID???????????????
                local foundBag, foundSlot, foundGuid = nil, nil, nil
                ForEachSearchBag(function(searchBag, numSlots)
                    for searchSlot = 1, numSlots do
                        local searchLink = GetContainerItemLink(searchBag, searchSlot)
                        if searchLink then
                            local searchItemString = string.match(searchLink, "item[%-?%d:]+")
                            if searchItemString == tooltipItemString then
                                if not foundBag then
                                    foundBag = searchBag
                                    foundSlot = searchSlot
                                    local _, searchGUID = ExtractItemInfo(searchLink)
                                    foundGuid = searchGUID
                                end
                            end
                        end
                    end
                end)

                if foundBag and foundSlot then
                    bagNum = foundBag
                    slotNum = foundSlot
                    if foundGuid and foundGuid > 0 then
                        guid = foundGuid
                    end
                else
                    bagNum = nil
                    slotNum = nil
                end
            end
        else
            bagNum = nil
            slotNum = nil
        end
    end

    -- 如果从tooltip无法获取（如聊天链接），则使用ExtractItemInfoEnhanced的结果
    -- 【飞升系统支持】飞升系统的物品已经有GUID，跳过槽位搜索
    if not isAscensionItem and (bagNum == nil or slotNum == nil) then
        bagNum = extractedBag
        slotNum = extractedSlot

        -- 如果是装备栏物品（isEquipped=true），bag应该设为255
        if isEquipped and extractedSlot then
            bagNum = 255
            slotNum = extractedSlot
        end

        -- 【关键修复】聊天框物品链接：通过完整物品指纹搜索玩家的背包/装备栏
        -- WoW 3.3.5物品链接不包含GUID，但包含完整的物品信息（附魔、宝石等）
        -- 如果物品在玩家身上，可以通过指纹精确匹配找到位置
        if (bagNum == nil or slotNum == nil) and tooltipItemString then
            if DB.debug then
                print(string.format("|cff00ffff[聊天框搜索]|r 开始搜索指纹: %s", tooltipItemString))
            end

            -- 先搜索装备栏
            for equipSlot = 0, 19 do
                local equipLink = GetInventoryItemLink("player", equipSlot)
                if equipLink then
                    local equipItemString = string.match(equipLink, "item[%-?%d:]+")
                    if DB.debug and equipItemString then
                        -- 只输出有内容的装备槽位（避免日志过多）
                        local equipItemID = tonumber(string.match(equipLink, "item:(%d+)"))
                        if equipItemID == itemID then
                            print(string.format("|cff00ffff[装备栏搜索]|r 槽位%d: 同ID物品, 指纹匹配=%s",
                                equipSlot, tostring(equipItemString == tooltipItemString)))
                        end
                    end
                    if equipItemString == tooltipItemString then
                        bagNum = 255
                        slotNum = equipSlot
                        if DB.debug then
                            print(string.format("|cff00ff00[聊天框搜索]|r 在装备栏找到! bag=255, slot=%d", equipSlot))
                        end
                        break
                    end
                end
            end

            -- 如果装备栏没找到，搜索背包
            if bagNum == nil then
                ForEachSearchBag(function(searchBag, numSlots)
                    for searchSlot = 1, numSlots do
                        local bagLink = GetContainerItemLink(searchBag, searchSlot)
                        if bagLink then
                            local bagItemString = string.match(bagLink, "item[%-?%d:]+")
                            if DB.debug and bagItemString then
                                local bagItemID = tonumber(string.match(bagLink, "item:(%d+)"))
                                if bagItemID == itemID then
                                    print(string.format("|cff00ffff[背包搜索]|r bag%d slot%d: 同ID物品, 指纹匹配=%s",
                                        searchBag, searchSlot, tostring(bagItemString == tooltipItemString)))
                                end
                            end
                            if bagItemString == tooltipItemString then
                                bagNum = searchBag
                                slotNum = searchSlot
                                if DB.debug then
                                    print(string.format("|cff00ff00[聊天框搜索]|r 在背包找到! bag=%d, slot=%d", searchBag, searchSlot))
                                end
                                return true
                            end
                        end
                    end
                end)
            end

            if DB.debug and bagNum == nil then
                print(string.format("|cffff8800[聊天框搜索]|r 未找到匹配物品，该物品可能不在玩家身上"))
            end
        end
    end

    -- 如果GUID无效，检查是否是背包物品，尝试从容器API获取
    -- 【注意】不使用PickupContainerItem，因为会干扰玩家的点击操作
    if not (guid and guid > 0) then
        if bagNum and slotNum and bagNum ~= 255 then
            local containerLink = GetContainerItemLink(bagNum, slotNum)
            if containerLink then
                local containerItemID, containerGUID = ExtractItemInfo(containerLink)
                if containerItemID == itemID and containerGUID and containerGUID > 0 then
                    guid = containerGUID
                end
            end
        end
    end

    -- 没有有效 GUID 的处理：允许使用bag/slot继续查询，只有检查他人且无GUID时才等待
    if not (guid and guid > 0) then
        if inspectUnit and UnitExists(inspectUnit) and not UnitIsUnit(inspectUnit, "player") then
            -- 【调试日志】观察其他玩家，等待GUID
            if DB.debug then
                print(string.format("|cffff8800[等待GUID]|r itemID=%d, inspectUnit=%s, 加入pendingInspectTooltips",
                    itemID, tostring(inspectUnit)))
            end
            if not State.pendingInspectTooltips then
                State.pendingInspectTooltips = {}
            end
            State.pendingInspectTooltips[tooltip] = {
                itemID = itemID,
                inspectUnit = inspectUnit,
                timestamp = GetTime()
            }
            return
        end
    end

    -- 统一使用位置信息生成缓存键
    local key
    local isChatLink = false  -- 标记是否为聊天框链接
    -- 【飞升系统支持】飞升系统的物品强制使用GUID格式
    if isAscensionItem and guid and guid > 0 then
        isChatLink = true
        key = MakeKey(itemID, guid, nil, nil, false)
    elseif bagNum ~= nil and slotNum ~= nil then
        key = MakeKey(itemID, nil, bagNum, slotNum, bagNum == 255)
    elseif guid and guid > 0 then
        -- 【修复】有GUID但没有位置信息，也是聊天框链接的情况
        -- 标记为聊天框链接，以便后续使用GUID格式查询
        isChatLink = true
        key = MakeKey(itemID, guid, nil, nil, false)
    else
        -- 没有位置信息且没有有效GUID，可能是聊天框链接
        -- 使用物品指纹作为缓存键（仅用于显示提示，不进行查询）
        isChatLink = true
        key = "CHAT:" .. tooltipItemString
    end

    -- 【调试日志】缓存键生成结果
    if DB.debug then
        print(string.format("|cff00ffff[缓存键]|r key=%s, isChatLink=%s, bagNum=%s, slotNum=%s, guid=%s",
            key, tostring(isChatLink), tostring(bagNum), tostring(slotNum), tostring(guid)))
    end

    TraceLog(itemID, "OnTooltipSetItem key=%s original=%s/%s extracted=%s/%s final=%s/%s guid=%s chat=%s equipTip=%s pendingEarly=%s",
        tostring(key), tostring(originalBag), tostring(originalSlot), tostring(extractedBag), tostring(extractedSlot), tostring(bagNum), tostring(slotNum), tostring(guid), tostring(isChatLink), tostring(isEquipmentSlotTooltip), tostring(isPendingIdentifyEarly))
    TraceLog(itemID, "OnTooltipSetItem tooltip=%s preferEquipped=%s", tostring(tooltipName), tostring(preferEquipped))

    -- 【关键修复】本地背包/装备 tooltip 已有位置 key 时，清理不可靠的 GUID 缓存
    if bagNum ~= nil and slotNum ~= nil and guid and guid > 0 then
        local guidKey = MakeKey(itemID, guid, nil, nil, false)
        if guidKey ~= key and (State.cache[guidKey] or State.pending[guidKey]) then
            TraceLog(itemID, "OnTooltipSetItem purgeGuidCache guidKey=%s positionKey=%s", tostring(guidKey), tostring(key))
            State.cache[guidKey] = nil
            State.cacheTime[guidKey] = nil
            State.pending[guidKey] = nil
            State.lastQuery[guidKey] = nil
            State.noDataUntil[guidKey] = nil
        end
    end

    -- 【关键修复】在查询前主动检查槽位指纹变化
    -- 因为tooltip查询可能发生在BAG_UPDATE事件之前，需要主动检测物品变化
    -- 【注意】装备栏物品(bagNum=255)也需要检查指纹
    if bagNum ~= nil and slotNum ~= nil then
        local slotKey = string.format("%d:%d", bagNum, slotNum)
        local previousFingerprint = SlotFingerprints[slotKey]

        -- 【关键修复】如果缓存存在，检查缓存的指纹是否与当前物品匹配
        local existingCache = State.cache[key]
        if existingCache then
            local shouldClearCache = false
            local clearReason = ""

            -- 情况1: 缓存有指纹，但与当前物品不同
            if existingCache.fingerprint and existingCache.fingerprint ~= tooltipItemString then
                shouldClearCache = true
                clearReason = "缓存指纹不匹配"
            end

            -- 情况2: SlotFingerprints有记录，但与当前物品不同（说明槽位物品变化了）
            if previousFingerprint and previousFingerprint ~= tooltipItemString then
                shouldClearCache = true
                clearReason = clearReason .. (clearReason ~= "" and "+" or "") .. "槽位指纹不匹配"
            end

            -- 【修改】情况3: 缓存没有指纹，但当前物品有指纹
            -- 这种情况下不清除缓存，而是更新指纹（避免旧缓存被反复清除）
            -- 原代码：if not existingCache.fingerprint and existingCache.guid then shouldClearCache = true
            -- 新逻辑：只有当缓存确实与当前物品不匹配时才清除

            if shouldClearCache then
                State.cache[key] = nil
                State.cacheTime[key] = nil
                State.pending[key] = nil
                State.lastQuery[key] = nil
            elseif not existingCache.fingerprint then
                -- 如果缓存没有指纹但物品匹配，更新指纹而不是清除缓存
                existingCache.fingerprint = tooltipItemString
            end
        end

        -- 更新指纹记录
        SlotFingerprints[slotKey] = tooltipItemString
    end

    -- 先触发幻境/批量查询
    local now = GetTime()
    HuanJingRequest(itemID, guid, key, now)

    -- 直接使用key查找缓存
    local cached = State.cache[key]

    -- 【关键修复】待鉴定物品使用提前检查的结果，跳过指纹验证
    -- 原因：同ID的待鉴定物品链接完全相同，指纹检查会导致它们匹配到已鉴定物品的缓存
    local isPendingIdentify = isPendingIdentifyEarly
    local pendingInfo = nil

    -- 如果槽位缓存与当前物品不匹配，清理以避免串数据
    -- 【关键】待鉴定物品跳过指纹检查，因为同ID物品的指纹完全相同
    if cached and not isPendingIdentifyEarly then
        local fingerprintMismatch = false
        local mismatchReason = ""
        if tooltipItemString then
            if cached.fingerprint and cached.fingerprint ~= tooltipItemString then
                fingerprintMismatch = true
                mismatchReason = "缓存指纹与当前不同"
            end
            -- 【修复】移除"缓存无指纹"导致清除缓存的逻辑
            -- 原代码会在缓存没有指纹时清除缓存，导致反复查询
            -- 现在：如果缓存没有指纹，稍后会更新指纹而不是清除
        end

        -- 【关键修复】使用位置键时，不再比较GUID
        -- 原因：位置键(P:bag:slot:itemID)已经唯一标识物品
        -- GUID比较会因为不同来源的GUID值不一致而导致误清除缓存
        -- 只有当使用GUID键时才比较GUID
        local isPositionKey = bagNum ~= nil and slotNum ~= nil
        local guidMismatch = false
        if not isPositionKey and cached.guid and guid and cached.guid ~= guid then
            guidMismatch = true
        end

        if (cached.itemID and cached.itemID ~= itemID)
            or guidMismatch
            or fingerprintMismatch then
            State.cache[key] = nil
            State.cacheTime[key] = nil
            State.pending[key] = nil
            if State.itemIdToKeys and State.itemIdToKeys[itemID] then
                for i = #State.itemIdToKeys[itemID], 1, -1 do
                    if State.itemIdToKeys[itemID][i] == key then
                        table.remove(State.itemIdToKeys[itemID], i)
                    end
                end
                if #State.itemIdToKeys[itemID] == 0 then
                    State.itemIdToKeys[itemID] = nil
                end
            end
            cached = nil
        elseif not cached.fingerprint and tooltipItemString then
            -- 如果缓存存在但没有指纹，更新指纹而不是清除
            cached.fingerprint = tooltipItemString
        end
    end

    -- 待鉴定物品：重新检查并设置状态（使用之前的early检查结果）
    if not isPendingIdentify and bagNum ~= nil and slotNum ~= nil and itemID then
        pendingInfo = GetPendingIdentifyInfo(bagNum, slotNum, itemID)
        if pendingInfo and pendingInfo.isPending then
            isPendingIdentify = true
        end
    end

    -- 待鉴定物品：不使用其他物品的缓存，确保每个槽位独立
    if isPendingIdentify and cached then
        -- 检查缓存是否属于当前槽位（而不是其他同ID物品）
        -- 如果缓存有数据但不是待鉴定状态，说明是其他物品的缓存
        if not cached.isPendingIdentify then
            -- 清除缓存，避免使用已鉴定物品的数据
            State.cache[key] = nil
            State.cacheTime[key] = nil
            cached = nil
        end
    end

    local hasCompleteCache = false

    if cached and cached.systems then
        hasCompleteCache = true

        -- 检查是否缺少系统数据
        for systemName, enabled in pairs(DB.systems) do
            if enabled then
                local systemData = cached.systems[systemName]
                if not systemData then
                    hasCompleteCache = false
                    break
                end
            end
        end
    end

    -- 【调试日志】缓存状态
    if DB.debug then
        print(string.format("|cff00ffff[缓存状态]|r key=%s, cached=%s, hasCompleteCache=%s",
            key, tostring(cached ~= nil), tostring(hasCompleteCache)))
    end

    TraceLog(itemID, "OnTooltipSetItem state key=%s pending=%s cached=%s complete=%s pendingIdentify=%s cacheSystems=%s",
        tostring(key), tostring(State.pending[key] ~= nil), tostring(cached ~= nil), tostring(hasCompleteCache), tostring(isPendingIdentify), CollectTraceKeys(cached and cached.systems))

    if not hasCompleteCache then
        -- 聊天框链接处理：如果有有效GUID可以使用GUID格式查询
        if isChatLink then
            -- 【修复】聊天框链接也可以查询，只要有有效GUID
            if guid and guid > 0 then
                -- 【调试日志】聊天框链接有GUID，尝试查询
                if DB.debug then
                    print(string.format("|cff00ff00[聊天框链接]|r 有有效GUID=%d，发送GUID格式查询 key=%s", guid, key))
                end
                -- 【修复】使用nil作为bag/slot，让SendQuery使用isChatLink分支处理
                SendQuery(itemID, nil, nil, guid, tooltipItemString, false, true)  -- isChatLink=true
            else
                -- 没有有效GUID，无法查询
                -- 为聊天框链接创建特殊标记缓存
                State.cache[key] = {
                    itemID = itemID,
                    guid = guid,
                    isChatLink = true,
                    noData = true,
                    systems = {}
                }
                State.cacheTime[key] = GetTime()
                -- 【调试日志】聊天框链接
                if DB.debug then
                    print(string.format("|cffff8800[聊天框链接]|r 无有效GUID，跳过查询 key=%s", key))
                end
            end
        elseif not isPendingIdentify then
            -- 【关键修复】检测是否正在观察其他玩家
            -- 如果是观察其他玩家，需要使用GUID格式查询（服务器无法访问其他玩家的背包）
            local isInspectOther = inspectUnit and UnitExists(inspectUnit) and not UnitIsUnit(inspectUnit, "player")

            -- 【调试日志】发送查询
            if DB.debug then
                print(string.format("|cff00ff00[发送查询]|r itemID=%d, bag=%s, slot=%s, guid=%s, key=%s, isInspectOther=%s",
                    itemID, tostring(bagNum), tostring(slotNum), tostring(guid), key, tostring(isInspectOther)))
            end
            SendQuery(itemID, bagNum, slotNum, guid, tooltipItemString, isInspectOther)
        else
            -- 为待鉴定物品创建空缓存标记，防止RefreshUnifiedFrame显示其他物品的数据
            State.cache[key] = {
                itemID = itemID,
                guid = guid,
                isPendingIdentify = true,
                noData = true,
                systems = {}
            }
            State.cacheTime[key] = GetTime()
        end
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

    -- 【新增】查询待鉴定物品列表（定期刷新）
    QueryPendingIdentifyList()

    if tName == "GameTooltip" and not IsTooltipFromChat(tooltip) then
        if not isEquipmentSlotTooltip and GameTooltip_ShowCompareItem then
            GameTooltip_ShowCompareItem(tooltip)
        else
            if ShoppingTooltip1 then
                ShoppingTooltip1:Hide()
            end
            if ShoppingTooltip2 then
                ShoppingTooltip2:Hide()
            end
        end
    end

    if tooltip.UIT_UnifiedFrame then
        local oldFrame = tooltip.UIT_UnifiedFrame
        oldFrame:Hide()

        if oldFrame.UIT_Key and UnifiedFramesByKey[oldFrame.UIT_Key] then
            local oldList = UnifiedFramesByKey[oldFrame.UIT_Key]
            for i = #oldList, 1, -1 do
                if oldList[i] == oldFrame then
                    table.remove(oldList, i)
                end
            end
            if #oldList == 0 then
                UnifiedFramesByKey[oldFrame.UIT_Key] = nil
            end
        end

        oldFrame.UIT_Key = nil
        oldFrame.UIT_ItemID = nil
        oldFrame.UIT_GUID = nil
        State.tooltips[oldFrame] = nil
    end

    ClearTooltipMeta(tooltip)
    RenderTooltip(tooltip, itemID, guid, bagNum, slotNum)

    local tooltipMeta = State.tooltips[tooltip]
    local hasCustomData = tooltipMeta and tooltipMeta.key == key and (
        tooltipMeta.rendered.identification
        or tooltipMeta.rendered.enhancement
        or tooltipMeta.rendered.growth
        or tooltipMeta.rendered.skills
        or tooltipMeta.rendered.runes
        or tooltipMeta.rendered.sets
        or tooltipMeta.rendered.magic
    )

    local cacheKey = nil
    if bagNum ~= nil and slotNum ~= nil and itemID then
        cacheKey = string.format("%d:%d:%d", bagNum, slotNum, itemID)
    end

    if hasCustomData and not isPendingIdentify then
        if cacheKey then
            PendingIdentifyState.cache[cacheKey] = nil
            SuppressPendingIdentify(bagNum, slotNum, itemID, 6)
        end
    else
        local pendingInfo = pendingInfo or GetPendingIdentifyInfo(bagNum, slotNum, itemID)
        if pendingInfo and pendingInfo.isPending then
            tooltip:AddLine(" ")
            local multiplierText = ""
            if HasHuanJingEffect(pendingInfo.multiplier, pendingInfo.mode) then
                if NormalizeHuanJingMode(pendingInfo.mode) == "+" then
                    multiplierText = " |cFF00FF00+" .. pendingInfo.multiplier .. "|r"
                else
                    multiplierText = " |cFF00FF00x" .. pendingInfo.multiplier .. "倍率|r"
                end
            end
            tooltip:AddLine("|cFFFF0000【待鉴定】|r" .. multiplierText)
            tooltip:Show()
        end
    end

end

local function OnTooltipCleared(tooltip)
    ClearTooltipMeta(tooltip)

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

        -- 【关键修复】清除frame的UIT_Key，防止下次悬停不同物品时使用旧key
        frame.UIT_Key = nil
        frame.UIT_ItemID = nil
        frame.UIT_GUID = nil

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
    ShoppingTooltip1:HookScript("OnShow", UIT_ForceHideTooltip)
end
if ShoppingTooltip2 then
    ShoppingTooltip2:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ShoppingTooltip2:HookScript("OnTooltipCleared", OnTooltipCleared)
    ShoppingTooltip2:HookScript("OnShow", UIT_ForceHideTooltip)
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

    elseif msg == "停止跟踪" or msg == "关闭跟踪" or msg == "trace off" or msg == "trace stop" then
        DB.traceEnabled = false
        DB.traceItemID = 0
        State.traceLast = {}
        print("|cff00ff00[统一提示框]|r 定向追踪: 已关闭")

    elseif msg:match("^跟踪%s+") or msg:match("^trace%s+") then
        local traceArg = msg:match("%s+(.+)$")
        if traceArg == "all" then
            DB.traceEnabled = true
            DB.traceItemID = 0
            State.traceLast = {}
            print("|cff00ff00[统一提示框]|r 定向追踪: 已开启（全部 itemID）")
        else
            local traceItemID = tonumber(traceArg and traceArg:match("%d+"))
            if traceItemID and traceItemID > 0 then
                DB.traceEnabled = true
                DB.traceItemID = traceItemID
                State.traceLast = {}
                print("|cff00ff00[统一提示框]|r 定向追踪: 已开启，itemID=" .. traceItemID)
                print("|cff888888示例: /提示框 停止跟踪|r")
            else
                print("|cffff0000[统一提示框]|r 用法: /提示框 跟踪 2649")
                print("|cff888888也支持: /提示框 trace 2649, /提示框 trace off|r")
            end
        end

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
        print("  定向追踪: " .. (DB.traceEnabled and ((tonumber(DB.traceItemID or 0) > 0) and ("开启 itemID=" .. DB.traceItemID) or "开启（全部）") or "关闭"))

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
