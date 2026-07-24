SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS `_图鉴系统`;
CREATE TABLE `_图鉴系统` (
  `注释` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '每个图鉴栏位的唯一ID',
  `一级菜单名称` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `一级菜单图标` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `二级菜单名称1` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `二级菜单名称2` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `二级菜单图标` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `第几页` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '每页不超过28项；1=小宠物，2=坐骑',
  `等级` int UNSIGNED NOT NULL DEFAULT 1,
  `最大等级` int UNSIGNED NOT NULL DEFAULT 1,
  `物品entry` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '激活时提交并消耗的副本掉落物品',
  `套装ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '本版本固定为0，不启用套装效果',
  `激活需求` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '对应 _模板_需求.id',
  `激活后执行GM命令` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '激活图鉴后执行GM命令',
  `属性生效模式` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=固定全属性值，1=装备属性，2=全属性百分比',
  `固定全属性值` decimal(65,0) NOT NULL DEFAULT 0 COMMENT '仅属性生效模式=0时生效',
  `全属性百分比` decimal(65,0) NOT NULL DEFAULT 0 COMMENT '仅属性生效模式=2时生效；100表示全属性+100%',
  PRIMARY KEY (`物品entry`) USING BTREE,
  UNIQUE KEY `uk_图鉴ID` (`id`) USING BTREE,
  KEY `idx_套装ID` (`套装ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '副本珍藏图鉴' ROW_FORMAT = DYNAMIC;

-- 清理旧版深渊装备图鉴生成的需求模板，再写入本版31个独立收集需求。
DELETE FROM `_模板_需求`
WHERE `注释` LIKE '深渊修仙图鉴激活-%'
   OR `id` BETWEEN 947001 AND 947035;

INSERT INTO `_模板_需求`
(`注释`, `id`, `消耗金币`, `是否消耗物品`, `消耗物品`, `客户端显示`)
VALUES
('图鉴激活·副本小宠物·绿翼鹦鹉', 947001, 0, 0, '8492 1', '提交 绿翼鹦鹉 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·邪恶的南瓜娃娃', 947002, 0, 0, '33154 1', '提交 邪恶的南瓜娃娃 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·凤凰宝宝', 947003, 0, 0, '35504 1', '提交 凤凰宝宝 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·吸血蝙蝠宝宝', 947004, 0, 0, '38658 1', '提交 吸血蝙蝠宝宝 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·变异幼龙', 947005, 0, 0, '48114 1', '提交 变异幼龙 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·拉扎什幼龙', 947006, 0, 0, '48126 1', '提交 拉扎什幼龙 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·毒毒', 947007, 0, 0, '50446 1', '提交 毒毒 x1，永久获得全属性+100%'),
('图鉴激活·副本小宠物·冰片', 947008, 0, 0, '53641 1', '提交 冰片 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·死亡军马', 947009, 0, 0, '13335 1', '提交 死亡军马的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·拉扎什迅猛龙', 947010, 0, 0, '19872 1', '提交 拉扎什迅猛龙 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·迅捷祖利安猛虎', 947011, 0, 0, '19902 1', '提交 迅捷祖利安猛虎 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·蓝色其拉共鸣水晶', 947012, 0, 0, '21218 1', '提交 蓝色其拉共鸣水晶 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·红色其拉共鸣水晶', 947013, 0, 0, '21321 1', '提交 红色其拉共鸣水晶 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·绿色其拉共鸣水晶', 947014, 0, 0, '21323 1', '提交 绿色其拉共鸣水晶 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·黄色其拉共鸣水晶', 947015, 0, 0, '21324 1', '提交 黄色其拉共鸣水晶 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·炽热战马', 947016, 0, 0, '30480 1', '提交 炽热战马的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·奥的灰烬', 947017, 0, 0, '32458 1', '提交 奥的灰烬 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·乌鸦之神', 947018, 0, 0, '32768 1', '提交 乌鸦之神的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·迅捷白色陆行鸟', 947019, 0, 0, '35513 1', '提交 迅捷白色陆行鸟 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·无头骑士', 947020, 0, 0, '37012 1', '提交 无头骑士的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·大型美酒节科多兽', 947021, 0, 0, '37828 1', '提交 大型美酒节科多兽 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·青铜幼龙', 947022, 0, 0, '43951 1', '提交 青铜幼龙的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·碧蓝幼龙', 947023, 0, 0, '43952 1', '提交 碧蓝幼龙的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·蓝色幼龙', 947024, 0, 0, '43953 1', '提交 蓝色幼龙的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·暮光幼龙', 947025, 0, 0, '43954 1', '提交 暮光幼龙的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·黑色幼龙', 947027, 0, 0, '43986 1', '提交 黑色幼龙的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·蓝色始祖幼龙', 947029, 0, 0, '44151 1', '提交 蓝色始祖幼龙的缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·米米尔隆的头部', 947030, 0, 0, '45693 1', '提交 米米尔隆的头部 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·奥妮克希亚座龙', 947033, 0, 0, '49636 1', '提交 奥妮克希亚座龙缰绳 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·爱情火箭', 947034, 0, 0, '50250 1', '提交 爱情火箭 x1，永久获得全属性+100%'),
('图鉴激活·副本坐骑·无敌', 947035, 0, 0, '50818 1', '提交 无敌的缰绳 x1，永久获得全属性+100%');

INSERT INTO `_图鉴系统`
(`注释`, `id`, `一级菜单名称`, `一级菜单图标`, `二级菜单名称1`, `二级菜单名称2`, `二级菜单图标`,
 `第几页`, `等级`, `最大等级`, `物品entry`, `套装ID`, `激活需求`, `激活后执行GM命令`,
 `属性生效模式`, `固定全属性值`, `全属性百分比`)
VALUES
('小宠物·绿翼鹦鹉·死亡矿井·迪菲亚海盗掉落', 1, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 8492, 0, 947001, '', 2, 0, 100),
('小宠物·邪恶的南瓜娃娃·血色修道院·大检察官怀特迈恩常驻掉落', 2, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 33154, 0, 947002, '', 2, 0, 100),
('小宠物·凤凰宝宝·魔导师平台·凯尔萨斯掉落', 3, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 35504, 0, 947003, '', 2, 0, 100),
('小宠物·吸血蝙蝠宝宝·卡拉赞·特里斯黯血王子掉落', 4, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 38658, 0, 947004, '', 2, 0, 100),
('小宠物·变异幼龙·哀嚎洞穴·变异破坏者/守护者掉落', 5, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 48114, 0, 947005, '', 2, 0, 100),
('小宠物·拉扎什幼龙·祖尔格拉布·拉扎什迅猛龙掉落', 6, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 48126, 0, 947006, '', 2, 0, 100),
('小宠物·毒毒·影牙城堡·大法师阿鲁高常驻掉落', 7, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 50446, 0, 947007, '', 2, 0, 100),
('小宠物·冰片·奴隶围栏·夸格米拉常驻掉落', 8, '副本珍藏', '', '小宠物', '稀有小宠物', '', 1, 1, 1, 53641, 0, 947008, '', 2, 0, 100),
('坐骑·死亡军马·斯坦索姆·瑞文戴尔男爵掉落', 9, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 13335, 0, 947009, '', 2, 0, 100),
('坐骑·拉扎什迅猛龙·祖尔格拉布·血领主曼多基尔掉落', 10, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 19872, 0, 947010, '', 2, 0, 100),
('坐骑·迅捷祖利安猛虎·祖尔格拉布·高阶祭司塞卡尔掉落', 11, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 19902, 0, 947011, '', 2, 0, 100),
('坐骑·蓝色其拉共鸣水晶·安其拉神殿·其拉怪物掉落', 12, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 21218, 0, 947012, '', 2, 0, 100),
('坐骑·红色其拉共鸣水晶·安其拉神殿·其拉怪物掉落', 13, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 21321, 0, 947013, '', 2, 0, 100),
('坐骑·绿色其拉共鸣水晶·安其拉神殿·其拉怪物掉落', 14, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 21323, 0, 947014, '', 2, 0, 100),
('坐骑·黄色其拉共鸣水晶·安其拉神殿·其拉怪物掉落', 15, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 21324, 0, 947015, '', 2, 0, 100),
('坐骑·炽热战马·卡拉赞·猎手阿图门掉落', 16, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 30480, 0, 947016, '', 2, 0, 100),
('坐骑·奥的灰烬·风暴要塞·凯尔萨斯掉落', 17, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 32458, 0, 947017, '', 2, 0, 100),
('坐骑·乌鸦之神·英雄塞泰克大厅·安苏掉落', 18, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 32768, 0, 947018, '', 2, 0, 100),
('坐骑·迅捷白色陆行鸟·英雄魔导师平台·凯尔萨斯掉落', 19, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 35513, 0, 947019, '', 2, 0, 100),
('坐骑·无头骑士·血色修道院·塞满战利品的南瓜', 20, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 37012, 0, 947020, '', 2, 0, 100),
('坐骑·大型美酒节科多兽·黑石深渊·达格兰·索瑞森大帝常驻掉落', 21, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 37828, 0, 947021, '', 2, 0, 100),
('坐骑·青铜幼龙·英雄净化斯坦索姆·永恒腐蚀者掉落', 22, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 43951, 0, 947022, '', 2, 0, 100),
('坐骑·碧蓝幼龙·永恒之眼·阿莱克丝塔萨的礼物', 23, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 43952, 0, 947023, '', 2, 0, 100),
('坐骑·蓝色幼龙·永恒之眼·阿莱克丝塔萨的礼物', 24, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 43953, 0, 947024, '', 2, 0, 100),
('坐骑·暮光幼龙·黑曜石圣殿·萨塔里奥掉落', 25, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 43954, 0, 947025, '', 2, 0, 100),
('坐骑·黑色幼龙·黑曜石圣殿·萨塔里奥掉落', 27, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 43986, 0, 947027, '', 2, 0, 100),
('坐骑·蓝色始祖幼龙·英雄乌特加德之巅·残忍的斯卡迪掉落', 29, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 44151, 0, 947029, '', 2, 0, 100),
('坐骑·米米尔隆的头部·奥杜尔·尤格萨隆掉落', 30, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 45693, 0, 947030, '', 2, 0, 100),
('坐骑·奥妮克希亚座龙·奥妮克希亚的巢穴·奥妮克希亚掉落', 33, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 49636, 0, 947033, '', 2, 0, 100),
('坐骑·爱情火箭·影牙城堡·大法师阿鲁高常驻掉落', 34, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 50250, 0, 947034, '', 2, 0, 100),
('坐骑·无敌·冰冠堡垒·巫妖王掉落', 35, '副本珍藏', '', '坐骑', '稀有坐骑', '', 2, 1, 1, 50818, 0, 947035, '', 2, 0, 100);

-- 将节日奖励物品复制为常驻副本首领的直接掉落；原有节日奖励箱来源保留不变。
-- 不依赖 game_event，节日未开启时也可从对应副本首领获得。
INSERT INTO creature_loot_template
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
(3977, 33154, 0, 1.0, 0, 1, 0, 1, 1, 'High Inquisitor Whitemane - Sinister Squashling (permanent dungeon drop)'),
(4275, 50250, 0, 1.0, 0, 1, 0, 1, 1, 'Archmage Arugal - Big Love Rocket (permanent dungeon drop)'),
(4275, 50446, 0, 1.0, 0, 1, 0, 1, 1, 'Archmage Arugal - Toxic Wasteling (permanent dungeon drop)'),
(9019, 37828, 0, 1.0, 0, 1, 0, 1, 1, 'Emperor Dagran Thaurissan - Great Brewfest Kodo (permanent dungeon drop)'),
(17942, 53641, 0, 1.0, 0, 1, 0, 1, 1, 'Quagmirran - Ice Chip (permanent dungeon drop)')
ON DUPLICATE KEY UPDATE
 `Chance` = VALUES(`Chance`),
 `QuestRequired` = VALUES(`QuestRequired`),
 `LootMode` = VALUES(`LootMode`),
 `MinCount` = VALUES(`MinCount`),
 `MaxCount` = VALUES(`MaxCount`),
 `Comment` = VALUES(`Comment`);

-- 四件联盟/部落专属坐骑已移出图鉴；恢复其官方物品说明，不改动阵营限制和原有掉落。
UPDATE item_template
SET `description` = RTRIM(
  TRIM(TRAILING CHAR(13) FROM
    TRIM(TRAILING CHAR(10) FROM
      LEFT(`description`, LOCATE('|cff00ff00图鉴加成：+100%全属性', `description`) - 1)
    )
  )
)
WHERE `entry` IN (43959, 44083, 49044, 49046)
  AND LOCATE('|cff00ff00图鉴加成：+100%全属性', `description`) > 0;

UPDATE item_template_locale
SET `Description` = RTRIM(
  TRIM(TRAILING CHAR(13) FROM
    TRIM(TRAILING CHAR(10) FROM
      LEFT(`Description`, LOCATE('|cff00ff00图鉴加成：+100%全属性', `Description`) - 1)
    )
  )
)
WHERE `locale` = 'zhCN'
  AND `ID` IN (43959, 44083, 49044, 49046)
  AND LOCATE('|cff00ff00图鉴加成：+100%全属性', `Description`) > 0;

-- 保留官方说明，先移除旧版图鉴说明；重复导入时不会叠加说明或空白行。
UPDATE item_template it
JOIN `_图鉴系统` tj ON tj.`物品entry` = it.`entry`
SET it.`description` = RTRIM(
  TRIM(TRAILING CHAR(13) FROM
    TRIM(TRAILING CHAR(10) FROM
      LEFT(it.`description`, LOCATE('|cff00ff00图鉴加成：+100%全属性', it.`description`) - 1)
    )
  )
)
WHERE tj.`物品entry` > 0
  AND LOCATE('|cff00ff00图鉴加成：+100%全属性', it.`description`) > 0;

-- 在官方说明底部追加两行绿色文字：加成一行，首领/宝箱与副本掉落信息一行。
UPDATE item_template it
JOIN `_图鉴系统` tj ON tj.`物品entry` = it.`entry`
SET it.`description` = CONCAT(
  COALESCE(it.`description`, ''),
  IF(COALESCE(it.`description`, '') = '', '', CHAR(10)),
  '|cff00ff00图鉴加成：+100%全属性|r',
  CHAR(10),
  '|cff00ff00掉落：',
  REPLACE(
    REPLACE(
      SUBSTRING(tj.`注释`, CHAR_LENGTH(SUBSTRING_INDEX(tj.`注释`, '·', 3)) + 2),
      '常驻掉落',
      ''
    ),
    '掉落',
    ''
  ),
  '，',
  SUBSTRING_INDEX(SUBSTRING_INDEX(tj.`注释`, '·', 3), '·', -1),
  IF(tj.`注释` LIKE '%常驻掉落%', '（常驻1%）', ''),
  '|r'
);

-- 图鉴物品允许重复持有：maxcount=0 表示不限数量；不改动其他物品属性和旗标。
UPDATE item_template it
JOIN `_图鉴系统` tj ON tj.`物品entry` = it.`entry`
SET it.`maxcount` = 0
WHERE tj.`物品entry` > 0
  AND it.`maxcount` <> 0;

-- 中文客户端优先读取 item_template_locale；先移除旧版中文图鉴说明，再同步新的两行格式。
UPDATE item_template_locale loc
JOIN `_图鉴系统` tj ON tj.`物品entry` = loc.`ID`
SET loc.`Description` = RTRIM(
  TRIM(TRAILING CHAR(13) FROM
    TRIM(TRAILING CHAR(10) FROM
      LEFT(loc.`Description`, LOCATE('|cff00ff00图鉴加成：+100%全属性', loc.`Description`) - 1)
    )
  )
)
WHERE loc.`locale` = 'zhCN'
  AND tj.`物品entry` > 0
  AND LOCATE('|cff00ff00图鉴加成：+100%全属性', loc.`Description`) > 0;

UPDATE item_template_locale loc
JOIN item_template it ON it.`entry` = loc.`ID`
JOIN `_图鉴系统` tj ON tj.`物品entry` = it.`entry`
SET loc.`Description` = CONCAT(
  COALESCE(loc.`Description`, ''),
  IF(COALESCE(loc.`Description`, '') = '', '', CHAR(10)),
  SUBSTRING(it.`description`, LOCATE('|cff00ff00图鉴加成：+100%全属性', it.`description`))
)
WHERE loc.`locale` = 'zhCN'
  AND LOCATE('|cff00ff00图鉴加成：+100%全属性', it.`description`) > 0;

-- 极少数物品若没有 zhCN 本地化记录，则以基础中文模板补齐。
INSERT INTO item_template_locale (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`)
SELECT it.`entry`, 'zhCN', it.`name`, it.`description`, it.`VerifiedBuild`
FROM item_template it
JOIN `_图鉴系统` tj ON tj.`物品entry` = it.`entry`
LEFT JOIN item_template_locale loc ON loc.`ID` = it.`entry` AND loc.`locale` = 'zhCN'
WHERE loc.`ID` IS NULL;

SET FOREIGN_KEY_CHECKS = 1;
