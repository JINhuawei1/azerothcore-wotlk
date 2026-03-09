DROP TABLE IF EXISTS `_物品鉴定_模板`;
CREATE TABLE `_物品鉴定_模板`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT '',
  `id` int NOT NULL,
  `组` int UNSIGNED NOT NULL DEFAULT 1,
  `等级` int UNSIGNED NOT NULL DEFAULT 0,
  `随机几率` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '公式：当前几率除以一个组的几率之和',
  `物品成长_系统` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT '' COMMENT '关联mod-item-growth模块的组字段，多个组用逗号隔开',
  `成长属性最小数量` int NOT NULL DEFAULT 0 COMMENT '成长属性最小条目数量',
  `成长属性最大数量` int NOT NULL DEFAULT 0 COMMENT '成长属性最大条目数量',
  `成长属性最小属性值` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '成长属性单条最小属性值',
  `成长属性最大属性值` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '成长属性单条最大属性值',
  `物品强化_系统` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT '' COMMENT '关联mod-item-enhancement模块的组字段，多个组用逗号隔开',
  `强化属性最小数量` int NOT NULL DEFAULT 0 COMMENT '强化属性最小条目数量',
  `强化属性最大数量` int NOT NULL DEFAULT 0 COMMENT '强化属性最大条目数量',
  `强化属性最小属性值` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '强化属性单条最小属性值',
  `强化属性最大属性值` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '强化属性单条最大属性值',
  `物品属性_模板` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT '' COMMENT '关联mod-item-attributes模块的组字段，多个组用逗号隔开',
  `基础属性最小数量` int NOT NULL DEFAULT 0,
  `基础属性最大数量` int NOT NULL DEFAULT 0,
  `基础最小属性值` int UNSIGNED NOT NULL DEFAULT 0,
  `基础最大属性值` int UNSIGNED NOT NULL DEFAULT 0,
  `基础属性允许重复` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0可以重复获取；1不可重复获取',
  `物品属性_模板_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-item-attributes模块的组字段，多个组用逗号隔开，按顺序每个组取一个',
  `追加属性最小数量` int NOT NULL DEFAULT 0,
  `追加属性最大数量` int NOT NULL DEFAULT 0,
  `追加属性最小值` int NOT NULL DEFAULT 0,
  `追加属性最大值` int NOT NULL DEFAULT 0,
  `追加属性允许重复` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0可以重复获取；1不可重复获取',
  `物品技能_模板_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-item-skills模块的组字段，多个组用逗号隔开，按顺序每个组取一个',
  `追加技能最小数量` int NOT NULL DEFAULT 0,
  `追加技能最大数量` int NOT NULL DEFAULT 0,
  `追加技能允许重复` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0可以重复获取；1不可重复获取',
  `技能魔次_模板_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-magic-hit-system模块的组字段，多个组用逗号隔开',
  `技能魔次最小数量` int NOT NULL DEFAULT 0 COMMENT '技能魔次最小数量',
  `技能魔次最大数量` int NOT NULL DEFAULT 0 COMMENT '技能魔次最大数量',
  `技能魔次最小魔次` int NOT NULL DEFAULT 0 COMMENT '技能魔次最小魔次值（每个魔次的触发次数下限）',
  `技能魔次最大魔次` int NOT NULL DEFAULT 0 COMMENT '技能魔次最大魔次值（每个魔次的触发次数上限）',
  `技能魔次允许重复` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0可以重复获取；1不可重复获取',
  `需求_模板` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联mod-requirement-template模块的id字段',
  `符文系统_符文` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-rune-system模块的组字段，多个组用逗号隔开',
  `符文凹槽最小数量` int UNSIGNED NOT NULL DEFAULT 0,
  `符文凹槽最大数量` int UNSIGNED NOT NULL DEFAULT 0,
  `技能模板_套装_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-item-sets模块套装系统表的组字段，多个组用逗号隔开，随机取一个组后再随机分配该组内的套装',
  `公告模板` int UNSIGNED NULL DEFAULT 0 COMMENT '成功后公告',
  `品质颜色` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '前缀+物品名+后缀 整体默认颜色，支持|cAARRGGBB、AARRGGBB或RRGGBB',
  `物品名字前缀` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示在物品名称前面的前缀',
  `物品名字后缀` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示在物品名称后面的后缀',
  `物品名字颜色_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '按完整显示名称逐字着色，多个颜色用逗号分隔；只有1个颜色则整串同色，支持|cAARRGGBB、AARRGGBB或RRGGBB',
  `物品底部描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示在提示底部的额外描述',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '物品鉴定模板' ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of _物品鉴定_模板
-- 同组的多个模板会按随机几率加权抽取
-- 例如：组1有5个模板，几率分别为30,30,20,10,5，总几率=95
-- 抽取概率：模板1=30/95≈31.6%, 模板2=30/95≈31.6%, 模板3=20/95≈21%, 模板4=10/95≈10.5%, 模板5=5/95≈5.3%
-- ----------------------------

-- 组1 模板1: 等级1，几率30 (普通品质)
INSERT INTO `_物品鉴定_模板` VALUES ('组1-普通品质', 1, 1, 1, 30, '1', 1, 1, 1, 9, '1', 1, 1, 1, 9, '0', 1, 1, 1, 9, 0, '1', 1, 1, 1, 9, 0, '1', 1, 1, 0, '1', 1, 1, 1, 9, 0, 0, '', 0, 0, '1', 0, '', '', '', '', '');

-- 组1 模板2: 等级2，几率30 (优秀品质)
INSERT INTO `_物品鉴定_模板` VALUES ('组1-优秀品质', 2, 1, 2, 30, '1', 1, 2, 2, 10, '1', 1, 2, 2, 10, '0', 1, 2, 2, 10, 0, '1', 1, 2, 2, 10, 0, '1', 1, 1, 0, '1', 1, 2, 2, 10, 0, 0, '', 0, 0, '1', 0, '', '', '', '', '');

-- 组1 模板3: 等级3，几率20 (精良品质)
INSERT INTO `_物品鉴定_模板` VALUES ('组1-精良品质', 3, 1, 3, 20, '1', 1, 3, 3, 11, '1', 1, 3, 3, 11, '0', 1, 3, 3, 11, 0, '1', 1, 3, 3, 11, 0, '1', 1, 1, 0, '1', 1, 3, 3, 11, 0, 0, '', 0, 0, '1', 0, '', '', '', '', '');

-- 组1 模板4: 等级4，几率10 (史诗品质)
INSERT INTO `_物品鉴定_模板` VALUES ('组1-史诗品质', 4, 1, 4, 10, '1', 1, 4, 4, 12, '1', 1, 4, 4, 12, '0', 1, 4, 4, 12, 0, '1', 1, 4, 4, 12, 0, '1', 1, 1, 0, '1', 1, 4, 4, 12, 0, 0, '', 0, 0, '1', 0, '', '', '', '', '');

-- 组1 模板5: 等级5，几率5 (传说品质)
INSERT INTO `_物品鉴定_模板` VALUES ('组1-传说品质', 5, 1, 5, 5, '1', 1, 5, 5, 15, '1', 1, 5, 5, 15, '0', 1, 5, 5, 15, 0, '1', 1, 5, 5, 15, 0, '1', 1, 1, 0, '1', 1, 5, 5, 15, 0, 0, '', 0, 0, '1', 0, '', '', '', '', '');

SET FOREIGN_KEY_CHECKS = 1;
