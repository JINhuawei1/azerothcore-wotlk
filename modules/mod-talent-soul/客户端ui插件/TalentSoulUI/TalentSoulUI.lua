-- 天赋之魂UI主逻辑文件

-- 本地变量
local skillButtons = {}
local attributeButtons = {}
local classTabButtons = {}
local MAX_SKILL_BUTTONS = 8
local SKILL_BUTTON_HEIGHT = 50
local initialized = false
local selectedClassFilter = nil  -- nil表示显示玩家职业, 0表示全部, 其他为职业ID

-- 职业ID顺序（用于显示）- 移除了"全部"(0)
local CLASS_ORDER = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 }  -- 战士, 圣骑士, 猎人, 盗贼, 牧师, 死骑, 萨满, 法师, 术士, 德鲁伊

-- 前向声明
local UpdateClassTabSelection
local UpdateSkillDetail
local UpdatePointsDisplay
local InitClassTabs

-- 调试输出
local function PrintDebug(msg)
    if TalentSoulConfig and TalentSoulConfig.DebugMode then
        DEFAULT_CHAT_FRAME:AddMessage("|cff9933FF[天赋UI]|r " .. tostring(msg))
    end
end

-- 信息输出
local function PrintInfo(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff9933FF[天赋之魂]|r " .. tostring(msg))
end

-- 错误输出
local function PrintError(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cffFF0000[天赋之魂]|r " .. tostring(msg))
end

-- 应用暗黑主题到按钮
local function ApplyDarkThemeToButton(button, isReset)
    if not button then return end

    local colors = TalentSoulConfig.Colors
    local normalColor, hoverColor

    if isReset then
        normalColor = colors.ResetButton
        hoverColor = colors.ResetButtonHover
    else
        normalColor = colors.UpgradeButton
        hoverColor = colors.UpgradeButtonHover
    end

    -- 设置按钮背景色
    button:SetScript("OnEnter", function(self)
        if self.SetBackdropColor then
            self:SetBackdropColor(hoverColor.r, hoverColor.g, hoverColor.b, hoverColor.a)
        end
        if self.originalOnEnter then
            self.originalOnEnter(self)
        end
    end)

    button:SetScript("OnLeave", function(self)
        if self.SetBackdropColor then
            self:SetBackdropColor(normalColor.r, normalColor.g, normalColor.b, normalColor.a)
        end
        if self.originalOnLeave then
            self.originalOnLeave(self)
        end
    end)
end

-- 创建技能按钮
local function CreateSkillButton(parent, index)
    local buttonName = "TalentSoulSkillButton" .. index
    local button = CreateFrame("Button", buttonName, parent)
    button:SetSize(210, SKILL_BUTTON_HEIGHT)

    -- 背景
    button:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 }
    })
    button:SetBackdropColor(0.1, 0.1, 0.15, 0.9)
    button:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)

    -- 技能图标
    button.icon = button:CreateTexture(buttonName .. "Icon", "ARTWORK")
    button.icon:SetSize(40, 40)
    button.icon:SetPoint("LEFT", button, "LEFT", 5, 0)
    button.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")

    -- 技能名称
    button.name = button:CreateFontString(buttonName .. "Name", "OVERLAY", "GameFontNormal")
    button.name:SetPoint("TOPLEFT", button.icon, "TOPRIGHT", 8, -3)
    button.name:SetWidth(140)
    button.name:SetJustifyH("LEFT")
    button.name:SetText("技能名称")
    button.name:SetTextColor(1, 1, 1, 1)

    -- 等级信息
    button.level = button:CreateFontString(buttonName .. "Level", "OVERLAY", "GameFontNormalSmall")
    button.level:SetPoint("BOTTOMLEFT", button.icon, "BOTTOMRIGHT", 8, 3)
    button.level:SetWidth(140)
    button.level:SetJustifyH("LEFT")
    button.level:SetText("等级: 0")
    button.level:SetTextColor(0.7, 0.7, 0.7, 1)

    -- 高亮效果
    button:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")

    -- 选中效果
    button.selected = button:CreateTexture(buttonName .. "Selected", "BACKGROUND")
    button.selected:SetAllPoints()
    button.selected:SetTexture("Interface\\Buttons\\UI-Listbox-Highlight2")
    button.selected:SetBlendMode("ADD")
    button.selected:SetVertexColor(0.6, 0.3, 0.9, 0.5)
    button.selected:Hide()

    -- 点击事件
    button:SetScript("OnClick", function(self)
        TalentSoulUI_SelectSkill(self.skillData)
    end)

    -- 悬停效果
    button:SetScript("OnEnter", function(self)
        self:SetBackdropBorderColor(0.6, 0.4, 0.8, 1)
        if self.skillData then
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:AddLine(self.skillData.name, 1, 1, 1)
            if self.skillData.description and self.skillData.description ~= "" then
                GameTooltip:AddLine(self.skillData.description, 0.8, 0.8, 0.8, true)
            end
            if self.skillData.requiredPoints and self.skillData.requiredPoints > 0 then
                GameTooltip:AddLine(" ")
                GameTooltip:AddLine(string.format("需要 %d 天赋点解锁", self.skillData.requiredPoints), 1, 0.5, 0)
            end
            GameTooltip:Show()
        end
    end)

    button:SetScript("OnLeave", function(self)
        self:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)
        GameTooltip:Hide()
    end)

    return button
end

-- 创建属性升级按钮
local function CreateAttributeButton(parent, index, attrType)
    local config = TalentSoulConfig.UpgradeTypes[attrType]
    local buttonName = "TalentSoulAttrButton" .. index

    local frame = CreateFrame("Frame", buttonName, parent)
    frame:SetSize(370, 55)

    -- 背景
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 }
    })
    frame:SetBackdropColor(0.08, 0.08, 0.12, 0.95)
    frame:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)

    -- 属性图标
    frame.icon = frame:CreateTexture(buttonName .. "Icon", "ARTWORK")
    frame.icon:SetSize(35, 35)
    frame.icon:SetPoint("LEFT", frame, "LEFT", 10, 0)
    frame.icon:SetTexture(config.icon)

    -- 属性名称
    frame.name = frame:CreateFontString(buttonName .. "Name", "OVERLAY", "GameFontNormal")
    frame.name:SetPoint("TOPLEFT", frame.icon, "TOPRIGHT", 10, -3)
    frame.name:SetText(config.color .. config.name .. "|r")

    -- 等级显示
    frame.level = frame:CreateFontString(buttonName .. "Level", "OVERLAY", "GameFontNormalSmall")
    frame.level:SetPoint("BOTTOMLEFT", frame.icon, "BOTTOMRIGHT", 10, 3)
    frame.level:SetText("0 / 10")
    frame.level:SetTextColor(0.7, 0.7, 0.7, 1)

    -- 进度条背景
    frame.progressBg = frame:CreateTexture(buttonName .. "ProgressBg", "ARTWORK")
    frame.progressBg:SetSize(150, 12)
    frame.progressBg:SetPoint("LEFT", frame.level, "RIGHT", 10, 0)
    frame.progressBg:SetTexture("Interface\\TargetingFrame\\UI-StatusBar")
    frame.progressBg:SetVertexColor(0.1, 0.1, 0.1, 1)

    -- 进度条
    frame.progress = frame:CreateTexture(buttonName .. "Progress", "OVERLAY")
    frame.progress:SetSize(0, 10)
    frame.progress:SetPoint("LEFT", frame.progressBg, "LEFT", 1, 0)
    frame.progress:SetTexture("Interface\\TargetingFrame\\UI-StatusBar")

    -- 根据属性类型设置进度条颜色
    local colors = {
        [1] = { 0, 1, 1 },      -- GCD - 青色
        [2] = { 0, 0.5, 1 },    -- 冷却 - 蓝色
        [3] = { 0, 1, 0.5 },    -- 消耗 - 绿色
        [4] = { 1, 0.3, 0 },    -- 伤害 - 橙红色
    }
    local color = colors[attrType] or { 1, 1, 1 }
    frame.progress:SetVertexColor(color[1], color[2], color[3], 1)

    -- 升级按钮
    frame.upgradeBtn = CreateFrame("Button", buttonName .. "UpgradeBtn", frame, "UIPanelButtonTemplate")
    frame.upgradeBtn:SetSize(60, 25)
    frame.upgradeBtn:SetPoint("RIGHT", frame, "RIGHT", -10, 0)
    frame.upgradeBtn:SetText("升级")
    frame.upgradeBtn.attrType = attrType

    frame.upgradeBtn:SetScript("OnClick", function(self)
        TalentSoulUI_UpgradeAttribute(self.attrType)
    end)

    frame.upgradeBtn:SetScript("OnEnter", function(self)
        local skill = TalentSoulData:GetSelectedSkill()
        if skill then
            local canUpgrade, reason = TalentSoulData:CanUpgradeAttribute(skill, self.attrType)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            if canUpgrade then
                GameTooltip:AddLine("点击升级", 0, 1, 0)
                GameTooltip:AddLine("消耗 1 点天赋点", 1, 1, 1)
            else
                GameTooltip:AddLine("无法升级", 1, 0, 0)
                if reason then
                    GameTooltip:AddLine(reason, 1, 0.5, 0)
                end
            end
            GameTooltip:Show()
        end
    end)

    frame.upgradeBtn:SetScript("OnLeave", function(self)
        GameTooltip:Hide()
    end)

    frame.attrType = attrType
    return frame
end

-- 创建职业标签按钮
local function CreateClassTabButton(parent, index, classId)
    local config = TalentSoulConfig.Classes[classId]
    if not config then return nil end

    local buttonName = "TalentSoulClassTab" .. index
    local button = CreateFrame("Button", buttonName, parent)
    button:SetSize(58, 28)

    -- 背景
    button:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 10,
        insets = { left = 2, right = 2, top = 2, bottom = 2 }
    })
    button:SetBackdropColor(0.1, 0.1, 0.15, 0.9)
    button:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)

    -- 职业图标
    button.icon = button:CreateTexture(buttonName .. "Icon", "ARTWORK")
    button.icon:SetSize(20, 20)
    button.icon:SetPoint("LEFT", button, "LEFT", 4, 0)
    button.icon:SetTexture(config.icon)
    button.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

    -- 职业名称（简短显示）
    button.text = button:CreateFontString(buttonName .. "Text", "OVERLAY", "GameFontNormalSmall")
    button.text:SetPoint("LEFT", button.icon, "RIGHT", 2, 0)
    button.text:SetWidth(30)
    button.text:SetJustifyH("LEFT")

    -- 根据职业ID设置显示名称
    local shortName = config.name
    if classId == 0 then
        shortName = "全部"
    elseif #config.name > 2 then
        shortName = string.sub(config.name, 1, 6)  -- UTF8中文每个字符3字节
    end
    button.text:SetText(shortName)

    -- 选中指示器
    button.selected = button:CreateTexture(buttonName .. "Selected", "BACKGROUND")
    button.selected:SetAllPoints()
    button.selected:SetTexture("Interface\\Buttons\\UI-Listbox-Highlight2")
    button.selected:SetBlendMode("ADD")
    button.selected:SetVertexColor(0.6, 0.3, 0.9, 0.6)
    button.selected:Hide()

    -- 点击事件
    button.classId = classId
    button:SetScript("OnClick", function(self)
        TalentSoulUI_SelectClassTab(self.classId)
    end)

    -- 悬停效果
    button:SetScript("OnEnter", function(self)
        self:SetBackdropBorderColor(0.6, 0.4, 0.8, 1)
        GameTooltip:SetOwner(self, "ANCHOR_BOTTOM")
        GameTooltip:AddLine(config.color .. config.name .. "|r")
        if classId == 0 then
            GameTooltip:AddLine("显示所有职业的技能", 0.7, 0.7, 0.7)
        else
            GameTooltip:AddLine("显示该职业专属技能", 0.7, 0.7, 0.7)
        end
        GameTooltip:Show()
    end)

    button:SetScript("OnLeave", function(self)
        if selectedClassFilter ~= self.classId then
            self:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)
        end
        GameTooltip:Hide()
    end)

    return button
end

-- 初始化职业标签栏
InitClassTabs = function()
    local tabFrame = TalentSoulMainFrameClassTabFrame
    if not tabFrame then return end

    -- 清除旧的标签按钮
    for _, btn in pairs(classTabButtons) do
        if btn then btn:Hide() end
    end
    classTabButtons = {}

    -- 创建标签按钮
    local xOffset = 5
    for i, classId in ipairs(CLASS_ORDER) do
        local button = CreateClassTabButton(tabFrame, i, classId)
        if button then
            button:SetPoint("LEFT", tabFrame, "LEFT", xOffset, 0)
            xOffset = xOffset + 60
            classTabButtons[classId] = button
        end
    end

    -- 默认选中玩家职业
    local playerClass = TalentSoulData:GetPlayerClass()
    selectedClassFilter = playerClass
    UpdateClassTabSelection()
end

-- 更新职业标签选中状态
UpdateClassTabSelection = function()
    for classId, button in pairs(classTabButtons) do
        if button then
            if classId == selectedClassFilter then
                button.selected:Show()
                button:SetBackdropBorderColor(0.6, 0.4, 0.8, 1)
                button:SetBackdropColor(0.2, 0.1, 0.3, 0.95)
            else
                button.selected:Hide()
                button:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)
                button:SetBackdropColor(0.1, 0.1, 0.15, 0.9)
            end
        end
    end
end

-- 选择职业标签
function TalentSoulUI_SelectClassTab(classId)
    selectedClassFilter = classId
    UpdateClassTabSelection()

    -- 清除选中的技能
    TalentSoulData:SetSelectedSkill(nil)

    -- 清空缓存，确保显示新数据
    TalentSoulData:InvalidateSkillCache()

    -- 立即更新UI（显示空列表）
    TalentSoulUI_UpdateSkillList()
    UpdateSkillDetail()

    -- 请求该职业的技能数据
    PrintDebug("请求职业 " .. classId .. " 的技能数据...")
    TalentSoulData:EnsureSkillData(function(success, skills)
        if success then
            TalentSoulUI_UpdateSkillList()
            UpdateSkillDetail()
            UpdatePointsDisplay()
            PrintDebug("职业 " .. classId .. " 数据加载完成，共 " .. #skills .. " 个技能")
        else
            PrintDebug("职业 " .. classId .. " 数据加载失败")
        end
    end, classId)

    PrintDebug("选择职业标签: " .. (TalentSoulConfig.Classes[classId] and TalentSoulConfig.Classes[classId].name or "未知"))
end

-- 获取当前筛选后的技能列表（服务器已按职业过滤，直接返回缓存）
local function GetFilteredSkillList()
    return TalentSoulData:GetSkillList()
end

-- 更新技能列表
function TalentSoulUI_UpdateSkillList()
    local skills = GetFilteredSkillList()
    local offset = FauxScrollFrame_GetOffset(TalentSoulMainFrameSkillListFrameScrollFrame) or 0
    local selectedSkill = TalentSoulData:GetSelectedSkill()

    FauxScrollFrame_Update(TalentSoulMainFrameSkillListFrameScrollFrame, #skills, MAX_SKILL_BUTTONS, SKILL_BUTTON_HEIGHT)

    for i = 1, MAX_SKILL_BUTTONS do
        local button = skillButtons[i]
        if not button then
            button = CreateSkillButton(TalentSoulMainFrameSkillListFrame, i)
            button:SetPoint("TOPLEFT", TalentSoulMainFrameSkillListFrame, "TOPLEFT", 10, -25 - (i - 1) * (SKILL_BUTTON_HEIGHT + 2))
            skillButtons[i] = button
        end

        local index = offset + i
        if index <= #skills then
            local skill = skills[index]
            button.skillData = skill
            button.icon:SetTexture(skill.icon)
            button.name:SetText(skill.name)

            -- 计算总等级
            local totalLevel = TalentSoulData:GetSkillTotalLevel(skill)
            local maxTotal = (skill.gcdMax or 10) + (skill.cooldownMax or 10) + (skill.costMax or 10) + (skill.damageMax or 10)

            if totalLevel > 0 then
                button.level:SetText(string.format("|cffFFD700等级: %d|r", totalLevel))
            else
                button.level:SetText("|cff666666未学习|r")
            end

            -- 高亮选中的技能
            if selectedSkill and selectedSkill.spellId == skill.spellId then
                button.selected:Show()
                button:SetBackdropBorderColor(0.6, 0.4, 0.8, 1)
            else
                button.selected:Hide()
                button:SetBackdropBorderColor(0.3, 0.3, 0.4, 1)
            end

            button:Show()
        else
            button.skillData = nil
            button:Hide()
        end
    end
end

-- 更新技能详情
UpdateSkillDetail = function()
    local skill = TalentSoulData:GetSelectedSkill()
    local detailFrame = TalentSoulMainFrameDetailFrame

    if not skill then
        detailFrame.SkillIcon = detailFrame.SkillIcon or _G[detailFrame:GetName() .. "SkillIcon"]
        detailFrame.SkillName = detailFrame.SkillName or _G[detailFrame:GetName() .. "SkillName"]
        detailFrame.SkillDesc = detailFrame.SkillDesc or _G[detailFrame:GetName() .. "SkillDesc"]

        if detailFrame.SkillIcon then
            detailFrame.SkillIcon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
        end
        if detailFrame.SkillName then
            detailFrame.SkillName:SetText("选择一个技能")
        end
        if detailFrame.SkillDesc then
            detailFrame.SkillDesc:SetText("从左侧列表选择一个技能来查看详情")
        end

        -- 隐藏所有属性按钮
        for i = 1, 4 do
            if attributeButtons[i] then
                attributeButtons[i]:Hide()
            end
        end
        return
    end

    -- 获取或创建UI元素引用
    detailFrame.SkillIcon = detailFrame.SkillIcon or _G[detailFrame:GetName() .. "SkillIcon"]
    detailFrame.SkillName = detailFrame.SkillName or _G[detailFrame:GetName() .. "SkillName"]
    detailFrame.SkillDesc = detailFrame.SkillDesc or _G[detailFrame:GetName() .. "SkillDesc"]

    -- 更新技能信息
    if detailFrame.SkillIcon then
        detailFrame.SkillIcon:SetTexture(skill.icon)
    end
    if detailFrame.SkillName then
        detailFrame.SkillName:SetText(skill.name)
    end
    if detailFrame.SkillDesc then
        local desc = skill.description or ""
        if skill.requiredPoints and skill.requiredPoints > 0 then
            desc = desc .. "\n|cffFFAA00需要 " .. skill.requiredPoints .. " 天赋点解锁|r"
        end
        detailFrame.SkillDesc:SetText(desc)
    end

    -- 更新属性按钮
    local attrs = {
        { level = skill.gcdLevel or 0, max = skill.gcdMax or 10 },
        { level = skill.cooldownLevel or 0, max = skill.cooldownMax or 10 },
        { level = skill.costLevel or 0, max = skill.costMax or 10 },
        { level = skill.damageLevel or 0, max = skill.damageMax or 10 },
    }

    for i = 1, 4 do
        local button = attributeButtons[i]
        if not button then
            button = CreateAttributeButton(detailFrame, i, i)
            button:SetPoint("TOPLEFT", detailFrame, "TOPLEFT", 15, -100 - (i - 1) * 58)
            attributeButtons[i] = button
        end

        local attr = attrs[i]
        button.level:SetText(string.format("%d / %d", attr.level, attr.max))

        -- 更新进度条
        local progress = attr.level / attr.max
        button.progress:SetWidth(math.max(1, 148 * progress))

        -- 更新按钮状态
        local canUpgrade, reason = TalentSoulData:CanUpgradeAttribute(skill, i)
        if canUpgrade then
            button.upgradeBtn:Enable()
        else
            button.upgradeBtn:Disable()
        end

        -- 等级颜色
        if attr.level >= attr.max then
            button.level:SetTextColor(1, 0.5, 0, 1)  -- 满级橙色
        elseif attr.level > 0 then
            button.level:SetTextColor(1, 1, 1, 1)    -- 有等级白色
        else
            button.level:SetTextColor(0.5, 0.5, 0.5, 1)  -- 未学习灰色
        end

        button:Show()
    end
end

-- 更新天赋点显示
UpdatePointsDisplay = function()
    local total = TalentSoulData:GetTotalPoints()
    local available = TalentSoulData:GetAvailablePoints()
    local used = TalentSoulData:GetUsedPoints()

    local pointsText = TalentSoulMainFramePointsText
    if pointsText then
        if available > 0 then
            pointsText:SetText(string.format("|cff00FF00可用: %d|r / 已用: %d / 总计: %d", available, used, total))
        else
            pointsText:SetText(string.format("可用: %d / 已用: %d / 总计: %d", available, used, total))
        end
    end
end

-- 选择技能
function TalentSoulUI_SelectSkill(skill)
    TalentSoulData:SetSelectedSkill(skill)
    TalentSoulUI_UpdateSkillList()
    UpdateSkillDetail()
    PrintDebug("选中技能: " .. (skill and skill.name or "无"))
end

-- 升级属性
function TalentSoulUI_UpgradeAttribute(attrType)
    local skill = TalentSoulData:GetSelectedSkill()
    if not skill then
        PrintError("请先选择一个技能")
        return
    end

    local canUpgrade, reason = TalentSoulData:CanUpgradeAttribute(skill, attrType)
    if not canUpgrade then
        PrintError(reason or "无法升级")
        return
    end

    TalentSoulComm:UpgradeSkill(skill.spellId, attrType, function(success, result, params)
        if success and result.upgrade then
            -- 更新本地数据
            TalentSoulData:UpdateSkillLevel(skill.spellId, attrType, result.level)
            TalentSoulData:AddUsedPoint(1)

            -- 刷新UI
            TalentSoulUI_UpdateSkillList()
            UpdateSkillDetail()
            UpdatePointsDisplay()
            -- 服务器已发送升级成功消息，这里不再重复输出
        else
            local errorMsg = result and result.message or "升级失败"
            PrintError(errorMsg)
        end
    end)
end

-- 显示重置确认
function TalentSoulUI_ShowResetConfirm()
    local used = TalentSoulData:GetUsedPoints()
    if used <= 0 then
        -- 服务器会发送"你还没有使用任何天赋点"消息
        return
    end

    TalentSoulResetConfirmFrame:Show()
end

-- 执行重置
function TalentSoulUI_DoReset()
    TalentSoulComm:ResetTalent(function(success, result, params)
        if success and result.reset then
            -- 清空本地缓存
            TalentSoulData:ResetUsedPoints()
            TalentSoulData:InvalidateSkillCache()
            TalentSoulData:SetSelectedSkill(nil)

            -- 立即更新UI显示（清空状态）
            TalentSoulUI_UpdateSkillList()
            UpdateSkillDetail()
            UpdatePointsDisplay()
            -- 服务器已发送重置成功消息，这里不再重复输出

            -- 重新从服务器加载最新数据
            local classId = selectedClassFilter or TalentSoulData:GetPlayerClass()
            TalentSoulData:EnsureSkillData(function(loadSuccess, skills)
                if loadSuccess then
                    TalentSoulUI_UpdateSkillList()
                    UpdateSkillDetail()
                    UpdatePointsDisplay()
                    PrintDebug("重置后数据刷新完成")
                end
            end, classId)
        else
            local errorMsg = result and result.message or "重置失败"
            PrintError(errorMsg)
        end
    end)
end

-- 刷新数据
function TalentSoulUI_RefreshData()
    -- 使用当前选中的职业ID进行刷新
    local classId = selectedClassFilter or TalentSoulData:GetPlayerClass()
    TalentSoulData:Refresh(function(success, skills)
        if success then
            TalentSoulUI_UpdateSkillList()
            UpdateSkillDetail()
            UpdatePointsDisplay()
        else
            PrintError("数据刷新失败")
        end
    end, classId)
end

-- 切换窗口显示
function TalentSoulUI_Toggle()
    if TalentSoulMainFrame:IsShown() then
        TalentSoulMainFrame:Hide()
    else
        TalentSoulMainFrame:Show()
    end
end

-- 窗口加载
function TalentSoulUI_OnLoad(self)
    -- 注册可拖动
    self:RegisterForDrag("LeftButton")

    -- 设置ESC关闭
    tinsert(UISpecialFrames, "TalentSoulMainFrame")

    -- 注册斜杠命令
    SLASH_TALENTSOUL1 = "/talentsoul"
    SLASH_TALENTSOUL2 = "/ts"
    SLASH_TALENTSOUL3 = "/天赋之魂"
    SlashCmdList["TALENTSOUL"] = function(msg)
        TalentSoulUI_Toggle()
    end

    -- 初始化职业标签栏
    InitClassTabs()

    PrintDebug("天赋之魂UI加载完成")
end

-- 窗口显示
function TalentSoulUI_OnShow(self)
    -- 加载数据
    if not initialized then
        -- 设置初始天赋点（玩家等级）
        TalentSoulData:SetTalentPoints(TalentSoulData:GetPlayerLevel(), 0)
        initialized = true
    end

    TalentSoulData:EnsureSkillData(function(success, skills)
        if success then
            TalentSoulUI_UpdateSkillList()
            UpdateSkillDetail()
            UpdatePointsDisplay()
        end
    end)
end

-- 窗口隐藏
function TalentSoulUI_OnHide(self)
    PrintDebug("天赋之魂UI隐藏")
end

-- 小地图按钮加载
function TalentSoulUI_MinimapButton_OnLoad(self)
    self:RegisterForDrag("LeftButton")

    -- 设置小地图按钮位置（围绕小地图）
    local angle = 220  -- 角度位置
    local radius = 80
    local x = math.cos(math.rad(angle)) * radius
    local y = math.sin(math.rad(angle)) * radius

    self:ClearAllPoints()
    self:SetPoint("CENTER", Minimap, "CENTER", x, y)

    PrintDebug("小地图按钮加载完成")
end

-- 图标按钮（屏幕顶部可拖动）
local iconButton = nil

local function CreateIconButton()
    if iconButton then return iconButton end

    iconButton = CreateFrame("Button", "TalentSoulIconButton", UIParent)
    iconButton:SetSize(24, 24)

    -- 使用保存的位置，默认屏幕上方中间（偏右侧，避免和其他插件冲突）
    local pos = TalentSoulUIDB and TalentSoulUIDB.iconPosition or {point = "TOP", x = 100, y = -10}
    iconButton:SetPoint(pos.point or "TOP", UIParent, pos.point or "TOP", pos.x, pos.y)

    -- 使用天赋相关图标
    iconButton:SetNormalTexture("Interface\\Icons\\Spell_Holy_BlessedResillience")
    local tex = iconButton:GetNormalTexture()
    if tex then tex:SetTexCoord(0.07, 0.93, 0.07, 0.93) end

    iconButton:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")

    -- 图标下方文字
    local text = iconButton:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    text:SetPoint("BOTTOM", 0, -12)
    text:SetText("|cff9933FF天赋之魂|r")

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
            if not TalentSoulUIDB then TalentSoulUIDB = {} end
            TalentSoulUIDB.iconPosition = { point = point, x = x, y = y }
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
            if TalentSoulMainFrame then
                if TalentSoulMainFrame:IsShown() then
                    PrintDebug("隐藏窗口")
                    TalentSoulMainFrame:Hide()
                else
                    PrintDebug("显示窗口")
                    TalentSoulMainFrame:Show()
                end
            else
                PrintError("主窗口不存在")
            end
        end
    end)

    iconButton:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText("天赋之魂", 1, 1, 1)
        GameTooltip:AddLine("左键: 打开界面", 0.8, 0.8, 0.8)
        GameTooltip:AddLine("拖拽: 移动位置", 0.8, 0.8, 0.8)
        GameTooltip:AddLine(" ")
        GameTooltip:AddLine("强化你的技能属性", 0.6, 0.2, 0.9)
        GameTooltip:Show()
    end)

    iconButton:SetScript("OnLeave", function(self)
        GameTooltip:Hide()
    end)

    return iconButton
end

-- 显示/隐藏图标按钮
function TalentSoulUI_ShowIconButton()
    if not iconButton then
        CreateIconButton()
    end
    if iconButton then
        iconButton:Show()
    end
end

function TalentSoulUI_HideIconButton()
    if iconButton then
        iconButton:Hide()
    end
end

function TalentSoulUI_ToggleIconButton()
    if iconButton and iconButton:IsShown() then
        TalentSoulUI_HideIconButton()
    else
        TalentSoulUI_ShowIconButton()
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
        if addonName == "TalentSoulUI" then
            PrintDebug("天赋之魂插件已加载")
            -- 初始化保存的变量
            if not TalentSoulUIDB then
                TalentSoulUIDB = {
                    showIconButton = true,
                    iconPosition = { point = "TOP", x = 100, y = -10 }
                }
            end
        end
    elseif event == "PLAYER_LOGIN" then
        PrintDebug("玩家登录，创建图标按钮")
        -- 创建图标按钮
        if TalentSoulUIDB and TalentSoulUIDB.showIconButton ~= false then
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
        -- 玩家进入世界时初始化
        PrintDebug("玩家进入世界，初始化天赋之魂UI")
    elseif event == "PLAYER_LEVEL_UP" then
        -- 玩家升级时更新天赋点
        local newLevel = ...
        local used = TalentSoulData:GetUsedPoints()
        TalentSoulData:SetTalentPoints(newLevel, used)

        if TalentSoulMainFrame:IsShown() then
            UpdatePointsDisplay()
        end

        PrintInfo(string.format("恭喜升级！你现在有 %d 点可用天赋点", TalentSoulData:GetAvailablePoints()))
    end
end)

-- 全局函数
_G.TalentSoulUI_Toggle = TalentSoulUI_Toggle
_G.TalentSoulUI_OnLoad = TalentSoulUI_OnLoad
_G.TalentSoulUI_OnShow = TalentSoulUI_OnShow
_G.TalentSoulUI_OnHide = TalentSoulUI_OnHide
_G.TalentSoulUI_UpdateSkillList = TalentSoulUI_UpdateSkillList
_G.TalentSoulUI_SelectSkill = TalentSoulUI_SelectSkill
_G.TalentSoulUI_UpgradeAttribute = TalentSoulUI_UpgradeAttribute
_G.TalentSoulUI_ShowResetConfirm = TalentSoulUI_ShowResetConfirm
_G.TalentSoulUI_DoReset = TalentSoulUI_DoReset
_G.TalentSoulUI_RefreshData = TalentSoulUI_RefreshData
_G.TalentSoulUI_MinimapButton_OnLoad = TalentSoulUI_MinimapButton_OnLoad
_G.TalentSoulUI_ShowIconButton = TalentSoulUI_ShowIconButton
_G.TalentSoulUI_HideIconButton = TalentSoulUI_HideIconButton
_G.TalentSoulUI_ToggleIconButton = TalentSoulUI_ToggleIconButton

PrintDebug("天赋之魂UI脚本加载完成")
