-- ============================================================
-- 自动宣传审核 - 任务配置表 (world 数据库)
-- 幂等迁移：重复导入只确保默认任务存在，不覆盖运营配置。
-- ============================================================

CREATE TABLE IF NOT EXISTS `_宣传审核任务` (
  `任务ID`                 INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '宣传任务唯一ID',
  `任务名称`               VARCHAR(128) NOT NULL DEFAULT '' COMMENT '任务显示名称',
  `启用`                   TINYINT NOT NULL DEFAULT 1 COMMENT '是否启用，0=禁用，1=启用',
  `奖励模式`               VARCHAR(16) NOT NULL DEFAULT 'CDK' COMMENT '奖励模式：CDK 或 ITEM',
  `奖励组`                 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'mod-redemption-code 使用的兑换码分组',
  `需求ID`                 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '兑换前置需求模板ID，0=无需求',
  `奖励ID`                 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'CDK 对接的奖励模板ID',
  `物品entry`              INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ITEM 模式发放的 item_template entry',
  `物品数量`               INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'ITEM 模式发放数量',
  `账号每日上限`           INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '同一账号每日最多获得次数',
  `IP每日上限`             INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '同一IP每日最多获得次数',
  `任务每日上限`           INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '同一任务每日最多发放次数',
  `提交方式`               VARCHAR(16) NOT NULL DEFAULT 'IMAGE' COMMENT 'IMAGE=群聊截图，URL=论坛链接',
  `所需素材数量`           INT UNSIGNED NOT NULL DEFAULT 5 COMMENT '一次有效宣传要求的素材数量',
  `已删除`                 TINYINT NOT NULL DEFAULT 0 COMMENT '0=正常，1=后台已删除',
  `宣传标题`               VARCHAR(255) NOT NULL DEFAULT '' COMMENT '玩家复制和长图顶部使用的宣传标题',
  `宣传长文`               TEXT NOT NULL COMMENT '论坛、群聊和宣传长图共同使用的完整纯文本',
  `关键词`                 VARCHAR(512) NOT NULL DEFAULT '' COMMENT '自动预检关键词，使用逗号分隔',
  `连续无效封号次数`       INT UNSIGNED NOT NULL DEFAULT 3 COMMENT '连续无效达到此次数后封禁账号',
  `创建时间`               DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `更新时间`               DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`任务ID`),
  KEY `idx_宣传审核任务_启用` (`启用`),
  KEY `idx_宣传审核任务_可见` (`已删除`, `启用`)
)
COMMENT = '自动宣传审核任务配置'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

SET @promotion_schema_sql = (
  SELECT IF(
    COALESCE(MAX(EXTRA), '') LIKE '%auto_increment%',
    'SELECT 1',
    'ALTER TABLE `_宣传审核任务` MODIFY COLUMN `任务ID` INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT ''宣传任务唯一ID'''
  )
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND COLUMN_NAME = '任务ID'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_宣传审核任务` ADD COLUMN `提交方式` VARCHAR(16) NOT NULL DEFAULT ''IMAGE'' COMMENT ''IMAGE=群聊截图，URL=论坛链接'' AFTER `任务每日上限`',
    'SELECT 1'
  )
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND COLUMN_NAME = '提交方式'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_宣传审核任务` ADD COLUMN `所需素材数量` INT UNSIGNED NOT NULL DEFAULT 5 COMMENT ''一次有效宣传要求的素材数量'' AFTER `提交方式`',
    'SELECT 1'
  )
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND COLUMN_NAME = '所需素材数量'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_宣传审核任务` ADD COLUMN `已删除` TINYINT NOT NULL DEFAULT 0 COMMENT ''0=正常，1=后台已删除'' AFTER `所需素材数量`',
    'SELECT 1'
  )
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND COLUMN_NAME = '已删除'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_宣传审核任务` ADD KEY `idx_宣传审核任务_可见` (`已删除`, `启用`)',
    'SELECT 1'
  )
  FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND INDEX_NAME = 'idx_宣传审核任务_可见'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_宣传审核任务` ADD COLUMN `宣传标题` VARCHAR(255) NOT NULL DEFAULT '''' COMMENT ''玩家复制和长图顶部使用的宣传标题'' AFTER `任务每日上限`',
    'SELECT 1'
  )
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND COLUMN_NAME = '宣传标题'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_宣传审核任务` ADD COLUMN `宣传长文` TEXT NULL COMMENT ''论坛、群聊和宣传长图共同使用的完整纯文本'' AFTER `宣传标题`',
    'SELECT 1'
  )
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传审核任务'
    AND COLUMN_NAME = '宣传长文'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

UPDATE `_宣传审核任务` SET `宣传长文` = '' WHERE `宣传长文` IS NULL;
ALTER TABLE `_宣传审核任务`
  MODIFY COLUMN `宣传长文` TEXT NOT NULL COMMENT '论坛、群聊和宣传长图共同使用的完整纯文本';

SET @promotion_title = '【335艾萨拉·疯狂传说】不删档测试｜65位超大数值｜2000级突破｜幻境无限成长';
SET @promotion_body = CONCAT_WS(CHAR(10),
  '【335艾萨拉·疯狂传说版】不删档测试开启！',
  '',
  '你玩过属性爆炸的335，但你不一定玩过真正能一路成长到离谱的服。',
  '',
  '335艾萨拉·疯狂传说版，主打无限成长、技能连发、装备起飞、仙门修炼、武魂觉醒、深渊挑战、飞升套装、幻境倍率。',
  '不是上线两天毕业，不是送一身装备就结束。',
  '这个服的核心就是：越玩越强，越刷越猛，越到后期越疯狂。',
  '',
  '【一、属性无限突破】',
  '本服属性支持65位超大数值成长，后期不是百万、千万就结束。',
  '幻境倍率、飞升装备、境界系统、2000级突破、转生系统、图鉴收集、装备鉴定全部叠加。',
  '真正体验属性冲天、战力爆炸、一刀清屏的爽感。',
  '',
  '【二、幻境倍率玩法】',
  '幻境系统是本服核心玩法之一。',
  '装备属性可以通过幻境倍率不断放大，百倍、千倍、万倍都不是梦。',
  '越往高层走，怪物越强，收益越高，成长越离谱。',
  '想体验真正的数值爆炸，就进幻境。',
  '',
  '【三、天赋之魂，打造自己的核心技能】',
  '天赋之魂不是简单加点，而是让你的技能真正成长。',
  '冷却、公共CD、消耗、伤害都能不断提升和优化。',
  '核心技能养起来之后，就是高速释放、疯狂连招。',
  '只要你电脑扛得住，技能就能一直打。',
  '',
  '【四、魔次系统，一个技能触发2000次】',
  '魔次系统是本服最疯狂的爆发玩法之一。',
  '一个技能不再只是打一下，而是可以同时触发多次效果。',
  '最高可达2000次魔次触发。',
  '配合天赋之魂、装备鉴定、突破技能、武魂技能，后期就是满屏技能、满屏伤害、满屏数字。',
  '',
  '【五、装备鉴定，一件装备直接起飞】',
  '装备不是只看装等，也不是固定属性。',
  '本服拥有庞大的装备鉴定体系，属性、技能、魔次、符文槽、特殊词条都有机会鉴定出来。',
  '一件普通装备，鉴定出极品属性，直接起飞。',
  '刷装备有惊喜，鉴定装备有期待，极品全看脸。',
  '',
  '【六、飞升装备，额外再穿一整套装备】',
  '飞升系统不是普通强化。',
  '本服飞升装备拥有额外19个装备槽位，相当于角色在原本装备之外，再穿一整套飞升装备。',
  '别人只有一套装备，你能多穿一套。',
  '飞升装备还能继续成长、继续叠属性、继续拉开差距。',
  '这才是真正的后期战力核心。',
  '',
  '【七、仙门系统，选择你的修仙流派】',
  '本服拥有完整仙门体系。',
  '太虚剑宗、九霄符箓、不灭体殿、御灵魂宗，不同仙门代表不同成长方向。',
  '仙门技能、仙器、仙门修为、扩展槽位共同组成新的修仙路线。',
  '你不只是选择职业，还能选择自己的仙门传承。',
  '想走爆发、符法、肉身、魂系流派，都能找到自己的玩法。',
  '',
  '【八、武魂系统，觉醒第二套战斗核心】',
  '武魂系统是角色成长的重要组成。',
  '武魂、魂环、武魂技能、武魂装备共同构成额外战斗体系。',
  '职业技能只是基础，武魂觉醒之后才是真正开始构筑流派。',
  '天赋之魂、突破技能、武魂技能、魔次系统组合起来，每个角色都能打出不一样的效果。',
  '',
  '【九、深渊修仙，长期挑战内容拉满】',
  '深渊系统不是摆设，是一整套长期挑战玩法。',
  '章节推进、深渊首领、深渊装备、词缀、特效、遗物全部都有。',
  '你可以刷装备、凑词条、打首领、拿遗物，不断推进自己的深渊强度。',
  '喜欢长期养成和挑战Boss的玩家，这里内容够你刷。',
  '',
  '【十、修仙、境界、突破、转生】',
  '修仙系统让角色从凡人一路成长到仙人。',
  '境界系统提供大量百分比属性加成。',
  '突破系统最高开放至2000级，可以解锁更多技能和成长方向。',
  '转生系统持续增加属性和天赋收益。',
  '这几个系统叠在一起，角色不是一天毕业，而是每天都能变强。',
  '',
  '【十一、长期收集系统】',
  '图鉴系统、套装图鉴、称号系统、切割系统、回血神符、符文管理、时装系统、装备强化、装备成长、物品自动回收、野外Boss系统全部开放。',
  '能刷的多，能养的多，能收集的也多。',
  '你喜欢堆属性、刷装备、打Boss、冲称号、收图鉴，都有路线。',
  '',
  '【十二、VIP80收益体系】',
  'VIP最高80级。',
  '经验、金币、声望、荣誉、竞技场点数、掉落、传送、副本重置、抽奖、泡点、随身银行、邮箱、修理等收益全面提升。',
  '长期玩，VIP收益非常明显。',
  '',
  '【十三、一命模式，高风险高收益】',
  '一命模式上线挑战更刺激。',
  '经验更高，奖励更猛，但死亡不可复活。',
  '真正给喜欢极限玩法的玩家准备。',
  '敢不敢一命冲到底，全看实力。',
  '',
  '【十四、宣传奖励，神级武器直接安排】',
  '按照当前网页任务要求完成有效宣传，即可领取宣传神武。',
  '宣传神武1级可用，神级品质，可持续成长至200级。',
  '宣传神武1级为1亿全属性，2级10亿，3级100亿，4级1000亿；之后仍逐级递增，第200级精确达到9999垓。',
  '越早宣传，越早拿神器，越早拉开差距。',
  '',
  'QQ群：909490346',
  '不删档测试中，进服自己看。',
  '内容太多，文案只能写一部分。',
  '真正的疯狂，进游戏才知道。'
);

INSERT INTO `_宣传审核任务`
  (`任务ID`, `任务名称`, `启用`, `奖励模式`, `奖励组`, `需求ID`, `奖励ID`,
   `物品entry`, `物品数量`, `账号每日上限`, `IP每日上限`, `任务每日上限`,
   `提交方式`, `所需素材数量`, `已删除`, `宣传标题`, `宣传长文`, `关键词`, `连续无效封号次数`)
VALUES
  (1, '论坛宣传', 1, 'CDK', 1, 0, 1100, 0, 1, 1, 1, 1,
   'URL', 2, 0, @promotion_title, @promotion_body, '服务器,宣传', 3),
  (2, '群聊宣传', 1, 'CDK', 1, 0, 1100, 0, 1, 1, 1, 1,
   'IMAGE', 5, 0, @promotion_title, @promotion_body, '服务器,宣传', 3)
ON DUPLICATE KEY UPDATE
  `奖励模式` = VALUES(`奖励模式`),
  `奖励组` = VALUES(`奖励组`),
  `需求ID` = VALUES(`需求ID`),
  `奖励ID` = VALUES(`奖励ID`),
  `物品entry` = VALUES(`物品entry`),
  `物品数量` = VALUES(`物品数量`),
  `宣传标题` = IF(`宣传标题`='', VALUES(`宣传标题`), `宣传标题`),
  `宣传长文` = IF(
    `宣传长文`='',
    VALUES(`宣传长文`),
    REPLACE(
      REPLACE(
        REPLACE(
          REPLACE(
            `宣传长文`,
            '按照当前网页任务要求完成有效宣传，即可领取宣传神器。',
            '按照当前网页任务要求完成有效宣传，即可领取宣传神武。'
          ),
          '宣传神器1级可用，神级品质，可持续成长至1000级。',
          '宣传神武1级可用，神级品质，可持续成长至200级。'
        ),
        '每完成一次有效宣传，系统回收旧等级并发放下一等级宣传神器，全属性每级增加100万。',
        '宣传神武1级为1亿全属性，2级10亿，3级100亿，4级1000亿；之后仍逐级递增，第200级精确达到9999垓。'
      ),
      '每完成一次有效宣传，系统回收旧等级并发放下一等级宣传神武；每级精确增加49.995垓，第200级达到9999垓。',
      '宣传神武1级为1亿全属性，2级10亿，3级100亿，4级1000亿；之后仍逐级递增，第200级精确达到9999垓。'
    )
  ),
  `更新时间` = CURRENT_TIMESTAMP;
