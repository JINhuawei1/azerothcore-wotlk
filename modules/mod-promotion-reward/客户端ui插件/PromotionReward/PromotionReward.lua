--[[
  宣传奖励 客户端 UI 插件 (暗黑金风格)
  风格参考: mod-plugin-manager/测试客户端ui插件/RedemptionCodeUI
  配合 mod-promotion-reward 服务端模块使用

  通信协议 (Addon Message, prefix=PROMOREWARD):
    Client -> Server:  REQ_INFO | REQ_CODES | REDEEM:<CDK>
    Server -> Client:  INFO:days|attr|weapon|base|perDay|claimed|nextWeapon|level|nextLevel|nextAttr|minDmg|maxDmg|nextMinDmg|nextMaxDmg
                       CODES:CDK1,CDK2,...
                       CODES_END
                       OPEN
                       REDEEM_OK:CDK | REDEEM_FAIL:CDK:reason
]]--

---@diagnostic disable: undefined-global, lowercase-global, unused-local

PromotionRewardUI = {}

local ADDON_NAME   = "PromotionReward"
local ADDON_PREFIX = "PROMOREWARD"
local PROMO_WEAPON_ENTRY_BASE = 997000
local PROMO_WEAPON_MAX_LEVEL  = 1000

PromotionRewardDB = PromotionRewardDB or {}

-- ============================================================
-- 状态
-- ============================================================

PromotionRewardUI.state = {
    days        = 0,
    totalAttr   = 0,
    weaponEntry = 0,
    nextWeaponEntry = 997001,
    weaponLevel = 0,
    nextWeaponLevel = 1,
    nextAttr = 1999,
    minDmg = 0,
    maxDmg = 0,
    nextMinDmg = 1999,
    nextMaxDmg = 2999,
    baseAttr    = 1999,
    perDay      = 1000,
    weaponHeld  = false,
    codes       = {},
    pendingCode = nil,
}

-- ============================================================
-- 工具
-- ============================================================

local function FormatNum(n)
    n = tonumber(n) or 0
    if n >= 10000 then
        return string.format("%.1fW", n / 10000)
    end
    return tostring(n)
end

local function ClampWeaponLevel(level)
    level = tonumber(level) or 0
    if level < 1 then return 0 end
    if level > PROMO_WEAPON_MAX_LEVEL then return PROMO_WEAPON_MAX_LEVEL end
    return level
end

local function WeaponEntryForLevel(level)
    level = ClampWeaponLevel(level)
    if level == 0 then return 0 end
    return PROMO_WEAPON_ENTRY_BASE + level
end

local function WeaponLevelFromEntry(entry)
    entry = tonumber(entry) or 0
    local level = entry - PROMO_WEAPON_ENTRY_BASE
    if level >= 1 and level <= PROMO_WEAPON_MAX_LEVEL then
        return level
    end
    return nil
end

local function AttrForLevel(level)
    level = ClampWeaponLevel(level)
    if level == 0 then return 0 end
    local s = PromotionRewardUI.state or {}
    return (tonumber(s.baseAttr) or 1999) + (level - 1) * (tonumber(s.perDay) or 1000)
end

local function DamageForLevel(level)
    local attr = AttrForLevel(level)
    local s = PromotionRewardUI.state or {}
    return attr, attr + (tonumber(s.perDay) or 1000)
end

local function WeaponName(entry, level)
    level = ClampWeaponLevel(level or WeaponLevelFromEntry(entry))
    if level > 0 then
        return "宣传神器" .. level
    end
    return "宣传神器"
end

local function BuildItemHyperlink(entry)
    entry = tonumber(entry) or 0
    if entry <= 0 then return nil end
    return "item:" .. entry .. ":0:0:0:0:0:0:0:0"
end

local function SendAddon(msg)
    local me = UnitName("player")
    if not me then return end
    if SendAddonMessage then
        SendAddonMessage(ADDON_PREFIX, msg, "WHISPER", me)
    elseif C_ChatInfo and C_ChatInfo.SendAddonMessage then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, msg, "WHISPER", me)
    end
end

-- ============================================================
-- 初始化 (XML OnLoad 调用)
-- ============================================================

function PromotionRewardUI:OnLoad()
    if PromotionRewardFrame then
        PromotionRewardFrame:RegisterForDrag("LeftButton")
        PromotionRewardFrame:Hide()
    end

    -- 注册事件
    PromotionRewardFrame:RegisterEvent("ADDON_LOADED")
    PromotionRewardFrame:RegisterEvent("PLAYER_LOGIN")
    PromotionRewardFrame:RegisterEvent("CHAT_MSG_ADDON")
    PromotionRewardFrame:SetScript("OnEvent", function(frame, event, ...)
        PromotionRewardUI:OnEvent(event, ...)
    end)

    -- 注册 Addon Message 前缀
    if C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
        pcall(C_ChatInfo.RegisterAddonMessagePrefix, ADDON_PREFIX)
    elseif RegisterAddonMessagePrefix then
        pcall(RegisterAddonMessagePrefix, ADDON_PREFIX)
    end

    -- 数据库
    if not PromotionRewardDB then PromotionRewardDB = {} end
    if not PromotionRewardDB.position then
        PromotionRewardDB.position = { point = "CENTER", x = 0, y = 0 }
    end
    if not PromotionRewardDB.iconPosition then
        PromotionRewardDB.iconPosition = { point = "TOP", x = -120, y = -10 }
    end

    self:RestorePosition()
    self:BindWeaponTooltips()
end

function PromotionRewardUI:OnShow()
    self:RestorePosition()
    self:RequestRefresh()
end

function PromotionRewardUI:OnEvent(event, ...)
    if event == "ADDON_LOADED" then
        local addonName = ...
        if addonName == ADDON_NAME then
            self:Initialize()
        end
    elseif event == "PLAYER_LOGIN" then
        -- 等服务器主动 push
    elseif event == "CHAT_MSG_ADDON" then
        local prefix, message, channel, sender = ...
        if not message then return end
        local fullMsg = message
        if prefix == ADDON_PREFIX and not message:find("\t") then
            fullMsg = prefix .. "\t" .. message
        elseif not message:find("\t") then
            return
        end
        self:HandleAddonMessage(fullMsg)
    end
end

function PromotionRewardUI:Initialize()
    SLASH_PROMOREWARD1 = "/promo"
    SLASH_PROMOREWARD2 = "/xc"
    SLASH_PROMOREWARD3 = "/宣传"
    SlashCmdList["PROMOREWARD"] = function(msg)
        msg = string.lower(strtrim(msg or ""))
        if msg == "hide" or msg == "隐藏" then
            self:Hide()
        else
            self:Toggle()
        end
    end
end

-- ============================================================
-- 显示控制
-- ============================================================

function PromotionRewardUI:Show()
    if PromotionRewardFrame then
        if not tContains(UISpecialFrames, "PromotionRewardFrame") then
            table.insert(UISpecialFrames, "PromotionRewardFrame")
        end
        PromotionRewardFrame:Show()
        self:RequestRefresh()
    end
end

function PromotionRewardUI:Hide()
    if PromotionRewardFrame then
        self:SavePosition()
        PromotionRewardFrame:Hide()
    end
end

function PromotionRewardUI:Toggle()
    if PromotionRewardFrame then
        if PromotionRewardFrame:IsShown() then
            self:Hide()
        else
            self:Show()
        end
    end
end

-- ============================================================
-- 请求 / 响应
-- ============================================================

function PromotionRewardUI:RequestRefresh()
    SendAddon("REQ_INFO")
    SendAddon("REQ_CODES")
end

function PromotionRewardUI:RedeemFromInput()
    local editBox = PromotionRewardFrameInputPanelCodeInput
    if not editBox then return end
    local code = strtrim(editBox:GetText() or ""):upper():gsub("%s+", "")
    if code == "" then
        self:ShowStatus("|cffff5555请输入兑换码|r")
        return
    end
    self:RequestRedeem(code)
    editBox:SetText("")
end

function PromotionRewardUI:RequestRedeem(code)
    if not code or code == "" then return end
    self.state.pendingCode = code
    self:ShowStatus("|cffffff00正在兑换 " .. code .. "...|r")
    SendAddon("REDEEM:" .. code)
end

-- ============================================================
-- 协议解析
-- ============================================================

local CodesAccum = nil

function PromotionRewardUI:HandleAddonMessage(msg)
    local tab = msg:find("\t")
    if not tab then return end
    local prefix = msg:sub(1, tab - 1)
    if prefix ~= ADDON_PREFIX then return end

    local payload = msg:sub(tab + 1)

    if payload:sub(1, 5) == "INFO:" then
        self:ParseInfo(payload:sub(6))
    elseif payload:sub(1, 6) == "CODES:" then
        self:ParseCodes(payload:sub(7))
    elseif payload == "CODES_END" then
        self:FlushCodes()
    elseif payload == "OPEN" then
        self:Show()
    elseif payload:sub(1, 10) == "REDEEM_OK:" then
        local code = payload:sub(11)
        self.state.pendingCode = nil
        self:ShowStatus("|cff00ff00兑换成功: " .. code .. "|r")
        for i = #self.state.codes, 1, -1 do
            if self.state.codes[i] == code then
                table.remove(self.state.codes, i)
            end
        end
        self:RenderCodes()
        self:RequestRefresh()
    elseif payload:sub(1, 12) == "REDEEM_FAIL:" then
        local rest = payload:sub(13)
        local sep = rest:find(":")
        local code = sep and rest:sub(1, sep - 1) or rest
        local reason = sep and rest:sub(sep + 1) or "未知错误"
        self.state.pendingCode = nil
        self:ShowStatus("|cffff5555兑换失败 [" .. code .. "]: " .. reason .. "|r")
        self:RenderCodes()
    end
end

function PromotionRewardUI:ParseInfo(payload)
    local parts = {}
    for v in string.gmatch(payload, "([^|]+)") do
        table.insert(parts, v)
    end
    self.state.days        = tonumber(parts[1]) or 0
    self.state.totalAttr   = tonumber(parts[2]) or 0
    self.state.weaponEntry = tonumber(parts[3]) or 0
    self.state.baseAttr    = tonumber(parts[4]) or 1999
    self.state.perDay      = tonumber(parts[5]) or 1000
    self.state.weaponHeld  = (parts[6] == "1")

    local fallbackLevel = ClampWeaponLevel(self.state.days)
    local parsedLevel = tonumber(parts[8]) or WeaponLevelFromEntry(self.state.weaponEntry) or fallbackLevel
    self.state.weaponLevel = ClampWeaponLevel(parsedLevel)
    if self.state.weaponEntry == 0 and self.state.weaponLevel > 0 then
        self.state.weaponEntry = WeaponEntryForLevel(self.state.weaponLevel)
    end

    local fallbackNextLevel = (self.state.days >= PROMO_WEAPON_MAX_LEVEL) and PROMO_WEAPON_MAX_LEVEL or (self.state.days + 1)
    self.state.nextWeaponLevel = ClampWeaponLevel(tonumber(parts[9]) or fallbackNextLevel)
    if self.state.nextWeaponLevel == 0 then
        self.state.nextWeaponLevel = 1
    end
    self.state.nextWeaponEntry = tonumber(parts[7]) or WeaponEntryForLevel(self.state.nextWeaponLevel)
    self.state.nextAttr = tonumber(parts[10]) or AttrForLevel(self.state.nextWeaponLevel)

    local minDmg, maxDmg = DamageForLevel(self.state.weaponLevel)
    self.state.minDmg = tonumber(parts[11]) or minDmg
    self.state.maxDmg = tonumber(parts[12]) or maxDmg

    local nextMinDmg, nextMaxDmg = DamageForLevel(self.state.nextWeaponLevel)
    self.state.nextMinDmg = tonumber(parts[13]) or nextMinDmg
    self.state.nextMaxDmg = tonumber(parts[14]) or nextMaxDmg

    self:RenderInfo()
end

function PromotionRewardUI:ParseCodes(payload)
    if not CodesAccum then CodesAccum = {} end
    if payload and payload ~= "" then
        for code in string.gmatch(payload, "([^,]+)") do
            table.insert(CodesAccum, code)
        end
    end
end

function PromotionRewardUI:FlushCodes()
    self.state.codes = CodesAccum or {}
    CodesAccum = nil
    self:RenderCodes()
end

-- ============================================================
-- 渲染
-- ============================================================

function PromotionRewardUI:RenderInfo()
    local s = self.state
    self:BindWeaponTooltips()

    -- 累计天数 (大数字 + 灰色单位)
    if PromotionRewardFrameDaysPanelValue then
        PromotionRewardFrameDaysPanelValue:SetText(s.days .. " |cffaaaaaa天|r")
    end

    -- 武器加成大数字
    if PromotionRewardFrameAttrPanelValue then
        PromotionRewardFrameAttrPanelValue:SetText("+" .. FormatNum(s.totalAttr))
        if s.weaponHeld then
            PromotionRewardFrameAttrPanelValue:SetTextColor(0.90, 0.60, 1.0)
        else
            PromotionRewardFrameAttrPanelValue:SetTextColor(0.55, 0.50, 0.55)
        end
    end

    -- "全属性" 单位文字随状态变色
    if PromotionRewardFrameAttrPanelUnit then
        if s.weaponLevel and s.weaponLevel > 0 then
            PromotionRewardFrameAttrPanelUnit:SetText(WeaponName(s.weaponEntry, s.weaponLevel))
        else
            PromotionRewardFrameAttrPanelUnit:SetText("未获得武器")
        end
        if s.weaponHeld then
            PromotionRewardFrameAttrPanelUnit:SetTextColor(0.72, 0.52, 0.88)
        else
            PromotionRewardFrameAttrPanelUnit:SetTextColor(0.50, 0.45, 0.50)
        end
    end

    -- 武器是否在身
    if PromotionRewardFrameAttrPanelHeld then
        if s.weaponHeld then
            PromotionRewardFrameAttrPanelHeld:SetText("● 武器在身 · 属性已生效")
            PromotionRewardFrameAttrPanelHeld:SetTextColor(0.30, 1.0, 0.40)
        else
            PromotionRewardFrameAttrPanelHeld:SetText("○ 武器不在身 · 属性未生效")
            PromotionRewardFrameAttrPanelHeld:SetTextColor(1.0, 0.32, 0.32)
        end
    end

    -- 下次奖励进度条
    local nextAttr = s.nextAttr or ((s.days == 0) and s.baseAttr or (s.totalAttr + s.perDay))
    if PromotionRewardFrameNextPanelLabel then
        if s.days == 0 then
            PromotionRewardFrameNextPanelLabel:SetText("首次宣传可得：")
        elseif s.days >= PROMO_WEAPON_MAX_LEVEL then
            PromotionRewardFrameNextPanelLabel:SetText("已达最高等级：")
        else
            PromotionRewardFrameNextPanelLabel:SetText("再坚持 1 天可得：")
        end
    end
    if PromotionRewardFrameNextPanelText then
        PromotionRewardFrameNextPanelText:SetText(WeaponName(s.nextWeaponEntry, s.nextWeaponLevel) .. "  +" .. FormatNum(nextAttr) .. " 全属性")
    end

    -- 公式
    if PromotionRewardFrameFormulaPanelText then
        PromotionRewardFrameFormulaPanelText:SetText(
            "公式: 初始 " .. s.baseAttr .. " + (天数-1) × " .. s.perDay)
    end
end

function PromotionRewardUI:ShowWeaponTooltip(frame, entry, level, minDmg, maxDmg)
    if not frame or not GameTooltip then return end

    entry = tonumber(entry) or 0
    level = ClampWeaponLevel(level or WeaponLevelFromEntry(entry))
    if entry <= 0 and level > 0 then
        entry = WeaponEntryForLevel(level)
    end
    if entry <= 0 then return end

    GameTooltip:SetOwner(frame, "ANCHOR_RIGHT")
    GameTooltip:ClearLines()

    local link = BuildItemHyperlink(entry)
    local itemName = GetItemInfo and GetItemInfo(entry)
    if link and itemName then
        local ok = pcall(function()
            GameTooltip:SetHyperlink(link)
        end)
        if ok then
            GameTooltip:Show()
            return
        end
        GameTooltip:ClearLines()
    end

    if level == 0 then
        level = WeaponLevelFromEntry(entry) or 1
    end

    local attr = AttrForLevel(level)
    local fallbackMin, fallbackMax = DamageForLevel(level)
    minDmg = tonumber(minDmg) or fallbackMin
    maxDmg = tonumber(maxDmg) or fallbackMax

    GameTooltip:SetText(WeaponName(entry, level), 0.95, 0.45, 1.0)
    GameTooltip:AddLine("物品ID: " .. entry .. "    等级: " .. level, 0.70, 0.70, 0.70, true)
    GameTooltip:AddLine("伤害: " .. FormatNum(minDmg) .. " - " .. FormatNum(maxDmg), 1.0, 0.82, 0.30, true)
    GameTooltip:AddLine(" ")
    GameTooltip:AddLine("力量 +" .. FormatNum(attr) .. "    敏捷 +" .. FormatNum(attr), 0.95, 0.95, 0.95, true)
    GameTooltip:AddLine("智力 +" .. FormatNum(attr) .. "    精神 +" .. FormatNum(attr), 0.95, 0.95, 0.95, true)
    GameTooltip:AddLine("命中 +" .. FormatNum(attr) .. "    暴击 +" .. FormatNum(attr), 0.95, 0.95, 0.95, true)
    GameTooltip:AddLine("急速 +" .. FormatNum(attr) .. "    攻强 +" .. FormatNum(attr), 0.95, 0.95, 0.95, true)
    GameTooltip:AddLine("法强 +" .. FormatNum(attr) .. "    法术穿透 +" .. FormatNum(attr), 0.95, 0.95, 0.95, true)
    GameTooltip:Show()
end

function PromotionRewardUI:HideWeaponTooltip()
    if GameTooltip then
        GameTooltip:Hide()
    end
end

function PromotionRewardUI:BindWeaponTooltips()
    local attrPanel = PromotionRewardFrameAttrPanel
    if attrPanel and not attrPanel._promoWeaponTooltipBound then
        attrPanel:EnableMouse(true)
        attrPanel:SetScript("OnEnter", function(frame)
            local s = PromotionRewardUI.state
            local entry = s.weaponEntry
            local level = s.weaponLevel
            local minDmg = s.minDmg
            local maxDmg = s.maxDmg
            if (not entry or entry == 0) and s.nextWeaponEntry then
                entry = s.nextWeaponEntry
                level = s.nextWeaponLevel
                minDmg = s.nextMinDmg
                maxDmg = s.nextMaxDmg
            end
            PromotionRewardUI:ShowWeaponTooltip(frame, entry, level, minDmg, maxDmg)
        end)
        attrPanel:SetScript("OnLeave", function()
            PromotionRewardUI:HideWeaponTooltip()
        end)
        attrPanel._promoWeaponTooltipBound = true
    end

    local nextPanel = PromotionRewardFrameNextPanel
    if nextPanel and not nextPanel._promoWeaponTooltipBound then
        nextPanel:EnableMouse(true)
        nextPanel:SetScript("OnEnter", function(frame)
            local s = PromotionRewardUI.state
            PromotionRewardUI:ShowWeaponTooltip(frame, s.nextWeaponEntry, s.nextWeaponLevel, s.nextMinDmg, s.nextMaxDmg)
        end)
        nextPanel:SetScript("OnLeave", function()
            PromotionRewardUI:HideWeaponTooltip()
        end)
        nextPanel._promoWeaponTooltipBound = true
    end
end

local CodeRows = {}
local ROW_H = 30

local function GetOrCreateRow(idx)
    local row = CodeRows[idx]
    if row then return row end

    local parent = PromotionRewardFrameListPanelScrollScrollChild
    if not parent then return nil end

    row = CreateFrame("Frame", "PromoCodeRow" .. idx, parent, "PromoCodeRowTemplate")
    row:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, -((idx - 1) * ROW_H))
    row.isOdd = (idx % 2 == 1)
    if row.isOdd then
        row:SetBackdropColor(0.12, 0.08, 0.04, 0.55)
    else
        row:SetBackdropColor(0.08, 0.05, 0.02, 0.55)
    end

    CodeRows[idx] = row
    return row
end

function PromotionRewardUI:RenderCodes()
    local codes = self.state.codes or {}
    local n = #codes

    for i = 1, math.max(n, #CodeRows) do
        local row = CodeRows[i]
        if i <= n then
            row = GetOrCreateRow(i)
            if row then
                local code = codes[i]
                local codeText = getglobal(row:GetName() .. "Code")
                if codeText then codeText:SetText(code) end

                local btn = getglobal(row:GetName() .. "Btn")
                if btn then
                    local btnText = getglobal(btn:GetName() .. "Text")
                    if self.state.pendingCode == code then
                        btn:Disable()
                        if btnText then btnText:SetText("...") end
                    else
                        btn:Enable()
                        if btnText then btnText:SetText("兑 换") end
                    end
                    btn:SetScript("OnClick", function()
                        PromotionRewardUI:RequestRedeem(code)
                    end)
                end
                row:Show()
            end
        elseif row then
            row:Hide()
        end
    end

    local child = PromotionRewardFrameListPanelScrollScrollChild
    if child then
        child:SetHeight(math.max(1, n * ROW_H))
    end

    -- 列表头部计数徽章
    if PromotionRewardFrameListPanelCount then
        PromotionRewardFrameListPanelCount:SetText(n .. " 张")
        if n == 0 then
            PromotionRewardFrameListPanelCount:SetTextColor(0.55, 0.48, 0.30)
        else
            PromotionRewardFrameListPanelCount:SetTextColor(0.95, 0.78, 0.35)
        end
    end

    -- 空状态提示
    if PromotionRewardFrameListPanelEmpty then
        if n == 0 then
            PromotionRewardFrameListPanelEmpty:Show()
        else
            PromotionRewardFrameListPanelEmpty:Hide()
        end
    end

    if n == 0 then
        self:ShowStatus("|cff888888暂无可用兑换码|r")
    else
        self:ShowStatus("|cff888888共 " .. n .. " 张可兑换|r")
    end
end

function PromotionRewardUI:ShowStatus(text)
    local statusText = PromotionRewardFrameButtonPanelStatus
    if statusText then
        statusText:SetText(text or "")
    end
end

-- ============================================================
-- 位置保存 / 恢复
-- ============================================================

function PromotionRewardUI:SavePosition()
    if not PromotionRewardFrame then return end
    local point, _, _, x, y = PromotionRewardFrame:GetPoint()
    if point and x and y then
        PromotionRewardDB.position = { point = point, x = x, y = y }
    end
end

function PromotionRewardUI:RestorePosition()
    if not PromotionRewardFrame then return end
    local pos = PromotionRewardDB and PromotionRewardDB.position
    if pos and pos.point and pos.x and pos.y then
        PromotionRewardFrame:ClearAllPoints()
        PromotionRewardFrame:SetPoint(pos.point, UIParent, pos.point, pos.x, pos.y)
    end
end

-- ============================================================
-- 图标按钮
-- ============================================================

function PromotionRewardUI:InitializeIconButton()
    if PromotionRewardIconFrame then
        PromotionRewardIconFrame:SetMovable(true)
        PromotionRewardIconFrame:EnableMouse(false)
        self:RestoreIconPosition()
    end
    if PromotionRewardIconFrameIconButton then
        PromotionRewardIconFrameIconButton:SetMovable(true)
        PromotionRewardIconFrameIconButton:EnableMouse(true)
        PromotionRewardIconFrameIconButton:RegisterForDrag("LeftButton")
    end
end

function PromotionRewardUI:SaveIconPosition()
    if not PromotionRewardIconFrame then return end
    local point, _, _, x, y = PromotionRewardIconFrame:GetPoint()
    if point and x and y then
        PromotionRewardDB.iconPosition = { point = point, x = x, y = y }
    end
end

function PromotionRewardUI:RestoreIconPosition()
    if not PromotionRewardIconFrame then return end
    local pos = PromotionRewardDB and PromotionRewardDB.iconPosition
    if pos and pos.point and pos.x and pos.y then
        PromotionRewardIconFrame:ClearAllPoints()
        PromotionRewardIconFrame:SetPoint(pos.point, UIParent, pos.point, pos.x, pos.y)
    end
end
