# 自动宣传奖励核心集成 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 AzerothCore 内实现宣传提交发奖队列、CDK兑换流水、直接物品 GUID 记录、审核追回和连续无效封号，供 Web 服务调用。

**Architecture:** Web 只写 `characters` 宣传提交/发奖请求表；worldserver 中的 `mod-promotion-reward` 消费请求并执行游戏状态变化。`mod-redemption-code` 保持 CDK 不绑定账号/角色，但在兑换成功前后通知宣传模块保存实际兑换角色和奖励快照；回收使用快照，不按物品 entry 批量删除。

**Tech Stack:** AzerothCore C++、MySQL InnoDB、现有 `mod-promotion-reward`/`mod-redemption-code`/`mod-reward-template` 接口、Python 静态测试、`az` 数据库和服务器验证。

---

## 文件边界

- `modules/mod-promotion-reward/sql/world/20260724_宣传审核配置.sql`: 任务和奖励策略配置。
- `modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql`: 提交、发奖、兑换、统计和审核流水。
- `modules/mod-promotion-reward/src/PromotionRewardAudit.h/.cpp`: 状态机、队列消费、兑换快照和回收。
- `modules/mod-promotion-reward/src/PromotionRewardPolicy.h`: 不依赖 worldserver 的状态转移和连续无效次数纯函数。
- `modules/mod-promotion-reward/src/PromotionRewardModule.h/.cpp`: 现有宣传奖励管理器接入审计管理器和物品捕获上下文。
- `modules/mod-redemption-code/src/RedemptionCodeModule.h/.cpp`: 兑换前后回调，不增加账号/角色绑定限制。
- `modules/mod-reward-template/src/RewardInterface.h`: 新增带发放回执的可选接口。
- `modules/interfaces/RewardReceipt.h`: 纯数据回执结构,供 standalone 测试和奖励接口复用。
- `modules/mod-reward-template/src/RewardTemplate.h/.cpp`: 返回新建物品 GUID和可逆资源差额。
- `modules/mod-promotion-reward/CMakeLists.txt`: 注册新审计源文件。
- `modules/mod-promotion-reward/tests/test_promotion_sql_static.py`: SQL 幂等性和关键字段静态测试。
- `modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp`: 状态转移、连续无效次数和回收策略纯逻辑测试。

## Task 1: 创建幂等数据库结构

**Files:**
- Create: `modules/mod-promotion-reward/sql/world/20260724_宣传审核配置.sql`
- Create: `modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql`
- Create: `modules/mod-promotion-reward/tests/test_promotion_sql_static.py`

- [ ] **Step 1: Write the failing SQL contract test**

```python
from pathlib import Path


ROOT = Path(__file__).parents[3]
WORLD_SQL = ROOT / "modules/mod-promotion-reward/sql/world/20260724_宣传审核配置.sql"
CHAR_SQL = ROOT / "modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql"


def test_promotion_sql_is_idempotent_and_non_destructive():
    world = WORLD_SQL.read_text(encoding="utf-8")
    chars = CHAR_SQL.read_text(encoding="utf-8")
    combined = world + "\n" + chars
    assert "CREATE TABLE IF NOT EXISTS `_宣传审核任务`" in world
    assert "CREATE TABLE IF NOT EXISTS `_宣传提交记录`" in chars
    assert "CREATE TABLE IF NOT EXISTS `_宣传奖励流水`" in chars
    assert "CREATE TABLE IF NOT EXISTS `_宣传兑换流水`" in chars
    assert "CREATE TABLE IF NOT EXISTS `_宣传账号统计`" in chars
    assert "CREATE TABLE IF NOT EXISTS `_宣传IP统计`" in chars
    assert "CREATE TABLE IF NOT EXISTS `_宣传审核日志`" in chars
    assert "ON DUPLICATE KEY UPDATE" in world
    assert "DROP TABLE" not in combined.upper()
    for token in ("账号每日上限", "IP每日上限", "奖励模式", "request_id", "兑换角色GUID", "回滚状态"):
        assert token in combined
```

- [ ] **Step 2: Run the contract test and verify it fails**

Run:

```powershell
python -m pytest modules/mod-promotion-reward/tests/test_promotion_sql_static.py -q
```

Expected: FAIL because the two migration files and test contract do not exist yet.

- [ ] **Step 3: Add the world task configuration SQL**

Create `_宣传审核任务` with `任务ID`, `名称`, `启用`, `奖励模式`, `奖励组`, `需求ID`, `奖励ID`, `物品entry`, `物品数量`, `账号每日上限`, `IP每日上限`, `任务每日上限`, `关键词`, `连续无效封号次数`, `创建时间`, `更新时间`.

Use `ENGINE=InnoDB`, `utf8mb4`, and this repeat-safe seed shape:

```sql
INSERT INTO `_宣传审核任务`
  (`任务ID`,`名称`,`启用`,`奖励模式`,`奖励组`,`需求ID`,`奖励ID`,
   `物品entry`,`物品数量`,`账号每日上限`,`IP每日上限`,`任务每日上限`,
   `关键词`,`连续无效封号次数`)
VALUES
  (1,'默认宣传任务',1,'CDK',9001,0,100,0,0,1,3,1,'艾萨拉,服务器,宣传',3)
ON DUPLICATE KEY UPDATE
  `名称`=VALUES(`名称`),
  `更新时间`=CURRENT_TIMESTAMP;
```

`奖励模式` 使用 `varchar(8)` 而不是 `ENUM`,以便以后增加模式而不改表结构。直接物品模式的角色绑定策略由 worldserver 强制执行,不是 CDK 绑定。

- [ ] **Step 4: Add the character-side audit tables**

Create the following InnoDB tables with primary keys and indexes on `(账号ID,创建时间)`, `(IP地址,创建时间)`, `状态`, `发放状态`, and `CDK`:

```sql
CREATE TABLE IF NOT EXISTS `_宣传提交记录` (
  `提交ID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `任务ID` int unsigned NOT NULL,
  `账号ID` int unsigned NOT NULL,
  `角色GUID` int unsigned NOT NULL DEFAULT 0,
  `IP地址` varchar(45) NOT NULL,
  `类型` varchar(8) NOT NULL,
  `内容引用` text NOT NULL,
  `内容哈希` char(64) NOT NULL,
  `预检状态` varchar(24) NOT NULL DEFAULT 'PENDING',
  `审核状态` varchar(24) NOT NULL DEFAULT 'PENDING',
  `审核人账号ID` int unsigned NOT NULL DEFAULT 0,
  `审核理由` varchar(500) DEFAULT NULL,
  `创建时间` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `审核时间` datetime DEFAULT NULL,
  `更新时间` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`提交ID`),
  KEY `idx_account_created` (`账号ID`,`创建时间`),
  KEY `idx_ip_created` (`IP地址`,`创建时间`),
  KEY `idx_review_status` (`审核状态`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `_宣传奖励流水` (
  `发放ID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `提交ID` bigint unsigned NOT NULL,
  `request_id` char(36) NOT NULL,
  `模式` varchar(8) NOT NULL,
  `CDK` varchar(64) DEFAULT NULL,
  `奖励ID` int unsigned NOT NULL DEFAULT 0,
  `物品entry` int unsigned NOT NULL DEFAULT 0,
  `物品数量` int unsigned NOT NULL DEFAULT 0,
  `发放状态` varchar(24) NOT NULL DEFAULT 'REQUESTED',
  `回滚状态` varchar(24) NOT NULL DEFAULT 'NONE',
  `错误信息` varchar(500) DEFAULT NULL,
  `创建时间` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `完成时间` datetime DEFAULT NULL,
  PRIMARY KEY (`发放ID`),
  UNIQUE KEY `uk_request_id` (`request_id`),
  UNIQUE KEY `uk_submission_grant` (`提交ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `_宣传IP统计` (
  `IP地址` varchar(45) NOT NULL,
  `统计日期` date NOT NULL,
  `提交次数` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`IP地址`,`统计日期`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Add `_宣传兑换流水` fields for `兑换角色GUID`, `兑换账号ID`, `兑换时间`, `兑换前天数`, `兑换后天数`, `旧物品GUID`, `新物品GUID`, `新增奖励GUID`, `资源快照`, `回滚状态`, `回滚时间`, and `回滚错误`; add `_宣传账号统计` keyed by `(账号ID,统计日期)` with `账号提交次数`, `连续无效次数`, and `封禁状态`; add `_宣传IP统计` keyed by `(IP地址,统计日期)` with `提交次数`; add `_宣传审核日志` with old/new status, operator, reason, and timestamp.

- [ ] **Step 5: Run the contract test and repeat-import checks**

Run:

```powershell
python -m pytest modules/mod-promotion-reward/tests/test_promotion_sql_static.py -q
$worldSql = Get-Content -Raw 'modules/mod-promotion-reward/sql/world/20260724_宣传审核配置.sql'
$charSql = Get-Content -Raw 'modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql'
az --json database query world $worldSql
az --json database query characters $charSql
```

Expected: the Python test passes; both SQL imports succeed twice; row counts and the default task remain unchanged after the second import.

- [ ] **Step 6: Commit the schema independently**

```powershell
git add modules/mod-promotion-reward/sql/world/20260724_宣传审核配置.sql modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql modules/mod-promotion-reward/tests/test_promotion_sql_static.py
git commit -m "feat: add promotion audit tables"
```

## Task 2: Add reward receipts for exact item tracking

**Files:**
- Modify: `modules/mod-reward-template/src/RewardInterface.h`
- Modify: `modules/mod-reward-template/src/RewardTemplate.h`
- Modify: `modules/mod-reward-template/src/RewardTemplate.cpp`
- Create: `modules/interfaces/RewardReceipt.h`
- Create: `modules/mod-reward-template/tests/RewardReceiptPolicyTest.cpp`

- [ ] **Step 1: Define the receipt contract and failing policy test**

Create a dependency-free `RewardReceipt.h` POD and include it from `RewardInterface.h`:

```cpp
struct RewardGrantReceipt
{
    std::vector<uint32> itemGuids;
    int64 moneyDelta = 0;
    std::map<std::string, int64> resourceDeltas;
};
```

Add a non-breaking virtual default method:

```cpp
virtual bool GiveRewardWithReceipt(Player* player, uint32 rewardId,
                                   RewardGrantReceipt& receipt,
                                   bool checkChance = true,
                                   bool showNotification = true)
{
    return GiveReward(player, rewardId, checkChance, showNotification);
}
```

The test must assert that a receipt is empty on a failed grant and that repeated receipt application is rejected by the audit policy. Run the new test through the repository's existing standalone MSVC/gtest command used for module tests.

- [ ] **Step 2: Run the test and verify it fails**

Run the standalone receipt test and then build worldserver:

```powershell
cl /nologo /std:c++17 /EHsc modules/mod-reward-template/tests/RewardReceiptPolicyTest.cpp /Fe:build\promotion_reward_receipt_tests.exe
build\promotion_reward_receipt_tests.exe
cmake --build build --config RelWithDebInfo --target worldserver
```

Expected: the standalone test/compile fails because `RewardGrantReceipt` and `GiveRewardWithReceipt` are not defined.

- [ ] **Step 3: Capture item GUIDs in `RewardTemplate`**

Change `GiveRewardInternal` to accept an optional `RewardGrantReceipt*`. In `ProcessRewardItem`, after `StoreNewItem` succeeds, append `newItem->GetGUID().GetCounter()` to the receipt. Snapshot money and the six custom currencies before processing, then write signed deltas after processing. Keep existing `GiveReward` behavior by passing `nullptr`.

Implement `GiveRewardWithReceipt` as:

```cpp
bool RewardTemplate::GiveRewardWithReceipt(Player* player, uint32 rewardId,
                                           RewardGrantReceipt& receipt,
                                           bool checkChance, bool showNotification)
{
    receipt = {};
    return GiveRewardInternal(player, rewardId, checkChance, showNotification, true, &receipt);
}
```

Do not change the existing `GiveReward` and `GiveRewardWithoutItems` call semantics.

- [ ] **Step 4: Run the reward-template build and test**

Run:

```powershell
cmake --build build --config RelWithDebInfo --target worldserver
ctest --test-dir build -C RelWithDebInfo -R RewardReceiptPolicyTest --output-on-failure
```

Expected: build succeeds and the receipt policy test passes.

- [ ] **Step 5: Commit the receipt contract**

```powershell
git add modules/mod-reward-template/src/RewardInterface.h modules/mod-reward-template/src/RewardTemplate.h modules/mod-reward-template/src/RewardTemplate.cpp modules/mod-reward-template/tests/RewardReceiptPolicyTest.cpp
git commit -m "feat: expose reward grant receipts"
```

## Task 3: Implement promotion audit state and queue consumption

**Files:**
- Create: `modules/mod-promotion-reward/src/PromotionRewardPolicy.h`
- Create: `modules/mod-promotion-reward/src/PromotionRewardAudit.h`
- Create: `modules/mod-promotion-reward/src/PromotionRewardAudit.cpp`
- Modify: `modules/mod-promotion-reward/src/PromotionRewardModule.h`
- Modify: `modules/mod-promotion-reward/src/PromotionRewardModule.cpp`
- Modify: `modules/mod-promotion-reward/CMakeLists.txt`
- Modify: `modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp`

- [ ] **Step 1: Add pure state-policy tests**

Create `PromotionRewardPolicy.h` with dependency-free functions and define tests for these transitions:

```cpp
#include <cassert>

int main()
{
    assert(NextReviewStatus(ReviewStatus::Pending, ReviewDecision::Approve) == ReviewStatus::Approved);
    assert(NextReviewStatus(ReviewStatus::Pending, ReviewDecision::Reject) == ReviewStatus::Rejected);
    assert(NextInvalidStreak(2, ReviewDecision::Reject, 3) == 3u);
    assert(NextInvalidStreak(2, ReviewDecision::Approve, 3) == 0u);
    assert(ShouldBanAfterReject(3, 3));
    assert(!ShouldBanAfterReject(2, 3));
}
```

Run the standalone policy test and verify it fails because the state types/functions do not exist.

- [ ] **Step 2: Define audit states and data contracts**

Include `PromotionRewardPolicy.h` from `PromotionRewardAudit.h`, then add these audit-only types and methods:

```cpp
enum class ReviewStatus : uint8 { Pending, Approved, Rejected };
enum class GrantMode : uint8 { Cdk, Item };
enum class GrantStatus : uint8 { Requested, Issued, Redeemed, Final, Revoked, RecoveryDebt };
enum class RollbackStatus : uint8 { None, Requested, Running, Completed, RecoveryDebt, Failed };

struct PromotionRedeemSnapshot
{
    uint64 submissionId = 0;
    uint64 grantId = 0;
    std::string code;
    uint32 accountId = 0;
    uint32 characterGuid = 0;
    uint32 beforeDays = 0;
    std::vector<uint32> oldItemGuids;
    std::vector<uint32> newItemGuids;
    RewardGrantReceipt rewardReceipt;
};
```

Add `PromotionRewardAuditMgr` methods:

```cpp
bool ConsumeGrantQueue(uint32 limit = 10);
bool ConsumeReviewQueue(uint32 limit = 10);
bool BeginCodeRedeem(Player* player, std::string const& code, PromotionRedeemSnapshot& snapshot);
void CompleteCodeRedeem(Player* player, std::string const& code, uint32 rewardId,
                        PromotionRedeemSnapshot const& snapshot, bool success);
bool RequestRollback(uint64 submissionId, std::string const& reason);
bool RollbackGrant(uint64 grantId);
void ApplyReviewDecision(uint64 submissionId, bool approved, uint32 reviewerAccountId,
                         std::string const& reason);
```

Expose `#define sPromotionRewardAuditMgr PromotionRewardAuditMgr::instance()` in the header. Keep the existing `sPromotionRewardMgr` macro for the current promotion weapon manager; redemption callbacks use the audit-manager macro below.

- [ ] **Step 3: Implement idempotent queue consumption**

In `OnUpdate`, poll at most 10 rows whose `发放状态='REQUESTED'` and atomically move each row to `PROCESSING` using `request_id` and `发放ID`. For `CDK`, call the existing unique-code generator and insert `_奖励_兑换码` with `兑换次数=1`; for `ITEM`, create the configured bound item and record its GUID from the active capture context. Set `发放状态='ISSUED'` only after the database receipt is written.

If the same `request_id` is seen again, return the existing `发放ID` and never create another reward.

`ConsumeReviewQueue` runs in the same bounded update loop. An approved row moves its grant to `FINAL`; a rejected row moves its grant to `回滚状态='REQUESTED'` and invokes `RollbackGrant`. The worker claims rows by status and compare-and-set updates so a repeated Web request is harmless.

- [ ] **Step 4: Add item capture context to the existing player script**

Add a per-player capture map in `PromotionRewardMgr` keyed by `player->GetGUID().GetCounter()`. While a grant or CDK redemption is active, `OnPlayerStoreNewItem` appends the item GUID to the corresponding receipt. Clear the context on success, failure, logout, and world shutdown. Existing promotion UI refresh behavior remains unchanged.

- [ ] **Step 5: Build the module and run policy tests**

Run:

```powershell
cl /nologo /std:c++17 /EHsc modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp /Fe:build\promotion_reward_audit_policy_tests.exe
build\promotion_reward_audit_policy_tests.exe
cmake --build build --config RelWithDebInfo --target worldserver
```

Expected: worldserver builds and all state/idempotency tests pass.

- [ ] **Step 6: Commit queue and state machine**

```powershell
git add modules/mod-promotion-reward/src/PromotionRewardAudit.h modules/mod-promotion-reward/src/PromotionRewardAudit.cpp modules/mod-promotion-reward/src/PromotionRewardModule.h modules/mod-promotion-reward/src/PromotionRewardModule.cpp modules/mod-promotion-reward/CMakeLists.txt modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp
git commit -m "feat: add promotion reward audit queue"
```

## Task 4: Record CDK users without adding CDK binding

**Files:**
- Modify: `modules/mod-redemption-code/src/RedemptionCodeModule.h`
- Modify: `modules/mod-redemption-code/src/RedemptionCodeModule.cpp`
- Modify: `modules/mod-promotion-reward/src/PromotionRewardAudit.h`
- Modify: `modules/mod-promotion-reward/src/PromotionRewardAudit.cpp`

- [ ] **Step 1: Add pre/post redemption callbacks**

Call the promotion audit manager immediately before the reward mutation and immediately after successful `RecordCodeUsage`:

```cpp
PromotionRedeemSnapshot snapshot;
bool trackedPromotionCode = sPromotionRewardAuditMgr->BeginCodeRedeem(player, code, snapshot);
// existing requirement and reward flow
if (trackedPromotionCode)
    sPromotionRewardAuditMgr->CompleteCodeRedeem(player, code, outRewardId, snapshot, true);
```

On any failed reward path, call the same completion method with `success=false` and do not decrement the CDK. The callback must only act when `_宣传奖励流水.CDK` matches; ordinary redemption codes keep their current behavior.

- [ ] **Step 2: Snapshot and record the actual redeemer**

Before `RedeemPromotionCode` or `GiveReward`, capture the character GUID, account ID, current promotion days, and all current promotion item GUIDs. After success, capture new GUIDs from the receipt/context, set `兑换角色GUID` from the actual player, and write `兑换时间=NOW()`.

Do not populate `_奖励_兑换码.兑换账号` or `_奖励_兑换码.兑换角色` ahead of time. The existing `RecordCodeUsage()` remains the source of actual CDK user information; the normalized promotion row is the rollback index.

- [ ] **Step 3: Test one-use and shared-code behavior**

Add tests asserting that a generated promotion code has `兑换次数=1`, that a second player can redeem it only before the first use, and that the recorded redeemer GUID—not the Web submitter GUID—is stored in `_宣传兑换流水`.

- [ ] **Step 4: Build and commit the redemption integration**

Run:

```powershell
cmake --build build --config RelWithDebInfo --target worldserver
```

Expected: worldserver builds with both modules enabled. Commit:

```powershell
git add modules/mod-redemption-code/src/RedemptionCodeModule.h modules/mod-redemption-code/src/RedemptionCodeModule.cpp modules/mod-promotion-reward/src/PromotionRewardAudit.h modules/mod-promotion-reward/src/PromotionRewardAudit.cpp
git commit -m "feat: record promotion code redeemers"
```

## Task 5: Implement precise rollback and consecutive ban

**Files:**
- Modify: `modules/mod-promotion-reward/src/PromotionRewardAudit.cpp`
- Modify: `modules/mod-promotion-reward/src/PromotionRewardModule.cpp`
- Modify: `modules/mod-promotion-reward/src/PromotionRewardCommands.cpp`
- Modify: `modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql`
- Modify: `modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp`

- [ ] **Step 1: Implement unused-CDK revocation**

For a rejected submission whose CDK still has `兑换次数>0`, update the CDK row to `兑换次数=0`, set grant `回滚状态='COMPLETED'`, and preserve the row. Never call the existing delete-code command.

- [ ] **Step 2: Implement used-CDK rollback**

Use `_宣传兑换流水.兑换角色GUID` first and `_奖励_兑换码.兑换角色` as the fallback lookup. Load the actual character state and:

1. remove only the recorded `newItemGuids` that still belong to that character;
2. restore recorded `oldItemGuids` or recreate the exact previous promotion item when the snapshot proves it was destroyed by the upgrade;
3. restore `beforeDays` through `PromotionRewardMgr::SavePlayerData`;
4. subtract only receipt-recorded reversible resource deltas;
5. mark `COMPLETED` only after every required operation succeeds.

When a recorded item no longer exists or a resource has already been consumed, set `回滚状态='RECOVERY_DEBT'`, write the exact missing GUID/resource to `回滚错误`, and block further automatic promotion grants for that account until an administrator clears the debt.

- [ ] **Step 3: Implement direct-item rollback**

For `ITEM` grants, delete only the exact recorded item GUIDs and require the configured item template to be character-bound. If the item is equipped, unequip it before destruction and save the character inventory state.

- [ ] **Step 4: Implement review decision and ban state**

The Web service writes the review decision and audit row; the worldserver review consumer applies it. Use a transaction for the claimed review row, account statistics, audit log, and rollback request. On rejection increment `连续无效次数`; on approval set it to zero. When the new count reaches the configured threshold (default 3), insert/refresh `auth.account_banned`, disconnect the online account, and set `封禁状态='BANNED'`. Replaying the same review request must not increment again or create another ban row.

- [ ] **Step 5: Add GM recovery commands**

Extend `.宣传奖励` with:

```text
.宣传奖励 审核通过 <提交ID>
.宣传奖励 审核无效 <提交ID> <理由>
.宣传奖励 回收重试 <提交ID>
.宣传奖励 回收查询 <提交ID>
```

Commands call the same audit manager methods as Web; they do not duplicate SQL logic. Use `SEC_GAMEMASTER` for query/retry and `SEC_ADMINISTRATOR` for decisions.

- [ ] **Step 6: Run rollback policy tests and commit**

Run:

```powershell
cl /nologo /std:c++17 /EHsc modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp /Fe:build\promotion_reward_audit_policy_tests.exe
build\promotion_reward_audit_policy_tests.exe
cmake --build build --config RelWithDebInfo --target worldserver
```

Expected: unused CDK, used CDK, missing-item debt, direct-item GUID, consecutive reset, and three-invalid ban tests pass. Commit:

```powershell
git add modules/mod-promotion-reward/src/PromotionRewardAudit.cpp modules/mod-promotion-reward/src/PromotionRewardModule.cpp modules/mod-promotion-reward/src/PromotionRewardCommands.cpp modules/mod-promotion-reward/sql/characters/20260724_宣传审核流水.sql modules/mod-promotion-reward/tests/PromotionRewardAuditPolicyTest.cpp
git commit -m "feat: add promotion reward rollback"
```

## Task 6: Validate against live databases and server health

**Files:**
- No source changes; use `agent-harness` and `az`.

- [ ] **Step 1: Import and inspect the new tables**

Run:

```powershell
az --json database query world "SHOW TABLES LIKE '_宣传审核任务'"
az --json database query characters "SHOW TABLES LIKE '_宣传提交记录'"
az --json database query characters "SHOW TABLES LIKE '_宣传兑换流水'"
```

Expected: all configured tables exist in the intended database.

- [ ] **Step 2: Run a controlled CDK flow**

Insert one `REQUESTED` row with a unique `request_id`, wait for worldserver queue consumption, redeem the resulting CDK with a test character, and query:

```powershell
az --json database query characters "SELECT `发放ID`,`CDK`,`发放状态` FROM `_宣传奖励流水` ORDER BY `发放ID` DESC LIMIT 1"
az --json database query characters "SELECT `兑换角色GUID`,`兑换前天数`,`兑换后天数`,`新物品GUID`,`回滚状态` FROM `_宣传兑换流水` ORDER BY `兑换流水ID` DESC LIMIT 1"
```

Expected: exactly one CDK use is recorded with the actual redeemer GUID and at least one new item GUID.

- [ ] **Step 3: Run approval, rejection, and retry flows**

Approve one submission and verify `回滚状态='NONE'` and invalid streak `0`. Reject a second submission and verify the item/CDK is removed. Delete a recorded item before retrying and verify `RECOVERY_DEBT` with no unrelated item deletion. Repeat rejection three times and verify `auth.account_banned` plus worldserver disconnect.

- [ ] **Step 4: Verify health and logs**

Run:

```powershell
az --json process health
az --json validate report
```

Expected: worldserver remains healthy, database connections are healthy, and console-facing lifecycle/recovery messages use `LOG_INFO("server.loading", ...)` or the existing visible module category.
