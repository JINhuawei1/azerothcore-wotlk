ALTER TABLE `_仙门_玩家`
  MODIFY `历史贡献` decimal(65,0) NOT NULL DEFAULT '0';

-- 幻境玩家等级：支持最高 65 位无符号整数
ALTER TABLE `玩家幻境数据`
  MODIFY `幻境等级` decimal(65,0) unsigned NOT NULL DEFAULT '1' COMMENT '当前幻境等级(uint256)';

-- 幻境掉落装备的待鉴定倍率：避免超过 uint32 上限 4294967295 后无法读取
ALTER TABLE `待鉴定物品标记`
  MODIFY `幻境倍率` decimal(65,0) unsigned NOT NULL DEFAULT '1' COMMENT '拾取时的幻境倍率(int256)';

-- 鉴定完成后保存到装备上的幻境属性倍率
ALTER TABLE `玩家装备属性增强`
  MODIFY `属性倍率` decimal(65,0) unsigned NOT NULL DEFAULT '1' COMMENT '属性倍率(int256)';

-- 单条鉴定属性可达到 65 位，多条属性合并后可能超过原 varchar(500)
ALTER TABLE `物品_鉴定记录`
  MODIFY `基础属性详情` text NOT NULL COMMENT '基础属性详情，支持int256属性值';
