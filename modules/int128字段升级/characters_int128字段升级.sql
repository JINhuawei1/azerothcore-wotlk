ALTER TABLE `characters`
  MODIFY `health` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power1` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power2` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power3` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power4` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power5` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power6` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `power7` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `character_stats`
  MODIFY `maxhealth` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower1` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower2` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower3` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower4` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower5` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower6` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `maxpower7` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `strength` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `agility` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `stamina` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `intellect` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `spirit` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `armor` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `attackPower` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `rangedAttackPower` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `spellPower` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `resilience` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `character_pet`
  MODIFY `curhealth` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `curmana` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_材料仓库玩家`
  MODIFY `数量` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '仓库内累计数量';

ALTER TABLE `_玩家武魂数据`
  MODIFY `魂力` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '当前可用魂力';

ALTER TABLE `任务奖励属性_玩家记录`
  MODIFY `总属性点数` decimal(39,0) unsigned NOT NULL DEFAULT '0';
