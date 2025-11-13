-- UnifiedItemTooltip.lua
-- 统一的物品提示框插件 - 根据服务器数据动态显示所有属性

local ADDON_NAME = "UnifiedItemTooltip"

-- ============================================================================
-- 配置
-- ============================================================================

local DEFAULTS = {
    debug = true,  -- 临时开启调试以排查问题
    queryInterval = 1,      -- 查询间隔（秒）- 只在所有数据齐全时生效
    timeout = 15,           -- 查询超时（秒）- 增加到15秒以应对服务器延迟
    emptyCooldown = 30,     -- 无数据冷却时间（秒）
    cacheExpiration = 300,  -- 缓存有效期（秒）- 默认5分钟
    commandDelay = 0.05,    -- 命令发送间隔（秒）- 避免一次性发送太多命令

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
    stats = {}
}

-- ============================================================================
-- 工具函数
-- ============================================================================

-- 生成缓存键
local function MakeKey(itemID, guid, bag, slot, equipFlag)
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

-- 文本换行工具函数
-- 将长文本按照指定宽度分割成多行，正确保留颜色代码和中文字符
local function WrapText(text, maxCharsPerLine)
    if not text or text == "" then return {text} end

    maxCharsPerLine = maxCharsPerLine or 40  -- 默认每行40个显示字符

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
-- 数据解析器 - 统一处理所有系统的消息
-- ============================================================================

local Parsers = {}

-- 解析魔次系统消息
function Parsers.Magic(message)
    if not message:match("^MAGICHIT:") then return nil end

    local parts = { strsplit(":", message) }
    if #parts < 8 then return nil end

    local itemID = tonumber(parts[2])
    local guid = tonumber(parts[3])
    local configsStr = parts[4] or ""

    if not itemID or not guid or guid == 0 then return nil end

    local configs = {}
    if configsStr ~= "" then
        for chunk in string.gmatch(configsStr, "([^,]+)") do
            local id, count, desc = chunk:match("(%d+)|(%d+)|(.+)")
            if id and count then
                table.insert(configs, {
                    id = tonumber(id),
                    count = tonumber(count),
                    desc = desc or ("配置" .. id)
                })
            end
        end
    end

    return {
        type = "magic",
        itemID = itemID,
        guid = guid,
        configs = configs
    }
end

-- 解析成长系统消息
function Parsers.Growth(message)
    if not message:match("^ITEMGROWTH:") then return nil end

    local parts = { strsplit(":", message) }
    if #parts < 7 then return nil end

    local itemID = tonumber(parts[2])
    local guid = tonumber(parts[3])
    local level = tonumber(parts[4])
    local currentExp = tonumber(parts[5])
    local requiredExp = tonumber(parts[6])
    local attrsStr = parts[7] or ""

    if not itemID or not guid or guid == 0 then return nil end

    local attributes = {}
    if attrsStr ~= "" then
        for pair in string.gmatch(attrsStr, "([^,]+)") do
            local attrType, value = pair:match("(%d+)%s+(%-?%d+)")
            if attrType and value then
                table.insert(attributes, {
                    type = tonumber(attrType),
                    value = tonumber(value)
                })
            end
        end
    end

    return {
        type = "growth",
        itemID = itemID,
        guid = guid,
        level = level,
        currentExp = currentExp,
        requiredExp = requiredExp,
        attributes = attributes
    }
end

-- 解析鉴定系统消息
function Parsers.Identification(message)
    -- 先去除可能的前缀
    local cleaned = message:gsub("^【鉴定属性】", ""):gsub("^【追加属性】", "")

    if not cleaned:match("ITEM_ID_ATTRS:") then return nil end

    -- 新格式: ITEM_ID_ATTRS:itemID:guid:基础属性:追加属性
    -- 使用手动分割，正确处理空字符串的情况
    local prefix, rest = cleaned:match("^(ITEM_ID_ATTRS):(.*)$")
    if not prefix or not rest then return nil end

    -- 手动分割，保留空字符串
    local colonPos1 = rest:find(":")
    if not colonPos1 then return nil end

    local itemIDStr = rest:sub(1, colonPos1 - 1)
    rest = rest:sub(colonPos1 + 1)

    local colonPos2 = rest:find(":")
    if not colonPos2 then return nil end

    local guidStr = rest:sub(1, colonPos2 - 1)
    rest = rest:sub(colonPos2 + 1)

    -- 找到第三个冒号，分离基础属性和追加属性
    local colonPos3 = rest:find(":")
    local baseAttrsStr, additionalAttrsStr

    if colonPos3 then
        baseAttrsStr = rest:sub(1, colonPos3 - 1)
        additionalAttrsStr = rest:sub(colonPos3 + 1)
    else
        -- 如果没有第三个冒号，说明只有基础属性，没有追加属性
        baseAttrsStr = rest
        additionalAttrsStr = ""
    end

    local itemID = tonumber(itemIDStr)
    local guid = tonumber(guidStr)

    if not itemID or not guid then return nil end

    -- 去除前后空白
    baseAttrsStr = baseAttrsStr:gsub("^%s+", ""):gsub("%s+$", "")
    additionalAttrsStr = additionalAttrsStr:gsub("^%s+", ""):gsub("%s+$", "")

    -- 解析基础属性（只有当字符串不为空时才解析）
    local baseAttributes = {}
    if baseAttrsStr ~= "" and #baseAttrsStr > 0 then
        for pair in string.gmatch(baseAttrsStr, "[^,]+") do
            local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
            if attrType and value then
                table.insert(baseAttributes, {
                    type = tonumber(attrType),
                    value = tonumber(value)
                })
            end
        end
    end

    -- 解析追加属性（只有当字符串不为空时才解析）
    local additionalAttributes = {}
    if additionalAttrsStr ~= "" and #additionalAttrsStr > 0 then
        for pair in string.gmatch(additionalAttrsStr, "[^,]+") do
            local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
            if attrType and value then
                table.insert(additionalAttributes, {
                    type = tonumber(attrType),
                    value = tonumber(value)
                })
            end
        end
    end

    -- 修复：注释掉此行，即使没有属性也应该返回数据以便缓存，避免未鉴定装备重复查询
    -- if #baseAttributes == 0 and #additionalAttributes == 0 then return nil end

    return {
        type = "identification",
        itemID = itemID,
        guid = guid,
        baseAttributes = baseAttributes,
        additionalAttributes = additionalAttributes
    }
end

-- 解析强化系统消息
function Parsers.Enhancement(message)
    local cleaned = message:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "")

    -- 尝试新格式：ITEM_ENHANCEMENT:itemID:guid:level:attrs
    if message:match("^ITEM_ENHANCEMENT:") then
        local parts = { strsplit(":", message) }
        if #parts >= 4 then
            local itemID = tonumber(parts[2])
            local guid = tonumber(parts[3])
            local level = tonumber(parts[4])
            local stats = parts[5] or ""

            if itemID and guid then
                local attributes = {}
                if stats ~= "" then
                    for pair in string.gmatch(stats, "([^,]+)") do
                        local attrType, value = pair:match("(%d+)%s+([%+%-]?%d+)")
                        if attrType and value then
                            table.insert(attributes, {
                                type = tonumber(attrType),
                                value = tonumber(value)
                            })
                        end
                    end
                end

                return {
                    type = "enhancement",
                    itemID = itemID,
                    guid = guid,
                    level = level,
                    attributes = attributes
                }
            end
        end
    end

    -- 尝试旧格式：[强化系统] 物品ID: xxx guid: xxx 强化等级: x 属性: ...
    if cleaned:match("%[强化系统%]") or cleaned:match("物品ID:") then
        local itemID, guid, level, stats = cleaned:match("物品ID:%s*(%d+)%s+[Gg][Uu][Ii][Dd]:%s*(%d+)%s+强化等级:%s*(%d+)%s+属性:%s*(.*)$")

        if itemID and guid then
            local attributes = {}
            if stats and stats ~= "" then
                for pair in string.gmatch(stats, "([^,]+)") do
                    local attrType, value = pair:match("(%d+)%s+([%+%-]?%d+)")
                    if attrType and value then
                        table.insert(attributes, {
                            type = tonumber(attrType),
                            value = tonumber(value)
                        })
                    end
                end
            end

            return {
                type = "enhancement",
                itemID = tonumber(itemID),
                guid = tonumber(guid),
                level = tonumber(level),
                attributes = attributes
            }
        end
    end

    -- 尝试简化格式：强化等级: x 属性: ...
    if cleaned:match("强化等级:") then
        local level, stats = cleaned:match("强化等级:%s*(%d+)%s+属性:%s*(.*)$")

        if level then
            local attributes = {}
            if stats and stats ~= "" then
                -- 解析属性：格式为 "attrType value" 对，用逗号分隔
                -- 例如: "4 2,5 2,7 2" 或 "4 +2, 5 +2, 7 +2"
                local pairs = {}
                for pair in string.gmatch(stats, "([^,]+)") do
                    table.insert(pairs, pair)
                end

                for _, pair in ipairs(pairs) do
                    local attrType, value = pair:match("(%d+)%s+([%+%-]?%d+)")
                    if attrType and value then
                        table.insert(attributes, {
                            type = tonumber(attrType),
                            value = tonumber(value)
                        })
                    end
                end
            end

            -- 从当前tooltip获取物品信息
            local currentItemID, currentGUID
            if GameTooltip and GameTooltip:IsShown() then
                local _, itemLink = GameTooltip:GetItem()
                if itemLink then
                    currentItemID, currentGUID = ExtractItemInfoEnhanced(itemLink)
                end
            end

            -- 如果找不到当前物品，尝试从pending查找
            if not currentItemID or not currentGUID or currentGUID == 0 then
                for key, pending in pairs(State.pending) do
                    if pending then
                        -- 从key中提取itemID和guid
                        local id, g = key:match("G:(%d+):(%d+)")
                        if id and g then
                            currentItemID = tonumber(id)
                            currentGUID = tonumber(g)
                            break
                        end
                    end
                end
            end

            if currentItemID and currentGUID and currentGUID > 0 then
                return {
                    type = "enhancement",
                    itemID = currentItemID,
                    guid = currentGUID,
                    level = tonumber(level),
                    attributes = attributes
                }
            end
        end
    end

    return nil
end

-- 解析追加技能消息
function Parsers.Skills(message)
    if not message:match("^ITEMSKILLS:") then return nil end

    local parts = { strsplit(":", message) }
    if #parts < 4 then return nil end

    local itemID = tonumber(parts[2])
    local guid = tonumber(parts[3])
    local skillsStr = parts[4] or ""

    if not itemID or not guid or guid == 0 then return nil end

    local skills = {}
    if skillsStr ~= "" then
        for skillInfo in string.gmatch(skillsStr, "([^,]+)") do
            local name, level = skillInfo:match("(.+)%(等级(%d+)%)")
            if name then
                table.insert(skills, {
                    name = name:gsub("^%s+", ""):gsub("%s+$", ""),
                    level = tonumber(level) or 1
                })
            else
                table.insert(skills, {
                    name = skillInfo:gsub("^%s+", ""):gsub("%s+$", ""),
                    level = 1
                })
            end
        end
    end

    return {
        type = "skills",
        itemID = itemID,
        guid = guid,
        skills = skills
    }
end

-- 解析符文系统消息
function Parsers.Runes(message)
    -- 支持新格式：[RUNEINFO]guid:totalSlots:filledSlots:slotData
    -- 格式示例：[RUNEINFO]4:5:3:1,61001,1,符文1,61001,火球术+1000耐力;2,61001,1,符文1,61001,火球术+1000耐力;3,0,0,,0,
    if message:match("%[RUNEINFO%]") then
        local dataStr = message:match("%[RUNEINFO%](.+)")
        if not dataStr then return nil end

        local parts = { strsplit(":", dataStr) }
        if #parts < 3 then return nil end

        local guid = tonumber(parts[1])
        local totalSlots = tonumber(parts[2])
        local filledSlots = tonumber(parts[3])
        local slotsStr = parts[4] or ""

        if not guid or guid == 0 or not totalSlots then return nil end

        local slots = {}
        if slotsStr ~= "" then
            -- 分割槽位数据，用分号分隔
            -- 格式：slotId,runeId,runeQuality,runeName,itemId,effectDesc
            for slotInfo in string.gmatch(slotsStr, "([^;]+)") do
                local slotParts = { strsplit(",", slotInfo) }
                if #slotParts >= 3 then
                    local slotId = tonumber(slotParts[1])
                    local runeId = tonumber(slotParts[2])
                    local runeQuality = tonumber(slotParts[3]) or 0
                    local runeName = slotParts[4] or ""
                    local runeItemId = tonumber(slotParts[5]) or 0
                    local effectDesc = slotParts[6] or ""

                    if slotId and slotId > 0 then
                        table.insert(slots, {
                            slotId = slotId,
                            runeId = runeId or 0,
                            runeName = runeName,
                            runeQuality = runeQuality,
                            effectDesc = effectDesc,
                            runeItemId = runeItemId
                        })
                    end
                end
            end
        end

        return {
            type = "runes",
            itemID = 0,  -- RUNEINFO格式没有itemID，需要从上下文获取
            guid = guid,
            totalSlots = totalSlots,
            filledSlots = filledSlots,
            slots = slots
        }
    end

    -- 支持旧格式：RUNEDATA:itemID:guid:totalSlots:filledSlots:slotData
    -- 格式示例：RUNEDATA:28800:4:5:5:1:61001:未知符文:0:,2:61001:未知符文:0:,...
    if not message:match("^RUNEDATA:") then return nil end

    local parts = { strsplit(":", message) }
    if #parts < 5 then return nil end

    local itemID = tonumber(parts[2])
    local guid = tonumber(parts[3])
    local totalSlots = tonumber(parts[4])
    local filledSlots = tonumber(parts[5])

    if not itemID or not guid or guid == 0 then return nil end

    -- 槽位数据从第6个参数开始，格式：slotId:runeId:runeName:runeQuality:,
    -- 注意：最后有一个逗号分隔符
    local slots = {}
    local i = 6
    while i <= #parts do
        local slotId = tonumber(parts[i])
        if slotId and i + 3 <= #parts then
            local runeId = tonumber(parts[i + 1])
            local runeName = parts[i + 2] or ""
            local runeQuality = tonumber(parts[i + 3]) or 0

            if slotId > 0 then
                table.insert(slots, {
                    slotId = slotId,
                    runeId = runeId or 0,
                    runeName = runeName,
                    runeQuality = runeQuality,
                    effectDesc = ""
                })
            end

            -- 跳过4个字段（slotId, runeId, runeName, runeQuality）+ 1个空字段（逗号分隔符）
            i = i + 5
        else
            break
        end
    end

    return {
        type = "runes",
        itemID = itemID,
        guid = guid,
        totalSlots = totalSlots,
        filledSlots = filledSlots,
        slots = slots
    }
end

-- 解析套装系统消息
function Parsers.Sets(message)
    -- 格式：ITEMSETS_HIDDEN:SPECIFIC_ITEM_DATA:itemId:itemGuid:setId:setName:attributes:effectDesc
    if not message:match("ITEMSETS_HIDDEN:") then return nil end

    local dataStr = message:match("ITEMSETS_HIDDEN:SPECIFIC_ITEM_DATA:(.+)")
    if not dataStr then return nil end

    local parts = { strsplit(":", dataStr) }
    if #parts < 6 then return nil end

    local itemID = tonumber(parts[1])
    local guid = tonumber(parts[2])
    local setId = tonumber(parts[3])
    local setName = parts[4] or ""
    local attributesStr = parts[5] or ""

    -- 效果描述可能包含冒号，需要拼接parts[6]及之后的所有内容
    local effectDescStr = ""
    if #parts >= 6 then
        for i = 6, #parts do
            if effectDescStr ~= "" then
                effectDescStr = effectDescStr .. ":"
            end
            effectDescStr = effectDescStr .. parts[i]
        end
    end

    if not itemID or not guid or guid == 0 or not setId or setId == 0 then return nil end

    -- 解析套装属性：格式 "4 20,7 30,31 15"
    local attributes = {}
    if attributesStr ~= "" then
        for pair in string.gmatch(attributesStr, "([^,]+)") do
            local attrType, value = pair:match("(%d+)%s+([%-]?%d+)")
            if attrType and value then
                table.insert(attributes, {
                    type = tonumber(attrType),
                    value = tonumber(value)
                })
            end
        end
    end

    -- 解析效果描述：格式 "2件套:效果1|4件套:效果2"
    local effects = {}
    if effectDescStr ~= "" then
        for effectInfo in string.gmatch(effectDescStr, "([^|]+)") do
            local count, desc = effectInfo:match("(%d+)件套:(.+)")
            if count and desc then
                table.insert(effects, {
                    count = tonumber(count),
                    desc = desc
                })
            end
        end
    end

    return {
        type = "sets",
        itemID = itemID,
        guid = guid,
        setId = setId,
        setName = setName,
        attributes = attributes,
        effects = effects
    }
end

-- ⭐ 新增：批量数据解析器（解析ALL_MODULE_DATA消息）
function Parsers.BatchQuery(message)
    -- 格式：ALL_MODULE_DATA:itemID:guid:base:additional:growth:enhancement:skills:magic:rune:set
    if not message:match("^ALL_MODULE_DATA:") then return nil end

    local parts = { strsplit(":", message) }
    if #parts < 3 then return nil end

    local itemID = tonumber(parts[2])
    local guid = tonumber(parts[3])

    if not itemID or not guid or guid == 0 then return nil end

    -- 解析所有系统数据
    local baseAttributes = parts[4] or ""
    local additionalAttributes = parts[5] or ""
    local growthData = parts[6] or ""
    local enhancementData = parts[7] or ""
    local skillsData = parts[8] or ""
    local magicData = parts[9] or ""
    local runeData = parts[10] or ""
    local setData = parts[11] or ""

    DebugPrint("[批量解析] itemID=", itemID, "guid=", guid, "parts=", #parts)

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

            result.systems.sets = {
                type = "sets",
                itemID = itemID,
                guid = guid,
                setId = tonumber(sParts[1]),
                setName = sParts[2] or "套装",  -- 如果没有setName，使用默认值
                attributes = attrs,
                effects = {}  -- 简化版，不解析详细效果
            }
        end
    end

    -- 统计解析的系统数量
    local sysCount = 0
    for _ in pairs(result.systems) do sysCount = sysCount + 1 end
    DebugPrint("[批量解析] 完成: 解析了", sysCount, "个系统")

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

        -- 使用WrapText函数将长文本分行显示，每行最多40个字符
        local lines = WrapText(text, 40)
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

-- 获取缓存数据
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
                -- 如果系统数据不存在，或者是空数据（isEmpty），需要重新查询
                if not systemData or systemData.isEmpty then
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

-- 发送查询请求（⭐ 优化：使用批量查询，只发送一个命令）
local function SendQuery(itemID, guid)
    if not itemID or not guid or guid == 0 then return end

    local key = MakeKey(itemID, guid, nil, nil, false)

    if ShouldSkipQuery(key) then
        return
    end

    -- 立即标记为查询中，防止重复发送
    local now = GetTime()
    State.lastQuery[key] = now
    State.pending[key] = { started = now, systems = {} }

    -- 获取已缓存的系统
    local cached = State.cache[key]
    local cachedSystems = {}
    if cached and cached.systems then
        for systemName, _ in pairs(cached.systems) do
            cachedSystems[systemName] = true
        end
    end

    -- ⭐ 新方案：使用批量查询命令，一次性查询所有系统数据
    -- 旧方案：7个命令，每个延迟0.05秒，总计0.3秒 + 7次网络往返
    -- 新方案：1个命令，0秒延迟，1次网络往返，性能提升70-85%

    local batchCommand = string.format(".鉴定 批量查询 %d %d", itemID, guid)

    DebugPrint("[批量查询] 发送命令:", batchCommand)

    -- 标记所有系统为查询中
    for systemName, enabled in pairs(DB.systems) do
        if enabled and not cachedSystems[systemName] then
            State.pending[key].systems[systemName] = now
        end
    end

    -- 发送批量查询命令（只发送一次！）
    SendChatMessage(batchCommand, "WHISPER", nil, UnitName("player"))

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

    -- 步骤1：只有当鉴定系统有基础属性时，才隐藏官方属性
    -- 如果只有追加属性（没有基础属性），保留官方属性显示
    if #baseAttributes > 0 then
        local numLines = tooltip:NumLines()
        for i = 1, numLines do
            local leftText = _G[tooltip:GetName() .. "TextLeft" .. i]
            if leftText then
                local text = leftText:GetText() or ""
                if text ~= "" and IsOfficialStatLine(text) then
                    leftText:SetText("")  -- 隐藏官方属性
                end
            end
        end
    end

    -- 步骤2：在tooltip末尾添加鉴定系统的基础属性
    if #baseAttributes > 0 then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "基础属性" .. DB.colors.reset)

        for _, attr in ipairs(baseAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            -- 使用AddDoubleLine实现完美对齐
            tooltip:AddDoubleLine(
                string.format("+%d", attr.value),
                name,
                0, 1, 0,  -- 左列：绿色
                0, 1, 0   -- 右列：绿色
            )
        end
    end

    -- 步骤3：在tooltip末尾添加鉴定系统的追加属性（如果有）
    if #additionalAttributes > 0 then
        tooltip:AddLine(" ")
        tooltip:AddLine(DB.colors.header .. "追加属性" .. DB.colors.reset)

        for _, attr in ipairs(additionalAttributes) do
            local name = ATTR_NAMES[attr.type] or ("属性" .. attr.type)
            -- 使用AddDoubleLine实现完美对齐
            tooltip:AddDoubleLine(
                string.format("+%d", attr.value),
                name,
                0, 1, 0,  -- 左列：绿色
                0, 1, 0   -- 右列：绿色
            )
        end
    end

    -- 步骤4：渲染强化系统的属性（独立区块）
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
                tooltip:AddDoubleLine(
                    string.format("+%d", attr.value),
                    name,
                    0, 1, 0,  -- 左列：绿色
                    0, 1, 0   -- 右列：绿色
                )
            end
        end
    end

    -- 步骤5：渲染成长系统的属性（独立区块）
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
                tooltip:AddDoubleLine(
                    string.format("+%d", attr.value),
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
local function RenderTooltip(tooltip, itemID, guid)
    if not itemID or not guid or guid == 0 then return end

    local key = MakeKey(itemID, guid, nil, nil, false)
    local meta = GetTooltipMeta(tooltip, key)
    local cached = GetCachedData(itemID, guid)

    -- 无论是否有缓存，都尝试发送查询（SendQuery内部会检查是否需要查询）
    SendQuery(itemID, guid)

    -- 检查是否正在查询中
    local isPending = State.pending[key] ~= nil

    if not cached then
        -- 完全没有缓存
        if isPending then
            -- 正在查询中
            tooltip:AddLine(" ")
            tooltip:AddLine(DB.colors.special .. "正在加载属性..." .. DB.colors.reset)
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
        DebugPrint("[渲染] 缓存标记为noData，跳过渲染")
        return
    end

    DebugPrint("[渲染] 开始渲染属性: itemID=", itemID, "guid=", guid, "系统数量=", #(cached.systems or {}))

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


-- 处理聊天消息
local function OnChatMessage(self, event, message, sender)
    if not message or type(message) ~= "string" then
        return
    end

    local receiveTime = GetTime()

    -- ⭐ 优先尝试批量数据解析器
    local batchData = Parsers.BatchQuery(message)
    if batchData then
    end
    if batchData then
        DebugPrint("[批量数据] 收到批量响应: itemID=", batchData.itemID, "guid=", batchData.guid)

        local key = MakeKey(batchData.itemID, batchData.guid, nil, nil, false)
        local hasAnyData = false

        -- 将批量数据拆分并缓存到各个系统
        for systemName, systemData in pairs(batchData.systems) do
            hasAnyData = true
            CacheData(batchData.itemID, batchData.guid, systemData)
        end

        -- 如果没有任何系统数据，创建空缓存并清除pending状态
        if not hasAnyData then
            -- 创建空缓存结构
            if not State.cache[key] then
                State.cache[key] = {
                    itemID = batchData.itemID,
                    guid = batchData.guid,
                    systems = {},
                    noData = true  -- 标记为无数据
                }
                State.cacheTime[key] = GetTime()
            end

            -- 清除pending状态，避免继续显示"正在加载"
            State.pending[key] = nil
            State.noDataUntil[key] = nil
        end

        -- 更新所有相关的提示框
        for tooltip, meta in pairs(State.tooltips) do
            if tooltip:IsShown() and meta.key == key then
                RenderTooltip(tooltip, batchData.itemID, batchData.guid)
            end
        end

        return
    end

    -- 尝试旧的单个解析器（向后兼容）
    for name, parser in pairs(Parsers) do
        if name ~= "BatchQuery" then  -- 跳过批量解析器
            local data = parser(message)
            if data then
                -- 特殊处理 RUNEINFO 格式（没有 itemID）
                if data.type == "runes" and (not data.itemID or data.itemID == 0) then
                    -- 尝试从当前显示的 tooltip 获取 itemID
                    for tooltip, meta in pairs(State.tooltips) do
                        if tooltip:IsShown() and meta.key then
                            local _, itemLink = tooltip:GetItem()
                            if itemLink then
                                local itemID, itemGUID = ExtractItemInfoEnhanced(itemLink)
                                if itemID and itemGUID == data.guid then
                                    data.itemID = itemID
                                    break
                                end
                            end
                        end
                    end

                    -- 如果还是没有 itemID，尝试从缓存中查找
                    if not data.itemID or data.itemID == 0 then
                        for key, cache in pairs(State.cache) do
                            if cache.guid == data.guid then
                                data.itemID = cache.itemID
                                break
                            end
                        end
                    end
                end

                -- 如果是符文系统数据但没有 itemID，跳过缓存但仍然更新 tooltip
                if data.type == "runes" and (not data.itemID or data.itemID == 0) then
                    -- 直接更新所有显示中的 tooltip
                    for tooltip, meta in pairs(State.tooltips) do
                        if tooltip:IsShown() then
                            local _, itemLink = tooltip:GetItem()
                            if itemLink then
                                local itemID, itemGUID = ExtractItemInfoEnhanced(itemLink)
                                if itemGUID == data.guid then
                                    data.itemID = itemID
                                    local key = MakeKey(itemID, itemGUID, nil, nil, false)
                                    CacheData(itemID, itemGUID, data)
                                    RenderTooltip(tooltip, itemID, itemGUID)
                                    break
                                end
                            end
                        end
                    end
                    return
                end

                local key = CacheData(data.itemID, data.guid, data)

                -- 更新所有相关的提示框
                for tooltip, meta in pairs(State.tooltips) do
                    if tooltip:IsShown() and meta.key == key then
                        RenderTooltip(tooltip, data.itemID, data.guid)
                    end
                end

                return
            end
        end
    end
end

-- 注册所有相关的聊天事件
EventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
EventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
EventFrame:RegisterEvent("CHAT_MSG_WHISPER")
EventFrame:RegisterEvent("CHAT_MSG_YELL")
EventFrame:RegisterEvent("CHAT_MSG_GUILD")
EventFrame:SetScript("OnEvent", OnChatMessage)


-- ============================================================================
-- Tooltip Hook
-- ============================================================================

local function OnTooltipSetItem(tooltip)
    local _, itemLink = tooltip:GetItem()
    if not itemLink then
        return
    end

    -- 使用增强版提取，如果链接没有 GUID，会尝试从背包/装备栏查找
    local itemID, guid, bag, slot, isEquipped = ExtractItemInfoEnhanced(itemLink)
    if not itemID then
        return
    end

    if guid and guid > 0 then
        RenderTooltip(tooltip, itemID, guid)
    else
        -- 没有找到 GUID，显示提示信息
        tooltip:AddLine(" ")
        tooltip:AddLine("|cffff8000[提示] 此物品不在你的背包或装备栏中|r")
        tooltip:AddLine("|cffff8000无法显示自定义属性|r")
        tooltip:Show()
    end
end

local function OnTooltipCleared(tooltip)
    ClearTooltipMeta(tooltip)
end

-- Hook游戏提示框

GameTooltip:HookScript("OnTooltipSetItem", OnTooltipSetItem)
GameTooltip:HookScript("OnTooltipCleared", OnTooltipCleared)


-- Hook物品对比提示框
if ShoppingTooltip1 then
    ShoppingTooltip1:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ShoppingTooltip1:HookScript("OnTooltipCleared", OnTooltipCleared)
    print("|cFFFF0000[步骤7]|r ShoppingTooltip1钩子注册完成")
end
if ShoppingTooltip2 then
    ShoppingTooltip2:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ShoppingTooltip2:HookScript("OnTooltipCleared", OnTooltipCleared)
    print("|cFFFF0000[步骤7]|r ShoppingTooltip2钩子注册完成")
end

-- Hook物品引用提示框（聊天框链接点击）
if ItemRefTooltip then
    ItemRefTooltip:HookScript("OnTooltipSetItem", OnTooltipSetItem)
    ItemRefTooltip:HookScript("OnTooltipCleared", OnTooltipCleared)
    print("|cFFFF0000[步骤7]|r ItemRefTooltip钩子注册完成")
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

    elseif msg:match("^设置间隔%s+") or msg:match("^delay%s+") then
        local delay = tonumber(msg:match("[%d%.]+"))
        if delay and delay >= 0 and delay <= 1 then
            DB.commandDelay = delay
            print("|cff00ff00[统一提示框]|r 命令间隔已设置为: " .. delay .. " 秒")
            print("|cff888888提示: 设置为0表示一次性发送所有命令（可能导致排队）|r")
        else
            print("|cffff0000[统一提示框]|r 无效的间隔值，请输入0-1之间的数字")
            print("|cff888888示例: /提示框 设置间隔 0.1|r （0.1秒）")
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
            local itemID, guid = ExtractItemInfoEnhanced(itemLink)
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
        print("|cff00ff00[统一提示框]|r 缓存状态:")
        print("  总缓存数: " .. (State.cache and #State.cache or 0))
        print("  查询中: " .. (State.pending and #State.pending or 0))
        print("  冷却中: " .. (State.noDataUntil and #State.noDataUntil or 0))
        print("\n|cff00ff00[统一提示框]|r 性能配置:")
        print("  缓存有效期: " .. DB.cacheExpiration .. " 秒 (" .. math.floor(DB.cacheExpiration / 60) .. " 分钟)")
        print("  查询超时: " .. DB.timeout .. " 秒")
        print("  命令间隔: " .. DB.commandDelay .. " 秒")
        print("  调试模式: " .. (DB.debug and "开启" or "关闭"))

        -- 显示当前装备的详细信息
        local _, itemLink = GameTooltip:GetItem()
        if itemLink then
            local itemID, guid = ExtractItemInfoEnhanced(itemLink)
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
        DB.commandDelay = 0.15
        print("|cff00ff00[统一提示框]|r 已应用慢速服务器优化:")
        print("  查询超时: 20 秒")
        print("  命令间隔: 0.15 秒")
        print("|cff888888此配置适用于响应慢的服务器|r")

    elseif msg == "优化速度" or msg == "快速模式" or msg == "fast" then
        DB.timeout = 10
        DB.commandDelay = 0.05
        print("|cff00ff00[统一提示框]|r 已应用快速服务器优化:")
        print("  查询超时: 10 秒")
        print("  命令间隔: 0.05 秒")
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
        print("\n|cffffcc00性能优化:|r")
        print("  |cffffcc00/提示框 性能|r - 显示各系统响应时间统计")
        print("  |cffffcc00/提示框 优化延迟|r - 应用慢速服务器预设（推荐⭐）")
        print("  |cffffcc00/提示框 优化速度|r - 应用快速服务器预设")
        print("  |cffffcc00/提示框 设置超时 <秒数>|r - 设置查询超时时间")
        print("    |cff888888示例: /提示框 设置超时 20|r （20秒）")
        print("  |cffffcc00/提示框 设置间隔 <秒数>|r - 设置命令发送间隔")
        print("    |cff888888示例: /提示框 设置间隔 0.15|r （0.15秒间隔）")
        print("  |cffffcc00/提示框 设置有效期 <秒数>|r - 设置缓存有效期")
        print("    |cff888888示例: /提示框 设置有效期 600|r （10分钟）")
        print("\n|cffffcc00其他:|r")
        print("  |cffffcc00/提示框 清除统计|r - 清除性能统计数据")
        print("  |cffffcc00/提示框 链接|r - 显示当前物品的完整链接格式（调试用）")
        print("  |cffffcc00/提示框 帮助|r - 显示此帮助信息")
        print(" ")
        print("|cff888888其他可用命令：/属性提示 /物品提示 /utt|r")
        print(" ")
        print("|cffff8000如果服务器响应慢，建议先执行:|r")
        print("  |cffffcc00/提示框 优化延迟|r")

    else
        print("|cffff0000[统一提示框]|r 未知命令: " .. msg)
        print("|cff888888输入 |cffffcc00/提示框 帮助|r |cff888888查看命令列表|r")
    end
end

-- ============================================================================
-- 插件加载完成
-- ============================================================================

-- 插件加载完成提示
print("|cff00ff00[统一提示框]|r v2.0 已加载 - 输入 |cffffcc00/提示框 帮助|r 查看命令")

