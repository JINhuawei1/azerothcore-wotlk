-- 转身系统UI主逻辑文件

-- 本地变量
local initialized = false
local iconButton = nil

-- 调试输出
local function PrintDebug(msg)
    if ReincarnationConfig and ReincarnationConfig.DebugMode then
        DEFAULT_CHAT_FRAME:AddMessage("|cff9966FF[转身UI]|r " .. tostring(msg))
    end
end

-- 信息输出
local function PrintInfo(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff9966FF[转身系统]|r " .. tostring(msg))
end

-- 错误输出
local function PrintError(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cffFF0000[转身系统]|r " .. tostring(msg))
end

-- 更新信息显示
local function UpdateInfoDisplay()
    local infoFrame = ReincarnationMainFrameInfoFrame
    if not infoFrame then return end

    local level = ReincarnationData:GetReincarnationLevel()
    local stats = ReincarnationData:GetBonusStats()
    local talent = ReincarnationData:GetBonusTalent()
    local playerLevel = ReincarnationData:GetPlayerLevel()

    -- 更新转身等级
    if infoFrame.LevelText then
        if level > 0 then
            infoFrame.LevelText:SetText(string.format("|cffFFD700%d|r 转", level))
        else
            infoFrame.LevelText:SetText("|cff666666未转身|r")
        end
    end

    -- 更新全属性加成
    if infoFrame.StatsText then
        if stats > 0 then
            infoFrame.StatsText:SetText(string.format("|cff00FF00+%.1f%%|r 全属性", stats))
        else
            infoFrame.StatsText:SetText("|cff666666无加成|r")
        end
    end

    -- 更新额外天赋点
    if infoFrame.TalentText then
        if talent > 0 then
            infoFrame.TalentText:SetText(string.format("|cff00FFFF+%d|r 天赋点", talent))
        else
            infoFrame.TalentText:SetText("|cff666666无额外天赋点|r")
        end
    end

    -- 更新玩家等级
    if infoFrame.PlayerLevelText then
        infoFrame.PlayerLevelText:SetText(string.format("角色等级: |cffFFFFFF%d|r", playerLevel))
    end

    -- 更新下一级预览
    if infoFrame.NextLevelText then
        local nextStats = ReincarnationData:GetNextLevelBonus()
        local nextTalent = ReincarnationData:GetNextLevelTalent()
        infoFrame.NextLevelText:SetText(string.format(
            "下一转: |cff00FF00+%.1f%%|r 全属性, |cff00FFFF+%d|r 天赋点",
            nextStats, nextTalent
        ))
    end

    -- 更新转身按钮状态
    local reincarnateBtn = ReincarnationMainFrameReincarnateButton
    local btnText = ReincarnationMainFrameReincarnateButtonText
    if reincarnateBtn then
        if ReincarnationData:CanReincarnate() then
            reincarnateBtn:Enable()
            if btnText then
                btnText:SetText("|cffFFD700转身升级|r")
            end
            reincarnateBtn:SetBackdropColor(0.4, 0.3, 0.1, 0.95)
            reincarnateBtn:SetBackdropBorderColor(1, 0.84, 0, 1)
        else
            reincarnateBtn:Disable()
            if btnText then
                btnText:SetText("|cff888888需要80级|r")
            end
            reincarnateBtn:SetBackdropColor(0.2, 0.2, 0.2, 0.95)
            reincarnateBtn:SetBackdropBorderColor(0.5, 0.5, 0.5, 1)
        end
    end
end

-- 更新属性列表显示
local function UpdateAttributeDisplay()
    local attrFrame = ReincarnationMainFrameAttributeFrame
    if not attrFrame then return end

    local stats = ReincarnationData:GetBonusStats()

    -- 更新所有20个属性行
    for i = 1, 20 do
        local row = attrFrame["AttrRow" .. i]
        if row and row.ValueText then
            if stats > 0 then
                row.ValueText:SetText(string.format("|cff00FF00+%.1f%%|r", stats))
            else
                row.ValueText:SetText("|cff666666+0%|r")
            end
        end
    end
end

-- 显示转身确认对话框
function ReincarnationUI_ShowConfirm()
    if not ReincarnationData:CanReincarnate() then
        PrintError(ReincarnationConfig.Messages.NotEnoughLevel)
        return
    end

    ReincarnationConfirmFrame:Show()
end

-- 执行转身
function ReincarnationUI_DoReincarnate()
    ReincarnationConfirmFrame:Hide()

    ReincarnationData:DoReincarnate(function(success, result)
        if success then
            UpdateInfoDisplay()
            UpdateAttributeDisplay()
            -- 服务器会发送成功消息
        else
            local errorMsg = result and result.message or ReincarnationConfig.Messages.ReincarnateFailed
            PrintError(errorMsg)
        end
    end)
end

-- 刷新数据
function ReincarnationUI_RefreshData()
    ReincarnationData:Refresh(function(success, data)
        if success then
            UpdateInfoDisplay()
            UpdateAttributeDisplay()
        else
            PrintError("数据刷新失败")
        end
    end)
end

-- 切换窗口显示
function ReincarnationUI_Toggle()
    if ReincarnationMainFrame:IsShown() then
        ReincarnationMainFrame:Hide()
    else
        ReincarnationMainFrame:Show()
    end
end

-- 窗口加载
function ReincarnationUI_OnLoad(self)
    -- 注册可拖动
    self:RegisterForDrag("LeftButton")

    -- 设置ESC关闭
    tinsert(UISpecialFrames, "ReincarnationMainFrame")

    -- 注册斜杠命令
    SLASH_REINCARNATION1 = "/reincarnation"
    SLASH_REINCARNATION2 = "/zs"
    SLASH_REINCARNATION3 = "/转身"
    SlashCmdList["REINCARNATION"] = function(msg)
        ReincarnationUI_Toggle()
    end

    PrintDebug("转身系统UI加载完成")
end

-- 窗口显示
function ReincarnationUI_OnShow(self)
    if not initialized then
        initialized = true
    end

    ReincarnationData:EnsureData(function(success, data)
        if success then
            UpdateInfoDisplay()
            UpdateAttributeDisplay()
        end
    end)
end

-- 窗口隐藏
function ReincarnationUI_OnHide(self)
    PrintDebug("转身系统UI隐藏")
end

-- 创建图标按钮
local function CreateIconButton()
    if iconButton then return iconButton end

    iconButton = CreateFrame("Button", "ReincarnationIconButton", UIParent)
    iconButton:SetSize(24, 24)

    -- 使用保存的位置
    local pos = ReincarnationUIDB and ReincarnationUIDB.iconPosition or { point = "TOP", x = 60, y = -10 }
    iconButton:SetPoint(pos.point or "TOP", UIParent, pos.point or "TOP", pos.x, pos.y)

    -- 使用转身相关图标
    iconButton:SetNormalTexture("Interface\\Icons\\Spell_Holy_Resurrection")
    local tex = iconButton:GetNormalTexture()
    if tex then tex:SetTexCoord(0.07, 0.93, 0.07, 0.93) end

    iconButton:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")

    -- 图标下方文字
    local text = iconButton:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    text:SetPoint("BOTTOM", 0, -12)
    text:SetText("|cff9966FF转身系统|r")

    -- 设置可拖动
    iconButton:SetMovable(true)
    iconButton:EnableMouse(true)
    iconButton:RegisterForDrag("LeftButton")

    local isDragging = false
    local dragStartTime = 0

    iconButton:SetScript("OnDragStart", function(self)
        self:StartMoving()
        isDragging = true
        dragStartTime = GetTime()
    end)

    iconButton:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        isDragging = false
        local point, _, _, x, y = self:GetPoint()
        if point and x and y then
            if not ReincarnationUIDB then ReincarnationUIDB = {} end
            ReincarnationUIDB.iconPosition = { point = point, x = x, y = y }
        end
    end)

    iconButton:SetScript("OnMouseDown", function(self, button)
        if button == "LeftButton" then
            dragStartTime = GetTime()
        end
    end)

    iconButton:SetScript("OnMouseUp", function(self, button)
        if button == "LeftButton" and (GetTime() - dragStartTime) < 0.3 and not isDragging then
            PrintDebug("图标点击")
            if ReincarnationMainFrame then
                if ReincarnationMainFrame:IsShown() then
                    ReincarnationMainFrame:Hide()
                else
                    ReincarnationMainFrame:Show()
                end
            else
                PrintError("主窗口不存在")
            end
        end
    end)

    iconButton:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText("转身系统", 1, 1, 1)
        GameTooltip:AddLine("左键: 打开界面", 0.8, 0.8, 0.8)
        GameTooltip:AddLine("拖拽: 移动位置", 0.8, 0.8, 0.8)
        GameTooltip:AddLine(" ")
        GameTooltip:AddLine("提升角色永久属性", 0.6, 0.2, 0.9)
        GameTooltip:Show()
    end)

    iconButton:SetScript("OnLeave", function(self)
        GameTooltip:Hide()
    end)

    return iconButton
end

-- 显示/隐藏图标按钮
function ReincarnationUI_ShowIconButton()
    if not iconButton then
        CreateIconButton()
    end
    if iconButton then
        iconButton:Show()
    end
end

function ReincarnationUI_HideIconButton()
    if iconButton then
        iconButton:Hide()
    end
end

function ReincarnationUI_ToggleIconButton()
    if iconButton and iconButton:IsShown() then
        ReincarnationUI_HideIconButton()
    else
        ReincarnationUI_ShowIconButton()
    end
end

-- 事件处理
local eventFrame = CreateFrame("Frame")
eventFrame:RegisterEvent("ADDON_LOADED")
eventFrame:RegisterEvent("PLAYER_LOGIN")
eventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
eventFrame:RegisterEvent("PLAYER_LEVEL_UP")

eventFrame:SetScript("OnEvent", function(self, event, ...)
    if event == "ADDON_LOADED" then
        local addonName = ...
        if addonName == "ReincarnationUI" then
            PrintDebug("转身系统插件已加载")
            -- 初始化保存的变量
            if not ReincarnationUIDB then
                ReincarnationUIDB = {
                    showIconButton = true,
                    iconPosition = { point = "TOP", x = 60, y = -10 }
                }
            end
        end
    elseif event == "PLAYER_LOGIN" then
        PrintDebug("玩家登录，创建图标按钮")
        -- 创建图标按钮
        if ReincarnationUIDB and ReincarnationUIDB.showIconButton ~= false then
            local success, err = pcall(function()
                CreateIconButton()
                if iconButton then
                    iconButton:Show()
                    PrintDebug("图标按钮已显示")
                else
                    PrintError("图标按钮创建失败")
                end
            end)
            if not success then
                PrintError("创建图标按钮时出错: " .. tostring(err))
            end
        else
            PrintDebug("图标按钮已禁用")
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        PrintDebug("玩家进入世界，初始化转身系统UI")
    elseif event == "PLAYER_LEVEL_UP" then
        -- 玩家升级时更新显示
        local newLevel = ...
        if ReincarnationMainFrame and ReincarnationMainFrame:IsShown() then
            UpdateInfoDisplay()
        end

        -- 达到80级时提示
        if newLevel == 80 then
            PrintInfo("恭喜达到80级！你现在可以进行转身了。输入 /转身 打开转身界面。")
        end
    end
end)

-- 全局函数
_G.ReincarnationUI_Toggle = ReincarnationUI_Toggle
_G.ReincarnationUI_OnLoad = ReincarnationUI_OnLoad
_G.ReincarnationUI_OnShow = ReincarnationUI_OnShow
_G.ReincarnationUI_OnHide = ReincarnationUI_OnHide
_G.ReincarnationUI_ShowConfirm = ReincarnationUI_ShowConfirm
_G.ReincarnationUI_DoReincarnate = ReincarnationUI_DoReincarnate
_G.ReincarnationUI_RefreshData = ReincarnationUI_RefreshData
_G.ReincarnationUI_ShowIconButton = ReincarnationUI_ShowIconButton
_G.ReincarnationUI_HideIconButton = ReincarnationUI_HideIconButton
_G.ReincarnationUI_ToggleIconButton = ReincarnationUI_ToggleIconButton

PrintDebug("转身系统UI脚本加载完成")
