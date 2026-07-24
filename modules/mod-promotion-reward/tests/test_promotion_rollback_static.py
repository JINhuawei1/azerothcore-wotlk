from pathlib import Path


MODULE_ROOT = Path(__file__).resolve().parents[1]
AUDIT_H = MODULE_ROOT / "src" / "PromotionRewardAudit.h"
AUDIT_CPP = MODULE_ROOT / "src" / "PromotionRewardAudit.cpp"
MODULE_CPP = MODULE_ROOT / "src" / "PromotionRewardModule.cpp"
MODULE_H = MODULE_ROOT / "src" / "PromotionRewardModule.h"
COMMANDS_H = MODULE_ROOT / "src" / "PromotionRewardCommands.h"
COMMANDS_CPP = MODULE_ROOT / "src" / "PromotionRewardCommands.cpp"
CHARACTERS_SQL = MODULE_ROOT / "sql" / "characters" / "20260724_宣传审核流水.sql"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_rollback_has_its_own_bounded_worker() -> None:
    header = _read(AUDIT_H)
    audit = _read(AUDIT_CPP)
    module = _read(MODULE_CPP)

    assert "ConsumeRollbackQueue" in header
    assert "ConsumeRollbackQueue" in audit
    assert "ConsumeRollbackQueue" in module

    review_start = audit.index("bool PromotionRewardAuditMgr::ConsumeReviewQueue")
    review_end = audit.index("bool PromotionRewardAuditMgr::BeginCodeRedeem", review_start)
    assert "RollbackGrant(" not in audit[review_start:review_end]


def test_rollback_claim_is_exclusive_and_recoverable() -> None:
    audit = _read(AUDIT_CPP)

    assert "ROLLBACK:" in audit
    assert "RecoverTimedOutRollbackClaims" in audit
    assert "`处理令牌`" in audit
    assert "DATE_SUB(NOW(), INTERVAL 5 MINUTE)" in audit


def test_unused_cdk_is_revoked_without_deleting_its_audit_row() -> None:
    audit = _read(AUDIT_CPP)
    start = audit.index("bool PromotionRewardAuditMgr::RollbackGrant")
    end = audit.index("bool PromotionRewardAuditMgr::RetryRollback", start)
    rollback = audit[start:end]

    assert "`兑换次数`=0" in rollback
    assert "DELETE FROM `_奖励_兑换码`" not in rollback


def test_used_cdk_and_direct_item_use_exact_guids_and_owner_checks() -> None:
    audit = _read(AUDIT_CPP)
    start = audit.index("bool PromotionRewardAuditMgr::RollbackGrant")
    end = audit.index("bool PromotionRewardAuditMgr::RetryRollback", start)
    rollback = audit[start:end]

    assert "_宣传兑换流水" in rollback
    assert "兑换角色GUID" in rollback
    assert "新增奖励GUID" in rollback
    assert "物品GUID" in rollback
    assert "LoadExactOwnedItems" in rollback
    assert "item_instance" in audit
    assert "owner_guid" in audit
    assert "character_inventory" in audit
    assert "DestroyItemCount" not in rollback


def test_direct_item_grants_reject_stack_merging_before_guid_capture() -> None:
    audit = _read(AUDIT_CPP)
    start = audit.index("bool PromotionRewardAuditMgr::IssueItemGrant")
    end = audit.index("bool PromotionRewardAuditMgr::ConsumeReviewQueue", start)
    issue = audit[start:end]

    assert "GetMaxStackSize" in issue
    assert "SetBinding(true)" in issue


def test_item_capture_marks_existing_stack_merges_as_inexact() -> None:
    header = _read(MODULE_H)
    module = _read(MODULE_CPP)
    audit = _read(AUDIT_CPP)

    assert "initialItemCounts" in header
    assert "receiptExact" in header
    assert "receiptError" in header
    assert "SnapshotItemCounts" in module
    assert "合并到玩家原有堆叠" in module
    assert "captureExact" in audit
    assert "captureError" in audit


def test_cdk_item_rollback_requires_original_stack_counts() -> None:
    audit = _read(AUDIT_CPP)

    # The production code emits the JSON key from an escaped C++ string literal.
    assert "itemCounts" in audit
    assert "LoadExpectedItemCounts" in audit
    assert "物品数量已变化GUID:" in audit


def test_resource_rollback_preflights_balance_and_records_debt() -> None:
    audit = _read(AUDIT_CPP)

    assert "CanReverseResourceDelta" in audit
    assert "CurrencySystem.h" in audit
    assert "玩家货币" in audit
    assert "LoadPlayerCurrencies" in audit
    assert "RECOVERY_DEBT" in audit or "'DEBT'" in audit
    assert "HasAccountRecoveryDebt" in audit
    assert "ClearRecoveryDebt" in audit


def test_review_application_is_idempotent_and_bans_at_configured_threshold() -> None:
    audit = _read(AUDIT_CPP)
    sql = _read(CHARACTERS_SQL)

    assert "审核处理状态" in sql
    assert "审核处理令牌" in sql
    assert "审核处理状态" in audit
    assert "连续无效次数" in audit
    assert "连续无效封号次数" in audit
    assert "ShouldBanAfterReject" in audit
    assert "sBan->BanAccount" in audit
    assert "ConsumeBanQueue" in audit


def test_rejection_without_source_ip_still_requests_rollback() -> None:
    audit = _read(AUDIT_CPP)
    start = audit.index("bool PromotionRewardAuditMgr::ConsumeReviewQueue")
    end = audit.index("bool PromotionRewardAuditMgr::BeginCodeRedeem", start)
    review = audit[start:end]

    reject = review.index("if (decision == PromotionRewardPolicy::ReviewDecision::Reject)")
    ip_optional = review.index("if (!sourceIp.empty())", reject)
    rollback_update = review.index("`回滚错误`=IF", reject)
    approve_update = review.index("`发放状态`='FINAL'", rollback_update)
    assert reject < ip_optional < rollback_update < approve_update


def test_used_cdk_precreates_old_artifact_before_deleting_new_items() -> None:
    audit = _read(AUDIT_CPP)
    start = audit.index("bool PromotionRewardAuditMgr::RollbackGrant")
    end = audit.index("bool PromotionRewardAuditMgr::RetryRollback", start)
    rollback = audit[start:end]

    create = rollback.index("Item::CreateItem")
    destroy = rollback.index("player->DestroyItem", create)
    store = rollback.index("player->StoreItem", destroy)
    assert create < destroy < store


def test_zero_resource_deltas_do_not_overwrite_currency_table_when_module_is_disabled() -> None:
    audit = _read(AUDIT_CPP)
    start = audit.index("bool PromotionRewardAuditMgr::RollbackGrant")
    end = audit.index("bool PromotionRewardAuditMgr::RetryRollback", start)
    rollback = audit[start:end]

    assert "writeCurrencyRollback" in rollback
    assert "if (writeCurrencyRollback)" in rollback


def test_gm_review_and_recovery_commands_share_audit_manager() -> None:
    header = _read(COMMANDS_H)
    source = _read(COMMANDS_CPP)

    for command in ("审核通过", "审核无效", "回收重试", "回收查询", "欠账清除"):
        assert command in source

    assert "HandleApproveSubmissionCommand" in header
    assert "HandleRejectSubmissionCommand" in header
    assert "HandleRetryRollbackCommand" in header
    assert "HandleQueryRollbackCommand" in header
    assert "HandleClearRecoveryDebtCommand" in header
    assert "sPromotionRewardAuditMgr" in source
    assert "SEC_GAMEMASTER" in source
    assert "SEC_ADMINISTRATOR" in source
