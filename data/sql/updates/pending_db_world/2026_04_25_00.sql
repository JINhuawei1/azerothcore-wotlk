ALTER TABLE `item_template`
  MODIFY `SellPrice` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `stat_value1` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value2` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value3` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value4` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value5` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value6` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value7` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value8` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value9` bigint NOT NULL DEFAULT '0',
  MODIFY `stat_value10` bigint NOT NULL DEFAULT '0',
  MODIFY `ScalingStatValue` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `dmg_min1` double NOT NULL DEFAULT '0',
  MODIFY `dmg_max1` double NOT NULL DEFAULT '0',
  MODIFY `dmg_min2` double NOT NULL DEFAULT '0',
  MODIFY `dmg_max2` double NOT NULL DEFAULT '0',
  MODIFY `armor` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `MaxDurability` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `ArmorDamageModifier` double NOT NULL DEFAULT '0',
  MODIFY `minMoneyLoot` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `maxMoneyLoot` bigint unsigned NOT NULL DEFAULT '0';

ALTER TABLE `creature_template`
  MODIFY `DamageModifier` double NOT NULL DEFAULT '1',
  MODIFY `mingold` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `maxgold` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `HealthModifier` double NOT NULL DEFAULT '1',
  MODIFY `ManaModifier` double NOT NULL DEFAULT '1',
  MODIFY `ArmorModifier` double NOT NULL DEFAULT '1',
  MODIFY `ExperienceModifier` double NOT NULL DEFAULT '1';

ALTER TABLE `creature_classlevelstats`
  MODIFY `basehp0` bigint unsigned NOT NULL DEFAULT '1',
  MODIFY `basehp1` bigint unsigned NOT NULL DEFAULT '1',
  MODIFY `basehp2` bigint unsigned NOT NULL DEFAULT '1',
  MODIFY `basemana` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `basearmor` double NOT NULL DEFAULT '1',
  MODIFY `attackpower` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `rangedattackpower` bigint unsigned NOT NULL DEFAULT '0',
  MODIFY `damage_base` double NOT NULL DEFAULT '0',
  MODIFY `damage_exp1` double NOT NULL DEFAULT '0',
  MODIFY `damage_exp2` double NOT NULL DEFAULT '0';
