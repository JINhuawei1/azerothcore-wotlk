-- 深渊修仙 UI
-- 当前实现：专属装备、装备模板图鉴、剧情章节详情
---@diagnostic disable: undefined-global

local ADDON_PREFIX = "ABYSS_UI"

-- 图标按钮拖拽状态
local iconButtonDragging = false
local iconButtonMouseDownAt = 0

AbyssCultivationUIDB = AbyssCultivationUIDB or {}
AbyssCultivationUI = AbyssCultivationUI or {}
local App = AbyssCultivationUI
App.iconButton = nil
App.itemIconCache = App.itemIconCache or {}
App.pendingItemInfo = App.pendingItemInfo or {}
App.itemInfoRefreshQueued = false
App.itemPrefetchQueue = App.itemPrefetchQueue or {}
App.itemPrefetchQueued = App.itemPrefetchQueued or {}
App.itemPrefetchIndex = App.itemPrefetchIndex or 1

local ITEM_PREFETCH_BATCH = 12
local ITEM_PREFETCH_INTERVAL = 0.03
local ICON_TEX_COORD_MIN = 0.08
local ICON_TEX_COORD_MAX = 0.92

-- 状态数据
App.state = {
    playerLevel = 0,
    currentChapter = 0,
    currentChapterName = "",
    highestChapter = 0,
    currentThreshold = 0,
    storyState = 0,
    unlockedModeMask = 0,
    mainRelic = 0,
    subRelic1 = 0,
    subRelic2 = 0,
    subRelic3 = 0,
    subRelic4 = 0,
    subRelic5 = 0,
    phaseArtifact = 0,
    ultimateArtifact = 0,
    highestCorruptionTier = 0,
    cacheBossFailCount = 0,
    inRun = false,
    runChapterId = 0,
    runModeType = 0,
    runCorruptionTier = 0,
    runMapId = 0,
    anchorBossKillMask = 0,
    abyssBossSummoned = false,
    cacheBossSummoned = false,
    nextChapter = 0,
    nextChapterName = "",
}
App.relicList = {}  -- 有序列表 { { id, name, relicType, activeSlot, actId, desc }, ... }
App.relicMap = {}   -- { [itemId] = relicData }
App.equipmentList = {}
App.equipmentMap = {}
App.setBonusMap = {}  -- { [setKey] = { setId, setName, actId, sourceMode, twoPieceDesc, fourPieceDesc } }
App.chapters = {}
App.chapterMap = {}
App.selectedChapterId = 0
App.rewardPreview = {
    current = { chapterId = 0, chapterName = "", categories = {} },
    next = { chapterId = 0, chapterName = "", categories = {} },
    view = { chapterId = 0, chapterName = "", categories = {} },
}
App.activeTab = "equip"
App.pendingRelicActivation = nil
App.pendingRelicCollection = nil
App.equipmentFilterType = 0
App.equipmentFilterMode = 0
App.equipmentOwnedOnly = false
App.equipmentFilterSlot = 0
App.equipmentFilterChapter = 0
App.setOverviewFilterMode = 0
App.setOverviewCurrentActOnly = false
App.artifactFilterType = 0
App.artifactOwnedOnly = false
App.selectedRelicButtonKey = ""
App.selectedArtifactButtonKey = ""
App.chapterEnterMode = 1
App.chapterLastActionText = ""
App.modePrompt = {
    active = false,
    chapterId = 0,
    chapterName = "",
    modeMask = 0,
    corruptionTier = 0,
}

-- 主题色
local THEME = {
    bg          = { 0.05, 0.04, 0.08, 0.95 },
    panel       = { 0.08, 0.06, 0.12, 0.92 },
    border      = { 0.35, 0.18, 0.55, 0.95 },
    accent      = { 0.86, 0.66, 0.16, 1.0 },
    text        = { 0.94, 0.92, 0.86, 1.0 },
    muted       = { 0.60, 0.58, 0.52, 1.0 },
    equipped    = { 0.30, 0.80, 0.40, 1.0 },
    empty       = { 0.40, 0.40, 0.40, 1.0 },
    cardBg      = { 0.10, 0.08, 0.15, 0.95 },
    cardEquip   = { 0.12, 0.18, 0.12, 0.95 },
    currentCh   = { 0.86, 0.66, 0.16, 0.25 },
    relicType1  = { 0.60, 0.80, 1.00, 1.0 },  -- 章节遗物 蓝
    relicType2  = { 1.00, 0.60, 0.20, 1.0 },  -- 阶段神器 橙
    relicType3  = { 1.00, 0.40, 0.80, 1.0 },  -- 终极神器 粉
    equipType1  = { 0.62, 0.84, 1.00, 1.0 },  -- 套装底材 蓝
    equipType2  = { 1.00, 0.74, 0.24, 1.0 },  -- 传奇唯一 金
    cacheBoss   = { 0.88, 0.30, 0.30, 1.0 },  -- 秘藏首领 红
}

-- 槽位配置：服务端命令名, 显示名, 对应state字段
local SLOTS = {
    { cmd = "main",     label = "主",   stateKey = "mainRelic" },
    { cmd = "sub1",     label = "副1",  stateKey = "subRelic1" },
    { cmd = "sub2",     label = "副2",  stateKey = "subRelic2" },
    { cmd = "sub3",     label = "副3",  stateKey = "subRelic3" },
    { cmd = "sub4",     label = "副4",  stateKey = "subRelic4" },
    { cmd = "sub5",     label = "副5",  stateKey = "subRelic5" },
    { cmd = "phase",    label = "阶段", stateKey = "phaseArtifact" },
    { cmd = "ultimate", label = "终极", stateKey = "ultimateArtifact" },
}

-- 遗物类型可用的槽位索引
local SLOT_MAP = {
    [1] = { 1, 2, 3, 4, 5, 6 }, -- 章节遗物 → 主/副1~副5
    [2] = { 7 },                 -- 阶段神器 → 阶段
    [3] = { 8 },                 -- 终极神器 → 终极
}

-- 工具函数
local function ToNumber(v) return tonumber(v) or 0 end

local function GetRelicTypeName(t)
    local names = { [1] = "章节遗物", [2] = "阶段神器", [3] = "终极神器" }
    return names[ToNumber(t)] or "未知"
end

local function GetRelicTypeColor(t)
    local colors = { [1] = THEME.relicType1, [2] = THEME.relicType2, [3] = THEME.relicType3 }
    return colors[ToNumber(t)] or THEME.muted
end

local function GetSlotName(slot)
    local names = {
        [1] = "主遗物",
        [2] = "副遗物1",
        [3] = "副遗物2",
        [4] = "副遗物3",
        [5] = "副遗物4",
        [6] = "副遗物5",
        [7] = "阶段神器",
        [8] = "终极神器"
    }
    return names[ToNumber(slot)] or ""
end

local function GetChapterTypeName(t)
    return ToNumber(t) == 2 and "团队本" or "五人本"
end

local function HasChapterModeUnlocked(chapter, modeType)
    if not chapter then
        return false
    end
    local mask = bit and bit.lshift(1, ToNumber(modeType) - 1)
    if not mask or ToNumber(modeType) <= 0 then
        return false
    end
    if App.modePrompt and App.modePrompt.active and App.modePrompt.chapterId == chapter.id then
        if bit.band(App.modePrompt.modeMask or 0, mask) ~= 0 then
            return true
        end
    end
    return bit.band(chapter.unlockedModeMask or 0, mask) ~= 0
end

local function HasModeInMask(modeMask, modeType)
    local mask = bit and bit.lshift(1, ToNumber(modeType) - 1)
    if not mask or ToNumber(modeType) <= 0 then
        return false
    end
    return bit.band(ToNumber(modeMask), mask) ~= 0
end

local function Split(text, sep)
    local result = {}
    if not text or text == "" then return result end
    for part in string.gmatch(text, "([^" .. (sep or "|") .. "]+)") do
        table.insert(result, part)
    end
    return result
end

local function SendAddon(cmd)
    local name = UnitName("player")
    if not name or not cmd then return end
    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, cmd, "WHISPER", name)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, cmd, "WHISPER", name)
    end
end

local function StylePanel(frame, r, g, b, a)
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    frame:SetBackdropColor(r or THEME.panel[1], g or THEME.panel[2], b or THEME.panel[3], a or THEME.panel[4])
    frame:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])
end

local function EnableScrollMouseWheel(scrollFrame, step)
    scrollFrame.scrollStep = step or 36
    scrollFrame:EnableMouseWheel(true)
    scrollFrame:SetScript("OnMouseWheel", function(self, delta)
        local minScroll = 0
        local maxScroll = self:GetVerticalScrollRange() or 0
        if maxScroll <= 0 then
            return
        end

        local nextScroll = (self:GetVerticalScroll() or 0) - delta * (self.scrollStep or 36)
        if nextScroll < minScroll then
            nextScroll = minScroll
        elseif nextScroll > maxScroll then
            nextScroll = maxScroll
        end

        self:SetVerticalScroll(nextScroll)
    end)
end

local GetEquipmentTypeName
local GetEquipmentTypeColor
local GetEquipmentSourceModeName
local GetEquipmentSlotMaskName

local function HideTooltipExtra()
    if App.tooltipExtraFrame then
        App.tooltipExtraFrame:Hide()
    end
end

local function EnsureTooltipExtraFrame()
    if App.tooltipExtraFrame then
        return App.tooltipExtraFrame
    end

    local frame = CreateFrame("Frame", "AbyssCultivationUITooltipExtra", UIParent)
    frame:SetFrameStrata("TOOLTIP")
    frame:SetFrameLevel(GameTooltip:GetFrameLevel() + 8)
    StylePanel(frame, 0.04, 0.03, 0.08, 0.97)
    frame.lines = {}
    frame:Hide()

    if GameTooltip and not App.tooltipExtraHooked then
        GameTooltip:HookScript("OnHide", HideTooltipExtra)
        App.tooltipExtraHooked = true
    end

    App.tooltipExtraFrame = frame
    return frame
end

local function ShowTooltipExtra(lines)
    if not lines or #lines == 0 then
        HideTooltipExtra()
        return
    end

    local frame = EnsureTooltipExtraFrame()
    local width = math.max(GameTooltip:GetWidth() or 280, 320)
    local y = -10

    frame:SetWidth(width)

    for index, line in ipairs(lines) do
        local fs = frame.lines[index]
        if not fs then
            fs = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
            fs:SetJustifyH("LEFT")
            fs:SetJustifyV("TOP")
            frame.lines[index] = fs
        end

        fs:ClearAllPoints()
        fs:SetPoint("TOPLEFT", 12, y)
        fs:SetWidth(width - 24)
        fs:SetText(line.text or "")
        fs:SetTextColor(line.r or THEME.text[1], line.g or THEME.text[2], line.b or THEME.text[3])
        fs:Show()
        y = y - (fs:GetStringHeight() or 14) - (line.gap or 4)
    end

    for index = #lines + 1, #frame.lines do
        frame.lines[index]:Hide()
    end

    frame:SetHeight(math.max(-y + 8, 24))
    frame:ClearAllPoints()
    frame:SetPoint("TOPLEFT", GameTooltip, "BOTTOMLEFT", 0, -2)
    frame:SetPoint("TOPRIGHT", GameTooltip, "BOTTOMRIGHT", 0, -2)
    frame:Show()
end

local function SetRelicItemTooltip(self)
    local itemId = self and self.itemId
    if not itemId or itemId <= 0 then
        return
    end

    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. itemId)
    local relic = App.relicMap and App.relicMap[itemId]
    local extraLines = {}
    if relic then
        local function AddRelicBonusLine(label, bonusBase)
            local health = bonusBase
            local armor = bonusBase * 0.75
            local attack = bonusBase * 0.8
            local spell = bonusBase * 0.9
            table.insert(extraLines, {
                text = string.format("%s：全属性+%.1f%%  生命+%.1f%%  护甲+%.1f%%  攻击+%.1f%%  法术+%.1f%%",
                    label, bonusBase, health, armor, attack, spell),
                r = 0.62, g = 0.92, b = 0.72
            })
        end

        table.insert(extraLines, {
            text = string.format("%s加成", GetRelicTypeName(relic.relicType)),
            r = relic.relicType == 3 and 1.0 or (relic.relicType == 2 and 1.0 or 0.78),
            g = relic.relicType == 3 and 0.55 or (relic.relicType == 2 and 0.74 or 0.92),
            b = relic.relicType == 3 and 0.86 or (relic.relicType == 2 and 0.26 or 1.0),
            gap = 2
        })

        if relic.relicType == 1 then
            AddRelicBonusLine("主槽", 2.0)
            AddRelicBonusLine("副槽", 2.0 * math.max(relic.subSlotScale or 0.5, 0.1))
        elseif relic.relicType == 2 then
            AddRelicBonusLine("阶段神器槽", 5.0)
        elseif relic.relicType == 3 then
            AddRelicBonusLine("终极神器槽", 10.0)
        end

        if relic.activeSlot and relic.activeSlot > 0 then
            table.insert(extraLines, { text = string.format("当前激活位置：%s", GetSlotName(relic.activeSlot)), r = 0.40, g = 0.95, b = 0.45 })
        elseif relic.recommendedSlot and relic.recommendedSlot > 0 then
            table.insert(extraLines, { text = string.format("推荐激活位置：%s", GetSlotName(relic.recommendedSlot)), r = 0.92, g = 0.82, b = 0.40 })
        end

        if relic.exclusiveGroup and relic.exclusiveGroup > 0 then
            table.insert(extraLines, { text = string.format("互斥组：%d", relic.exclusiveGroup), r = 0.85, g = 0.60, b = 0.60 })
        end

        if relic.desc and relic.desc ~= "" then
            table.insert(extraLines, { text = "核心特效", r = 1.0, g = 0.82, b = 0.28, gap = 2 })
            table.insert(extraLines, { text = relic.desc, r = 0.82, g = 0.82, b = 0.78 })
        end
    end
    GameTooltip:Show()
    table.insert(extraLines, { text = "双击图标可自动激活到可用槽位", r = THEME.accent[1], g = THEME.accent[2], b = THEME.accent[3], gap = 2 })
    table.insert(extraLines, { text = "右键图标可选择穿戴或收藏专属", r = THEME.muted[1], g = THEME.muted[2], b = THEME.muted[3] })
    ShowTooltipExtra(extraLines)
end

local function SetEquipmentItemTooltip(self)
    local itemId = self and self.itemId
    if not itemId or itemId <= 0 then
        return
    end

    HideTooltipExtra()
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. itemId)
    GameTooltip:Show()
end

local function NotifyMessage(text)
    if DEFAULT_CHAT_FRAME and text and text ~= "" then
        DEFAULT_CHAT_FRAME:AddMessage("|cffDBA64A深渊修仙:|r " .. text)
    end
end

local function EnsureIconCacheStore()
    if not AbyssCultivationUIDB.iconCache then
        AbyssCultivationUIDB.iconCache = {}
    end
    return AbyssCultivationUIDB.iconCache
end

local function CacheItemIcon(itemId, iconPath)
    if not itemId or itemId <= 0 or not iconPath or iconPath == "" or iconPath == "Interface\\Icons\\INV_Misc_QuestionMark" then
        return
    end

    App.itemIconCache[itemId] = iconPath
    EnsureIconCacheStore()[itemId] = iconPath
end

local function GetCachedItemIcon(itemId)
    if not itemId or itemId <= 0 then
        return nil
    end

    local runtimeIcon = App.itemIconCache[itemId]
    if runtimeIcon and runtimeIcon ~= "" then
        return runtimeIcon
    end

    local savedIcon = EnsureIconCacheStore()[itemId]
    if savedIcon and savedIcon ~= "" then
        App.itemIconCache[itemId] = savedIcon
        return savedIcon
    end

    return nil
end

local function NormalizeIconPath(iconPath)
    if not iconPath or iconPath == "" then
        return nil
    end

    if string.find(iconPath, "\\") or string.find(iconPath, "/") then
        return iconPath
    end

    return "Interface\\Icons\\" .. iconPath
end

local function ApplyIconTexCoords(texture)
    if not texture then
        return
    end

    texture:SetTexCoord(
        ICON_TEX_COORD_MIN,
        ICON_TEX_COORD_MAX,
        ICON_TEX_COORD_MIN,
        ICON_TEX_COORD_MAX
    )
end

local function ResolveItemDisplay(itemId, fallbackName, preferredIcon)
    local directIcon = NormalizeIconPath(preferredIcon)
    if directIcon then
        CacheItemIcon(itemId, directIcon)
        return fallbackName, directIcon
    end

    local cachedIcon = NormalizeIconPath(GetCachedItemIcon(itemId))
    local itemName, _, _, _, _, _, _, _, _, itemIcon = GetItemInfo(itemId)
    local iconPath = NormalizeIconPath(itemIcon) or NormalizeIconPath(GetItemIcon(itemId)) or cachedIcon or "Interface\\Icons\\INV_Misc_QuestionMark"

    if itemId and itemId > 0 and iconPath == "Interface\\Icons\\INV_Misc_QuestionMark" then
        App.pendingItemInfo[itemId] = fallbackName or true
        GetItemInfo(itemId)
        GetItemIcon(itemId)
    end

    if iconPath and iconPath ~= "Interface\\Icons\\INV_Misc_QuestionMark" then
        CacheItemIcon(itemId, iconPath)
        App.pendingItemInfo[itemId] = nil
    end

    return itemName or fallbackName, iconPath
end

local function ResolveRelicItemDisplay(itemId, fallbackName, preferredIcon)
    return ResolveItemDisplay(itemId, fallbackName, preferredIcon)
end

local function EnsureItemPrefetchDriver()
    if App.itemPrefetchDriver then
        return App.itemPrefetchDriver
    end

    local frame = CreateFrame("Frame")
    frame.elapsed = 0
    frame:Hide()
    frame:SetScript("OnUpdate", function(self, elapsed)
        self.elapsed = self.elapsed + elapsed
        if self.elapsed < ITEM_PREFETCH_INTERVAL then
            return
        end
        self.elapsed = 0

        local processed = 0
        while processed < ITEM_PREFETCH_BATCH do
            local queueIndex = App.itemPrefetchIndex or 1
            local itemId = App.itemPrefetchQueue[queueIndex]
            if not itemId then
                wipe(App.itemPrefetchQueue)
                wipe(App.itemPrefetchQueued)
                App.itemPrefetchIndex = 1
                self:Hide()
                return
            end

            App.itemPrefetchQueue[queueIndex] = nil
            App.itemPrefetchIndex = queueIndex + 1
            App.itemPrefetchQueued[itemId] = nil
            processed = processed + 1

            if not GetCachedItemIcon(itemId) then
                local _, _, _, _, _, _, _, _, _, itemIcon = GetItemInfo(itemId)
                if itemIcon and itemIcon ~= "Interface\\Icons\\INV_Misc_QuestionMark" then
                    CacheItemIcon(itemId, itemIcon)
                    App.pendingItemInfo[itemId] = nil
                else
                    App.pendingItemInfo[itemId] = true
                    GetItemInfo(itemId)
                    GetItemIcon(itemId)
                end
            end
        end
    end)

    App.itemPrefetchDriver = frame
    return frame
end

local function QueueItemPrefetch(itemId)
    itemId = ToNumber(itemId)
    if itemId <= 0 then
        return
    end

    if GetCachedItemIcon(itemId) or App.pendingItemInfo[itemId] or App.itemPrefetchQueued[itemId] then
        return
    end

    table.insert(App.itemPrefetchQueue, itemId)
    App.itemPrefetchQueued[itemId] = true

    local driver = EnsureItemPrefetchDriver()
    if driver and not driver:IsShown() then
        driver.elapsed = 0
        driver:Show()
    end
end

local function QueueItemPrefetchList(items)
    if not items then
        return
    end

    for _, item in ipairs(items) do
        if type(item) == "table" then
            QueueItemPrefetch(item.id or item.itemId)
        else
            QueueItemPrefetch(item)
        end
    end
end

local function FormatResultMessage(action, success, message)
    if success then
        if action == "COLLECT_RELIC" then
            return "专属装备已收藏成功。"
        end
        if action == "SET_RELIC" then
            return "专属装备激活成功。"
        end
        if action == "CLEAR_RELIC" then
            return "专属装备卸下成功。"
        end
        if action == "ENTER" then
            return "已成功进入深渊流程。"
        end
        if action == "LEAVE" then
            return "已退出当前深渊流程。"
        end
        return nil
    end

    local reasonMap = {
        active_run_locked = "当前正在深渊局内，不能切换专属装备。",
        player_data_missing = "角色深渊数据缺失，请重新打开界面后再试。",
        invalid_slot = "目标槽位无效。",
        slot_empty = "该槽位当前没有已激活的专属装备。",
        relic_not_collected = "该专属装备尚未进入收藏，暂时不能激活。",
        relic_not_configured = "该专属装备未配置激活规则。",
        item_not_in_bag = "背包中没有该专属装备，无法收藏。",
        already_collected = "该专属装备已经收藏过了。",
        invalid_item = "专属装备物品无效。",
        collect_failed = "收藏失败。",
        slot_incompatible = "该专属装备与目标槽位类型不匹配。",
        exclusive_conflict = "存在互斥专属装备，请先卸下冲突装备。",
        invalid_args = "激活参数无效。",
        no_run = "当前不在深渊流程中。",
        failed = "操作失败。",
        mode_locked = "该模式尚未解锁，暂时无法进入。",
        missing_prerequisite = "前置章节尚未完成，暂时无法进入。",
        level_too_low = "角色等级或门槛不足，暂时无法进入。",
        chapter_not_found = "章节不存在或配置未加载。",
        no_player = "玩家对象无效。",
        abyss_not_ready = "当前尚未满足深渊模式切换条件。",
        abyss_already_summoned = "当前局内的深渊首领已经召出。",
        summon_failed = "深渊首领召唤失败，请稍后重试。",
        teleport_failed = "传送失败，已取消本次深渊进入。",
        teleport_unavailable = "当前章节缺少可用入口坐标，无法直接传送。",
        enter_rolled_back = "进入失败，已回滚本次深渊流程。",
        ready = "章节已满足进入条件。"
    }

    if action == "COLLECT_RELIC" then
        return reasonMap[message] or ("收藏失败: " .. (message or "unknown"))
    end
    if action == "SET_RELIC" then
        return reasonMap[message] or ("激活失败: " .. (message or "unknown"))
    end
    if action == "CLEAR_RELIC" then
        return reasonMap[message] or ("卸下失败: " .. (message or "unknown"))
    end
    if action == "ENTER" then
        return reasonMap[message] or ("进入失败: " .. (message or "unknown"))
    end
    if action == "LEAVE" then
        return reasonMap[message] or ("退出失败: " .. (message or "unknown"))
    end
    return nil
end

local function GetAutoRelicSlotIndex(relic)
    if not relic then
        return 0
    end

    if relic.activeSlot and relic.activeSlot > 0 then
        return relic.activeSlot
    end

    local validSlots = SLOT_MAP[relic.relicType] or {}
    for _, slotIdx in ipairs(validSlots) do
        local slotDef = SLOTS[slotIdx]
        if slotDef and App.state[slotDef.stateKey] == 0 then
            return slotIdx
        end
    end

    return 0
end

local function StartRelicActivation(itemId, slotIdx)
    local relic = App.relicMap[itemId]
    local slotDef = SLOTS[slotIdx]
    if not relic or not slotDef then
        return false
    end

    App.pendingRelicActivation = {
        itemId = itemId,
        slotIdx = slotIdx,
        requestedAt = GetTime and GetTime() or 0,
    }

    NotifyMessage(string.format("正在尝试将【%s】激活到%s...", relic.name or ("#" .. itemId), GetSlotName(slotIdx)))
    SendAddon(string.format("SET_RELIC:%s %d", slotDef.cmd, itemId))
    SendAddon("REQ_STATE")
    SendAddon("REQ_RELICS")
    return true
end

local function StartRelicCollection(itemId, slotIdx, autoActivate)
    local relic = App.relicMap[itemId]
    if not relic then
        return false
    end

    App.pendingRelicActivation = autoActivate and {
        itemId = itemId,
        slotIdx = slotIdx,
        requestedAt = GetTime and GetTime() or 0,
    } or nil

    App.pendingRelicCollection = {
        itemId = itemId,
        slotIdx = slotIdx,
        autoActivate = autoActivate and true or false,
        requestedAt = GetTime and GetTime() or 0,
    }

    if autoActivate and slotIdx and slotIdx > 0 then
        NotifyMessage(string.format("正在尝试收藏【%s】并激活到%s...", relic.name or ("#" .. itemId), GetSlotName(slotIdx)))
    else
        NotifyMessage(string.format("正在尝试收藏【%s】...", relic.name or ("#" .. itemId)))
    end

    SendAddon(string.format("COLLECT_RELIC:%d", itemId))
    return true
end

local function FindBagItemById(itemId)
    if not itemId or itemId <= 0 then
        return nil, nil
    end

    for bag = 0, (NUM_BAG_SLOTS or 4) do
        local slotCount = GetContainerNumSlots and GetContainerNumSlots(bag) or 0
        for slot = 1, slotCount do
            local itemLink = GetContainerItemLink and GetContainerItemLink(bag, slot)
            if itemLink then
                local bagItemId = tonumber(string.match(itemLink, "item:(%d+)"))
                if bagItemId == itemId then
                    return bag, slot
                end
            end
        end
    end

    return nil, nil
end

local function UseRelicBagItem(itemId)
    local bag, slot = FindBagItemById(itemId)
    if not bag or not slot then
        NotifyMessage("背包中没有该专属装备，无法穿戴。")
        return
    end

    if App.HideRelicContextMenu then
        App:HideRelicContextMenu()
    end
    UseContainerItem(bag, slot)
end

function App:HideRelicContextMenu()
    if self.relicContextMenu then
        self.relicContextMenu.contextItemId = nil
        self.relicContextMenu.contextAnchor = nil
        if self.relicContextMenu:IsShown() then
            self.relicContextMenu:Hide()
        end
    end
end

local function IsCursorOverFrame(frame)
    if not frame or not frame:IsShown() then
        return false
    end

    local left, right, top, bottom = frame:GetLeft(), frame:GetRight(), frame:GetTop(), frame:GetBottom()
    if not left or not right or not top or not bottom then
        return false
    end

    local scale = UIParent:GetEffectiveScale() or 1
    local cursorX, cursorY = GetCursorPosition()
    cursorX = cursorX / scale
    cursorY = cursorY / scale

    return cursorX >= left and cursorX <= right and cursorY >= bottom and cursorY <= top
end

function App:GetOrCreateRelicContextMenuButton(index)
    if not self.relicContextMenuButtons then
        self.relicContextMenuButtons = {}
    end

    if self.relicContextMenuButtons[index] then
        return self.relicContextMenuButtons[index]
    end

    local btn = CreateFrame("Button", nil, self.relicContextMenu)
    btn:SetSize(164, 28)
    StylePanel(btn, 0.07, 0.05, 0.11, 0.96)
    btn:RegisterForClicks("LeftButtonUp")

    btn.label = btn:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    btn.label:SetPoint("LEFT", 12, 0)
    btn.label:SetPoint("RIGHT", -12, 0)
    btn.label:SetJustifyH("LEFT")

    btn:SetScript("OnEnter", function(self)
        if self.disabled then
            return
        end
        self:SetBackdropBorderColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)
        self:SetBackdropColor(0.13, 0.09, 0.18, 0.98)
    end)
    btn:SetScript("OnLeave", function(self)
        if self.disabled then
            self:SetBackdropBorderColor(0.20, 0.20, 0.24, 0.70)
            self:SetBackdropColor(0.05, 0.05, 0.07, 0.88)
        else
            self:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], 0.95)
            self:SetBackdropColor(0.07, 0.05, 0.11, 0.96)
        end
    end)

    self.relicContextMenuButtons[index] = btn
    return btn
end

function App:EnsureRelicContextMenu()
    if self.relicContextMenu then
        return self.relicContextMenu
    end

    local menuParent = self.frame or UIParent
    local menu = CreateFrame("Frame", "AbyssCultivationUIRelicContextMenu", menuParent)
    menu:SetFrameStrata("TOOLTIP")
    menu:SetFrameLevel(60)
    menu:SetClampedToScreen(true)
    menu:EnableMouse(true)
    StylePanel(menu, 0.03, 0.02, 0.06, 0.97)

    menu.title = menu:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    menu.title:SetPoint("TOPLEFT", 14, -10)
    menu.title:SetPoint("TOPRIGHT", -14, -10)
    menu.title:SetJustifyH("LEFT")
    menu.title:SetTextColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)

    menu.separator = menu:CreateTexture(nil, "ARTWORK")
    menu.separator:SetPoint("TOPLEFT", 12, -34)
    menu.separator:SetPoint("TOPRIGHT", -12, -34)
    menu.separator:SetHeight(1)
    menu.separator:SetTexture("Interface\\BUTTONS\\WHITE8X8")
    menu.separator:SetVertexColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 0.45)

    menu:SetScript("OnShow", function(self)
        self.hoverGraceUntil = (GetTime and GetTime() or 0) + 0.10
    end)
    menu:SetScript("OnUpdate", function(self)
        local now = GetTime and GetTime() or 0
        if now < (self.hoverGraceUntil or 0) then
            return
        end

        if IsCursorOverFrame(self) then
            return
        end

        if self.contextAnchor and IsCursorOverFrame(self.contextAnchor) then
            return
        end

        App:HideRelicContextMenu()
    end)

    self.relicContextMenu = menu
    return menu
end

local function ShowRelicContextMenu(anchor, itemId)
    local relic = App.relicMap[itemId]
    if not relic or not anchor then
        return
    end

    local menu = App:EnsureRelicContextMenu()
    if menu:IsShown() and menu.contextItemId == itemId then
        App:HideRelicContextMenu()
        return
    end
    menu.contextItemId = itemId
    menu.contextAnchor = anchor

    local slotIdx = GetAutoRelicSlotIndex(relic)
    local hasBagItem = FindBagItemById(itemId) ~= nil
    local menuItems = {}

    table.insert(menuItems, {
        text = "穿戴",
        color = relic.owned and { 0.96, 0.96, 0.94, 1.0 } or { THEME.muted[1], THEME.muted[2], THEME.muted[3], 1.0 },
        disabled = not relic.owned,
        onClick = function()
            if slotIdx <= 0 then
                NotifyMessage("当前没有可用槽位，请先卸下同类遗物。")
                return
            end
            StartRelicActivation(itemId, slotIdx)
        end,
    })

    table.insert(menuItems, {
        text = "收藏",
        color = relic.owned and { THEME.muted[1], THEME.muted[2], THEME.muted[3], 1.0 } or { 0.58, 0.92, 0.62, 1.0 },
        disabled = relic.owned or not hasBagItem,
        onClick = function()
            StartRelicCollection(itemId, 0, false)
        end,
    })

    menu.title:SetText(relic.name or ("#" .. itemId))

    local topOffset = -44
    for index, item in ipairs(menuItems) do
        local btn = App:GetOrCreateRelicContextMenuButton(index)
        btn:ClearAllPoints()
        btn:SetPoint("TOPLEFT", 10, topOffset - (index - 1) * 32)
        btn:SetPoint("RIGHT", -10, 0)
        btn.label:SetText(item.text)
        btn.label:SetTextColor(unpack(item.color or { THEME.text[1], THEME.text[2], THEME.text[3], 1.0 }))
        btn.disabled = item.disabled and true or false
        if btn.disabled then
            btn:SetBackdropColor(0.05, 0.05, 0.07, 0.88)
            btn:SetBackdropBorderColor(0.20, 0.20, 0.24, 0.70)
        else
            btn:SetBackdropColor(0.07, 0.05, 0.11, 0.96)
            btn:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], 0.95)
        end
        btn:SetScript("OnClick", function(self)
            if self.disabled then
                return
            end
            App:HideRelicContextMenu()
            if item.onClick then
                item.onClick()
            end
        end)
        btn:Show()
    end

    if App.relicContextMenuButtons then
        for index = #menuItems + 1, #App.relicContextMenuButtons do
            App.relicContextMenuButtons[index]:Hide()
        end
    end

    menu:SetSize(184, 48 + #menuItems * 32)
    menu:ClearAllPoints()

    local scale = UIParent:GetEffectiveScale() or 1
    local cursorX, cursorY = GetCursorPosition()
    cursorX = cursorX / scale
    cursorY = cursorY / scale
    menu:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", cursorX - 24, cursorY - 8)
    menu:Show()
end

local function TryActivateRelicByItemId(itemId)
    if not itemId or itemId <= 0 then
        return
    end

    local relic = App.relicMap[itemId]
    if not relic then
        return
    end

    local slotIdx = GetAutoRelicSlotIndex(relic)
    if slotIdx <= 0 then
        NotifyMessage("当前没有可用槽位，请先卸下同类遗物。")
        return
    end

    if relic.owned then
        StartRelicActivation(itemId, slotIdx)
        return
    end

    StartRelicCollection(itemId, slotIdx, true)
end

-- (已移至文件前部 ToNumber 之后)

local function GetModeTypeName(t)
    local names = {
        [1] = "正传",
        [2] = "深渊",
        [3] = "腐化",
        [4] = "轮回",
    }
    return names[ToNumber(t)] or "未知"
end

local function GetRewardBossCategoryName(category)
    local names = {
        anchor = "锚点首领",
        final = "最终首领",
        abyss = "深渊首领",
        cache = "秘藏首领",
    }
    return names[category] or category
end

local function FormatBossDisplay(entry, name)
    entry = ToNumber(entry)
    if name and name ~= "" then
        if entry > 0 then
            return string.format("【%s】(%d)", name, entry)
        end
        return string.format("【%s】", name)
    end

    if entry > 0 then
        return string.format("entry:%d", entry)
    end

    return "未配置"
end

local function GetChapterBossCategoryTitle(chapter, category)
    local names = {
        anchor = { entryKey = "anchorBossEntry", nameKey = "anchorBossName" },
        final = { entryKey = "finalBossEntry", nameKey = "finalBossName" },
        abyss = { entryKey = "abyssBossEntry", nameKey = "abyssBossName" },
        cache = { entryKey = "cacheBossEntry", nameKey = "cacheBossName" },
    }

    local info = names[category]
    local label = GetRewardBossCategoryName(category)
    if not chapter or not info then
        return label
    end

    local bossName = chapter[info.nameKey] or ""
    local bossEntry = ToNumber(chapter[info.entryKey])
    if bossName ~= "" then
        return string.format("%s【%s】", label, bossName)
    end
    if bossEntry > 0 then
        return string.format("%s(entry:%d)", label, bossEntry)
    end

    return label
end

local function GetRewardDockingTypeName(dockingType)
    local names = {
        [1] = "章节遗物",
        [2] = "阶段神器",
        [3] = "终极神器",
        [4] = "材料",
        [5] = "传奇唯一",
    }
    return names[ToNumber(dockingType)] or ("类型#" .. ToNumber(dockingType))
end

local function GetChapterAccessSummary(chapter)
    if not chapter then
        return "未选择章节", false
    end

    local prerequisiteMet = HasChapterModeUnlocked(chapter, 1)
    local levelMet = (App.state.playerLevel or 0) >= (chapter.threshold or 0)
    local ready = prerequisiteMet and levelMet

    if ready then
        return "满足进入条件", true
    end

    if not prerequisiteMet and not levelMet then
        return "缺少前置章节且等级不足", false
    end
    if not prerequisiteMet then
        return "缺少前置章节", false
    end
    return "等级不足", false
end

local function HasModeUnlocked(modeType)
    local mask = bit and bit.lshift(1, ToNumber(modeType) - 1)
    if not mask or ToNumber(modeType) <= 0 then
        return false
    end
    return bit.band(App.state.unlockedModeMask or 0, mask) ~= 0
end

-- (HasChapterModeUnlocked / HasModeInMask 已移至文件前部)

local function GetEffectQualityColor(quality)
    if ToNumber(quality) >= 4 then
        return "|cffffa54a"
    end
    if ToNumber(quality) == 3 then
        return "|cffc97cff"
    end
    if ToNumber(quality) == 2 then
        return "|cff4dd0ff"
    end
    return "|cff9f9f9f"
end

local function ParseEffectList(payload)
    local effects = {}
    if not payload or payload == "" then
        return effects
    end

    for _, raw in ipairs(Split(payload, "~")) do
        local f = Split(raw, "%^")
        if f[1] and f[1] ~= "" then
            table.insert(effects, {
                id = ToNumber(f[1]),
                name = f[2] or "",
                quality = ToNumber(f[3]),
                family = f[4] or "",
                desc = f[5] or "",
            })
        end
    end

    return effects
end

local function ParseEventData(payload)
    if not payload or payload == "" then
        return nil
    end

    local f = Split(payload, "%^")
    if not f[1] or f[1] == "" then
        return nil
    end

    return {
        id = ToNumber(f[1]),
        name = f[2] or "",
        eventType = ToNumber(f[3]),
        rewardText = f[4] or "",
        desc = f[5] or "",
    }
end

GetEquipmentTypeName = function(t)
    return ToNumber(t) == 2 and "传奇唯一" or "套装底材"
end

GetEquipmentTypeColor = function(t)
    return ToNumber(t) == 2 and THEME.equipType2 or THEME.equipType1
end

GetEquipmentSourceModeName = function(mode)
    local names = {
        [1] = "通用",
        [2] = "深渊",
        [3] = "腐化",
        [4] = "轮回",
    }
    return names[ToNumber(mode)] or "未知"
end

GetEquipmentSlotMaskName = function(mask)
    local names = {
        [1] = "武器",
        [2] = "头部",
        [4] = "胸甲",
        [16] = "腰带",
        [32] = "靴子",
        [64] = "戒指",
        [128] = "饰品",
        [256] = "披风",
        [512] = "法器",
    }

    return names[ToNumber(mask)] or ("部位#" .. ToNumber(mask))
end

local function GetChapterNameById(chapterId)
    local chapter = App.chapterMap and App.chapterMap[chapterId]
    if chapter then
        return chapter.name or ("章节#" .. ToNumber(chapterId))
    end
    return "章节#" .. ToNumber(chapterId)
end

local function ResetEquipmentFilters()
    App.equipmentFilterType = 0
    App.equipmentFilterMode = 0
    App.equipmentOwnedOnly = false
    App.equipmentFilterSlot = 0
    App.equipmentFilterChapter = 0
end

local function ApplyEquipmentChapterFilter(chapterId)
    ResetEquipmentFilters()
    App.equipmentFilterChapter = ToNumber(chapterId)
end

local function GetEquipmentFilterSummary()
    local parts = {}

    if App.equipmentFilterType ~= 0 then
        table.insert(parts, "类型:" .. GetEquipmentTypeName(App.equipmentFilterType))
    end

    if App.equipmentFilterMode ~= 0 then
        table.insert(parts, "模式:" .. GetEquipmentSourceModeName(App.equipmentFilterMode))
    end

    if App.equipmentFilterChapter ~= 0 then
        table.insert(parts, "章节:" .. GetChapterNameById(App.equipmentFilterChapter))
    end

    if App.equipmentFilterSlot ~= 0 then
        table.insert(parts, "部位:" .. GetEquipmentSlotMaskName(App.equipmentFilterSlot))
    end

    if App.equipmentOwnedOnly then
        table.insert(parts, "仅已持有")
    end

    if #parts == 0 then
        return "无"
    end

    return table.concat(parts, " / ")
end

local function GetCurrentActId()
    local currentChapter = App.chapterMap and App.chapterMap[App.state.currentChapter or 0]
    return currentChapter and currentChapter.actId or 0
end

local function ExtractEquipmentSetTheme(name)
    local theme = name or ""
    local suffixes = {
        "断刃胚", "法轮胚", "头冠胚", "胸铠胚", "战带胚",
        "战靴胚", "指环胚", "魂坠胚", "披影胚",
    }

    for _, suffix in ipairs(suffixes) do
        theme = string.gsub(theme, suffix .. "$", "")
    end

    return theme ~= "" and theme or "无名"
end

local function BuildEquipmentSetGroups()
    local groupsByKey = {}

    for _, equipment in ipairs(App.equipmentList) do
        if equipment.equipmentType == 1 then
            local key = string.format("%d:%d:%d", equipment.sourceMode, equipment.actId, equipment.sourceChapter)
            local group = groupsByKey[key]
            if not group then
                group = {
                    key = key,
                    sourceMode = equipment.sourceMode,
                    actId = equipment.actId,
                    sourceChapter = equipment.sourceChapter,
                    sourceChapterName = equipment.sourceChapterName,
                    theme = ExtractEquipmentSetTheme(equipment.name),
                    items = {},
                }
                groupsByKey[key] = group
            end

            table.insert(group.items, equipment)
        end
    end

    local groups = {}
    for _, group in pairs(groupsByKey) do
        table.sort(group.items, function(a, b)
            if a.slotMask ~= b.slotMask then
                return a.slotMask < b.slotMask
            end
            return a.id < b.id
        end)

        group.slotSummary = ""
        local slotNames = {}
        for _, item in ipairs(group.items) do
            table.insert(slotNames, GetEquipmentSlotMaskName(item.slotMask))
        end

        group.slotSummary = table.concat(slotNames, " / ")
        group.pieceCount = #group.items
        group.representative = group.items[1]
        group.name = string.format("%s·第%d幕·%s套装", GetEquipmentSourceModeName(group.sourceMode), group.actId, group.theme)
        table.insert(groups, group)
    end

    table.sort(groups, function(a, b)
        if a.actId ~= b.actId then
            return a.actId < b.actId
        end
        if a.sourceMode ~= b.sourceMode then
            return a.sourceMode < b.sourceMode
        end
        return a.sourceChapter < b.sourceChapter
    end)

    return groups
end

local function SendEnterChapter(chapterId, modeType, corruptionTier, skipUnlockCheck)
    if chapterId <= 0 then
        NotifyMessage("当前没有选中章节。")
        return
    end

    local chapter = App.chapterMap and App.chapterMap[chapterId] or nil
    if not skipUnlockCheck and not HasChapterModeUnlocked(chapter, modeType) then
        NotifyMessage("该章节的此模式尚未解锁。")
        return
    end

    App.chapterLastActionText = string.format(
        "最近请求：进入 [%s]  模式[%s]  腐化层[%d]  状态[请求中]",
        GetChapterNameById(chapterId),
        GetModeTypeName(modeType),
        ToNumber(corruptionTier)
    )
    SendAddon(string.format("ENTER:%d %d %d", chapterId, ToNumber(modeType), ToNumber(corruptionTier)))
end

local function SendEnterSelectedChapter(modeType, corruptionTier)
    local chapterId = App.selectedChapterId or 0
    SendEnterChapter(chapterId, modeType, corruptionTier, false)
end

-------------------------------------------------------
-- 数据解析
-------------------------------------------------------
local function ParseState(payload)
    local f = Split(payload, "|")
    App.state.playerLevel        = ToNumber(f[2])
    App.state.currentChapter     = ToNumber(f[3])
    App.state.currentChapterName = f[4] or ""
    App.state.highestChapter     = ToNumber(f[5])
    App.state.currentThreshold   = ToNumber(f[6])
    App.state.storyState         = ToNumber(f[7])
    App.state.unlockedModeMask   = ToNumber(f[8])
    App.state.mainRelic          = ToNumber(f[9])
    App.state.subRelic1          = ToNumber(f[10])
    App.state.subRelic2          = ToNumber(f[11])
    App.state.subRelic3          = ToNumber(f[12])
    App.state.subRelic4          = ToNumber(f[13])
    App.state.subRelic5          = ToNumber(f[14])
    App.state.phaseArtifact      = ToNumber(f[15])
    App.state.ultimateArtifact   = ToNumber(f[16])
    App.state.highestCorruptionTier = ToNumber(f[17])
    App.state.cacheBossFailCount = ToNumber(f[18])
    App.state.inRun              = ToNumber(f[19]) == 1
    App.state.runChapterId       = ToNumber(f[20])
    App.state.runModeType        = ToNumber(f[21])
    App.state.runCorruptionTier  = ToNumber(f[22])
    App.state.runMapId           = ToNumber(f[23])
    App.state.anchorBossKillMask = ToNumber(f[24])
    App.state.abyssBossSummoned  = ToNumber(f[25]) == 1
    App.state.cacheBossSummoned  = ToNumber(f[26]) == 1
    App.state.nextChapter        = ToNumber(f[27])
    App.state.nextChapterName    = f[28] or ""

    if App.HideModePrompt and (not App.state.inRun or App.state.runModeType >= 2) then
        App:HideModePrompt()
    end
end

local function ParseModePrompt(payload)
    local f = Split(payload, "|")
    local chapterId = ToNumber(f[1])
    local chapterName = f[2] or ""
    local modeMask = ToNumber(f[3])
    local corruptionTier = ToNumber(f[4])

    if App.chapterMap and App.chapterMap[chapterId] then
        local chapter = App.chapterMap[chapterId]
        chapter.unlockedModeMask = bit and bit.bor(chapter.unlockedModeMask or 0, modeMask) or modeMask
    end

    NotifyMessage(string.format(
        "调试: 收到模式选择请求 章节[%s] mask[%d] 腐化层[%d]",
        chapterName ~= "" and chapterName or ("#" .. chapterId),
        modeMask,
        corruptionTier
    ))

    if App.ShowModePrompt then
        App:ShowModePrompt(chapterId, chapterName, modeMask, corruptionTier)
    end
end

local function ParseRelics(payload)
    App.relicList = {}
    App.relicMap = {}
    if not payload or payload == "" then return end
    for _, raw in ipairs(Split(payload, "~")) do
        local f = Split(raw, "%^")
        if f[1] and f[1] ~= "" then
            local relic = {
                id = ToNumber(f[1]),
                name = f[2] or "",
                relicType = ToNumber(f[3]),
                chapterId = ToNumber(f[4]),
                actId = ToNumber(f[5]),
                owned = ToNumber(f[6]) == 1,  -- 是否已收藏
                activeRule = ToNumber(f[7]),
                exclusiveGroup = ToNumber(f[8]),
                recommendedSlot = ToNumber(f[9]),
                subSlotScale = tonumber(f[10]) or 0,
                activeSlot = ToNumber(f[11]),
                desc = f[12] or "",
                icon = f[13] or "",
            }
            if relic.icon and relic.icon ~= "" then
                CacheItemIcon(relic.id, NormalizeIconPath(relic.icon))
            end
            table.insert(App.relicList, relic)
            App.relicMap[relic.id] = relic
        end
    end
    -- 排序：已装配 > 已收藏 > 未收藏，再按类型、幕、ID
    table.sort(App.relicList, function(a, b)
        if (a.activeSlot > 0) ~= (b.activeSlot > 0) then
            return a.activeSlot > 0
        end
        if a.owned ~= b.owned then return a.owned end
        if a.relicType ~= b.relicType then return a.relicType < b.relicType end
        if a.actId ~= b.actId then return a.actId < b.actId end
        return a.id < b.id
    end)

    QueueItemPrefetchList(App.relicList)
end

local function ParseEquipments(payload)
    App.equipmentList = {}
    App.equipmentMap = {}
    if not payload or payload == "" then return end
    for _, raw in ipairs(Split(payload, "~")) do
        local f = Split(raw, "%^")
        if f[1] and f[1] ~= "" then
            local equipment = {
                id = ToNumber(f[1]),
                name = f[2] or "",
                equipmentType = ToNumber(f[3]),
                sourceChapter = ToNumber(f[4]),
                sourceChapterName = f[5] or "",
                sourceMode = ToNumber(f[6]),
                slotMask = ToNumber(f[7]),
                actId = ToNumber(f[8]),
                baseItemLevel = ToNumber(f[9]),
                fromCacheBoss = ToNumber(f[10]) == 1,
                requiresFragments = ToNumber(f[11]) == 1,
                desc = f[12] or "",
                icon = f[13] or "",
            }
            if equipment.icon and equipment.icon ~= "" then
                CacheItemIcon(equipment.id, NormalizeIconPath(equipment.icon))
            end
            table.insert(App.equipmentList, equipment)
            App.equipmentMap[equipment.id] = equipment
        end
    end

    table.sort(App.equipmentList, function(a, b)
        if a.equipmentType ~= b.equipmentType then
            return a.equipmentType > b.equipmentType
        end
        if a.actId ~= b.actId then return a.actId < b.actId end
        if a.sourceChapter ~= b.sourceChapter then return a.sourceChapter < b.sourceChapter end
        if a.sourceMode ~= b.sourceMode then return a.sourceMode < b.sourceMode end
        if a.slotMask ~= b.slotMask then return a.slotMask < b.slotMask end
        return a.id < b.id
    end)

    QueueItemPrefetchList(App.equipmentList)
end

local function ParseSetBonuses(payload)
    App.setBonusMap = {}
    if not payload or payload == "" then return end
    for _, raw in ipairs(Split(payload, "~")) do
        local f = Split(raw, "%^")
        if f[1] and f[1] ~= "" then
            local bonus = {
                setId = ToNumber(f[1]),
                setName = f[2] or "",
                actId = ToNumber(f[3]),
                sourceMode = ToNumber(f[4]),
                twoPieceDesc = f[5] or "",
                fourPieceDesc = f[6] or "",
            }
            -- 用 mode:actId 作为key匹配套装分组
            local key = string.format("%d:%d", bonus.sourceMode, bonus.actId)
            App.setBonusMap[key] = bonus
        end
    end
end

local function ParseChapters(payload)
    App.chapters = {}
    App.chapterMap = {}
    if not payload or payload == "" then return end
    for _, raw in ipairs(Split(payload, "~")) do
        local f = Split(raw, "%^")
        if f[1] and f[1] ~= "" then
            local chapter = {
                id = ToNumber(f[1]),
                name = f[2] or "",
                actId = ToNumber(f[3]),
                mapId = ToNumber(f[4]),
                chapterType = ToNumber(f[5]),
                threshold = ToNumber(f[6]),
                startQuestId = ToNumber(f[7]),
                completeQuestId = ToNumber(f[8]),
                unlockedModeMask = ToNumber(f[9]),
                anchorBossEntry = ToNumber(f[10]),
                anchorBossName = f[11] or "",
                finalBossEntry = ToNumber(f[12]),
                finalBossName = f[13] or "",
                abyssBossEntry = ToNumber(f[14]),
                abyssBossName = f[15] or "",
                cacheBossEntry = ToNumber(f[16]),
                cacheBossName = f[17] or "",
            }
            table.insert(App.chapters, chapter)
            App.chapterMap[chapter.id] = chapter
        end
    end

    if App.selectedChapterId == 0 or not App.chapterMap[App.selectedChapterId] then
        if App.state.currentChapter > 0 and App.chapterMap[App.state.currentChapter] then
            App.selectedChapterId = App.state.currentChapter
        elseif App.chapters[1] then
            App.selectedChapterId = App.chapters[1].id
        end
    end
end

local function ParseRewardPreview(scope, category, payload)
    if not scope or not category then
        return
    end

    if not App.rewardPreview[scope] then
        App.rewardPreview[scope] = { chapterId = 0, chapterName = "", categories = {} }
    end

    local preview = App.rewardPreview[scope]
    preview.categories = preview.categories or {}

    if not payload or payload == "" then
        preview.categories[category] = {}
        return
    end

    local rows = Split(payload, "~")
    local header = Split(rows[1] or "", "%^")
    preview.chapterId = ToNumber(header[1])
    preview.chapterName = header[2] or ""

    local rewards = {}
    for index = 2, #rows do
        local fields = Split(rows[index], "%^")
        if fields[1] and fields[1] ~= "" then
            table.insert(rewards, {
                itemId = ToNumber(fields[1]),
                itemName = fields[2] or "",
                dockingType = ToNumber(fields[3]),
                quality = ToNumber(fields[4]),
                inventoryType = ToNumber(fields[5]),
                sourceMode = ToNumber(fields[6]),
                baseItemLevel = ToNumber(fields[7]),
            })
        end
    end

    preview.categories[category] = rewards
end

-------------------------------------------------------
-- UI 辅助：小按钮
-------------------------------------------------------
local function CreateSmallButton(parent, text, width, onClick)
    local btn = CreateFrame("Button", nil, parent)
    btn:SetSize(width, 20)
    StylePanel(btn)
    btn:EnableMouse(true)
    btn:RegisterForClicks("LeftButtonUp")

    btn.label = btn:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    btn.label:SetPoint("CENTER")
    btn.label:SetText(text)
    btn.isSelected = false
    btn.isEquippedChoice = false
    btn.isHovered = false

    btn.ApplySelectedState = function(self, selected, equippedChoice)
        self.isSelected = selected and true or false
        self.isEquippedChoice = equippedChoice and true or false

        if self.isEquippedChoice and self.isHovered then
            self:SetBackdropColor(0.08, 0.22, 0.10, 1)
            self:SetBackdropBorderColor(0.42, 1.00, 0.52, 1)
            self.label:SetTextColor(0.82, 1.00, 0.84, 1)
        elseif self.isEquippedChoice then
            self:SetBackdropColor(0.05, 0.17, 0.08, 0.98)
            self:SetBackdropBorderColor(0.30, 0.85, 0.40, 1)
            self.label:SetTextColor(0.72, 0.98, 0.76, 1)
        elseif self.isSelected and self.isHovered then
            self:SetBackdropColor(0.30, 0.20, 0.05, 1)
            self:SetBackdropBorderColor(1.0, 0.86, 0.30, 1)
            self.label:SetTextColor(1.0, 0.96, 0.72, 1)
        elseif self.isSelected then
            self:SetBackdropColor(0.22, 0.15, 0.04, 0.98)
            self:SetBackdropBorderColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)
            self.label:SetTextColor(1.0, 0.92, 0.62, 1)
        elseif self.isHovered then
            self:SetBackdropColor(0.12, 0.09, 0.18, 0.98)
            self:SetBackdropBorderColor(0.78, 0.60, 0.18, 0.95)
            self.label:SetTextColor(0.98, 0.90, 0.76, 1)
        else
            self:SetBackdropColor(THEME.panel[1], THEME.panel[2], THEME.panel[3], 0.92)
            self:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])
            self.label:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)
        end
    end

    btn:SetScript("OnClick", onClick)
    btn:SetScript("OnEnter", function(self)
        self.isHovered = true
        self:ApplySelectedState(self.isSelected, self.isEquippedChoice)
    end)
    btn:SetScript("OnLeave", function(self)
        self.isHovered = false
        self:ApplySelectedState(self.isSelected, self.isEquippedChoice)
    end)
    btn:ApplySelectedState(false, false)

    return btn
end

function App:HideModePrompt()
    self.modePrompt.active = false
    if self.modePromptFrame then
        self.modePromptFrame:Hide()
    end
end

function App:CreateModePromptFrame()
    if self.modePromptFrame then
        return
    end

    local frame = CreateFrame("Frame", nil, UIParent)
    frame:SetSize(420, 168)
    frame:SetPoint("CENTER", UIParent, "CENTER", 0, 120)
    frame:SetFrameStrata("DIALOG")
    frame:EnableMouse(true)
    StylePanel(frame, 0.05, 0.04, 0.08, 0.98)
    frame:Hide()

    frame.title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    frame.title:SetPoint("TOP", 0, -14)
    frame.title:SetTextColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)
    frame.title:SetText("深渊模式选择")

    frame.desc = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    frame.desc:SetPoint("TOPLEFT", 20, -42)
    frame.desc:SetPoint("TOPRIGHT", -20, -42)
    frame.desc:SetJustifyH("LEFT")
    frame.desc:SetJustifyV("TOP")

    frame.buttons = {}
    for i = 1, 4 do
        local btn = CreateSmallButton(frame, "", 90, function(self)
            if self.isCancel then
                App:HideModePrompt()
                return
            end

            local modeType = ToNumber(self.modeType)
            if modeType <= 0 then
                return
            end

            local chapterId = ToNumber(App.modePrompt.chapterId)
            App.chapterEnterMode = modeType
            App.selectedChapterId = chapterId

            local corruptionTier = modeType == 3 and ToNumber(App.modePrompt.corruptionTier) or 0
            App:HideModePrompt()
            SendEnterChapter(chapterId, modeType, corruptionTier, true)
        end)
        btn:SetPoint("BOTTOMLEFT", 22 + (i - 1) * 96, 16)
        frame.buttons[i] = btn
    end

    self.modePromptFrame = frame
end

function App:ShowModePrompt(chapterId, chapterName, modeMask, corruptionTier)
    self:CreateModePromptFrame()

    local availableModes = {}
    for _, modeType in ipairs({ 1, 2, 3, 4 }) do
        if HasModeInMask(modeMask, modeType) then
            table.insert(availableModes, modeType)
        end
    end

    if #availableModes == 0 then
        NotifyMessage("当前没有可进入的已解锁深渊模式。")
        self:HideModePrompt()
        return
    end

    NotifyMessage(string.format(
        "调试: 准备显示模式弹窗 章节[%s] 可选数量[%d]",
        chapterName ~= "" and chapterName or ("#" .. chapterId),
        #availableModes
    ))

    self.modePrompt.active = true
    self.modePrompt.chapterId = chapterId
    self.modePrompt.chapterName = chapterName
    self.modePrompt.modeMask = modeMask
    self.modePrompt.corruptionTier = corruptionTier

    if self.chapterMap and self.chapterMap[chapterId] then
        local chapter = self.chapterMap[chapterId]
        chapter.unlockedModeMask = bit and bit.bor(chapter.unlockedModeMask or 0, modeMask) or modeMask
    end

    local displayName = chapterName ~= "" and chapterName or ("章节#" .. ToNumber(chapterId))
    local detailText = string.format("已击破【%s】的锚点首领，是否立即进入更高模式？", displayName)
    if HasModeInMask(modeMask, 3) then
        detailText = detailText .. string.format("\n当前腐化层沿用：%d", ToNumber(corruptionTier))
    end
    self.modePromptFrame.desc:SetText(detailText)

    local buttonIndex = 1
    for _, modeType in ipairs(availableModes) do
        local btn = self.modePromptFrame.buttons[buttonIndex]
        btn.modeType = modeType
        btn.isCancel = false
        btn.label:SetText("进入" .. GetModeTypeName(modeType))
        btn:Show()
        buttonIndex = buttonIndex + 1
    end

    local cancelBtn = self.modePromptFrame.buttons[buttonIndex]
    if cancelBtn then
        cancelBtn.modeType = 0
        cancelBtn.isCancel = true
        cancelBtn.label:SetText("稍后再说")
        cancelBtn:Show()
        buttonIndex = buttonIndex + 1
    end

    for i = buttonIndex, #self.modePromptFrame.buttons do
        self.modePromptFrame.buttons[i]:Hide()
    end

    self.modePromptFrame:Show()
end

-------------------------------------------------------
-- 装备页：槽位概览 + 遗物卡片网格
-------------------------------------------------------
-- 卡片尺寸和布局
local CARD_W = 274
local CARD_H = 72
local CARD_GAP = 8
local CARDS_PER_ROW = 4
local CARD_ICON_SIZE = 36
local CARD_TEXT_LEFT = 18 + CARD_ICON_SIZE
local EQUIP_CONTENT_W = CARD_W * CARDS_PER_ROW + CARD_GAP * (CARDS_PER_ROW - 1)
local CHAPTER_CONTENT_W = 1120
local CHAPTER_LIST_W = 392
local SLOT_BAR_W = 1140
local SLOT_INDICATOR_W = 260
local SLOT_INDICATOR_STEP = 274
local SLOT_INDICATORS_PER_ROW = 4

function App:BuildEquipPage(parent)
    -- 顶部：8个槽位概览
    self.slotBar = CreateFrame("Frame", nil, parent)
    self.slotBar:SetSize(SLOT_BAR_W, 82)
    self.slotBar:SetPoint("TOPLEFT", 6, -4)
    StylePanel(self.slotBar)

    self.slotIndicators = {}
    for i, slot in ipairs(SLOTS) do
        local ind = CreateFrame("Frame", nil, self.slotBar)
        ind:SetSize(SLOT_INDICATOR_W, 34)
        local col = (i - 1) % SLOT_INDICATORS_PER_ROW
        local row = math.floor((i - 1) / SLOT_INDICATORS_PER_ROW)
        ind:SetPoint("TOPLEFT", 8 + col * SLOT_INDICATOR_STEP, -6 - row * 34)

        ind.dot = ind:CreateTexture(nil, "OVERLAY")
        ind.dot:SetSize(8, 8)
        ind.dot:SetPoint("LEFT", 4, 0)
        ind.dot:SetTexture("Interface\\BUTTONS\\WHITE8X8")

        ind.slotName = ind:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
        ind.slotName:SetPoint("LEFT", ind.dot, "RIGHT", 4, 0)
        ind.slotName:SetText(GetSlotName(i))

        ind.relicName = ind:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        ind.relicName:SetPoint("LEFT", ind.slotName, "RIGHT", 4, 0)
        ind.relicName:SetPoint("RIGHT", -4, 0)
        ind.relicName:SetJustifyH("LEFT")

        self.slotIndicators[i] = ind
    end

    -- 状态栏
    self.equipStatus = parent:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.equipStatus:SetPoint("TOPLEFT", 10, -92)

    -- 滚动区域
    self.equipScroll = CreateFrame("ScrollFrame", nil, parent, "UIPanelScrollFrameTemplate")
    self.equipScroll:SetPoint("TOPLEFT", 6, -110)
    self.equipScroll:SetPoint("BOTTOMRIGHT", -28, 6)

    self.equipContent = CreateFrame("Frame", nil, self.equipScroll)
    self.equipContent:SetWidth(EQUIP_CONTENT_W)
    self.equipContent:SetHeight(100)
    self.equipScroll:SetScrollChild(self.equipContent)
    EnableScrollMouseWheel(self.equipScroll, CARD_H)

    self.relicCards = {}
end

function App:GetOrCreateRelicCard(index)
    if self.relicCards[index] then
        return self.relicCards[index]
    end

    local card = CreateFrame("Frame", nil, self.equipContent)
    card:SetSize(CARD_W, CARD_H)

    -- 背景
    card.bg = card:CreateTexture(nil, "BACKGROUND")
    card.bg:SetAllPoints()
    card.bg:SetTexture("Interface\\BUTTONS\\WHITE8X8")
    card.bg:SetVertexColor(THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])

    StylePanel(card, THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])

    -- 状态指示条（左侧竖条）
    card.statusBar = card:CreateTexture(nil, "ARTWORK")
    card.statusBar:SetSize(3, CARD_H - 8)
    card.statusBar:SetPoint("LEFT", 4, 0)
    card.statusBar:SetTexture("Interface\\BUTTONS\\WHITE8X8")

    card.iconButton = CreateFrame("Button", nil, card)
    card.iconButton:SetSize(CARD_ICON_SIZE, CARD_ICON_SIZE)
    card.iconButton:SetPoint("LEFT", 12, 0)
    card.iconButton:EnableMouse(true)
    card.iconButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    card.iconButton:SetScript("OnEnter", SetRelicItemTooltip)
    card.iconButton:SetScript("OnLeave", function()
        GameTooltip:Hide()
        HideTooltipExtra()
    end)
    card.iconButton:SetScript("OnClick", function(self, button)
        if button == "RightButton" then
            ShowRelicContextMenu(self, self.itemId)
            return
        end

        if button ~= "LeftButton" then
            return
        end

        local now = GetTime and GetTime() or 0
        local itemId = self.itemId or 0
        if itemId > 0 and self.lastClickItemId == itemId and (now - (self.lastClickTime or 0)) <= 0.50 then
            self.lastClickTime = 0
            self.lastClickItemId = 0
            TryActivateRelicByItemId(itemId)
            return
        end

        self.lastClickTime = now
        self.lastClickItemId = itemId
    end)

    card.icon = card.iconButton:CreateTexture(nil, "ARTWORK")
    card.icon:SetAllPoints()
    card.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
    ApplyIconTexCoords(card.icon)

    card.iconBorder = CreateFrame("Frame", nil, card)
    card.iconBorder:SetPoint("TOPLEFT", card.iconButton, "TOPLEFT", -3, 3)
    card.iconBorder:SetPoint("BOTTOMRIGHT", card.iconButton, "BOTTOMRIGHT", 3, -3)
    card.iconBorder:SetFrameLevel(math.max(card:GetFrameLevel(), card.iconButton:GetFrameLevel() - 1))
    StylePanel(card.iconBorder, 0.02, 0.02, 0.03, 0.35)

    -- 遗物名称
    card.nameLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    card.nameLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -8)
    card.nameLabel:SetPoint("RIGHT", -8, 0)
    card.nameLabel:SetJustifyH("LEFT")

    -- 类型标签
    card.typeLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.typeLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -26)

    -- 槽位状态
    card.slotLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    card.slotLabel:SetPoint("TOPRIGHT", -8, -8)

    -- 描述（单行）
    card.descLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.descLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -40)
    card.descLabel:SetPoint("RIGHT", -80, 0)
    card.descLabel:SetJustifyH("LEFT")

    -- 操作按钮区（右下角）
    card.actionButtons = {}

    self.relicCards[index] = card
    return card
end

function App:RefreshEquipPage()
    QueueItemPrefetchList(self.relicList)

    local equippedRelicsBySlot = {}
    for _, relic in ipairs(self.relicList) do
        if relic.activeSlot and relic.activeSlot > 0 then
            equippedRelicsBySlot[relic.activeSlot] = relic
        end
    end

    -- 刷新槽位概览
    for i, slot in ipairs(SLOTS) do
        local ind = self.slotIndicators[i]
        local relic = equippedRelicsBySlot[i]
        local itemId = relic and relic.id or self.state[slot.stateKey]
        if relic then
            ind.dot:SetVertexColor(THEME.equipped[1], THEME.equipped[2], THEME.equipped[3], 1)
            ind.relicName:SetText(relic.name)
            ind.relicName:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)
        elseif itemId > 0 then
            ind.dot:SetVertexColor(THEME.equipped[1], THEME.equipped[2], THEME.equipped[3], 1)
            ind.relicName:SetText("#" .. itemId)
            ind.relicName:SetTextColor(THEME.muted[1], THEME.muted[2], THEME.muted[3], 1)
        else
            ind.dot:SetVertexColor(THEME.empty[1], THEME.empty[2], THEME.empty[3], 1)
            ind.relicName:SetText("空")
            ind.relicName:SetTextColor(THEME.empty[1], THEME.empty[2], THEME.empty[3], 1)
        end
    end

    -- 统计
    local total = #self.relicList
    local owned = 0
    local equipped = 0
    for _, r in ipairs(self.relicList) do
        if r.owned then owned = owned + 1 end
        if r.activeSlot and r.activeSlot > 0 then equipped = equipped + 1 end
    end
    self.equipStatus:SetText(string.format(
        "全部: %d 件  |  已收藏: %d  |  已装配: %d / %d  |  当前章节: %s",
        total, owned, equipped, #SLOTS,
        self.state.currentChapterName ~= "" and self.state.currentChapterName or "无"
    ))

    -- 隐藏所有旧卡片
    for _, card in ipairs(self.relicCards) do
        card:Hide()
        for _, btn in ipairs(card.actionButtons) do btn:Hide() end
    end

    -- 生成卡片
    for idx, relic in ipairs(self.relicList) do
        local card = self:GetOrCreateRelicCard(idx)
        local col = (idx - 1) % CARDS_PER_ROW
        local row = math.floor((idx - 1) / CARDS_PER_ROW)
        card:SetPoint("TOPLEFT", col * (CARD_W + CARD_GAP), -row * (CARD_H + CARD_GAP))

        local isOwned = relic.owned
        local isEquipped = relic.activeSlot > 0
        local itemName, iconPath = ResolveRelicItemDisplay(relic.id, relic.name, relic.icon)

        card.icon:SetTexture(iconPath)
        card.iconButton.itemId = relic.id

        -- 名称
        card.nameLabel:SetText(itemName or relic.name)
        if not isOwned then
            card.nameLabel:SetTextColor(0.40, 0.40, 0.40, 1)
        elseif isEquipped then
            card.nameLabel:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)
        else
            card.nameLabel:SetTextColor(0.80, 0.80, 0.78, 1)
        end

        -- 类型
        local typeColor = GetRelicTypeColor(relic.relicType)
        card.typeLabel:SetText(GetRelicTypeName(relic.relicType) .. (relic.actId > 0 and ("  第" .. relic.actId .. "幕") or ""))
        if isOwned then
            card.typeLabel:SetTextColor(typeColor[1], typeColor[2], typeColor[3], 0.8)
        else
            card.typeLabel:SetTextColor(0.35, 0.35, 0.35, 0.8)
        end

        -- 描述
        local descText = relic.desc
        if descText and #descText > 30 then
            descText = string.sub(descText, 1, 30) .. "..."
        end
        card.descLabel:SetText(descText or "")
        if not isOwned then
            card.descLabel:SetTextColor(0.30, 0.30, 0.30, 1)
        else
            card.descLabel:SetTextColor(THEME.muted[1], THEME.muted[2], THEME.muted[3], 1)
        end

        -- 卡片背景和状态条
        if isEquipped then
            card.statusBar:SetVertexColor(THEME.equipped[1], THEME.equipped[2], THEME.equipped[3], 1)
            card.iconBorder:SetBackdropBorderColor(THEME.equipped[1], THEME.equipped[2], THEME.equipped[3], 0.95)
            card.slotLabel:SetText("|cff4dcc4d" .. GetSlotName(relic.activeSlot) .. "|r")
            card:SetBackdropColor(THEME.cardEquip[1], THEME.cardEquip[2], THEME.cardEquip[3], THEME.cardEquip[4])
            card:SetBackdropBorderColor(THEME.equipped[1], THEME.equipped[2], THEME.equipped[3], 0.6)
        elseif isOwned then
            card.statusBar:SetVertexColor(typeColor[1], typeColor[2], typeColor[3], 0.6)
            card.iconBorder:SetBackdropBorderColor(typeColor[1], typeColor[2], typeColor[3], 0.90)
            card.slotLabel:SetText("")
            card:SetBackdropColor(THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])
            card:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])
        else
            card.statusBar:SetVertexColor(0.25, 0.25, 0.25, 0.5)
            card.iconBorder:SetBackdropBorderColor(0.30, 0.30, 0.32, 0.75)
            card.slotLabel:SetText("|cff666666\230\156\170\230\148\182\233\155\134|r")  -- "未收藏"
            card:SetBackdropColor(0.04, 0.04, 0.06, 0.85)
            card:SetBackdropBorderColor(0.20, 0.20, 0.25, 0.6)
        end

        -- 操作按钮：先隐藏旧的
        for _, btn in ipairs(card.actionButtons) do btn:Hide() end

        -- 只有已收藏的遗物才显示操作按钮
        if isOwned then
            local btnIdx = 0

            -- 卸下按钮
            if isEquipped then
                btnIdx = btnIdx + 1
                local btn = card.actionButtons[btnIdx]
                if not btn then
                    btn = CreateSmallButton(card, "卸下", 36, nil)
                    card.actionButtons[btnIdx] = btn
                end
                btn:SetPoint("BOTTOMRIGHT", -8, 6)
                btn.label:SetText("卸下")
                btn.label:SetTextColor(1.0, 0.5, 0.5, 1)
                local slotCmd = SLOTS[relic.activeSlot] and SLOTS[relic.activeSlot].cmd
                btn:SetScript("OnClick", function()
                    if slotCmd then
                        App.selectedRelicButtonKey = ""
                        SendAddon("CLEAR_RELIC:" .. slotCmd)
                        SendAddon("REQ_STATE")
                        SendAddon("REQ_RELICS")
                        App:RefreshEquipPage()
                    end
                end)
                btn:Show()
            end

            -- 装配按钮
            local validSlots = SLOT_MAP[relic.relicType] or {}
            for si = #validSlots, 1, -1 do
                local slotIdx = validSlots[si]
                local slotDef = SLOTS[slotIdx]
                btnIdx = btnIdx + 1
                local btn = card.actionButtons[btnIdx]
                if not btn then
                    btn = CreateSmallButton(card, "", 36, nil)
                    card.actionButtons[btnIdx] = btn
                end
                btn:SetPoint("BOTTOMRIGHT", -(8 + (btnIdx - 1) * 40), 6)
                btn.label:SetText(slotDef.label)
                local buttonKey = string.format("%d:%d", relic.id, slotIdx)
                btn:ApplySelectedState(App.selectedRelicButtonKey == buttonKey, relic.activeSlot == slotIdx)
                local relicId = relic.id
                local slotCommand = slotDef.cmd
                btn:SetScript("OnClick", function()
                    App.selectedRelicButtonKey = buttonKey
                    SendAddon(string.format("SET_RELIC:%s %d", slotCommand, relicId))
                    SendAddon("REQ_STATE")
                    SendAddon("REQ_RELICS")
                    App:RefreshEquipPage()
                end)
                btn:Show()
            end
        end

        card:Show()
    end

    -- 设置滚动区域高度
    local totalRows = math.ceil(#self.relicList / CARDS_PER_ROW)
    self.equipContent:SetHeight(math.max(totalRows * (CARD_H + CARD_GAP), 100))
    self.equipScroll:UpdateScrollChildRect()
end

-------------------------------------------------------
-- 神器页
-------------------------------------------------------
function App:BuildArtifactPage(parent)
    self.artifactStatus = parent:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.artifactStatus:SetPoint("TOPLEFT", 10, -8)
    self.artifactStatus:SetPoint("RIGHT", -10, 0)
    self.artifactStatus:SetJustifyH("LEFT")

    self.artifactActionBar = CreateFrame("Frame", nil, parent)
    self.artifactActionBar:SetPoint("TOPLEFT", 8, -28)
    self.artifactActionBar:SetPoint("RIGHT", -8, 0)
    self.artifactActionBar:SetHeight(24)

    self.artifactAllBtn = CreateSmallButton(self.artifactActionBar, "全部神器", 68, function()
        App.artifactFilterType = 0
        App:RefreshArtifactPage()
    end)
    self.artifactAllBtn:SetPoint("LEFT", 0, 0)

    self.artifactPhaseBtn = CreateSmallButton(self.artifactActionBar, "阶段神器", 68, function()
        App.artifactFilterType = 2
        App:RefreshArtifactPage()
    end)
    self.artifactPhaseBtn:SetPoint("LEFT", self.artifactAllBtn, "RIGHT", 6, 0)

    self.artifactUltimateBtn = CreateSmallButton(self.artifactActionBar, "终极神器", 68, function()
        App.artifactFilterType = 3
        App:RefreshArtifactPage()
    end)
    self.artifactUltimateBtn:SetPoint("LEFT", self.artifactPhaseBtn, "RIGHT", 6, 0)

    self.artifactOwnedBtn = CreateSmallButton(self.artifactActionBar, "仅已持有", 68, function()
        App.artifactOwnedOnly = not App.artifactOwnedOnly
        App:RefreshArtifactPage()
    end)
    self.artifactOwnedBtn:SetPoint("LEFT", self.artifactUltimateBtn, "RIGHT", 16, 0)

    self.artifactScroll = CreateFrame("ScrollFrame", nil, parent, "UIPanelScrollFrameTemplate")
    self.artifactScroll:SetPoint("TOPLEFT", 6, -56)
    self.artifactScroll:SetPoint("BOTTOMRIGHT", -28, 6)

    self.artifactContent = CreateFrame("Frame", nil, self.artifactScroll)
    self.artifactContent:SetWidth(EQUIP_CONTENT_W)
    self.artifactContent:SetHeight(100)
    self.artifactScroll:SetScrollChild(self.artifactContent)
    EnableScrollMouseWheel(self.artifactScroll, CARD_H)

    self.artifactCards = {}
end

function App:GetOrCreateArtifactCard(index)
    if self.artifactCards[index] then
        return self.artifactCards[index]
    end

    local card = CreateFrame("Frame", nil, self.artifactContent)
    card:SetSize(CARD_W, CARD_H)
    StylePanel(card, THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])

    card.statusBar = card:CreateTexture(nil, "ARTWORK")
    card.statusBar:SetSize(3, CARD_H - 8)
    card.statusBar:SetPoint("LEFT", 4, 0)
    card.statusBar:SetTexture("Interface\\BUTTONS\\WHITE8X8")

    card.iconButton = CreateFrame("Button", nil, card)
    card.iconButton:SetSize(CARD_ICON_SIZE, CARD_ICON_SIZE)
    card.iconButton:SetPoint("LEFT", 12, 0)
    card.iconButton:EnableMouse(true)
    card.iconButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    card.iconButton:SetScript("OnEnter", SetRelicItemTooltip)
    card.iconButton:SetScript("OnLeave", function()
        GameTooltip:Hide()
        HideTooltipExtra()
    end)

    card.icon = card.iconButton:CreateTexture(nil, "ARTWORK")
    card.icon:SetAllPoints()
    card.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
    ApplyIconTexCoords(card.icon)

    card.iconBorder = CreateFrame("Frame", nil, card)
    card.iconBorder:SetPoint("TOPLEFT", card.iconButton, "TOPLEFT", -3, 3)
    card.iconBorder:SetPoint("BOTTOMRIGHT", card.iconButton, "BOTTOMRIGHT", 3, -3)
    card.iconBorder:SetFrameLevel(math.max(card:GetFrameLevel(), card.iconButton:GetFrameLevel() - 1))
    StylePanel(card.iconBorder, 0.02, 0.02, 0.03, 0.35)

    card.nameLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    card.nameLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -8)
    card.nameLabel:SetPoint("RIGHT", -8, 0)
    card.nameLabel:SetJustifyH("LEFT")

    card.typeLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.typeLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -26)
    card.typeLabel:SetPoint("RIGHT", -76, 0)
    card.typeLabel:SetJustifyH("LEFT")

    card.slotLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    card.slotLabel:SetPoint("TOPRIGHT", -8, -8)

    card.descLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.descLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -40)
    card.descLabel:SetPoint("RIGHT", -84, 0)
    card.descLabel:SetJustifyH("LEFT")

    card.actionButtons = {}
    self.artifactCards[index] = card
    return card
end

function App:RefreshArtifactButtons()
    if not self.artifactActionBar then
        return
    end

    self.artifactAllBtn:ApplySelectedState(App.artifactFilterType == 0)
    self.artifactPhaseBtn:ApplySelectedState(App.artifactFilterType == 2)
    self.artifactUltimateBtn:ApplySelectedState(App.artifactFilterType == 3)
    self.artifactOwnedBtn:ApplySelectedState(App.artifactOwnedOnly)
end

function App:RefreshArtifactPage()
    QueueItemPrefetchList(self.relicList)
    self:RefreshArtifactButtons()

    local filteredRelics = {}
    local phaseOwned = 0
    local ultimateOwned = 0

    for _, relic in ipairs(self.relicList) do
        if relic.relicType == 2 or relic.relicType == 3 then
            if relic.relicType == 2 and relic.owned then
                phaseOwned = phaseOwned + 1
            elseif relic.relicType == 3 and relic.owned then
                ultimateOwned = ultimateOwned + 1
            end

            local typeMatch = App.artifactFilterType == 0 or relic.relicType == App.artifactFilterType
            local ownedMatch = not App.artifactOwnedOnly or relic.owned
            if typeMatch and ownedMatch then
                table.insert(filteredRelics, relic)
            end
        end
    end

    self.artifactStatus:SetText(string.format(
        "当前页为神器图鉴  |  显示: %d  |  阶段神器: %d  |  终极神器: %d  |  当前激活: %s / %s",
        #filteredRelics,
        phaseOwned,
        ultimateOwned,
        self.state.phaseArtifact > 0 and "阶段已激活" or "阶段未激活",
        self.state.ultimateArtifact > 0 and "终极已激活" or "终极未激活"
    ))

    for _, card in ipairs(self.artifactCards) do
        card:Hide()
        for _, btn in ipairs(card.actionButtons) do
            btn:Hide()
        end
    end

    for idx, relic in ipairs(filteredRelics) do
        local card = self:GetOrCreateArtifactCard(idx)
        local col = (idx - 1) % CARDS_PER_ROW
        local row = math.floor((idx - 1) / CARDS_PER_ROW)
        local isOwned = relic.owned
        local isEquipped = relic.activeSlot and relic.activeSlot > 0
        local typeColor = GetRelicTypeColor(relic.relicType)
        local itemName, iconPath = ResolveRelicItemDisplay(relic.id, relic.name, relic.icon)
        local descText = relic.desc

        if descText and #descText > 32 then
            descText = string.sub(descText, 1, 32) .. "..."
        end

        card:SetPoint("TOPLEFT", col * (CARD_W + CARD_GAP), -row * (CARD_H + CARD_GAP))
        card.icon:SetTexture(iconPath)
        card.iconButton.itemId = relic.id

        card.nameLabel:SetText(itemName or relic.name)
        card.nameLabel:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)
        card.typeLabel:SetText(string.format("%s  |  第%d幕", GetRelicTypeName(relic.relicType), relic.actId or 0))
        card.typeLabel:SetTextColor(typeColor[1], typeColor[2], typeColor[3], 0.9)
        card.slotLabel:SetText(isEquipped and ("|cff4dcc4d" .. GetSlotName(relic.activeSlot) .. "|r") or "")
        card.descLabel:SetText(descText or "")
        card.descLabel:SetTextColor(THEME.muted[1], THEME.muted[2], THEME.muted[3], 1)
        card.statusBar:SetVertexColor(typeColor[1], typeColor[2], typeColor[3], isEquipped and 1 or 0.75)
        card.iconBorder:SetBackdropBorderColor(typeColor[1], typeColor[2], typeColor[3], isEquipped and 1 or 0.9)

        local btnIdx = 0
        if isOwned then
            if isEquipped then
                btnIdx = btnIdx + 1
                local btn = card.actionButtons[btnIdx]
                if not btn then
                    btn = CreateSmallButton(card, "卸下", 36, nil)
                    card.actionButtons[btnIdx] = btn
                end
                btn:SetPoint("BOTTOMRIGHT", -8, 6)
                btn.label:SetText("卸下")
                btn.label:SetTextColor(1.0, 0.5, 0.5, 1)
                local slotCmd = SLOTS[relic.activeSlot] and SLOTS[relic.activeSlot].cmd
                btn:SetScript("OnClick", function()
                    if slotCmd then
                        App.selectedArtifactButtonKey = ""
                        SendAddon("CLEAR_RELIC:" .. slotCmd)
                        SendAddon("REQ_STATE")
                        SendAddon("REQ_RELICS")
                        App:RefreshArtifactPage()
                    end
                end)
                btn:Show()
            end

            local validSlots = SLOT_MAP[relic.relicType] or {}
            for _, slotIdx in ipairs(validSlots) do
                btnIdx = btnIdx + 1
                local btn = card.actionButtons[btnIdx]
                if not btn then
                    btn = CreateSmallButton(card, "", 42, nil)
                    card.actionButtons[btnIdx] = btn
                end
                btn:SetPoint("BOTTOMRIGHT", -(8 + (btnIdx - 1) * 46), 6)
                btn.label:SetText(SLOTS[slotIdx].label)
                local buttonKey = string.format("%d:%d", relic.id, slotIdx)
                btn:ApplySelectedState(App.selectedArtifactButtonKey == buttonKey, relic.activeSlot == slotIdx)
                local relicId = relic.id
                local slotCommand = SLOTS[slotIdx].cmd
                btn:SetScript("OnClick", function()
                    App.selectedArtifactButtonKey = buttonKey
                    SendAddon(string.format("SET_RELIC:%s %d", slotCommand, relicId))
                    SendAddon("REQ_STATE")
                    SendAddon("REQ_RELICS")
                    App:RefreshArtifactPage()
                end)
                btn:Show()
            end
        end

        card:Show()
    end

    local totalRows = math.ceil(#filteredRelics / CARDS_PER_ROW)
    self.artifactContent:SetHeight(math.max(totalRows * (CARD_H + CARD_GAP), 100))
    self.artifactScroll:UpdateScrollChildRect()
end

-------------------------------------------------------
-- 套装页
-------------------------------------------------------
function App:BuildEquipmentPage(parent)
    self.equipmentStatus = parent:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.equipmentStatus:SetPoint("TOPLEFT", 10, -8)
    self.equipmentStatus:SetPoint("RIGHT", -10, 0)
    self.equipmentStatus:SetJustifyH("LEFT")

    self.equipmentActionBar = CreateFrame("Frame", nil, parent)
    self.equipmentActionBar:SetPoint("TOPLEFT", 8, -28)
    self.equipmentActionBar:SetPoint("RIGHT", -8, 0)
    self.equipmentActionBar:SetHeight(24)

    self.equipmentFilterAllBtn = CreateSmallButton(self.equipmentActionBar, "全部", 52, function()
        App.equipmentFilterType = 0
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterAllBtn:SetPoint("LEFT", 0, 0)

    self.equipmentFilterBaseBtn = CreateSmallButton(self.equipmentActionBar, "底材", 52, function()
        App.equipmentFilterType = 1
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterBaseBtn:SetPoint("LEFT", self.equipmentFilterAllBtn, "RIGHT", 6, 0)

    self.equipmentFilterUniqueBtn = CreateSmallButton(self.equipmentActionBar, "唯一", 52, function()
        App.equipmentFilterType = 2
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterUniqueBtn:SetPoint("LEFT", self.equipmentFilterBaseBtn, "RIGHT", 6, 0)

    self.equipmentModeAllBtn = CreateSmallButton(self.equipmentActionBar, "全模式", 60, function()
        App.equipmentFilterMode = 0
        App:RefreshEquipmentPage()
    end)
    self.equipmentModeAllBtn:SetPoint("LEFT", self.equipmentFilterUniqueBtn, "RIGHT", 18, 0)

    self.equipmentModeStoryBtn = CreateSmallButton(self.equipmentActionBar, "正传", 52, function()
        App.equipmentFilterMode = 1
        App:RefreshEquipmentPage()
    end)
    self.equipmentModeStoryBtn:SetPoint("LEFT", self.equipmentModeAllBtn, "RIGHT", 6, 0)

    self.equipmentModeAbyssBtn = CreateSmallButton(self.equipmentActionBar, "深渊", 52, function()
        App.equipmentFilterMode = 2
        App:RefreshEquipmentPage()
    end)
    self.equipmentModeAbyssBtn:SetPoint("LEFT", self.equipmentModeStoryBtn, "RIGHT", 6, 0)

    self.equipmentModeCorruptBtn = CreateSmallButton(self.equipmentActionBar, "腐化", 52, function()
        App.equipmentFilterMode = 3
        App:RefreshEquipmentPage()
    end)
    self.equipmentModeCorruptBtn:SetPoint("LEFT", self.equipmentModeAbyssBtn, "RIGHT", 6, 0)

    self.equipmentModeReincarnationBtn = CreateSmallButton(self.equipmentActionBar, "轮回", 52, function()
        App.equipmentFilterMode = 4
        App:RefreshEquipmentPage()
    end)
    self.equipmentModeReincarnationBtn:SetPoint("LEFT", self.equipmentModeCorruptBtn, "RIGHT", 6, 0)

    self.equipmentOwnedBtn = CreateSmallButton(self.equipmentActionBar, "仅已持有", 68, function()
        App.equipmentOwnedOnly = not App.equipmentOwnedOnly
        App:RefreshEquipmentPage()
    end)
    self.equipmentOwnedBtn:SetPoint("LEFT", self.equipmentModeReincarnationBtn, "RIGHT", 18, 0)

    self.equipmentClearFilterBtn = CreateSmallButton(self.equipmentActionBar, "清筛选", 60, function()
        ResetEquipmentFilters()
        App:RefreshEquipmentPage()
    end)
    self.equipmentClearFilterBtn:SetPoint("LEFT", self.equipmentOwnedBtn, "RIGHT", 8, 0)

    self.equipmentFilterCurrentChapterBtn = CreateSmallButton(self.equipmentActionBar, "当前章节", 68, function()
        if App.equipmentFilterChapter == (App.state.currentChapter or 0) then
            App.equipmentFilterChapter = 0
        else
            App.equipmentFilterChapter = App.state.currentChapter or 0
        end
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterCurrentChapterBtn:SetPoint("LEFT", self.equipmentClearFilterBtn, "RIGHT", 12, 0)

    self.equipmentFilterWeaponBtn = CreateSmallButton(self.equipmentActionBar, "武器", 52, function()
        App.equipmentFilterSlot = App.equipmentFilterSlot == 1 and 0 or 1
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterWeaponBtn:SetPoint("LEFT", self.equipmentFilterCurrentChapterBtn, "RIGHT", 18, 0)

    self.equipmentFilterAccessoryBtn = CreateSmallButton(self.equipmentActionBar, "饰品", 52, function()
        App.equipmentFilterSlot = App.equipmentFilterSlot == 128 and 0 or 128
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterAccessoryBtn:SetPoint("LEFT", self.equipmentFilterWeaponBtn, "RIGHT", 6, 0)

    self.equipmentFilterHeadBtn = CreateSmallButton(self.equipmentActionBar, "头部", 52, function()
        App.equipmentFilterSlot = App.equipmentFilterSlot == 2 and 0 or 2
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterHeadBtn:SetPoint("LEFT", self.equipmentFilterAccessoryBtn, "RIGHT", 6, 0)

    self.equipmentFilterChestBtn = CreateSmallButton(self.equipmentActionBar, "胸甲", 52, function()
        App.equipmentFilterSlot = App.equipmentFilterSlot == 4 and 0 or 4
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterChestBtn:SetPoint("LEFT", self.equipmentFilterHeadBtn, "RIGHT", 6, 0)

    self.equipmentFilterRingBtn = CreateSmallButton(self.equipmentActionBar, "戒指", 52, function()
        App.equipmentFilterSlot = App.equipmentFilterSlot == 64 and 0 or 64
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterRingBtn:SetPoint("LEFT", self.equipmentFilterChestBtn, "RIGHT", 6, 0)

    self.equipmentFilterFocusBtn = CreateSmallButton(self.equipmentActionBar, "法器", 52, function()
        App.equipmentFilterSlot = App.equipmentFilterSlot == 512 and 0 or 512
        App:RefreshEquipmentPage()
    end)
    self.equipmentFilterFocusBtn:SetPoint("LEFT", self.equipmentFilterRingBtn, "RIGHT", 6, 0)

    self.equipmentScroll = CreateFrame("ScrollFrame", nil, parent, "UIPanelScrollFrameTemplate")
    self.equipmentScroll:SetPoint("TOPLEFT", 6, -56)
    self.equipmentScroll:SetPoint("BOTTOMRIGHT", -28, 6)

    self.equipmentContent = CreateFrame("Frame", nil, self.equipmentScroll)
    self.equipmentContent:SetWidth(EQUIP_CONTENT_W)
    self.equipmentContent:SetHeight(100)
    self.equipmentScroll:SetScrollChild(self.equipmentContent)
    EnableScrollMouseWheel(self.equipmentScroll, CARD_H)

    self.equipmentCards = {}
end

function App:GetOrCreateEquipmentCard(index)
    if self.equipmentCards[index] then
        return self.equipmentCards[index]
    end

    local card = CreateFrame("Frame", nil, self.equipmentContent)
    card:SetSize(CARD_W, CARD_H)

    card.bg = card:CreateTexture(nil, "BACKGROUND")
    card.bg:SetAllPoints()
    card.bg:SetTexture("Interface\\BUTTONS\\WHITE8X8")
    card.bg:SetVertexColor(THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])

    StylePanel(card, THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])

    card.statusBar = card:CreateTexture(nil, "ARTWORK")
    card.statusBar:SetSize(3, CARD_H - 8)
    card.statusBar:SetPoint("LEFT", 4, 0)
    card.statusBar:SetTexture("Interface\\BUTTONS\\WHITE8X8")

    card.iconButton = CreateFrame("Button", nil, card)
    card.iconButton:SetSize(CARD_ICON_SIZE, CARD_ICON_SIZE)
    card.iconButton:SetPoint("LEFT", 12, 0)
    card.iconButton:EnableMouse(true)
    card.iconButton:RegisterForClicks("LeftButtonUp")
    card.iconButton:SetScript("OnEnter", SetEquipmentItemTooltip)
    card.iconButton:SetScript("OnLeave", function()
        GameTooltip:Hide()
        HideTooltipExtra()
    end)
    card.iconButton:SetScript("OnClick", function(self)
        if self.equipmentData and self.equipmentData.sourceChapter and self.equipmentData.sourceChapter > 0 then
            App:SetSelectedChapter(self.equipmentData.sourceChapter, true)
            App:ShowTab("chapter")
        end
    end)

    card.icon = card.iconButton:CreateTexture(nil, "ARTWORK")
    card.icon:SetAllPoints()
    card.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
    ApplyIconTexCoords(card.icon)

    card.iconBorder = CreateFrame("Frame", nil, card)
    card.iconBorder:SetPoint("TOPLEFT", card.iconButton, "TOPLEFT", -3, 3)
    card.iconBorder:SetPoint("BOTTOMRIGHT", card.iconButton, "BOTTOMRIGHT", 3, -3)
    card.iconBorder:SetFrameLevel(math.max(card:GetFrameLevel(), card.iconButton:GetFrameLevel() - 1))
    StylePanel(card.iconBorder, 0.02, 0.02, 0.03, 0.35)

    card.nameLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    card.nameLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -8)
    card.nameLabel:SetPoint("RIGHT", -8, 0)
    card.nameLabel:SetJustifyH("LEFT")

    card.typeLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.typeLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -26)
    card.typeLabel:SetPoint("RIGHT", -74, 0)
    card.typeLabel:SetJustifyH("LEFT")

    card.slotLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    card.slotLabel:SetPoint("TOPRIGHT", -8, -8)

    card.descLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.descLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -40)
    card.descLabel:SetPoint("RIGHT", -8, 0)
    card.descLabel:SetJustifyH("LEFT")

    self.equipmentCards[index] = card
    return card
end

function App:RefreshEquipmentFilterButtons()
    if not self.equipmentActionBar then
        return
    end

    self.equipmentFilterAllBtn:ApplySelectedState(App.equipmentFilterType == 0)
    self.equipmentFilterBaseBtn:ApplySelectedState(App.equipmentFilterType == 1)
    self.equipmentFilterUniqueBtn:ApplySelectedState(App.equipmentFilterType == 2)

    self.equipmentModeAllBtn:ApplySelectedState(App.equipmentFilterMode == 0)
    self.equipmentModeStoryBtn:ApplySelectedState(App.equipmentFilterMode == 1)
    self.equipmentModeAbyssBtn:ApplySelectedState(App.equipmentFilterMode == 2)
    self.equipmentModeCorruptBtn:ApplySelectedState(App.equipmentFilterMode == 3)
    self.equipmentModeReincarnationBtn:ApplySelectedState(App.equipmentFilterMode == 4)

    self.equipmentOwnedBtn:ApplySelectedState(App.equipmentOwnedOnly)
    self.equipmentClearFilterBtn:ApplySelectedState(false)
    self.equipmentFilterCurrentChapterBtn:ApplySelectedState(App.equipmentFilterChapter ~= 0)

    self.equipmentFilterWeaponBtn:ApplySelectedState(App.equipmentFilterSlot == 1)
    self.equipmentFilterAccessoryBtn:ApplySelectedState(App.equipmentFilterSlot == 128)
    self.equipmentFilterHeadBtn:ApplySelectedState(App.equipmentFilterSlot == 2)
    self.equipmentFilterChestBtn:ApplySelectedState(App.equipmentFilterSlot == 4)
    self.equipmentFilterRingBtn:ApplySelectedState(App.equipmentFilterSlot == 64)
    self.equipmentFilterFocusBtn:ApplySelectedState(App.equipmentFilterSlot == 512)
end

function App:RefreshEquipmentPage()
    local total = #self.equipmentList
    local uniqueCount = 0
    local baseCount = 0
    for _, equipment in ipairs(self.equipmentList) do
        if equipment.equipmentType == 2 then
            uniqueCount = uniqueCount + 1
        else
            baseCount = baseCount + 1
        end
    end

    local filteredEquipments = {}
    for _, equipment in ipairs(self.equipmentList) do
        local typeMatch = App.equipmentFilterType == 0 or equipment.equipmentType == App.equipmentFilterType
        local modeMatch = App.equipmentFilterMode == 0 or equipment.sourceMode == App.equipmentFilterMode
        local chapterMatch = App.equipmentFilterChapter == 0 or equipment.sourceChapter == App.equipmentFilterChapter
        local slotMatch = App.equipmentFilterSlot == 0 or equipment.slotMask == App.equipmentFilterSlot
        local ownedMatch = true
        if App.equipmentOwnedOnly then
            ownedMatch = App.relicMap[equipment.id] ~= nil and App.relicMap[equipment.id].owned
        end

        if typeMatch and modeMatch and chapterMatch and slotMatch and ownedMatch then
            table.insert(filteredEquipments, equipment)
        end
    end

    QueueItemPrefetchList(filteredEquipments)
    self:RefreshEquipmentFilterButtons()

    self.equipmentStatus:SetText(string.format(
        "当前页为装备图鉴  |  显示: %d / %d  |  传奇唯一: %d  |  底材: %d  |  当前章节: %s  |  当前筛选: %s  |  点击图标跳转章节",
        #filteredEquipments, total, uniqueCount, baseCount,
        self.state.currentChapterName ~= "" and self.state.currentChapterName or "无",
        GetEquipmentFilterSummary()
    ))

    for _, card in ipairs(self.equipmentCards) do
        card:Hide()
    end

    for idx, equipment in ipairs(filteredEquipments) do
        local card = self:GetOrCreateEquipmentCard(idx)
        local col = (idx - 1) % CARDS_PER_ROW
        local row = math.floor((idx - 1) / CARDS_PER_ROW)
        card:SetPoint("TOPLEFT", col * (CARD_W + CARD_GAP), -row * (CARD_H + CARD_GAP))

        local itemName, iconPath = ResolveItemDisplay(equipment.id, equipment.name, equipment.icon)
        local chapterName = equipment.sourceChapterName ~= "" and equipment.sourceChapterName or ("章节#" .. equipment.sourceChapter)
        local typeColor = equipment.fromCacheBoss and THEME.cacheBoss or GetEquipmentTypeColor(equipment.equipmentType)
        local descText = equipment.desc
        local ownedRelic = App.relicMap[equipment.id]
        local isOwned = ownedRelic and ownedRelic.owned
        if descText and #descText > 30 then
            descText = string.sub(descText, 1, 30) .. "..."
        end

        card.icon:SetTexture(iconPath)
        card.iconButton.itemId = equipment.id
        card.iconButton.equipmentData = equipment

        card.nameLabel:SetText((itemName or equipment.name) .. (isOwned and "  |cff4dcc4d[已持有]|r" or ""))
        card.nameLabel:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)

        card.typeLabel:SetText(string.format(
            "%s  |  第%d幕  |  %s  |  %s  |  ilvl %d",
            GetEquipmentTypeName(equipment.equipmentType),
            equipment.actId,
            chapterName,
            GetEquipmentSourceModeName(equipment.sourceMode),
            equipment.baseItemLevel
        ))
        card.typeLabel:SetTextColor(typeColor[1], typeColor[2], typeColor[3], 0.85)

        card.slotLabel:SetText("|cffDBA64A" .. GetEquipmentSlotMaskName(equipment.slotMask) .. "|r")

        if equipment.fromCacheBoss then
            card.descLabel:SetText((descText or "") .. "|cffff6b6b  [秘藏首领]|r")
        elseif equipment.requiresFragments then
            card.descLabel:SetText((descText or "") .. "|cff7edc8b  [碎片合成]|r")
        elseif isOwned then
            card.descLabel:SetText((descText or "") .. "|cff7edc8b  [已持有，可点图标跳转章节]|r")
        else
            card.descLabel:SetText(descText or "")
        end
        card.descLabel:SetTextColor(THEME.muted[1], THEME.muted[2], THEME.muted[3], 1)

        card.statusBar:SetVertexColor(typeColor[1], typeColor[2], typeColor[3], 0.9)
        card.iconBorder:SetBackdropBorderColor(typeColor[1], typeColor[2], typeColor[3], 0.9)
        if equipment.equipmentType == 2 then
            card:SetBackdropColor(0.17, 0.12, 0.08, 0.95)
            card:SetBackdropBorderColor(typeColor[1], typeColor[2], typeColor[3], 0.7)
        else
            card:SetBackdropColor(THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])
            card:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])
        end

        card:Show()
    end

    local totalRows = math.ceil(#filteredEquipments / CARDS_PER_ROW)
    self.equipmentContent:SetHeight(math.max(totalRows * (CARD_H + CARD_GAP), 100))
    self.equipmentScroll:UpdateScrollChildRect()
end

-------------------------------------------------------
-- 套装图鉴页
-------------------------------------------------------
function App:BuildSetOverviewPage(parent)
    self.setOverviewStatus = parent:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.setOverviewStatus:SetPoint("TOPLEFT", 10, -8)
    self.setOverviewStatus:SetPoint("RIGHT", -10, 0)
    self.setOverviewStatus:SetJustifyH("LEFT")

    self.setOverviewActionBar = CreateFrame("Frame", nil, parent)
    self.setOverviewActionBar:SetPoint("TOPLEFT", 8, -28)
    self.setOverviewActionBar:SetPoint("RIGHT", -8, 0)
    self.setOverviewActionBar:SetHeight(24)

    self.setOverviewAllBtn = CreateSmallButton(self.setOverviewActionBar, "全部", 52, function()
        App.setOverviewFilterMode = 0
        App:RefreshSetOverviewPage()
    end)
    self.setOverviewAllBtn:SetPoint("LEFT", 0, 0)

    self.setOverviewStoryBtn = CreateSmallButton(self.setOverviewActionBar, "正传", 52, function()
        App.setOverviewFilterMode = 1
        App:RefreshSetOverviewPage()
    end)
    self.setOverviewStoryBtn:SetPoint("LEFT", self.setOverviewAllBtn, "RIGHT", 6, 0)

    self.setOverviewCorruptBtn = CreateSmallButton(self.setOverviewActionBar, "腐化", 52, function()
        App.setOverviewFilterMode = 3
        App:RefreshSetOverviewPage()
    end)
    self.setOverviewCorruptBtn:SetPoint("LEFT", self.setOverviewStoryBtn, "RIGHT", 6, 0)

    self.setOverviewReincarnationBtn = CreateSmallButton(self.setOverviewActionBar, "轮回", 52, function()
        App.setOverviewFilterMode = 4
        App:RefreshSetOverviewPage()
    end)
    self.setOverviewReincarnationBtn:SetPoint("LEFT", self.setOverviewCorruptBtn, "RIGHT", 6, 0)

    self.setOverviewCurrentActBtn = CreateSmallButton(self.setOverviewActionBar, "当前幕", 60, function()
        App.setOverviewCurrentActOnly = not App.setOverviewCurrentActOnly
        App:RefreshSetOverviewPage()
    end)
    self.setOverviewCurrentActBtn:SetPoint("LEFT", self.setOverviewReincarnationBtn, "RIGHT", 16, 0)

    self.setOverviewScroll = CreateFrame("ScrollFrame", nil, parent, "UIPanelScrollFrameTemplate")
    self.setOverviewScroll:SetPoint("TOPLEFT", 6, -56)
    self.setOverviewScroll:SetPoint("BOTTOMRIGHT", -28, 6)

    self.setOverviewContent = CreateFrame("Frame", nil, self.setOverviewScroll)
    self.setOverviewContent:SetWidth(EQUIP_CONTENT_W)
    self.setOverviewContent:SetHeight(100)
    self.setOverviewScroll:SetScrollChild(self.setOverviewContent)
    EnableScrollMouseWheel(self.setOverviewScroll, CARD_H)

    self.setOverviewCards = {}
end

function App:GetOrCreateSetOverviewCard(index)
    if self.setOverviewCards[index] then
        return self.setOverviewCards[index]
    end

    local card = CreateFrame("Frame", nil, self.setOverviewContent)
    card:SetSize(CARD_W, CARD_H)
    StylePanel(card, THEME.cardBg[1], THEME.cardBg[2], THEME.cardBg[3], THEME.cardBg[4])

    card.statusBar = card:CreateTexture(nil, "ARTWORK")
    card.statusBar:SetSize(3, CARD_H - 8)
    card.statusBar:SetPoint("LEFT", 4, 0)
    card.statusBar:SetTexture("Interface\\BUTTONS\\WHITE8X8")

    card.iconButton = CreateFrame("Button", nil, card)
    card.iconButton:SetSize(CARD_ICON_SIZE, CARD_ICON_SIZE)
    card.iconButton:SetPoint("LEFT", 12, 0)
    card.iconButton:RegisterForClicks("LeftButtonUp")
    card.iconButton:SetScript("OnClick", function(self)
        local setData = self.setData
        if not setData then
            return
        end

        ResetEquipmentFilters()
        App.equipmentFilterType = 1
        App.equipmentFilterMode = setData.sourceMode
        App.equipmentFilterChapter = setData.sourceChapter
        App:ShowTab("set")
    end)

    card.icon = card.iconButton:CreateTexture(nil, "ARTWORK")
    card.icon:SetAllPoints()
    card.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
    ApplyIconTexCoords(card.icon)

    card.iconBorder = CreateFrame("Frame", nil, card)
    card.iconBorder:SetPoint("TOPLEFT", card.iconButton, "TOPLEFT", -3, 3)
    card.iconBorder:SetPoint("BOTTOMRIGHT", card.iconButton, "BOTTOMRIGHT", 3, -3)
    card.iconBorder:SetFrameLevel(math.max(card:GetFrameLevel(), card.iconButton:GetFrameLevel() - 1))
    StylePanel(card.iconBorder, 0.02, 0.02, 0.03, 0.35)

    card.nameLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    card.nameLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -8)
    card.nameLabel:SetPoint("RIGHT", -8, 0)
    card.nameLabel:SetJustifyH("LEFT")

    card.typeLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.typeLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -26)
    card.typeLabel:SetPoint("RIGHT", -8, 0)
    card.typeLabel:SetJustifyH("LEFT")

    card.slotLabel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    card.slotLabel:SetPoint("TOPRIGHT", -8, -8)

    card.descLabel = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    card.descLabel:SetPoint("TOPLEFT", CARD_TEXT_LEFT, -40)
    card.descLabel:SetPoint("RIGHT", -8, 0)
    card.descLabel:SetJustifyH("LEFT")

    self.setOverviewCards[index] = card
    return card
end

function App:RefreshSetOverviewButtons()
    if not self.setOverviewActionBar then
        return
    end

    self.setOverviewAllBtn:ApplySelectedState(App.setOverviewFilterMode == 0)
    self.setOverviewStoryBtn:ApplySelectedState(App.setOverviewFilterMode == 1)
    self.setOverviewCorruptBtn:ApplySelectedState(App.setOverviewFilterMode == 3)
    self.setOverviewReincarnationBtn:ApplySelectedState(App.setOverviewFilterMode == 4)
    self.setOverviewCurrentActBtn:ApplySelectedState(App.setOverviewCurrentActOnly)
end

function App:RefreshSetOverviewPage()
    local currentActId = GetCurrentActId()
    local allGroups = BuildEquipmentSetGroups()
    local filteredGroups = {}

    for _, group in ipairs(allGroups) do
        local modeMatch = App.setOverviewFilterMode == 0 or group.sourceMode == App.setOverviewFilterMode
        local actMatch = not App.setOverviewCurrentActOnly or (currentActId > 0 and group.actId == currentActId)
        if modeMatch and actMatch then
            table.insert(filteredGroups, group)
        end
    end

    self:RefreshSetOverviewButtons()
    self.setOverviewStatus:SetText(string.format(
        "当前页为套装图鉴  |  显示: %d / %d  |  当前幕: %s  |  点击图标跳转到对应装备图鉴",
        #filteredGroups,
        #allGroups,
        currentActId > 0 and ("第" .. currentActId .. "幕") or "无"
    ))

    for _, card in ipairs(self.setOverviewCards) do
        card:Hide()
    end

    for idx, group in ipairs(filteredGroups) do
        local card = self:GetOrCreateSetOverviewCard(idx)
        local col = (idx - 1) % CARDS_PER_ROW
        local row = math.floor((idx - 1) / CARDS_PER_ROW)
        local rep = group.representative
        local itemName, iconPath = ResolveItemDisplay(rep.id, rep.name, rep.icon)
        local typeColor = GetEquipmentTypeColor(1)

        card:SetPoint("TOPLEFT", col * (CARD_W + CARD_GAP), -row * (CARD_H + CARD_GAP))
        card.icon:SetTexture(iconPath)
        card.iconButton.itemId = rep.id
        card.iconButton.setData = group

        card.nameLabel:SetText(group.name)
        card.nameLabel:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)
        card.typeLabel:SetText(string.format(
            "来源章节: %s  |  模式: %s  |  件数: %d/9",
            group.sourceChapterName ~= "" and group.sourceChapterName or ("章节#" .. group.sourceChapter),
            GetEquipmentSourceModeName(group.sourceMode),
            group.pieceCount or 0
        ))
        card.typeLabel:SetTextColor(typeColor[1], typeColor[2], typeColor[3], 0.85)
        card.slotLabel:SetText("|cffDBA64A套装|r")

        -- 卡片本体只显示部位概览
        card.descLabel:SetText(group.slotSummary ~= "" and group.slotSummary or (itemName or rep.name))
        card.descLabel:SetTextColor(THEME.muted[1], THEME.muted[2], THEME.muted[3], 1)

        -- tooltip显示套装效果（不重复卡片内容）
        local bonusKey = string.format("%d:%d", group.sourceMode, group.actId)
        local bonus = App.setBonusMap[bonusKey]
        card.bonusData = bonus
        card.groupData = group
        card:EnableMouse(true)
        card:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:AddLine(self.groupData.name, 1.0, 0.82, 0.28)
            GameTooltip:AddLine(string.format("件数: %d/9  |  模式: %s", self.groupData.pieceCount or 0, GetEquipmentSourceModeName(self.groupData.sourceMode)), 0.7, 0.7, 0.7)
            local b = self.bonusData
            if b then
                if b.twoPieceDesc ~= "" then
                    GameTooltip:AddLine(" ")
                    GameTooltip:AddLine("|cff00ff00(2件) " .. b.twoPieceDesc .. "|r", 0, 1, 0, true)
                end
                if b.fourPieceDesc ~= "" then
                    GameTooltip:AddLine("|cffff8000(4件) " .. b.fourPieceDesc .. "|r", 1, 0.5, 0, true)
                end
            else
                GameTooltip:AddLine(" ")
                GameTooltip:AddLine("暂无套装效果数据", 0.5, 0.5, 0.5)
            end
            GameTooltip:Show()
        end)
        card:SetScript("OnLeave", function() GameTooltip:Hide() end)
        card.statusBar:SetVertexColor(typeColor[1], typeColor[2], typeColor[3], 0.85)
        card.iconBorder:SetBackdropBorderColor(typeColor[1], typeColor[2], typeColor[3], 0.9)
        card:Show()
    end

    local totalRows = math.ceil(#filteredGroups / CARDS_PER_ROW)
    self.setOverviewContent:SetHeight(math.max(totalRows * (CARD_H + CARD_GAP), 100))
    self.setOverviewScroll:UpdateScrollChildRect()
end

-------------------------------------------------------
-- 章节页
-------------------------------------------------------
function App:BuildChapterPage(parent)
    self.chapterSummary = parent:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.chapterSummary:SetPoint("TOPLEFT", 10, -8)
    self.chapterSummary:SetPoint("RIGHT", -10, 0)
    self.chapterSummary:SetJustifyH("LEFT")

    self.chapterScroll = CreateFrame("ScrollFrame", nil, parent, "UIPanelScrollFrameTemplate")
    self.chapterScroll:SetPoint("TOPLEFT", 8, -28)
    self.chapterScroll:SetPoint("BOTTOMLEFT", 8, 8)
    self.chapterScroll:SetWidth(CHAPTER_LIST_W)

    self.chapterContent = CreateFrame("Frame", nil, self.chapterScroll)
    self.chapterContent:SetWidth(CHAPTER_LIST_W)
    self.chapterContent:SetHeight(100)
    self.chapterScroll:SetScrollChild(self.chapterContent)
    EnableScrollMouseWheel(self.chapterScroll, 56)

    self.chapterDetailPanel = CreateFrame("Frame", nil, parent)
    self.chapterDetailPanel:SetPoint("TOPLEFT", self.chapterScroll, "TOPRIGHT", 12, 0)
    self.chapterDetailPanel:SetPoint("BOTTOMRIGHT", -8, 8)
    StylePanel(self.chapterDetailPanel, 0.07, 0.05, 0.11, 0.94)

    self.chapterDetailTitle = self.chapterDetailPanel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    self.chapterDetailTitle:SetPoint("TOPLEFT", 16, -10)
    self.chapterDetailTitle:SetPoint("RIGHT", -16, 0)
    self.chapterDetailTitle:SetJustifyH("LEFT")
    self.chapterDetailTitle:SetTextColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)

    self.chapterDetailMeta = self.chapterDetailPanel:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.chapterDetailMeta:SetPoint("TOPLEFT", 16, -34)
    self.chapterDetailMeta:SetPoint("RIGHT", -16, 0)
    self.chapterDetailMeta:SetJustifyH("LEFT")

    self.chapterActionStatus = self.chapterDetailPanel:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    self.chapterActionStatus:SetPoint("TOPLEFT", 16, -58)
    self.chapterActionStatus:SetPoint("RIGHT", -16, 0)
    self.chapterActionStatus:SetJustifyH("LEFT")

    self.chapterEquipBtn = CreateSmallButton(self.chapterDetailPanel, "查看本章装备", 96, function()
        local chapterId = App.selectedChapterId or 0
        if chapterId <= 0 then
            return
        end
        ApplyEquipmentChapterFilter(chapterId)
        App:ShowTab("set")
    end)
    self.chapterEquipBtn:SetPoint("TOPRIGHT", -16, -10)

    self.chapterAllEquipBtn = CreateSmallButton(self.chapterDetailPanel, "查看全部装备", 96, function()
        ResetEquipmentFilters()
        App:ShowTab("set")
    end)
    self.chapterAllEquipBtn:SetPoint("RIGHT", self.chapterEquipBtn, "LEFT", -8, 0)

    self.chapterEnterStoryBtn = CreateSmallButton(self.chapterDetailPanel, "进入正传", 84, function()
        App.chapterEnterMode = 1
        SendEnterSelectedChapter(1, 0)
    end)
    self.chapterEnterStoryBtn:SetPoint("TOPLEFT", 16, -78)

    self.chapterEnterAbyssBtn = CreateSmallButton(self.chapterDetailPanel, "进入深渊", 84, function()
        App.chapterEnterMode = 2
        SendEnterSelectedChapter(2, 0)
    end)
    self.chapterEnterAbyssBtn:SetPoint("LEFT", self.chapterEnterStoryBtn, "RIGHT", 6, 0)

    self.chapterEnterCorruptBtn = CreateSmallButton(self.chapterDetailPanel, "进入腐化", 84, function()
        App.chapterEnterMode = 3
        local tier = self.state.highestCorruptionTier and math.max(0, self.state.highestCorruptionTier) or 0
        SendEnterSelectedChapter(3, tier)
    end)
    self.chapterEnterCorruptBtn:SetPoint("LEFT", self.chapterEnterAbyssBtn, "RIGHT", 6, 0)

    self.chapterEnterReincarnationBtn = CreateSmallButton(self.chapterDetailPanel, "进入轮回", 84, function()
        App.chapterEnterMode = 4
        SendEnterSelectedChapter(4, 0)
    end)
    self.chapterEnterReincarnationBtn:SetPoint("LEFT", self.chapterEnterCorruptBtn, "RIGHT", 6, 0)

    self.chapterDetailScroll = CreateFrame("ScrollFrame", nil, self.chapterDetailPanel, "UIPanelScrollFrameTemplate")
    self.chapterDetailScroll:SetPoint("TOPLEFT", 12, -110)
    self.chapterDetailScroll:SetPoint("BOTTOMRIGHT", -30, 12)
    EnableScrollMouseWheel(self.chapterDetailScroll, 56)

    self.chapterDetailContent = CreateFrame("Frame", nil, self.chapterDetailScroll)
    self.chapterDetailContent:SetWidth(720)
    self.chapterDetailContent:SetHeight(100)
    self.chapterDetailScroll:SetScrollChild(self.chapterDetailContent)

    self.chapterDetailText = self.chapterDetailContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    self.chapterDetailText:SetPoint("TOPLEFT", 8, -4)
    self.chapterDetailText:SetPoint("RIGHT", -8, 0)
    self.chapterDetailText:SetJustifyH("LEFT")
    self.chapterDetailText:SetJustifyV("TOP")

    self.chapterRows = {}
end

function App:RefreshChapterPage()
    if self.selectedChapterId == 0 or not self.chapterMap[self.selectedChapterId] then
        if self.chapterMap[self.state.currentChapter] then
            self.selectedChapterId = self.state.currentChapter
        elseif self.chapters[1] then
            self.selectedChapterId = self.chapters[1].id
        end
    end

    self.chapterSummary:SetText(string.format(
        "当前章节: %s  |  下一章节: %s  |  角色等级: %d  |  修仙门槛: %d  |  最高腐化: %d",
        self.state.currentChapterName ~= "" and self.state.currentChapterName or "无",
        self.state.nextChapterName ~= "" and self.state.nextChapterName or "无",
        self.state.playerLevel or 0,
        self.state.currentThreshold or 0,
        self.state.highestCorruptionTier or 0
    ))

    -- 仅在章节列表变化时重建 Frame 结构
    if self._builtChapterCount ~= #self.chapters then
        for _, row in ipairs(self.chapterRows) do row:Hide() end
        local currentAct = -1
        local y = 0
        for _, ch in ipairs(self.chapters) do
            if ch.actId ~= currentAct then
                currentAct = ch.actId
                local header = self:GetOrCreateChapterRow(#self.chapterRows + 1)
                header:SetSize(CHAPTER_LIST_W, 24)
                header:SetPoint("TOPLEFT", 0, -y)
                header:SetBackdrop(nil)
                header.nameLabel:SetText(string.format("|cffdba64a--- 第 %d 幕 ---|r", ch.actId))
                header.nameLabel:ClearAllPoints()
                header.nameLabel:SetPoint("LEFT", 8, 0)
                header.infoLabel:SetText("")
                header.bg:Hide()
                header.chapterId = 0
                header:SetScript("OnMouseDown", nil)
                header:Show()
                y = y + 28
            end
            -- 章节行
            local row = self:GetOrCreateChapterRow(#self.chapterRows + 1)
            row:SetSize(CHAPTER_LIST_W, 32)
            row:SetPoint("TOPLEFT", 0, -y)
            row.nameLabel:ClearAllPoints()
            row.nameLabel:SetPoint("LEFT", 16, 0)
            row.nameLabel:SetPoint("RIGHT", -132, 0)
            row.infoLabel:SetText(string.format("|cffFFD700%s  Lv%d|r", GetChapterTypeName(ch.chapterType), ch.threshold))
            row.chapterId = ch.id
            row:SetScript("OnMouseDown", function()
                App:SetSelectedChapter(ch.id, true)
            end)
            y = y + 34
        end
        self.chapterContent:SetHeight(math.max(y + 10, 100))
        self.chapterScroll:UpdateScrollChildRect()
        self._builtChapterCount = #self.chapters
    end

    -- 快速通道：每次只更新颜色/背景，不重建 Frame
    for _, row in ipairs(self.chapterRows) do
        local chId = row.chapterId
        if chId and chId > 0 then
            local ch = self.chapterMap[chId]
            if ch then
                local isCurrent  = chId == self.state.currentChapter
                local isPast     = chId <= self.state.highestChapter
                local isSelected = chId == self.selectedChapterId
                local nameColor  = isCurrent and "4dff4d" or (isPast and "3aaa3a" or "2a5c2a")
                local marker     = isCurrent and "  |cffffff00<< \229\189\147\229\137\141 >>|r" or ""
                row.nameLabel:SetText(string.format("|cff%s[%02d] %s|r%s", nameColor, ch.id, ch.name, marker))
                if isSelected then
                    row.bg:SetVertexColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 0.18)
                    row.bg:Show()
                elseif isCurrent then
                    row.bg:SetVertexColor(THEME.currentCh[1], THEME.currentCh[2], THEME.currentCh[3], THEME.currentCh[4])
                    row.bg:Show()
                else
                    row.bg:Hide()
                end
                row:Show()
            end
        end
    end
    self:RefreshChapterDetail()
end

function App:GetOrCreateChapterRow(index)
    if self.chapterRows[index] then
        return self.chapterRows[index]
    end

    local row = CreateFrame("Frame", nil, self.chapterContent)
    row:SetSize(CHAPTER_LIST_W, 32)
    row:EnableMouse(true)

    row.bg = row:CreateTexture(nil, "BACKGROUND")
    row.bg:SetAllPoints()
    row.bg:SetTexture("Interface\\BUTTONS\\WHITE8X8")
    row.bg:Hide()

    row.nameLabel = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row.nameLabel:SetPoint("LEFT", 16, 0)
    row.nameLabel:SetPoint("RIGHT", -160, 0)
    row.nameLabel:SetJustifyH("LEFT")

    row.infoLabel = row:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    row.infoLabel:SetPoint("RIGHT", -8, 0)

    self.chapterRows[index] = row
    return row
end

function App:GetChapterPreviewForDisplay(chapterId)
    if chapterId <= 0 then
        return nil
    end

    if self.rewardPreview.view and self.rewardPreview.view.chapterId == chapterId then
        return self.rewardPreview.view
    end

    if self.rewardPreview.current and self.rewardPreview.current.chapterId == chapterId then
        return self.rewardPreview.current
    end

    if self.rewardPreview.next and self.rewardPreview.next.chapterId == chapterId then
        return self.rewardPreview.next
    end

    return nil
end

function App:RequestSelectedChapterPreview()
    if self.selectedChapterId and self.selectedChapterId > 0 then
        SendAddon("REQ_REWARD_CHAPTER:" .. self.selectedChapterId)
    end
end

function App:SetSelectedChapter(chapterId, requestPreview)
    if not chapterId or chapterId <= 0 then
        return
    end

    self.selectedChapterId = chapterId
    if requestPreview then
        self:RequestSelectedChapterPreview()
    end
    self:RefreshCurrent()
end

function App:RefreshChapterDetail()
    local chapter = self.chapterMap[self.selectedChapterId]
    if not chapter then
        self.chapterDetailTitle:SetText("未选择章节")
        self.chapterDetailMeta:SetText("")
        self.chapterDetailText:SetText("请选择左侧章节查看详情。")
        self.chapterDetailContent:SetHeight(100)
        self.chapterDetailScroll:UpdateScrollChildRect()
        return
    end

    local stateLabel = "未解锁"
    if chapter.id == self.state.currentChapter then
        stateLabel = "当前推进"
    elseif chapter.id <= self.state.highestChapter then
        stateLabel = "已完成"
    elseif chapter.id == self.state.nextChapter then
        stateLabel = "下一目标"
    end

    local accessSummary, accessReady = GetChapterAccessSummary(chapter)
    local prerequisiteName = chapter.id > 1 and GetChapterNameById(chapter.id - 1) or "无"

    self.chapterDetailTitle:SetText(string.format("第 %d 幕  [%02d] %s", chapter.actId, chapter.id, chapter.name))
    self.chapterDetailMeta:SetText(string.format(
        "状态: %s  |  类型: %s  |  地图ID: %d  |  建议修仙门槛: %d  |  进入判断: %s",
        stateLabel,
        GetChapterTypeName(chapter.chapterType),
        chapter.mapId,
        chapter.threshold,
        accessReady and ("|cff4dcc4d" .. accessSummary .. "|r") or ("|cffff6b6b" .. accessSummary .. "|r")
    ))
    self.chapterActionStatus:SetText(App.chapterLastActionText ~= "" and App.chapterLastActionText or "最近请求：暂无")

    if self.chapterEnterStoryBtn then
        self.chapterEnterStoryBtn:SetAlpha((self.state.inRun or not accessReady or not HasChapterModeUnlocked(chapter, 1)) and 0.45 or 1.0)
        self.chapterEnterAbyssBtn:SetAlpha((self.state.inRun or not accessReady or not HasChapterModeUnlocked(chapter, 2)) and 0.45 or 1.0)
        self.chapterEnterCorruptBtn:SetAlpha((self.state.inRun or not accessReady or not HasChapterModeUnlocked(chapter, 3)) and 0.45 or 1.0)
        self.chapterEnterReincarnationBtn:SetAlpha((self.state.inRun or not accessReady or not HasChapterModeUnlocked(chapter, 4)) and 0.45 or 1.0)
    end

    local lines = {}
    table.insert(lines, string.format("|cffDBA64A章节任务|r  起始任务: |cffffffff%d|r  完成任务: |cffffffff%d|r", chapter.startQuestId, chapter.completeQuestId))
    table.insert(lines, string.format(
        "|cffDBA64A首领映射|r  锚点: |cffffffff%s|r  |  最终: |cffffffff%s|r",
        FormatBossDisplay(chapter.anchorBossEntry, chapter.anchorBossName),
        FormatBossDisplay(chapter.finalBossEntry, chapter.finalBossName)
    ))
    table.insert(lines, string.format(
        "|cffDBA64A模式首领|r  深渊: |cffffffff%s|r  |  秘藏: |cffffffff%s|r",
        FormatBossDisplay(chapter.abyssBossEntry, chapter.abyssBossName),
        FormatBossDisplay(chapter.cacheBossEntry, chapter.cacheBossName)
    ))
    table.insert(lines, string.format(
        "|cffDBA64A章节模式|r  正传:%s  深渊:%s  腐化:%s  轮回:%s",
        HasChapterModeUnlocked(chapter, 1) and "|cff4dcc4d已解锁|r" or "|cffff6b6b未解锁|r",
        HasChapterModeUnlocked(chapter, 2) and "|cff4dcc4d已解锁|r" or "|cffff6b6b未解锁|r",
        HasChapterModeUnlocked(chapter, 3) and "|cff4dcc4d已解锁|r" or "|cffff6b6b未解锁|r",
        HasChapterModeUnlocked(chapter, 4) and "|cff4dcc4d已解锁|r" or "|cffff6b6b未解锁|r"
    ))
    table.insert(lines, string.format(
        "|cffDBA64A进入条件|r  前置章节: |cffffffff%s|r  |  角色等级: |cffffffff%d|r / 需求: |cffffffff%d|r  |  历史最高章节: |cffffffff%d|r  |  推荐状态: %s",
        prerequisiteName,
        self.state.playerLevel or 0,
        chapter.threshold or 0,
        self.state.highestChapter or 0,
        accessReady and "|cff4dcc4d可尝试进入|r" or "|cffff6b6b当前不建议进入|r"
    ))
    table.insert(lines, "|cffDBA64A联动操作|r  上方可直接进入选中章节，下方可切到装备图鉴查看本章相关装备。")

    if self.state.inRun and self.state.runChapterId == chapter.id then
        table.insert(lines, string.format(
            "|cff7edc8b当前正在该章节局内|r  模式: %s  腐化层: %d  地图ID: %d  深渊首领: %s  秘藏首领: %s",
            GetModeTypeName(self.state.runModeType),
            self.state.runCorruptionTier,
            self.state.runMapId,
            self.state.abyssBossSummoned and "已召出" or "未召出",
            self.state.cacheBossSummoned and "已召出" or "未召出"
        ))
    else
        table.insert(lines, string.format(
            "|cff9f9f9f局外概览|r  当前章节: %s  下一章节: %s  秘藏保底失败计数: %d",
            self.state.currentChapterName ~= "" and self.state.currentChapterName or "无",
            self.state.nextChapterName ~= "" and self.state.nextChapterName or "无",
            self.state.cacheBossFailCount or 0
        ))
    end

    local preview = self:GetChapterPreviewForDisplay(chapter.id)
    local categoryOrder = { "anchor", "final", "abyss", "cache" }
    for _, category in ipairs(categoryOrder) do
        table.insert(lines, " ")
        table.insert(lines, string.format("|cffDBA64A%s掉落预览|r", GetChapterBossCategoryTitle(chapter, category)))

        local rewards = preview and preview.categories and preview.categories[category] or nil
        if rewards and #rewards > 0 then
            for index, reward in ipairs(rewards) do
                if index > 5 then
                    table.insert(lines, string.format("|cff808080... 还有 %d 项未展示|r", #rewards - 5))
                    break
                end

                table.insert(lines, string.format(
                    "  |cffd8d3c7- %s|r  |cff9f9f9f[%s / %s / ilvl %d / 品质%d]|r",
                    reward.itemName ~= "" and reward.itemName or ("物品#" .. reward.itemId),
                    GetRewardDockingTypeName(reward.dockingType),
                    GetModeTypeName(reward.sourceMode),
                    reward.baseItemLevel,
                    reward.quality
                ))
            end
        else
            table.insert(lines, "  |cff666666暂无预览数据，可点击章节后等待服务端返回。|r")
        end
    end

    self.chapterDetailText:SetText(table.concat(lines, "\n"))
    local textHeight = self.chapterDetailText:GetStringHeight() or 0
    self.chapterDetailContent:SetHeight(math.max(textHeight + 20, 100))
    self.chapterDetailScroll:UpdateScrollChildRect()
end

-------------------------------------------------------
-- 标签按钮
-------------------------------------------------------
local function CreateTabButton(parent, text, x, onClick)
    local btn = CreateFrame("Button", nil, parent)
    btn:SetSize(130, 28)
    btn:SetPoint("TOPLEFT", x, -36)
    StylePanel(btn)

    btn.label = btn:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    btn.label:SetPoint("CENTER")
    btn.label:SetText(text)

    btn:EnableMouse(true)
    btn:RegisterForClicks("LeftButtonUp")
    btn:SetScript("OnClick", onClick)

    btn:SetScript("OnEnter", function(self)
        self:SetBackdropBorderColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)
    end)
    btn:SetScript("OnLeave", function(self)
        if not self.active then
            self:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])
        end
    end)

    function btn:SetActive(active)
        self.active = active
        if active then
            self:SetBackdropBorderColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)
            self.label:SetTextColor(THEME.accent[1], THEME.accent[2], THEME.accent[3], 1)
        else
            self:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])
            self.label:SetTextColor(THEME.text[1], THEME.text[2], THEME.text[3], 1)
        end
    end

    return btn
end

-------------------------------------------------------
-- 初始化
-------------------------------------------------------
function App:OnLoad(frame)
    self.frame = frame
    frame:HookScript("OnHide", function()
        App:HideRelicContextMenu()
    end)

    -- 主框架样式
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    frame:SetBackdropColor(THEME.bg[1], THEME.bg[2], THEME.bg[3], THEME.bg[4])
    frame:SetBackdropBorderColor(THEME.border[1], THEME.border[2], THEME.border[3], THEME.border[4])

    -- 标题
    self.titleText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    self.titleText:SetPoint("TOPLEFT", 16, -10)
    self.titleText:SetText("|cffDBA64A深渊修仙|r")

    -- 刷新按钮
    self.refreshBtn = CreateSmallButton(frame, "刷新", 48, function()
        App:RequestData()
    end)
    self.refreshBtn:SetPoint("TOPRIGHT", -36, -6)

    -- 标签
    self.tabEquip = CreateTabButton(frame, "专属装备", 16, function() App:ShowTab("equip") end)
    self.tabArtifact = CreateTabButton(frame, "神器图鉴", 152, function() App:ShowTab("artifact") end)
    self.tabSet = CreateTabButton(frame, "装备图鉴", 288, function() App:ShowTab("set") end)
    self.tabSuit = CreateTabButton(frame, "套装图鉴", 424, function() App:ShowTab("suit") end)
    self.tabChapter = CreateTabButton(frame, "剧情章节", 560, function() App:ShowTab("chapter") end)

    -- 内容区域
    self.contentFrame = CreateFrame("Frame", nil, frame)
    self.contentFrame:SetPoint("TOPLEFT", 8, -70)
    self.contentFrame:SetPoint("BOTTOMRIGHT", -8, 8)

    -- 装备页
    self.equipFrame = CreateFrame("Frame", nil, self.contentFrame)
    self.equipFrame:SetAllPoints()
    self:BuildEquipPage(self.equipFrame)

    self.artifactFrame = CreateFrame("Frame", nil, self.contentFrame)
    self.artifactFrame:SetAllPoints()
    self:BuildArtifactPage(self.artifactFrame)

    -- 套装页
    self.setFrame = CreateFrame("Frame", nil, self.contentFrame)
    self.setFrame:SetAllPoints()
    self:BuildEquipmentPage(self.setFrame)

    self.suitFrame = CreateFrame("Frame", nil, self.contentFrame)
    self.suitFrame:SetAllPoints()
    self:BuildSetOverviewPage(self.suitFrame)

    -- 章节页
    self.chapterFrame = CreateFrame("Frame", nil, self.contentFrame)
    self.chapterFrame:SetAllPoints()
    self:BuildChapterPage(self.chapterFrame)

    self:ShowTab("equip")
    frame:Hide()
end

function App:ShowTab(tab)
    self.activeTab = tab
    self:HideRelicContextMenu()
    self.tabEquip:SetActive(tab == "equip")
    self.tabArtifact:SetActive(tab == "artifact")
    self.tabSet:SetActive(tab == "set")
    self.tabSuit:SetActive(tab == "suit")
    self.tabChapter:SetActive(tab == "chapter")

    if tab == "equip" then
        SendAddon("REQ_STATE")
        SendAddon("REQ_RELICS")
        self.equipFrame:Show()
        self.artifactFrame:Hide()
        self.setFrame:Hide()
        self.suitFrame:Hide()
        self.chapterFrame:Hide()
        self:RefreshEquipPage()
    elseif tab == "artifact" then
        SendAddon("REQ_STATE")
        SendAddon("REQ_RELICS")
        self.equipFrame:Hide()
        self.artifactFrame:Show()
        self.setFrame:Hide()
        self.suitFrame:Hide()
        self.chapterFrame:Hide()
        self:RefreshArtifactPage()
    elseif tab == "set" then
        SendAddon("REQ_EQUIPMENTS")
        self.equipFrame:Hide()
        self.artifactFrame:Hide()
        self.setFrame:Show()
        self.suitFrame:Hide()
        self.chapterFrame:Hide()
        self:RefreshEquipmentPage()
    elseif tab == "suit" then
        SendAddon("REQ_EQUIPMENTS")
        self.equipFrame:Hide()
        self.artifactFrame:Hide()
        self.setFrame:Hide()
        self.suitFrame:Show()
        self.chapterFrame:Hide()
        self:RefreshSetOverviewPage()
    else
        SendAddon("REQ_STATE")
        SendAddon("REQ_CHAPTERS")
        SendAddon("REQ_REWARD_CURRENT")
        SendAddon("REQ_REWARD_NEXT")
        if self.selectedChapterId and self.selectedChapterId > 0 then
            SendAddon("REQ_REWARD_CHAPTER:" .. self.selectedChapterId)
        end
        self.equipFrame:Hide()
        self.artifactFrame:Hide()
        self.setFrame:Hide()
        self.suitFrame:Hide()
        self.chapterFrame:Show()
        self:RefreshChapterPage()
    end
end

function App:RequestData()
    SendAddon("REQ_ALL")
    if self.selectedChapterId and self.selectedChapterId > 0 then
        SendAddon("REQ_REWARD_CHAPTER:" .. self.selectedChapterId)
    end
end

function App:RefreshCurrent()
    if not self.frame or not self.frame:IsShown() then return end
    if self.activeTab == "equip" then
        self:RefreshEquipPage()
    elseif self.activeTab == "artifact" then
        self:RefreshArtifactPage()
    elseif self.activeTab == "set" then
        self:RefreshEquipmentPage()
    elseif self.activeTab == "suit" then
        self:RefreshSetOverviewPage()
    else
        self:RefreshChapterPage()
    end
end

function App:Toggle()
    if not self.frame then return end
    self:HideRelicContextMenu()
    if self.frame:IsShown() then
        self.frame:Hide()
    else
        self.frame:Show()
        self:RequestData()
        self:RefreshCurrent()
    end
    self:UpdateIconButtonVisibility()
end

-------------------------------------------------------
-- 图标按钮
-------------------------------------------------------
local function EnsureIconButtonConfig()
    local db = AbyssCultivationUIDB
    if not db.iconButton then db.iconButton = {} end
    local cfg = db.iconButton
    cfg.point = cfg.point or "BOTTOMLEFT"
    cfg.relativePoint = cfg.relativePoint or "BOTTOMLEFT"
    cfg.x = cfg.x or 28
    cfg.y = cfg.y or 220
    cfg.width = cfg.width or 22
    cfg.height = cfg.height or 22
    if cfg.enabled == nil then cfg.enabled = true end
    return cfg
end

local function SaveIconButtonPosition(button)
    if not button then return end
    local point, _, relativePoint, x, y = button:GetPoint()
    local cfg = EnsureIconButtonConfig()
    cfg.point = point or "BOTTOMLEFT"
    cfg.relativePoint = relativePoint or cfg.point
    cfg.x = x or 28
    cfg.y = y or 220
end

function App:UpdateIconButtonVisibility()
    if not self.iconButton then return end
    local cfg = EnsureIconButtonConfig()
    if self.frame and self.frame:IsShown() then
        self.iconButton:Hide()
    elseif cfg.enabled then
        self.iconButton:Show()
    else
        self.iconButton:Hide()
    end
end

function App:CreateIconButton()
    if self.iconButton then return end

    local cfg = EnsureIconButtonConfig()

    local btn = CreateFrame("Button", "AbyssCultivationUIIconButton", UIParent)
    btn:SetFrameStrata("MEDIUM")
    btn:SetFrameLevel(10)
    btn:SetMovable(true)
    btn:EnableMouse(true)
    btn:RegisterForDrag("LeftButton")
    btn:RegisterForClicks("LeftButtonUp")
    btn:SetClampedToScreen(true)
    btn:SetSize(cfg.width, cfg.height)
    btn:ClearAllPoints()
    btn:SetPoint(cfg.point, UIParent, cfg.relativePoint, cfg.x, cfg.y)

    -- 边框光环
    local border = btn:CreateTexture(nil, "BACKGROUND")
    border:SetAllPoints()
    border:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    border:SetBlendMode("ADD")
    border:SetVertexColor(0.60, 0.28, 0.95, 0.90)

    -- 主图标
    local icon = btn:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 3, -3)
    icon:SetPoint("BOTTOMRIGHT", -3, 3)
    icon:SetTexture("Interface\\Icons\\Spell_Shadow_DeathCoil")
    btn.texture = icon

    -- 外发光
    local glow = btn:CreateTexture(nil, "OVERLAY")
    glow:SetPoint("CENTER")
    glow:SetSize(48, 48)
    glow:SetTexture("Interface\\Buttons\\CheckButtonGlow")
    glow:SetBlendMode("ADD")
    glow:SetAlpha(0.35)

    -- 文字标签
    local label = btn:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    label:SetPoint("LEFT", btn, "RIGHT", 4, 0)
    label:SetText("|cffDBA64A深渊修仙|r")

    -- 拖拽
    btn:SetScript("OnDragStart", function(self)
        iconButtonDragging = true
        iconButtonMouseDownAt = GetTime()
        self:StartMoving()
        GameTooltip:Hide()
    end)

    btn:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        self:SetUserPlaced(false)
        iconButtonDragging = false
        SaveIconButtonPosition(self)
    end)

    btn:SetScript("OnMouseDown", function()
        iconButtonMouseDownAt = GetTime()
    end)

    -- 点击
    btn:SetScript("OnClick", function(self, button)
        if button == "LeftButton" and not iconButtonDragging and (GetTime() - iconButtonMouseDownAt) < 0.35 then
            App:Toggle()
        end
    end)

    -- 悬停提示
    btn:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText("深渊修仙", 0.86, 0.66, 0.16)
        GameTooltip:AddLine("点击打开主界面", 1, 1, 1)
        GameTooltip:AddLine("拖动移动图标位置", 0.7, 0.7, 0.7)
        GameTooltip:Show()
    end)

    btn:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)

    self.iconButton = btn
    self:UpdateIconButtonVisibility()
end

-------------------------------------------------------
-- 消息处理
-------------------------------------------------------
local chunkBuffer = nil

local function HandleMessage(message)
    local cmd, payload = string.match(message, "^([^:]+):?(.*)$")
    if not cmd then return end

    if cmd == "STATE" then
        ParseState(payload)
    elseif cmd == "RELICS" then
        ParseRelics(payload)
        if App.pendingRelicActivation then
            local pending = App.pendingRelicActivation
            local relic = App.relicMap[pending.itemId]
            if relic and relic.activeSlot == pending.slotIdx then
                NotifyMessage(string.format("【%s】已成功激活到%s。", relic.name or ("#" .. pending.itemId), GetSlotName(pending.slotIdx)))
                App.pendingRelicActivation = nil
            end
        end
    elseif cmd == "EQUIPMENTS" then
        ParseEquipments(payload)
    elseif cmd == "SET_BONUSES" then
        ParseSetBonuses(payload)
    elseif cmd == "CHAPTERS" then
        ParseChapters(payload)
        if App.activeTab == "chapter" and App.selectedChapterId and App.selectedChapterId > 0 then
            App:RequestSelectedChapterPreview()
        end
    elseif cmd == "MODE_PROMPT" then
        ParseModePrompt(payload)
    elseif string.find(cmd, "^REWARD_") then
        local scope, category = string.match(cmd, "^REWARD_([A-Z]+)_([A-Z]+)$")
        if scope and category then
            ParseRewardPreview(string.lower(scope), string.lower(category), payload)
        else
            return
        end
    elseif cmd == "RESULT" then
        local f = Split(payload, "%^")
        local action = f[1] or ""
        local success = ToNumber(f[2]) == 1
        local resultMessage = f[3] or ""
        local displayMessage = FormatResultMessage(action, success, resultMessage)
        if displayMessage then
            NotifyMessage(displayMessage)
        end
        if action == "ENTER" and success and App.HideModePrompt then
            App:HideModePrompt()
        end
        if action == "LEAVE" and App.HideModePrompt then
            App:HideModePrompt()
        end
        if action == "ENTER" then
            local chapterName = GetChapterNameById(App.selectedChapterId or 0)
            local modeName = GetModeTypeName(App.chapterEnterMode or 1)
            App.chapterLastActionText = string.format(
                "最近请求：进入 [%s]  模式[%s]  状态[%s]",
                chapterName,
                modeName,
                success and "成功" or ("失败: " .. (displayMessage or resultMessage or "unknown"))
            )
        elseif action == "LEAVE" then
            App.chapterLastActionText = success and "最近请求：退出当前深渊流程  状态[成功]" or ("最近请求：退出当前深渊流程  状态[失败: " .. (displayMessage or resultMessage or "unknown") .. "]")
        end
        if action == "COLLECT_RELIC" then
            if success and App.pendingRelicCollection then
                local pending = App.pendingRelicCollection
                App.pendingRelicCollection = nil
                local relic = App.relicMap[pending.itemId]
                local slotDef = pending.autoActivate and SLOTS[pending.slotIdx] or nil
                if slotDef then
                    NotifyMessage(string.format("正在尝试将【%s】激活到%s...", (relic and relic.name) or ("#" .. pending.itemId), GetSlotName(pending.slotIdx)))
                    SendAddon(string.format("SET_RELIC:%s %d", slotDef.cmd, pending.itemId))
                    SendAddon("REQ_STATE")
                    SendAddon("REQ_RELICS")
                else
                    App.pendingRelicActivation = nil
                    SendAddon("REQ_RELICS")
                end
            elseif not success then
                App.pendingRelicCollection = nil
                App.pendingRelicActivation = nil
            end
        end
        if action == "SET_RELIC" and not success then
            App.pendingRelicActivation = nil
        end
    else
        return
    end

    App:RefreshCurrent()
end

-------------------------------------------------------
-- 事件
-------------------------------------------------------
local eventFrame = CreateFrame("Frame")
eventFrame:RegisterEvent("PLAYER_LOGIN")
eventFrame:RegisterEvent("CHAT_MSG_ADDON")
eventFrame:RegisterEvent("GET_ITEM_INFO_RECEIVED")
eventFrame:SetScript("OnEvent", function(_, event, ...)
    if event == "PLAYER_LOGIN" then
        if RegisterAddonMessagePrefix then
            RegisterAddonMessagePrefix(ADDON_PREFIX)
        elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
            C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
        end
        App:CreateModePromptFrame()
        App:CreateIconButton()
        return
    end

    if event == "GET_ITEM_INFO_RECEIVED" then
        local itemId, ok = ...
        if not itemId or itemId <= 0 or not App.pendingItemInfo[itemId] then
            return
        end

        if ok then
            local _, _, _, _, _, _, _, _, _, itemIcon = GetItemInfo(itemId)
            if itemIcon and itemIcon ~= "Interface\\Icons\\INV_Misc_QuestionMark" then
                CacheItemIcon(itemId, itemIcon)
            end
        end

        App.pendingItemInfo[itemId] = nil

        if App.frame and App.frame:IsShown() and not App.itemInfoRefreshQueued then
            App.itemInfoRefreshQueued = true
            local refresh = function()
                App.itemInfoRefreshQueued = false
                if App.frame and App.frame:IsShown() then
                    App:RefreshCurrent()
                end
            end

            if C_Timer and C_Timer.After then
                C_Timer.After(0.05, refresh)
            else
                refresh()
            end
        end
        return
    end

    local prefix, message, channel, sender = ...
    if prefix ~= ADDON_PREFIX or channel ~= "WHISPER" then return end
    if sender and sender ~= UnitName("player") then return end

    -- 分块重组
    if string.sub(message, 1, 6) == "CHUNK:" then
        local cur, total, chunk = string.match(message, "^CHUNK:(%d+):(%d+):(.*)$")
        cur = ToNumber(cur)
        total = ToNumber(total)
        if cur == 1 then
            chunkBuffer = { total = total, parts = {} }
        end
        if chunkBuffer then
            chunkBuffer.parts[cur] = chunk
            local ready = true
            for i = 1, chunkBuffer.total do
                if not chunkBuffer.parts[i] then ready = false; break end
            end
            if ready then
                local full = table.concat(chunkBuffer.parts, "")
                chunkBuffer = nil
                HandleMessage(full)
            end
        end
        return
    end

    HandleMessage(message)
end)

-------------------------------------------------------
-- 斜杠命令
-------------------------------------------------------
SLASH_ABYSSCULTIVATIONUI1 = "/abyssui"
SLASH_ABYSSCULTIVATIONUI2 = "/abysscult"
SlashCmdList.ABYSSCULTIVATIONUI = function()
    App:Toggle()
end
