-- 转身系统配置表
-- 每达到指定等级可进行一次转身，转身后等级重置为1级，获得属性加成和天赋点奖励
-- 支持无限转身，通过模板_需求关联需求系统控制转身条件
DROP TABLE IF EXISTS `_转身系统`;
CREATE TABLE `_转身系统` (
  `id` int unsigned NOT NULL AUTO_INCREMENT COMMENT '配置ID',
  `转身等级` int unsigned NOT NULL DEFAULT 0 COMMENT '转身等级(0表示默认配置)',
  `模板_需求` int unsigned NOT NULL DEFAULT 0 COMMENT '需求模板ID,关联_模板_需求表',
  `奖励属性` float NOT NULL DEFAULT 5.0 COMMENT '全属性加成百分比',
  `奖励天赋` int unsigned NOT NULL DEFAULT 10 COMMENT '奖励天赋点数',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_转身等级` (`转身等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='转身系统配置表';

-- 插入默认配置(转身等级=0表示所有转身的默认配置)
-- 如果需要为特定转身等级设置不同的需求或奖励，可以添加对应等级的记录
INSERT INTO `_转身系统` (`id`, `转身等级`, `模板_需求`, `奖励属性`, `奖励天赋`) VALUES
(1, 0, 0, 5.0, 10);
