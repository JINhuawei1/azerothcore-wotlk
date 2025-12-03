DROP TABLE IF EXISTS `物品_鉴定系统`;
CREATE TABLE `物品_鉴定系统`  (
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
  PRIMARY KEY (`id`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of 物品_鉴定系统
-- ----------------------------
INSERT INTO `物品_鉴定系统` (
    `注释`, `id`, `组`, `等级`, `随机几率`,
    `物品成长_系统`, `成长属性最小数量`, `成长属性最大数量`, `成长属性最小属性值`, `成长属性最大属性值`,
    `物品强化_系统`, `强化属性最小数量`, `强化属性最大数量`, `强化属性最小属性值`, `强化属性最大属性值`,
    `物品属性_模板`,
    `基础属性最小数量`, `基础属性最大数量`, `基础最小属性值`, `基础最大属性值`, `基础属性允许重复`,
    `物品属性_模板_组`, `追加属性最小数量`, `追加属性最大数量`, `追加属性最小值`, `追加属性最大值`, `追加属性允许重复`,
    `物品技能_模板_组`, `追加技能最小数量`, `追加技能最大数量`, `追加技能允许重复`,
    `技能魔次_模板_组`, `技能魔次最小数量`, `技能魔次最大数量`, `技能魔次最小魔次`, `技能魔次最大魔次`, `技能魔次允许重复`,
    `需求_模板`, `符文系统_符文`, `符文凹槽最小数量`, `符文凹槽最大数量`, `技能模板_套装_组`, `公告模板`
) VALUES (
    '基础鉴定模板 - 适用于1-20级装备', 1, 1, 1, 100,
    '1', 0, 0, 0, 0,
    '1', 0, 0, 0, 0,
    '0',
    5, 10, 10, 10, 0,
    '1', 5, 10, 10, 20, 0,
    '1', 5, 10, 0,
    '1', 3, 5, 500, 1000, 0,
    0, '1', 5, 5, '1', 1
);

SET FOREIGN_KEY_CHECKS = 1;
