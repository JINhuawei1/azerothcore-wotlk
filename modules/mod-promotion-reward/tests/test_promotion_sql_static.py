import math
from pathlib import Path


MODULE_ROOT = Path(__file__).resolve().parents[1]
WORLD_SQL = MODULE_ROOT / "sql" / "world" / "20260724_宣传审核配置.sql"
CHARACTERS_SQL = MODULE_ROOT / "sql" / "characters" / "20260724_宣传审核流水.sql"
AUTH_SQL = MODULE_ROOT / "sql" / "auth" / "20260726_宣传审核账号.sql"
REWARD_SYSTEM_SQL = MODULE_ROOT / "sql" / "world" / "_宣传奖励系统.sql"
PLAYER_PROGRESS_SQL = MODULE_ROOT / "sql" / "characters" / "_宣传奖励系统玩家.sql"
DIVINE_WEAPON_SQL = MODULE_ROOT / "sql" / "world" / "_宣传神武物品模板.sql"
AUDIT_CPP = MODULE_ROOT / "src" / "PromotionRewardAudit.cpp"
AUDIT_H = MODULE_ROOT / "src" / "PromotionRewardAudit.h"
MODULE_CPP = MODULE_ROOT / "src" / "PromotionRewardModule.cpp"
REDEMPTION_CPP = MODULE_ROOT.parent / "mod-redemption-code" / "src" / "RedemptionCodeModule.cpp"
PROMOTION_UI_ROOT = MODULE_ROOT.parent / "mod-plugin-manager" / "测试客户端ui插件" / "PromotionReward"
PROMOTION_LUA = PROMOTION_UI_ROOT / "PromotionReward.lua"
PROMOTION_XML = PROMOTION_UI_ROOT / "PromotionReward.xml"

WORLD_TABLE = "_宣传审核任务"
CHARACTERS_TABLES = (
    "_宣传全局状态",
    "_宣传提交记录",
    "_宣传提交素材",
    "_宣传奖励流水",
    "_宣传兑换流水",
    "_宣传账号统计",
    "_宣传IP统计",
    "_宣传审核日志",
    "_宣传管理日志",
)


def _read_sql(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _table_block(sql: str, table: str) -> str:
    marker = f"CREATE TABLE IF NOT EXISTS `{table}`"
    start = sql.index(marker)
    next_table = sql.find("\nCREATE TABLE IF NOT EXISTS `", start + len(marker))
    end = len(sql) if next_table == -1 else next_table
    return sql[start:end]


def test_promotion_sql_files_are_idempotent_and_non_destructive():
    world_sql = _read_sql(WORLD_SQL)
    characters_sql = _read_sql(CHARACTERS_SQL)
    sql = f"{world_sql}\n{characters_sql}"

    assert "DROP TABLE" not in sql.upper()
    assert "ON DUPLICATE KEY UPDATE" in world_sql

    for table in (WORLD_TABLE, *CHARACTERS_TABLES):
        block = _table_block(sql, table)
        assert block.startswith(f"CREATE TABLE IF NOT EXISTS `{table}`")
        assert "ENGINE = InnoDB" in block
        assert "CHARACTER SET = utf8mb4" in block


def test_world_task_table_contains_configurable_fields_and_safe_upsert():
    sql = _read_sql(WORLD_SQL)
    block = _table_block(sql, WORLD_TABLE)

    for field in (
        "任务ID",
        "任务名称",
        "启用",
        "奖励模式",
        "奖励组",
        "需求ID",
        "奖励ID",
        "物品entry",
        "物品数量",
        "账号每日上限",
        "IP每日上限",
        "任务每日上限",
        "提交方式",
        "所需素材数量",
        "已删除",
        "宣传标题",
        "宣传长文",
        "关键词",
        "连续无效封号次数",
        "创建时间",
        "更新时间",
    ):
        assert f"`{field}`" in block

    assert "PRIMARY KEY (`任务ID`)" in block
    assert "`任务ID`                 INT UNSIGNED NOT NULL AUTO_INCREMENT" in block
    assert "VALUES\n  (1," in sql
    upsert = sql.split("ON DUPLICATE KEY UPDATE", 1)[1].strip()
    assert upsert.startswith("`奖励模式` = VALUES(`奖励模式`)")
    assert "`奖励组` = VALUES(`奖励组`)" in upsert
    assert "`奖励ID` = VALUES(`奖励ID`)" in upsert
    assert "`宣传标题` = IF(`宣传标题`='', VALUES(`宣传标题`), `宣传标题`)" in upsert
    assert "`宣传长文` = IF(" in upsert
    assert "`宣传长文`=''," in upsert
    assert "VALUES(`宣传长文`)" in upsert
    assert "REPLACE(" in upsert
    assert upsert.endswith("`更新时间` = CURRENT_TIMESTAMP;")


def test_default_seed_enables_image_and_forum_tasks_together():
    sql = _read_sql(WORLD_SQL)

    assert "(1, '论坛宣传'" in sql
    assert "'URL', 2" in sql
    assert "(2, '群聊宣传'" in sql
    assert "'IMAGE', 5" in sql


def test_default_copy_seed_is_full_length_and_existing_tables_are_migrated_idempotently():
    sql = _read_sql(WORLD_SQL)

    assert "information_schema.COLUMNS" in sql
    assert "PREPARE promotion_schema_stmt" in sql
    assert "ALTER TABLE `_宣传审核任务` ADD COLUMN `宣传标题`" in sql
    assert "ALTER TABLE `_宣传审核任务` ADD COLUMN `宣传长文`" in sql
    assert "ALTER TABLE `_宣传审核任务` ADD COLUMN `提交方式`" in sql
    assert "ALTER TABLE `_宣传审核任务` ADD COLUMN `所需素材数量`" in sql
    assert "ALTER TABLE `_宣传审核任务` ADD COLUMN `已删除`" in sql
    assert "MODIFY COLUMN `任务ID` INT UNSIGNED NOT NULL AUTO_INCREMENT" in sql
    assert "'IMAGE'" in sql
    assert "`所需素材数量`" in sql

    for heading in (
        "【一、属性无限突破】",
        "【二、幻境倍率玩法】",
        "【三、天赋之魂，打造自己的核心技能】",
        "【四、魔次系统，一个技能触发2000次】",
        "【五、装备鉴定，一件装备直接起飞】",
        "【六、飞升装备，额外再穿一整套装备】",
        "【七、仙门系统，选择你的修仙流派】",
        "【八、武魂系统，觉醒第二套战斗核心】",
        "【九、深渊修仙，长期挑战内容拉满】",
        "【十、修仙、境界、突破、转生】",
        "【十一、长期收集系统】",
        "【十二、VIP80收益体系】",
        "【十三、一命模式，高风险高收益】",
        "【十四、宣传奖励，神级武器直接安排】",
    ):
        assert heading in sql

    assert "65位超大数值" in sql
    assert "2000级突破" in sql
    growth_text = "宣传神武1级为1亿全属性，2级10亿，3级100亿，4级1000亿；之后仍逐级递增，第200级精确达到9999垓。"
    assert growth_text in sql
    assert sql.count("每级精确增加49.995垓") == 1


def test_divine_weapon_experiment_has_exact_200_level_ranges_and_final_value():
    sql = _read_sql(DIVINE_WEAPON_SQL)

    first_item = 450001
    first_reward = 1100
    levels = 200
    decay = 0.92857123960004195277

    assert first_item + levels - 1 == 450200
    assert first_reward + levels - 1 == 1299

    values = []
    for level in range(1, levels + 1):
        if level == 1:
            value = 100000000
        elif level == 2:
            value = 1000000000
        elif level == 3:
            value = 10000000000
        elif level == levels:
            value = 999900000000000000000000
        else:
            exponent = 10 + (1 - math.pow(decay, level - 3)) / (1 - decay)
            value = round(math.pow(10, exponent))
        values.append(value)

    assert values[:4] == [100000000, 1000000000, 10000000000, 100000000000]
    assert all(current < following for current, following in zip(values, values[1:]))
    assert values[-1] == 999900000000000000000000

    assert "SET @promotion_weapon_entry_first = 450001;" in sql
    assert "SET @promotion_reward_id_first = 1100;" in sql
    assert "SET @promotion_weapon_levels = 200;" in sql
    assert "SET @promotion_curve_decay = 0.92857123960004195277;" in sql
    assert "SET @promotion_attr_max = CAST(999900000000000000000000 AS DECIMAL(65,0));" in sql
    assert "WHEN 1 THEN CAST(100000000 AS DECIMAL(65,0))" in sql
    assert "WHEN 2 THEN CAST(1000000000 AS DECIMAL(65,0))" in sql
    assert "WHEN 3 THEN CAST(10000000000 AS DECIMAL(65,0))" in sql
    assert "WHEN 200 THEN @promotion_attr_max" in sql
    assert "POW(@promotion_curve_decay, `lvl` - 3)" in sql
    assert "@promotion_attr_step" not in sql
    assert "<= @promotion_weapon_levels" in sql
    assert "CONCAT('宣传神武', l.`lvl`)" in sql
    assert "@promotion_weapon_entry_first + l.`lvl` - 1" in sql
    assert "@promotion_reward_id_first + l.`lvl` - 1" in sql
    assert "第200把全属性 = 999900000000000000000000（精确）" in sql


def test_divine_weapon_copies_the_existing_ten_stat_layout_and_uses_decimal_65_values():
    sql = _read_sql(DIVINE_WEAPON_SQL)

    stat_start = sql.index("  10,\n  4,  l.`attr_value`")
    stat_select = sql[stat_start:sql.index("  CAST(l.`attr_value` AS DOUBLE)", stat_start)]
    for stat_type in (4, 3, 5, 6, 31, 32, 36, 38, 45, 47):
        assert f"  {stat_type}," in stat_select or f"  {stat_type}, " in stat_select
    assert stat_select.count("l.`attr_value`") == 10
    assert "`attr_value` decimal(65,0)" in sql
    assert "`next_attr_value` decimal(65,0)" in sql
    assert "CAST(l.`next_attr_value` AS DOUBLE)" in sql


def test_divine_weapon_has_no_bottom_tooltip_description():
    sql = _read_sql(DIVINE_WEAPON_SQL)

    assert "本级全属性+" not in sql
    assert "  1900,\n  1,\n  '',\n  1," in sql


def test_divine_weapon_sql_never_overwrites_foreign_rows_in_the_selected_ranges():
    sql = _read_sql(DIVINE_WEAPON_SQL)

    assert "REPLACE INTO" not in sql.upper()
    assert "AND `name` REGEXP '^宣传神武[0-9]+$'" in sql
    assert "AND `注释` REGEXP '^宣传神武[0-9]+奖励$'" in sql
    assert "AND `注释` REGEXP '^宣传神武[0-9]+背包银行属性生效$'" in sql
    assert "ON DUPLICATE KEY UPDATE" not in sql.upper()


def test_new_and_legacy_promotion_activities_coexist_with_separate_groups():
    reward_sql = _read_sql(REWARD_SYSTEM_SQL)
    task_sql = _read_sql(WORLD_SQL)

    assert "UNIQUE KEY `uk_宣传奖励系统_奖励组` (`对接奖励组`)" in reward_sql
    assert "(1,'旧宣传神器：997001-998000，奖励100-1099',1,0,'宣传神器',997001,1000," in reward_sql
    assert "(2,'宣传神武实验：200级，第200级全属性9999垓',1,1,'宣传神武',450001,200," in reward_sql
    assert "100000000,0,1,0,1100,27" in reward_sql
    assert "WHERE `id`=2 AND `对接奖励组`=1" in reward_sql
    assert "(1, '论坛宣传', 1, 'CDK', 1, 0, 1100" in task_sql
    assert "(2, '群聊宣传', 1, 'CDK', 1, 0, 1100" in task_sql


def test_promotion_ui_formats_int256_values_with_all_requested_chinese_units():
    lua = PROMOTION_LUA.read_text(encoding="utf-8")
    xml = PROMOTION_XML.read_text(encoding="utf-8")

    units = (
        "万", "亿", "兆", "京", "垓", "秭", "穰", "沟", "涧", "正", "载", "极",
        "恒河沙", "阿僧祇", "那由他", "不可思议",
    )
    unit_table = lua[lua.index("local LARGE_NUMBER_UNITS"):lua.index("local function NormalizeIntegerString")]
    for unit in units:
        assert f'"{unit}"' in unit_table

    assert "local function FormatCompactNum(value)" in lua
    assert 'PromotionRewardFrameAttrPanelValue:SetText("+" .. FormatCompactNum(s.totalAttr))' in lua
    assert "FormatCompactNum(nextAttr)" in lua
    assert "FormatCompactNum(minDmg)" in lua
    assert "FormatCompactNum(maxDmg)" in lua

    parse_info = lua[lua.index("function PromotionRewardUI:ParseInfo"):lua.index("function PromotionRewardUI:RenderInfo")]
    for part_index in (2, 4, 5, 10, 11, 12, 13, 14, 18):
        assert f"tonumber(parts[{part_index}])" not in parse_info

    assert "self.state.maxLevel    = tonumber(parts[15])" in lua
    assert "self.state.firstWeaponEntry = tonumber(parts[16])" in lua
    assert "self.state.weaponName  = (parts[17]" in lua
    assert "self.state.maxAttr     = parts[18]" in lua
    assert "DEFAULT_WEAPON_FIRST_ENTRY = 450001" in lua
    assert "DEFAULT_WEAPON_MAX_LEVEL   = 200" in lua
    assert 'text="宣传神武 +0 全属性"' in xml


def test_server_info_protocol_sends_dynamic_weapon_curve_metadata():
    source = MODULE_CPP.read_text(encoding="utf-8")

    assert "std::vector<int256> levelAttrValues" in (MODULE_ROOT / "src" / "PromotionRewardModule.h").read_text(encoding="utf-8")
    assert "SELECT `entry`,`stat_value1` FROM `item_template`" in source
    assert "return config->levelAttrValues[level - 1];" in source
    assert "<< '|' << config.maxLevel" in source
    assert "<< '|' << config.weaponEntry" in source
    assert "<< '|' << config.weaponName" in source
    assert "<< '|' << Acore::ToString(maxAttr)" in source


def test_player_progress_is_isolated_by_reward_group_and_legacy_rows_migrate_to_9001():
    sql = _read_sql(PLAYER_PROGRESS_SQL)
    source = MODULE_CPP.read_text(encoding="utf-8")

    assert "`奖励组`   int UNSIGNED NOT NULL DEFAULT 9001" in sql
    assert "PRIMARY KEY (`奖励组`,`玩家GUID`)" in sql
    assert "ADD COLUMN `奖励组` int UNSIGNED NOT NULL DEFAULT 9001" in sql
    assert "SELECT `奖励组`,`玩家GUID`,`宣传天数`" in source
    assert "BuildPlayerKey(guid, groupId)" in source
    assert "INSERT INTO `_宣传奖励系统玩家` (`奖励组`,`玩家GUID`,`宣传天数`)" in source


def test_redemption_and_rollback_keep_the_cdk_reward_group_context():
    audit_header = AUDIT_H.read_text(encoding="utf-8")
    audit_source = AUDIT_CPP.read_text(encoding="utf-8")
    redemption_source = REDEMPTION_CPP.read_text(encoding="utf-8")

    assert "std::uint32_t groupId = 0;" in audit_header
    assert "BeginCodeRedeem(player, code, groupId, snapshot)" in redemption_source
    assert "RedeemPromotionCode(player, groupId, promoRewardId)" in redemption_source
    assert "snapshot.groupId = groupId;" in audit_source
    assert "GetPlayerDataForGroup(snapshot.characterGuid, groupId)" in audit_source
    assert "CollectPromotionItemGuids(player, groupId)" in audit_source
    assert "LoadPromotionGroupForCode(code)" in audit_source
    assert "GetWeaponEntryForLevel(beforeDays, promotionGroupId)" in audit_source


def test_characters_tables_have_their_required_audit_fields():
    sql = _read_sql(CHARACTERS_SQL)
    expected_fields = {
        "_宣传全局状态": (
            "状态ID",
            "当前限额周期",
            "限额重置时间",
            "最后操作人账号ID",
            "最后操作人名称",
            "最后操作理由",
            "最后操作IP",
            "更新时间",
        ),
        "_宣传提交记录": (
            "提交ID",
            "任务ID",
            "限额周期",
            "账号ID",
            "角色GUID",
            "来源IP",
            "图片存储引用",
            "论坛链接",
            "内容哈希",
            "预检状态",
            "审核状态",
            "审核人账号ID",
            "审核理由",
        ),
        "_宣传提交素材": (
            "素材ID",
            "提交ID",
            "素材类型",
            "素材序号",
            "素材引用",
            "内容哈希",
            "预检状态",
            "预检结果",
            "创建时间",
        ),
        "_宣传奖励流水": (
            "提交ID",
            "request_id",
            "发放模式",
            "CDK",
            "奖励ID",
            "物品entry",
            "物品数量",
            "物品GUID",
            "资源快照",
            "发放状态",
            "回滚状态",
            "回滚错误",
            "处理令牌",
        ),
        "_宣传兑换流水": (
            "奖励流水ID",
            "request_id",
            "CDK",
            "兑换角色GUID",
            "兑换账号名",
            "兑换时间",
            "兑换前宣传天数",
            "兑换后宣传天数",
            "旧宣传物品GUID",
            "新宣传物品GUID",
            "新增奖励GUID",
            "资源快照",
            "回滚错误",
        ),
        "_宣传账号统计": (
            "账号ID",
            "统计日期",
            "今日提交次数",
            "今日发放次数",
            "连续无效次数",
            "封禁状态",
        ),
        "_宣传IP统计": (
            "IP地址",
            "统计日期",
            "今日提交次数",
            "今日发放次数",
            "今日无效次数",
        ),
        "_宣传审核日志": (
            "提交ID",
            "奖励流水ID",
            "原状态",
            "新状态",
            "审核人账号ID",
            "审核人名称",
            "审核理由",
            "操作时间",
        ),
        "_宣传管理日志": (
            "日志ID",
            "关联提交ID",
            "目标账号ID",
            "操作类型",
            "操作前状态",
            "操作后状态",
            "审核人账号ID",
            "审核人名称",
            "操作理由",
            "操作IP",
            "操作结果",
            "操作时间",
        ),
    }

    for table, fields in expected_fields.items():
        block = _table_block(sql, table)
        for field in fields:
            assert f"`{field}`" in block, f"{table} 缺少字段 {field}"


def test_required_unique_keys_and_indexes_are_table_specific():
    sql = _read_sql(CHARACTERS_SQL)

    reward = _table_block(sql, "_宣传奖励流水")
    assert "UNIQUE KEY `uk_宣传奖励流水_提交ID` (`提交ID`)" in reward
    assert "UNIQUE KEY `uk_宣传奖励流水_request_id` (`request_id`)" in reward
    assert "KEY `idx_宣传奖励流水_CDK` (`CDK`)" in reward
    assert "KEY `idx_宣传奖励流水_账号状态` (`账号ID`, `发放状态`, `回滚状态`)" in reward

    submission = _table_block(sql, "_宣传提交记录")
    assert "KEY `idx_宣传提交记录_任务账号时间` (`任务ID`, `账号ID`, `提交时间`)" in submission
    assert "KEY `idx_宣传提交记录_任务周期` (`任务ID`, `限额周期`)" in submission
    assert "KEY `idx_宣传提交记录_IP时间` (`来源IP`, `提交时间`)" in submission
    assert "KEY `idx_宣传提交记录_状态` (`审核状态`, `预检状态`)" in submission

    material = _table_block(sql, "_宣传提交素材")
    assert "UNIQUE KEY `uk_宣传提交素材_提交序号` (`提交ID`, `素材序号`)" in material
    assert "KEY `idx_宣传提交素材_提交类型` (`提交ID`, `素材类型`)" in material
    assert "KEY `idx_宣传提交素材_内容哈希` (`内容哈希`)" in material

    exchange = _table_block(sql, "_宣传兑换流水")
    assert "UNIQUE KEY `uk_宣传兑换流水_奖励流水ID` (`奖励流水ID`)" in exchange
    assert "KEY `idx_宣传兑换流水_CDK` (`CDK`)" in exchange
    assert "KEY `idx_宣传兑换流水_回滚状态` (`回滚状态`)" in exchange

    audit = _table_block(sql, "_宣传审核日志")
    assert "KEY `idx_宣传审核日志_状态` (`原状态`, `新状态`)" in audit

    management = _table_block(sql, "_宣传管理日志")
    assert "KEY `idx_宣传管理日志_账号时间` (`目标账号ID`, `操作时间`)" in management
    assert "KEY `idx_宣传管理日志_审核人时间` (`审核人账号ID`, `操作时间`)" in management


def test_account_and_ip_statistics_are_global_per_day():
    sql = _read_sql(CHARACTERS_SQL)
    account = _table_block(sql, "_宣传账号统计")
    ip = _table_block(sql, "_宣传IP统计")

    assert "PRIMARY KEY (`账号ID`, `统计日期`)" in account
    assert "`任务ID`" not in account
    assert "KEY `idx_宣传账号统计_状态` (`封禁状态`, `连续无效次数`)" in account

    assert "PRIMARY KEY (`IP地址`, `统计日期`)" in ip
    assert "`任务ID`" not in ip
    assert "KEY `idx_宣传IP统计_IP日期` (`IP地址`, `统计日期`)" in ip


def test_grant_claim_uses_a_dedicated_field_and_processing_rollback_is_deferred():
    source = AUDIT_CPP.read_text(encoding="utf-8")

    claim_start = source.index("bool ClaimPendingGrant")
    claim_end = source.index("void RestorePendingGrant", claim_start)
    claim = source[claim_start:claim_end]
    assert "`处理令牌`" in claim
    assert "`回滚错误`" not in claim

    rollback_start = source.index("bool PromotionRewardAuditMgr::RollbackGrant")
    rollback_end = source.index("bool PromotionRewardAuditMgr::RetryRollback", rollback_start)
    rollback = source[rollback_start:rollback_end]
    assert 'grantStatus == "PROCESSING"' in rollback
    assert rollback.index('grantStatus == "PROCESSING"') < rollback.index('grantStatus != "PENDING"')


def test_item_processing_recovery_requires_guid_and_resource_snapshot():
    source = AUDIT_CPP.read_text(encoding="utf-8")
    start = source.index("uint32 RecoverTimedOutProcessingGrants")
    end = source.index("PromotionRewardAuditMgr* PromotionRewardAuditMgr::instance", start)
    recovery = source[start:end]

    assert "`资源快照`" in recovery
    assert "HasReliableItemReceipt" in recovery
    assert "`回滚状态`" in recovery
    assert "RevokeWithoutIssue" in recovery


def test_pending_rollbacks_are_retried_by_the_independent_rollback_worker():
    source = AUDIT_CPP.read_text(encoding="utf-8")
    review_start = source.index("bool PromotionRewardAuditMgr::ConsumeReviewQueue")
    review_end = source.index("bool PromotionRewardAuditMgr::BeginCodeRedeem", review_start)
    review = source[review_start:review_end]
    rollback_start = source.index("bool PromotionRewardAuditMgr::ConsumeRollbackQueue")
    rollback_end = source.index("bool PromotionRewardAuditMgr::ConsumeBanQueue", rollback_start)
    rollback = source[rollback_start:rollback_end]

    assert "RollbackGrant(" not in review
    assert "`回滚状态`='PENDING'" in rollback
    assert "RollbackGrant(grantId)" in rollback


def test_auth_reviewer_allowlist_is_idempotent_and_seeds_manager_without_passwords():
    sql = _read_sql(AUTH_SQL)

    assert "DROP TABLE" not in sql.upper()
    assert "CREATE TABLE IF NOT EXISTS `_宣传审核账号`" in sql
    assert "CREATE TABLE IF NOT EXISTS `_宣传审核账号日志`" in sql
    allowlist = _table_block(sql, "_宣传审核账号")
    audit = _table_block(sql, "_宣传审核账号日志")
    for field in ("账号ID", "权限角色", "启用", "备注", "授权人账号ID", "授权时间", "更新时间"):
        assert f"`{field}`" in allowlist
    for field in ("日志ID", "目标账号ID", "操作类型", "操作前状态", "操作后状态", "操作人账号ID", "操作理由", "操作IP"):
        assert f"`{field}`" in audit
    assert "PRIMARY KEY (`账号ID`)" in allowlist
    assert "KEY `idx_宣传审核账号_角色状态` (`权限角色`, `启用`)" in allowlist
    assert "INSERT IGNORE INTO `_宣传审核账号`" in sql
    assert "UPPER(`username`) = 'QQ5354414'" in sql
    assert "password" not in allowlist.lower()
    assert "verifier" not in allowlist.lower()
