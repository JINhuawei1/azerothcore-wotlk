from pathlib import Path


MODULE_ROOT = Path(__file__).resolve().parents[1]
WORLD_SQL = MODULE_ROOT / "sql" / "world" / "20260724_宣传审核配置.sql"
CHARACTERS_SQL = MODULE_ROOT / "sql" / "characters" / "20260724_宣传审核流水.sql"
AUDIT_CPP = MODULE_ROOT / "src" / "PromotionRewardAudit.cpp"

WORLD_TABLE = "_宣传审核任务"
CHARACTERS_TABLES = (
    "_宣传提交记录",
    "_宣传奖励流水",
    "_宣传兑换流水",
    "_宣传账号统计",
    "_宣传IP统计",
    "_宣传审核日志",
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
        "关键词",
        "连续无效封号次数",
        "创建时间",
        "更新时间",
    ):
        assert f"`{field}`" in block

    assert "PRIMARY KEY (`任务ID`)" in block
    assert "VALUES\n  (1," in sql
    upsert = sql.split("ON DUPLICATE KEY UPDATE", 1)[1].strip()
    assert upsert.startswith("`更新时间` = CURRENT_TIMESTAMP;")


def test_characters_tables_have_their_required_audit_fields():
    sql = _read_sql(CHARACTERS_SQL)
    expected_fields = {
        "_宣传提交记录": (
            "提交ID",
            "任务ID",
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
    assert "KEY `idx_宣传提交记录_IP时间` (`来源IP`, `提交时间`)" in submission
    assert "KEY `idx_宣传提交记录_状态` (`审核状态`, `预检状态`)" in submission

    exchange = _table_block(sql, "_宣传兑换流水")
    assert "UNIQUE KEY `uk_宣传兑换流水_奖励流水ID` (`奖励流水ID`)" in exchange
    assert "KEY `idx_宣传兑换流水_CDK` (`CDK`)" in exchange
    assert "KEY `idx_宣传兑换流水_回滚状态` (`回滚状态`)" in exchange

    audit = _table_block(sql, "_宣传审核日志")
    assert "KEY `idx_宣传审核日志_状态` (`原状态`, `新状态`)" in audit


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
