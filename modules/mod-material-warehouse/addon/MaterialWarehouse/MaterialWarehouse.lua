local PREFIX = "MATWH"
local ADDON = CreateFrame("Frame", "MaterialWarehouseFrame", UIParent)
local items = {}
local cards = {}
local chunkBuffer = {}
local iconButton = nil
local iconDragging = false
local withdrawDrag = nil
local withdrawGhost = nil
local suppressIconClickUntil = 0
local RefreshCards
local GRID_COLUMNS = 10
local CARD_WIDTH = 98
local CARD_HEIGHT = 112
local CARD_GAP = 8
local ICON_SIZE = 32
local LAYER_LOW = 2
local LAYER_HIGH = 80

local qualityColors = {
    [0] = {0.62, 0.62, 0.62},
    [1] = {1.00, 1.00, 1.00},
    [2] = {0.12, 1.00, 0.00},
    [3] = {0.00, 0.44, 0.87},
    [4] = {0.64, 0.21, 0.93},
    [5] = {1.00, 0.50, 0.00},
    [6] = {0.90, 0.80, 0.50},
    [7] = {0.90, 0.80, 0.50},
}

local function SetWidgetSize(widget, width, height)
    widget:SetWidth(width)
    widget:SetHeight(height)
end

local function SetWidgetShown(widget, shown)
    if shown then
        widget:Show()
    else
        widget:Hide()
    end
end

local function ShortText(text, maxChars)
    text = text or ""
    maxChars = maxChars or 10

    local length = 0
    local bytePos = 1
    local textBytes = string.len(text)
    while bytePos <= textBytes do
        if length >= maxChars then
            return string.sub(text, 1, bytePos - 1) .. ".."
        end

        local byte = string.byte(text, bytePos)
        if not byte then
            break
        elseif byte < 0x80 then
            bytePos = bytePos + 1
        elseif byte < 0xE0 then
            bytePos = bytePos + 2
        elseif byte < 0xF0 then
            bytePos = bytePos + 3
        else
            bytePos = bytePos + 4
        end

        length = length + 1
    end

    if length <= maxChars then
        return text
    end

    return text
end

local function NormalizeSearchText(text)
    text = text or ""
    text = string.gsub(text, "^%s+", "")
    text = string.gsub(text, "%s+$", "")
    return string.lower(text)
end

local function ItemMatchesSearch(data, query)
    if not query or query == "" then
        return true
    end

    local name = NormalizeSearchText(data.name)
    local itemId = tostring(data.itemId or "")
    return string.find(name, query, 1, true) ~= nil or string.find(itemId, query, 1, true) ~= nil
end

local function Split(text, delimiter)
    local result = {}
    local pattern = "([^" .. delimiter .. "]*)"
    local start = 1
    while start <= string.len(text) + 1 do
        local pos = string.find(text, delimiter, start, true)
        if pos then
            table.insert(result, string.sub(text, start, pos - 1))
            start = pos + 1
        else
            table.insert(result, string.sub(text, start))
            break
        end
    end
    return result
end

local function Send(payload)
    if not payload or payload == "" then
        return false
    end

    local command = nil
    if payload == "OPEN" or payload == "REQ" then
        command = ".材料仓库 列表"
    elseif payload == "STORE_ALL" or payload == "DEPOSIT_ALL" then
        command = ".材料仓库 存储 all"
    else
        local fields = Split(payload, ":")
        local action = fields[1]
        local itemId = tonumber(fields[2] or "0") or 0
        local count = fields[3]

        if itemId > 0 then
            if action == "ADD" then
                command = ".材料仓库 添加 " .. itemId
            elseif action == "STORE" or action == "DEPOSIT" then
                command = ".材料仓库 存储 " .. itemId
                if count and count ~= "" and count ~= "0" then
                    command = command .. " " .. count
                end
            elseif action == "WITHDRAW" or action == "EXTRACT" then
                command = ".材料仓库 提取 " .. itemId
                if count and count ~= "" and count ~= "0" then
                    command = command .. " " .. count
                end
            elseif action == "REMOVE" then
                command = ".材料仓库 删除 " .. itemId
            elseif action == "AUTO" then
                command = ".材料仓库 自动 " .. itemId .. " " .. (fields[3] == "1" and "1" or "0")
            end
        end
    end

    if command and SendChatMessage then
        SendChatMessage(command, "SAY")
        return true
    end

    local playerName = UnitName("player")
    if playerName and playerName ~= "" then
        if C_ChatInfo and C_ChatInfo.SendAddonMessage then
            C_ChatInfo.SendAddonMessage(PREFIX, payload, "WHISPER", playerName)
            return true
        end

        if SendAddonMessage then
            SendAddonMessage(PREFIX, payload, "WHISPER", playerName)
            return true
        end
    end

    DEFAULT_CHAT_FRAME:AddMessage("|cffff3333材料仓库: 当前客户端无法发送请求|r")
    return false
end

local function SetStatus(text, r, g, b)
    ADDON.status:SetText(text or "")
    ADDON.status:SetTextColor(r or 0.90, g or 0.86, b or 0.76)
end

local function StoreCursorItem()
    local cursorType, itemId, link = GetCursorInfo()
    if cursorType ~= "item" then
        return false
    end

    if not itemId and link then
        itemId = tonumber(string.match(link, "item:(%d+)"))
    end

    if not itemId then
        return false
    end

    ClearCursor()

    if Send("STORE:" .. itemId) then
        SetStatus("已发送真实存入请求: " .. itemId)
    end

    return true
end

local function RegisterStoreDropTarget(frame)
    if not frame then
        return
    end

    frame:EnableMouse(true)

    local oldReceiveDrag = frame:GetScript("OnReceiveDrag")
    frame:SetScript("OnReceiveDrag", function(self, ...)
        if StoreCursorItem() then
            return
        end

        if oldReceiveDrag then
            return oldReceiveDrag(self, ...)
        end
    end)

    local oldMouseUp = frame:GetScript("OnMouseUp")
    frame:SetScript("OnMouseUp", function(self, button, ...)
        if button == "LeftButton" and StoreCursorItem() then
            return
        end

        if oldMouseUp then
            return oldMouseUp(self, button, ...)
        end
    end)
end

local function RaiseWarehouse()
    ADDON:SetFrameLevel(LAYER_HIGH)
end

local function LowerWarehouse()
    ADDON:SetFrameLevel(LAYER_LOW)
end

local function HookBackpackFrame(frame)
    if not frame or frame.materialWarehouseLayerHooked then
        return
    end

    frame.materialWarehouseLayerHooked = true
    frame:HookScript("OnMouseDown", LowerWarehouse)
    frame:HookScript("OnShow", LowerWarehouse)
end

local function HookBackpackFrames()
    for i = 1, 13 do
        HookBackpackFrame(_G["ContainerFrame" .. i])
    end

    HookBackpackFrame(_G["MainMenuBarBackpackButton"])
    for i = 0, 3 do
        HookBackpackFrame(_G["CharacterBag" .. i .. "Slot"])
    end
end

local function WithdrawCardItem(card)
    if not card or not card.itemId then
        return
    end

    local count = "0"

    if Send("WITHDRAW:" .. card.itemId .. ":" .. count) then
        if count == "0" then
            SetStatus("已拖出提取全部: " .. card.itemId)
        else
            SetStatus("已拖出提取 " .. count .. " 个: " .. card.itemId)
        end
    end
end

local function EnsureWithdrawGhost()
    if withdrawGhost then
        return withdrawGhost
    end

    withdrawGhost = CreateFrame("Frame", "MaterialWarehouseWithdrawGhost", UIParent)
    SetWidgetSize(withdrawGhost, 42, 42)
    withdrawGhost:SetFrameStrata("TOOLTIP")
    withdrawGhost:EnableMouse(false)

    withdrawGhost.icon = withdrawGhost:CreateTexture(nil, "ARTWORK")
    withdrawGhost.icon:SetAllPoints()
    withdrawGhost.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

    withdrawGhost.border = withdrawGhost:CreateTexture(nil, "OVERLAY")
    SetWidgetSize(withdrawGhost.border, 52, 52)
    withdrawGhost.border:SetPoint("CENTER")
    withdrawGhost.border:SetTexture("Interface\\Buttons\\UI-Quickslot2")

    withdrawGhost.count = withdrawGhost:CreateFontString(nil, "OVERLAY", "NumberFontNormal")
    withdrawGhost.count:SetPoint("BOTTOMRIGHT", -2, 2)

    withdrawGhost:Hide()
    return withdrawGhost
end

local function MoveWithdrawGhost()
    if not withdrawGhost or not withdrawGhost:IsShown() then
        return
    end

    local x, y = GetCursorPosition()
    local scale = UIParent:GetEffectiveScale() or 1
    withdrawGhost:ClearAllPoints()
    withdrawGhost:SetPoint("CENTER", UIParent, "BOTTOMLEFT", x / scale + 12, y / scale - 12)
end

local function IsBackpackTarget(frame)
    while frame do
        local name = frame:GetName()
        if name then
            if string.match(name, "^ContainerFrame%d+") or name == "MainMenuBarBackpackButton" or string.match(name, "^CharacterBag%dSlot$") then
                return true
            end
        end
        frame = frame:GetParent()
    end

    return false
end

local function FinishWithdrawDrag()
    if not withdrawDrag then
        return
    end

    local card = withdrawDrag.card
    local droppedToBag = IsBackpackTarget(GetMouseFocus())
    withdrawDrag = nil
    suppressIconClickUntil = GetTime() + 0.25
    ADDON:SetScript("OnUpdate", nil)

    if withdrawGhost then
        withdrawGhost:Hide()
    end

    if droppedToBag then
        WithdrawCardItem(card)
    else
        SetStatus("拖到背包格子或背包按钮上松开即可提取", 1, 0.78, 0.28)
    end
end

local function BeginWithdrawDrag(card)
    if not card or not card.itemId then
        return
    end

    withdrawDrag = { card = card }

    local ghost = EnsureWithdrawGhost()
    ghost.icon:SetTexture(card.icon:GetTexture() or GetItemIcon(card.itemId) or "Interface\\Icons\\INV_Misc_QuestionMark")
    ghost.count:SetText(card.count and string.match(card.count:GetText() or "", "%d+") or "")
    ghost:Show()
    MoveWithdrawGhost()

    SetStatus("拖到背包格子或背包按钮上松开提取: " .. card.itemId)
    ADDON:SetScript("OnUpdate", function()
        MoveWithdrawGhost()
        if not IsMouseButtonDown("LeftButton") then
            FinishWithdrawDrag()
        end
    end)
end

local function OpenWarehouse()
    ADDON:Show()
    RaiseWarehouse()
    Send("OPEN")
end

local function ToggleWarehouse()
    if ADDON:IsShown() then
        ADDON:Hide()
        return
    end

    OpenWarehouse()
end

local function SaveIconPosition(button)
    if not button then
        return
    end

    MaterialWarehouseDB = MaterialWarehouseDB or {}
    MaterialWarehouseDB.iconPosition = MaterialWarehouseDB.iconPosition or {}

    local point, _, relativePoint, x, y = button:GetPoint(1)
    MaterialWarehouseDB.iconPosition.point = point or "TOP"
    MaterialWarehouseDB.iconPosition.relativePoint = relativePoint or point or "TOP"
    MaterialWarehouseDB.iconPosition.x = x or 0
    MaterialWarehouseDB.iconPosition.y = y or -40

    local pm = _G.PluginManagerClient
    if pm and pm.SaveFramePosition then
        pm:SaveFramePosition("MaterialWarehouse", button)
    end
end

local function CreateIconButton()
    if iconButton then
        return iconButton
    end

    MaterialWarehouseDB = MaterialWarehouseDB or {}

    iconButton = CreateFrame("Button", "MaterialWarehouseIconButton", UIParent)
    SetWidgetSize(iconButton, 24, 24)
    iconButton:SetFrameStrata("MEDIUM")
    iconButton:SetMovable(true)
    iconButton:EnableMouse(true)
    iconButton:RegisterForDrag("LeftButton")
    iconButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    if iconButton.SetClampedToScreen then
        iconButton:SetClampedToScreen(true)
    end

    local pos = MaterialWarehouseDB.iconPosition or { point = "TOP", relativePoint = "TOP", x = 30, y = -40 }
    iconButton:SetPoint(pos.point or "TOP", UIParent, pos.relativePoint or pos.point or "TOP", pos.x or 30, pos.y or -40)

    iconButton:SetNormalTexture("Interface\\Icons\\INV_Misc_Bag_10")
    local normalTexture = iconButton:GetNormalTexture()
    if normalTexture then
        normalTexture:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    end

    iconButton:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
    iconButton.texture = normalTexture

    iconButton.border = iconButton:CreateTexture(nil, "OVERLAY")
    SetWidgetSize(iconButton.border, 32, 32)
    iconButton.border:SetPoint("CENTER", iconButton, "CENTER", 0, 0)
    iconButton.border:SetTexture("Interface\\Buttons\\UI-Quickslot2")

    iconButton.text = iconButton:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    iconButton.text:SetPoint("BOTTOM", iconButton, "BOTTOM", 0, -13)
    iconButton.text:SetText("|cffFFD700材料仓库|r")

    iconButton:SetScript("OnDragStart", function(self)
        iconDragging = true
        GameTooltip:Hide()
        self:StartMoving()
    end)

    iconButton:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        SaveIconPosition(self)
        iconDragging = false
    end)

    iconButton:SetScript("OnEnter", function(self)
        if iconDragging then
            return
        end

        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText("材料仓库", 1, 1, 1)
        GameTooltip:AddLine("点击: 打开/关闭界面", 0.8, 0.8, 0.8)
        GameTooltip:AddLine("拖拽: 移动图标位置", 0.8, 0.8, 0.8)
        GameTooltip:Show()

        if self.texture then
            self.texture:SetVertexColor(1.25, 1.25, 1.25)
        end
    end)

    iconButton:SetScript("OnLeave", function(self)
        GameTooltip:Hide()
        if self.texture then
            self.texture:SetVertexColor(1, 1, 1)
        end
    end)

    iconButton:SetScript("OnClick", function()
        if iconDragging then
            return
        end

        ToggleWarehouse()
    end)

    return iconButton
end

local function ShowIconButton()
    local button = CreateIconButton()
    if button then
        button:Show()
    end
end

local function ResetIconPosition()
    local button = CreateIconButton()
    if not button then
        return
    end

    button:ClearAllPoints()
    button:SetPoint("TOP", UIParent, "TOP", 30, -40)
    SaveIconPosition(button)
    button:Show()
    SetStatus("图标位置已重置")
end

local function CreatePanel()
    SetWidgetSize(ADDON, 1120, 690)
    ADDON:SetPoint("CENTER")
    ADDON:SetFrameStrata("MEDIUM")
    ADDON:SetFrameLevel(LAYER_LOW)
    ADDON:SetMovable(true)
    ADDON:EnableMouse(true)
    ADDON:RegisterForDrag("LeftButton")
    ADDON:SetScript("OnMouseDown", RaiseWarehouse)
    ADDON:SetScript("OnDragStart", function(self)
        RaiseWarehouse()
        self:StartMoving()
    end)
    ADDON:SetScript("OnDragStop", ADDON.StopMovingOrSizing)
    RegisterStoreDropTarget(ADDON)
    ADDON:Hide()

    ADDON.bg = ADDON:CreateTexture(nil, "BACKGROUND")
    ADDON.bg:SetAllPoints()
    ADDON.bg:SetTexture(0.05, 0.06, 0.07, 0.96)

    ADDON.border = CreateFrame("Frame", nil, ADDON)
    ADDON.border:SetPoint("TOPLEFT", 1, -1)
    ADDON.border:SetPoint("BOTTOMRIGHT", -1, 1)
    ADDON.border:SetBackdrop({
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 14,
    })
    ADDON.border:SetBackdropBorderColor(0.45, 0.40, 0.32, 1)

    ADDON.title = ADDON:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    ADDON.title:SetPoint("TOPLEFT", 18, -16)
    ADDON.title:SetText("材料仓库")

    ADDON.close = CreateFrame("Button", nil, ADDON, "UIPanelCloseButton")
    ADDON.close:SetPoint("TOPRIGHT", -8, -8)

    ADDON.search = CreateFrame("EditBox", nil, ADDON, "InputBoxTemplate")
    SetWidgetSize(ADDON.search, 220, 22)
    ADDON.search:SetPoint("LEFT", ADDON.title, "RIGHT", 18, -1)
    ADDON.search:SetAutoFocus(false)
    ADDON.search:SetText("")
    ADDON.search:SetScript("OnTextChanged", function()
        RefreshCards()
    end)
    ADDON.search:SetScript("OnEnterPressed", function(self)
        self:ClearFocus()
    end)
    ADDON.search:SetScript("OnEscapePressed", function(self)
        if self:GetText() ~= "" then
            self:SetText("")
            self:ClearFocus()
            return
        end

        ADDON:Hide()
    end)

    ADDON.searchHint = ADDON:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    ADDON.searchHint:SetPoint("LEFT", ADDON.search, "RIGHT", 10, 0)
    ADDON.searchHint:SetText("搜索名称/ID")

    ADDON.drop = CreateFrame("Frame", nil, ADDON)
    SetWidgetSize(ADDON.drop, 1, 1)
    ADDON.drop:SetPoint("TOPLEFT", ADDON, "TOPLEFT", -4, 4)
    ADDON.drop.itemId = nil
    ADDON.drop:Hide()

    ADDON.status = ADDON:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    ADDON.status:SetPoint("LEFT", ADDON.searchHint, "RIGHT", 14, 0)
    ADDON.status:SetPoint("RIGHT", ADDON, "RIGHT", -18, 0)
    ADDON.status:SetJustifyH("LEFT")
    ADDON.status:SetText("")

    ADDON.scroll = CreateFrame("ScrollFrame", "MaterialWarehouseScroll", ADDON, "UIPanelScrollFrameTemplate")
    ADDON.scroll:SetPoint("TOPLEFT", 18, -58)
    ADDON.scroll:SetPoint("BOTTOMRIGHT", -34, 18)
    ADDON.scroll:SetScript("OnMouseDown", RaiseWarehouse)
    ADDON.content = CreateFrame("Frame", nil, ADDON.scroll)
    SetWidgetSize(ADDON.content, GRID_COLUMNS * CARD_WIDTH + (GRID_COLUMNS - 1) * CARD_GAP, 540)
    ADDON.content:SetScript("OnMouseDown", RaiseWarehouse)
    ADDON.scroll:SetScrollChild(ADDON.content)

    ADDON.empty = ADDON.content:CreateFontString(nil, "OVERLAY", "GameFontDisableLarge")
    ADDON.empty:SetPoint("TOP", ADDON.content, "TOP", 0, -80)
    ADDON.empty:SetText("把背包物品拖到这里，松手后会直接显示为仓库卡片")

    RegisterStoreDropTarget(ADDON.scroll)
    RegisterStoreDropTarget(ADDON.content)
end

local function UpdateCard(card, data)
    card.itemId = data.itemId
    card.icon:SetTexture(data.icon or GetItemIcon(data.itemId) or "Interface\\Icons\\INV_Misc_QuestionMark")
    local color = qualityColors[data.quality] or qualityColors[1]
    card:SetBackdropBorderColor(color[1] * 0.75, color[2] * 0.75, color[3] * 0.75, 1)
    card.name:SetText(ShortText(data.name, 10))
    card.name:SetTextColor(color[1], color[2], color[3])
    card.count:SetText(data.count)
    card.auto:SetChecked(data.auto == 1)
end

local function CreateCard(index)
    local card = CreateFrame("Frame", nil, ADDON.content)
    SetWidgetSize(card, CARD_WIDTH, CARD_HEIGHT)
    card:SetScript("OnMouseDown", RaiseWarehouse)
    card:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 9,
    })
    card:SetBackdropColor(0.10, 0.11, 0.13, 0.94)
    card:SetBackdropBorderColor(0.30, 0.27, 0.22, 1)

    local col = (index - 1) % GRID_COLUMNS
    local row = math.floor((index - 1) / GRID_COLUMNS)
    card:SetPoint("TOPLEFT", ADDON.content, "TOPLEFT", col * (CARD_WIDTH + CARD_GAP), -row * (CARD_HEIGHT + CARD_GAP))

    card.iconButton = CreateFrame("Button", nil, card)
    SetWidgetSize(card.iconButton, ICON_SIZE, ICON_SIZE)
    card.iconButton:SetPoint("TOP", card, "TOP", 0, -8)
    card.iconButton:RegisterForDrag("LeftButton")
    card.iconButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")

    card.icon = card.iconButton:CreateTexture(nil, "ARTWORK")
    card.icon:SetAllPoints(card.iconButton)

    card.iconButton:SetScript("OnDragStart", function()
        BeginWithdrawDrag(card)
    end)

    card.iconButton:SetScript("OnDragStop", function()
        FinishWithdrawDrag()
    end)

    card.iconButton:SetScript("OnClick", function(_, button)
        RaiseWarehouse()
        if StoreCursorItem() then
            return
        end

        if GetTime() < suppressIconClickUntil then
            return
        end

        if button == "RightButton" then
            WithdrawCardItem(card)
        end
    end)

    card.iconButton:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        if card.itemId then
            GameTooltip:SetHyperlink("item:" .. card.itemId)
            GameTooltip:AddLine("右键点击: 提取到背包", 0.8, 0.8, 0.8, true)
            GameTooltip:AddLine("拖到背包格子: 提取到背包", 0.8, 0.8, 0.8, true)
        end
        GameTooltip:Show()
    end)

    card.iconButton:SetScript("OnLeave", GameTooltip_Hide)

    card.name = card:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    card.name:SetPoint("TOPLEFT", card.iconButton, "BOTTOMLEFT", -26, -3)
    card.name:SetPoint("RIGHT", card, "RIGHT", -6, 0)
    card.name:SetHeight(24)
    card.name:SetJustifyH("CENTER")
    card.name:SetJustifyV("TOP")

    card.count = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    card.count:SetPoint("TOP", card.name, "BOTTOM", 0, -1)
    card.count:SetJustifyH("CENTER")

    card.auto = CreateFrame("CheckButton", nil, card, "UICheckButtonTemplate")
    SetWidgetSize(card.auto, 20, 20)
    card.auto:SetPoint("BOTTOM", card, "BOTTOM", -8, 4)
    card.auto:SetScript("OnClick", function(self)
        RaiseWarehouse()
        Send("AUTO:" .. card.itemId .. ":" .. (self:GetChecked() and "1" or "0"))
    end)
    card.autoText = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    card.autoText:SetPoint("LEFT", card.auto, "RIGHT", -5, 0)
    card.autoText:SetText("自")

    card:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetHyperlink("item:" .. self.itemId)
        GameTooltip:AddLine("右键图标提取", 0.8, 0.8, 0.8, true)
        GameTooltip:Show()
    end)
    card:SetScript("OnLeave", GameTooltip_Hide)
    RegisterStoreDropTarget(card)

    return card
end

function RefreshCards()
    local ordered = {}
    local query = ADDON.search and NormalizeSearchText(ADDON.search:GetText()) or ""
    for itemId, data in pairs(items) do
        if ItemMatchesSearch(data, query) then
            table.insert(ordered, data)
        end
    end
    table.sort(ordered, function(a, b) return a.itemId < b.itemId end)

    SetWidgetShown(ADDON.empty, #ordered == 0)

    for i, data in ipairs(ordered) do
        if not cards[i] then
            cards[i] = CreateCard(i)
        end
        cards[i]:Show()
        UpdateCard(cards[i], data)
    end

    for i = #ordered + 1, #cards do
        cards[i]:Hide()
    end

    local rows = math.max(1, math.ceil(#ordered / GRID_COLUMNS))
    ADDON.content:SetHeight(rows * (CARD_HEIGHT + CARD_GAP) + 20)
end

local function HandlePayload(payload)
    if payload == "OPEN" then
        ADDON:Show()
        Send("REQ")
        return
    end

    if payload == "BEGIN" then
        items = {}
        return
    end

    if payload == "END" then
        RefreshCards()
        return
    end

    if string.sub(payload, 1, 5) == "ITEM:" then
        local fields = Split(string.sub(payload, 6), "^")
        local itemId = tonumber(fields[1] or "0")
        local count = tonumber(fields[2] or "0") or 0
        if itemId and itemId > 0 and count > 0 then
            items[itemId] = {
                itemId = itemId,
                count = fields[2] or "0",
                auto = tonumber(fields[3] or "1") or 1,
                name = fields[4] or ("item:" .. itemId),
                quality = tonumber(fields[5] or "1") or 1,
                icon = fields[6],
                stack = tonumber(fields[7] or "1") or 1,
            }
        end
        return
    end

    if string.sub(payload, 1, 7) == "RESULT:" then
        local fields = Split(string.sub(payload, 8), "^")
        local ok = tonumber(fields[2] or "0") == 1
        SetStatus(fields[3] or "", ok and 0.35 or 1.0, ok and 0.95 or 0.35, ok and 0.45 or 0.25)
        return
    end

    if string.sub(payload, 1, 6) == "CHUNK:" then
        local indexText, totalText, body = string.match(payload, "^CHUNK:(%d+)%^(%d+)%^(.*)$")
        local index = tonumber(indexText or "0")
        local total = tonumber(totalText or "0")
        body = body or ""
        if index and total and index > 0 and total > 0 then
            chunkBuffer[total] = chunkBuffer[total] or {}
            chunkBuffer[total][index] = body
            local complete = true
            for i = 1, total do
                if not chunkBuffer[total][i] then
                    complete = false
                    break
                end
            end
            if complete then
                local full = ""
                for i = 1, total do
                    full = full .. chunkBuffer[total][i]
                end
                chunkBuffer[total] = nil
                HandlePayload(full)
            end
        end
    end
end

ADDON:SetScript("OnEvent", function(_, event, ...)
    if event == "PLAYER_LOGIN" then
        if RegisterAddonMessagePrefix then
            RegisterAddonMessagePrefix(PREFIX)
        elseif C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
            C_ChatInfo.RegisterAddonMessagePrefix(PREFIX)
        end
        SLASH_MATERIALWAREHOUSE1 = "/mw"
        SLASH_MATERIALWAREHOUSE2 = "/材料仓库"
        SlashCmdList.MATERIALWAREHOUSE = function(msg)
            msg = msg or ""
            if msg == "重置" or msg == "reset" then
                ResetIconPosition()
                return
            end

            if msg == "图标" or msg == "icon" then
                ShowIconButton()
                return
            end

            ToggleWarehouse()
        end
        ShowIconButton()
        HookBackpackFrames()
        DEFAULT_CHAT_FRAME:AddMessage("|cffFFD700材料仓库|r 已加载，点击屏幕上方图标或输入 /材料仓库 打开。")
        return
    end

    if event == "BAG_UPDATE" or event == "BAG_UPDATE_DELAYED" then
        HookBackpackFrames()
        return
    end

    if event == "CHAT_MSG_ADDON" then
        local prefix, payload, _, sender = ...
        if prefix ~= PREFIX or sender ~= UnitName("player") then
            return
        end
        HandlePayload(payload)
        return
    end

    if event == "CHAT_MSG_WHISPER" then
        local message = ...
        if not message then
            return
        end

        local tabPos = string.find(message, "\t", 1, true)
        if not tabPos or string.sub(message, 1, tabPos - 1) ~= PREFIX then
            return
        end

        HandlePayload(string.sub(message, tabPos + 1))
    end
end)

CreatePanel()
if UISpecialFrames then
    local exists = false
    for _, frameName in ipairs(UISpecialFrames) do
        if frameName == "MaterialWarehouseFrame" then
            exists = true
            break
        end
    end

    if not exists then
        table.insert(UISpecialFrames, "MaterialWarehouseFrame")
    end
end

ADDON:RegisterEvent("PLAYER_LOGIN")
ADDON:RegisterEvent("CHAT_MSG_ADDON")
ADDON:RegisterEvent("CHAT_MSG_WHISPER")
ADDON:RegisterEvent("BAG_UPDATE")
ADDON:RegisterEvent("BAG_UPDATE_DELAYED")

if ChatFrame_AddMessageEventFilter then
    ChatFrame_AddMessageEventFilter("CHAT_MSG_WHISPER", function(_, _, message)
        if message and string.sub(message, 1, string.len(PREFIX) + 1) == PREFIX .. "\t" then
            return true
        end
        return false
    end)
end
