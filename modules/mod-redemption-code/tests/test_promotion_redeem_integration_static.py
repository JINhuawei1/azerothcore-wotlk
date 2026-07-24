from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
REDEMPTION_CPP = REPO_ROOT / "modules/mod-redemption-code/src/RedemptionCodeModule.cpp"
REWARD_TEMPLATE_H = REPO_ROOT / "modules/mod-reward-template/src/RewardTemplate.h"
REWARD_TEMPLATE_CPP = REPO_ROOT / "modules/mod-reward-template/src/RewardTemplate.cpp"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _promotion_branch(source: str) -> str:
    start = source.index("if (sPromotionRewardMgr->IsPromotionCodeGroup(groupId))")
    end = source.index("// 发放奖励", start)
    return source[start:end]


def test_tracked_code_starts_snapshot_before_any_reward_mutation() -> None:
    source = _read(REDEMPTION_CPP)
    start = source.index("// 检查需求")
    end = source.index("// 返回奖励ID", start)
    redeem_flow = source[start:end]

    begin = redeem_flow.index("sPromotionRewardAuditMgr->BeginCodeRedeem")
    promotion_mutate = redeem_flow.index("sPromotionRewardMgr->RedeemPromotionCode")
    ordinary_mutate = redeem_flow.index("GiveRewardWithReceipt")

    assert begin < promotion_mutate
    assert begin < ordinary_mutate


def test_tracked_code_does_not_redeem_when_snapshot_capture_cannot_start() -> None:
    source = _read(REDEMPTION_CPP)

    assert "snapshot.grantId != 0 && !trackedPromotionCode" in source


def test_tracked_code_uses_an_exclusive_redeem_claim() -> None:
    audit = _read(REPO_ROOT / "modules/mod-promotion-reward/src/PromotionRewardAudit.cpp")
    header = _read(REPO_ROOT / "modules/mod-promotion-reward/src/PromotionRewardAudit.h")
    begin_start = audit.index("bool PromotionRewardAuditMgr::BeginCodeRedeem")
    complete_start = audit.index("void PromotionRewardAuditMgr::CompleteCodeRedeem", begin_start)
    begin = audit[begin_start:complete_start]
    complete_end = audit.index("bool PromotionRewardAuditMgr::RequestRollback", complete_start)
    complete = audit[complete_start:complete_end]

    assert "redeemToken" in header
    assert "REDEEM:" in begin
    assert "`处理令牌`" in begin
    assert "NOT EXISTS (SELECT 1 FROM `_宣传兑换流水`" in begin
    assert "snapshot.redeemToken" in begin
    assert "snapshot.redeemToken" in complete
    assert "`处理令牌`=''" in complete

    rollback_start = audit.index("bool PromotionRewardAuditMgr::RollbackGrant")
    rollback_end = audit.index("bool PromotionRewardAuditMgr::RetryRollback", rollback_start)
    rollback = audit[rollback_start:rollback_end]
    assert "`处理令牌`" in rollback
    assert "REDEEM:" in rollback


def test_failed_promotion_mutation_cancels_the_snapshot() -> None:
    branch = _promotion_branch(_read(REDEMPTION_CPP))

    assert "sPromotionRewardAuditMgr->CompleteCodeRedeem" in branch
    assert "snapshot, false" in branch


def test_successful_promotion_redemption_records_receipt_after_usage() -> None:
    branch = _promotion_branch(_read(REDEMPTION_CPP))

    receipt_grant = branch.index("GiveRewardWithoutItemsWithReceipt")
    usage = branch.index("RecordCodeUsage")
    complete = branch.index("sPromotionRewardAuditMgr->CompleteCodeRedeem", usage)

    assert receipt_grant < usage < complete
    assert "snapshot.rewardReceipt" in branch
    assert "snapshot, true" in branch
    assert "snapshot.receiptComplete = false" in branch
    assert "snapshot.receiptError" in branch


def test_successful_ordinary_tracked_code_uses_receipt_and_completion() -> None:
    source = _read(REDEMPTION_CPP)
    start = source.index("// 发放奖励")
    end = source.index("// 返回奖励ID", start)
    ordinary = source[start:end]

    assert "GiveRewardWithReceipt" in ordinary
    assert "snapshot.rewardReceipt" in ordinary
    assert "snapshot, false" in ordinary
    assert "snapshot, true" in ordinary


def test_audit_completion_persists_actual_redeemer_and_receipt() -> None:
    audit = _read(REPO_ROOT / "modules/mod-promotion-reward/src/PromotionRewardAudit.cpp")

    begin_start = audit.index("bool PromotionRewardAuditMgr::BeginCodeRedeem")
    complete_start = audit.index("void PromotionRewardAuditMgr::CompleteCodeRedeem", begin_start)
    begin = audit[begin_start:complete_start]
    complete_end = audit.index("bool PromotionRewardAuditMgr::RequestRollback", complete_start)
    complete = audit[complete_start:complete_end]

    assert "`回滚状态`='NONE'" in begin
    assert "`发放状态`,`回滚状态`" in begin
    assert 'grantStatus != "ISSUED"' in begin
    assert 'rollbackStatus != "NONE"' in begin
    assert "CollectPromotionItemGuids" in audit
    assert "snapshot.oldItemGuids" in audit
    assert "EndItemCapture(" in complete
    assert "snapshot.grantId, &captureExact, &captureError" in complete
    assert "INSERT INTO `_宣传兑换流水`" in audit
    assert "player->GetSession()->GetAccountId()" in audit
    assert "player->GetGUID().GetCounter()" in audit
    assert "player->GetSession()->GetRemoteAddress()" in audit
    assert "snapshot.rewardReceipt" in audit
    assert "snapshot.receiptComplete" in audit
    assert "`回滚状态`,`回滚错误`" in audit
    assert "SELECT `兑换角色GUID`,`账号ID`,`CDK` FROM `_宣传兑换流水`" in audit
    assert "player->SaveInventoryAndGoldToDB" in complete
    assert "CharacterDatabase.DirectCommitTransaction" in complete


def test_stale_redeem_claims_are_recovered_without_duplicate_rewards() -> None:
    audit = _read(REPO_ROOT / "modules/mod-promotion-reward/src/PromotionRewardAudit.cpp")
    start = audit.index("uint32 RecoverTimedOutRedeemClaims")
    end = audit.index("uint32 RecoverTimedOutProcessingGrants", start)
    recovery = audit[start:end]

    assert "LIKE 'REDEEM:%'" in recovery
    assert "_宣传兑换流水" in recovery
    assert "`回滚状态`='DEBT'" in recovery
    assert "`处理令牌`=''" in recovery

    consume_start = audit.index("bool PromotionRewardAuditMgr::ConsumeGrantQueue")
    consume_end = audit.index("bool PromotionRewardAuditMgr::IssueCdkGrant", consume_start)
    consume = audit[consume_start:consume_end]
    assert "RecoverTimedOutRedeemClaims(limit)" in consume


def test_reward_template_exposes_receipt_without_item_granting() -> None:
    header = _read(REWARD_TEMPLATE_H)
    source = _read(REWARD_TEMPLATE_CPP)

    assert "GiveRewardWithoutItemsWithReceipt" in header
    assert "GiveRewardWithoutItemsWithReceipt" in source
    assert "GiveRewardInternal(player, rewardId, checkChance, showNotification, false, &receipt)" in source
