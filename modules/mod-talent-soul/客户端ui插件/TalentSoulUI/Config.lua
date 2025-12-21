-- 天赋之魂UI配置文件
-- 暗黑主题配色方案

TalentSoulConfig = {
    -- 版本信息
    Version = "1.0.0",

    -- 调试模式
    DebugMode = false,

    -- UI设置
    UI = {
        WindowWidth = 700,
        WindowHeight = 550,
        MaxSkillButtons = 12,       -- 每页显示的技能数量
        SkillButtonSize = 50,       -- 技能按钮大小
        RefreshInterval = 0.5,
    },

    -- 暗黑主题颜色配置
    Colors = {
        -- 背景色 (暗黑)
        Background = { r = 0.05, g = 0.05, b = 0.08, a = 0.95 },
        BackgroundLight = { r = 0.08, g = 0.08, b = 0.12, a = 0.90 },

        -- 边框色
        Border = { r = 0.3, g = 0.3, b = 0.4, a = 1 },
        BorderHighlight = { r = 0.6, g = 0.4, b = 0.8, a = 1 },

        -- 按钮色
        ButtonNormal = { r = 0.15, g = 0.15, b = 0.2, a = 1 },
        ButtonHover = { r = 0.25, g = 0.2, b = 0.35, a = 1 },
        ButtonPressed = { r = 0.1, g = 0.1, b = 0.15, a = 1 },
        ButtonDisabled = { r = 0.1, g = 0.1, b = 0.1, a = 0.5 },

        -- 重置按钮特殊颜色 (红色系)
        ResetButton = { r = 0.6, g = 0.1, b = 0.1, a = 1 },
        ResetButtonHover = { r = 0.8, g = 0.2, b = 0.2, a = 1 },

        -- 升级按钮颜色 (紫色系)
        UpgradeButton = { r = 0.4, g = 0.2, b = 0.6, a = 1 },
        UpgradeButtonHover = { r = 0.5, g = 0.3, b = 0.7, a = 1 },

        -- 文字颜色
        Title = "|cff9933FF",
        Normal = "|cffDDDDDD",
        Highlight = "|cffFFFFFF",
        Success = "|cff00FF00",
        Error = "|cffFF0000",
        Warning = "|cffFFAA00",
        Info = "|cff00BFFF",
        Muted = "|cff888888",

        -- 天赋点颜色
        TalentPoint = "|cffFFD700",
        TalentPointAvailable = "|cff00FF00",
        TalentPointUsed = "|cffFF6600",

        -- 属性颜色
        GCD = "|cff00FFFF",          -- 公共CD - 青色
        Cooldown = "|cff0088FF",     -- 技能冷却 - 蓝色
        Cost = "|cff00FF88",         -- 消耗 - 绿色
        Damage = "|cffFF4400",       -- 伤害 - 橙红色

        -- 等级颜色
        LevelMax = "|cffFF8800",     -- 满级
        LevelNormal = "|cffFFFFFF",  -- 普通
        LevelZero = "|cff666666",    -- 未学习
    },

    -- 职业配置
    Classes = {
        [0]  = { name = "全职业",     color = "|cffFFFFFF", icon = "Interface\\Icons\\Spell_Holy_InnerFire" },
        [1]  = { name = "战士",       color = "|cffc79c6e", icon = "Interface\\Icons\\ClassIcon_Warrior" },
        [2]  = { name = "圣骑士",     color = "|cfff58cba", icon = "Interface\\Icons\\ClassIcon_Paladin" },
        [3]  = { name = "猎人",       color = "|cffabd473", icon = "Interface\\Icons\\ClassIcon_Hunter" },
        [4]  = { name = "盗贼",       color = "|cfffff569", icon = "Interface\\Icons\\ClassIcon_Rogue" },
        [5]  = { name = "牧师",       color = "|cffffffff", icon = "Interface\\Icons\\ClassIcon_Priest" },
        [6]  = { name = "死亡骑士",   color = "|cffc41f3b", icon = "Interface\\Icons\\ClassIcon_DeathKnight" },
        [7]  = { name = "萨满祭司",   color = "|cff0070de", icon = "Interface\\Icons\\ClassIcon_Shaman" },
        [8]  = { name = "法师",       color = "|cff69ccf0", icon = "Interface\\Icons\\ClassIcon_Mage" },
        [9]  = { name = "术士",       color = "|cff9482c9", icon = "Interface\\Icons\\ClassIcon_Warlock" },
        [11] = { name = "德鲁伊",     color = "|cffff7d0a", icon = "Interface\\Icons\\ClassIcon_Druid" },
    },

    -- 升级类型
    UpgradeTypes = {
        [1] = { name = "公共CD",     key = "gcd",      color = "|cff00FFFF", icon = "Interface\\Icons\\Spell_Holy_BorrowedTime" },
        [2] = { name = "技能冷却",   key = "cooldown", color = "|cff0088FF", icon = "Interface\\Icons\\Spell_Frost_Wisp" },
        [3] = { name = "技能消耗",   key = "cost",     color = "|cff00FF88", icon = "Interface\\Icons\\Spell_Nature_EnchantArmor" },
        [4] = { name = "伤害加成",   key = "damage",   color = "|cffFF4400", icon = "Interface\\Icons\\Ability_Warrior_Rampage" },
    },

    -- 通信设置
    Communication = {
        CommandPrefix = ".天赋之魂",
        QueryInterval = 2,
        Timeout = 10,
        MaxRetries = 3,
        AddonPrefix = "TALENTSOUL",   -- Addon 消息前缀（需与服务器端保持一致）
        UseAddon = true,               -- 默认启用 Addon 通信
    },

    -- 提示信息
    Messages = {
        NoSkills = "当前职业没有可用技能",
        SelectSkill = "请选择要强化的技能",
        UpgradeSuccess = "技能强化成功",
        UpgradeFailed = "技能强化失败",
        ResetSuccess = "天赋重置成功，返还了 %d 点天赋点",
        ResetFailed = "天赋重置失败",
        ResetConfirm = "确定要重置所有天赋吗？\n这将返还你所有已使用的天赋点。",
        NotEnoughPoints = "天赋点不足",
        MaxLevel = "已达到最高等级",
        RequireTalentPoints = "需要 %d 天赋点才能学习此技能",
        Loading = "正在加载数据...",
        Timeout = "服务器响应超时",
    },
}

_G.TalentSoulConfig = TalentSoulConfig
