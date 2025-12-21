-- 转身系统UI配置文件

ReincarnationConfig = {
    -- 调试模式
    DebugMode = false,

    -- 通信配置
    Communication = {
        AddonPrefix = "REINCARNATION",
        CommandPrefix = ".转身",
        Timeout = 10,
        UseAddon = true,
    },

    -- 颜色配置（暗黑主题）
    Colors = {
        -- 主背景色
        Background = { r = 0.05, g = 0.05, b = 0.08, a = 0.95 },
        -- 边框色
        Border = { r = 0.4, g = 0.2, b = 0.6, a = 1 },
        -- 高亮边框
        BorderHighlight = { r = 0.6, g = 0.3, b = 0.9, a = 1 },
        -- 标题栏
        TitleBar = { r = 0.1, g = 0.05, b = 0.15, a = 0.98 },
        -- 转身按钮
        ReincarnateButton = { r = 0.4, g = 0.1, b = 0.5, a = 0.9 },
        ReincarnateButtonHover = { r = 0.5, g = 0.2, b = 0.7, a = 1 },
        -- 查看按钮
        InfoButton = { r = 0.2, g = 0.2, b = 0.3, a = 0.9 },
        InfoButtonHover = { r = 0.3, g = 0.3, b = 0.5, a = 1 },
        -- 文字颜色
        TextNormal = { r = 0.9, g = 0.9, b = 0.9 },
        TextHighlight = { r = 1, g = 0.85, b = 0.3 },
        TextGreen = { r = 0.3, g = 1, b = 0.3 },
        TextRed = { r = 1, g = 0.3, b = 0.3 },
        TextPurple = { r = 0.7, g = 0.4, b = 1 },
    },

    -- 消息文本
    Messages = {
        Title = "转身系统",
        ReincarnateConfirm = "确定要进行转身吗？\n转身后等级将重置为1级！",
        ReincarnateSuccess = "转身成功！",
        ReincarnateFailed = "转身失败",
        NotEnoughLevel = "等级不足，需要80级",
        RequirementNotMet = "不满足转身条件",
        Loading = "正在加载数据...",
        Timeout = "请求超时，请稍后再试",
    },

    -- 属性显示配置
    Attributes = {
        { name = "力量", icon = "Interface\\Icons\\INV_Gauntlets_04", color = "|cffC69B6D" },
        { name = "敏捷", icon = "Interface\\Icons\\INV_Boots_05", color = "|cff00FF00" },
        { name = "耐力", icon = "Interface\\Icons\\INV_Shield_06", color = "|cffFF7D0A" },
        { name = "智力", icon = "Interface\\Icons\\INV_Staff_13", color = "|cff69CCF0" },
        { name = "精神", icon = "Interface\\Icons\\INV_Jewelry_Talisman_12", color = "|cffFFFFFF" },
    },

    -- 界面尺寸
    UI = {
        MainFrameWidth = 380,
        MainFrameHeight = 450,
        IconButtonSize = 24,
    },
}

_G.ReincarnationConfig = ReincarnationConfig
