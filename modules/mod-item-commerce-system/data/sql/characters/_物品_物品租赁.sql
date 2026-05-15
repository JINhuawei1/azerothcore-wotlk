DROP TABLE IF EXISTS `_物品_物品租赁`;
CREATE TABLE `_物品_物品租赁` (
  `角色ID` int UNSIGNED NOT NULL COMMENT '角色ID',
  `物品GUID` int UNSIGNED NOT NULL COMMENT '物品GUID',
  `过期时间` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品过期时间戳',
  PRIMARY KEY (`角色ID`, `物品GUID`)
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '角色_物品_租赁记录'; 