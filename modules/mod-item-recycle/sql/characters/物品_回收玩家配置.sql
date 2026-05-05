-- =====================================================
-- 物品回收系统 - 角色数据库玩家配置表
-- =====================================================
-- 说明：此表存储每个玩家的个人回收设置和偏好
-- 功能：支持自动回收、手动回收、类型过滤、品质等级限制等
-- 新增：集成需求_模板和奖励_模板系统，支持"所有类型"一键开关
-- 位置：角色数据库（characters database）

DROP TABLE IF EXISTS `物品_回收玩家配置`;
CREATE TABLE `物品_回收玩家配置` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT '玩家唯一标识符',

  -- =====================================================
  -- 基础回收设置
  -- =====================================================
  `自动回收启用` tinyint(1) NOT NULL DEFAULT 0 COMMENT '是否启用自动回收功能',
  `回收间隔` int UNSIGNED NOT NULL DEFAULT 60 COMMENT '自动回收间隔时间（秒）',

  -- =====================================================
  -- 回收类型控制（支持精细化和一键控制）
  -- =====================================================
  `回收类型1` tinyint(1) NOT NULL DEFAULT 0 COMMENT '装备回收开关（武器、护甲）',
  `回收类型2` tinyint(1) NOT NULL DEFAULT 0 COMMENT '消耗品回收开关（药水、食物等）',
  `回收类型3` tinyint(1) NOT NULL DEFAULT 0 COMMENT '任务物品回收开关（任务相关道具）',
  `回收类型4` tinyint(1) NOT NULL DEFAULT 1 COMMENT '垃圾回收开关（灰色品质物品等）- 默认启用',
  `回收类型5` tinyint(1) NOT NULL DEFAULT 0 COMMENT '宝石回收开关（各种宝石）',
  `回收类型6` tinyint(1) NOT NULL DEFAULT 0 COMMENT '附魔材料回收开关（附魔用材料）',
  `回收所有类型` tinyint(1) NOT NULL DEFAULT 0 COMMENT '【新增】所有类型一键开关，启用后忽略上述具体类型设置',

  -- =====================================================
  -- 物品过滤条件
  -- =====================================================
  `最小品质` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '回收最小品质要求 (0=灰色 1=白色 2=绿色 3=蓝色 4=紫色 5=橙色 6=红色)',
  `最大品质` int UNSIGNED NOT NULL DEFAULT 2 COMMENT '回收最大品质要求 - 默认到绿色',
  `最小等级` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '回收最小物品等级要求',
  `最大等级` int UNSIGNED NOT NULL DEFAULT 60 COMMENT '回收最大物品等级要求',
  `保护装备` tinyint(1) NOT NULL DEFAULT 1 COMMENT '是否保护已装备的物品不被回收 - 默认启用',

  -- =====================================================
  -- 物品过滤系统
  -- =====================================================
  `过滤物品列表` TEXT DEFAULT NULL COMMENT '不回收的物品ID列表，用逗号分隔，例如：12345,67890,11111',



  PRIMARY KEY (`玩家GUID`) USING BTREE,
  INDEX `idx_auto_recycle` (`自动回收启用`) USING BTREE COMMENT '自动回收查询索引'
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci
COMMENT = '玩家物品回收个人配置表 - 存储每个玩家的回收设置和偏好' ROW_FORMAT = DYNAMIC;


