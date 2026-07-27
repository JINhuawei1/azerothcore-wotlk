/*
 * 宣传奖励系统 (mod-promotion-reward)
 *
 * 设计:
 *   - 兑换码:对接 mod-redemption-code 的 `_奖励_兑换码` 表(直接 INSERT,不依赖其代码)
 *   - 奖励:对接 mod-reward-template 的 `_模板_奖励` 表(由 mod-redemption-code 兑换流程负责发放)
 *   - 玩家用 .兑换码 兑换 [CDK] 走 mod-redemption-code 现有命令拿到武器与其它奖励
 *   - 宣传武器属性由 mod-item-backpack-bonus / 装备系统生效,本模块只负责发放和升级物品
 *
 * 数据流:
 *   GM `.宣传奖励 发放 玩家A 1`
 *     → INSERT INTO `_奖励_兑换码` VALUES (注释,通用CDK,组,需求,奖励,...)
 *     → 在线玩家收到私聊提示
 *
 *   玩家 `.兑换码 兑换 XXX` (mod-redemption-code 命令)
 *     → 宣传组 CDK 兑换成功后,按玩家自己的宣传天数升级武器
 *     → 第1次给当前活动的1级武器,后续回收旧等级并发放下一等级
 *     → 玩家收到武器
 *
 *   持有宣传武器的玩家:
 *     OnPlayerStoreNewItem  → 入手武器 → 刷新 UI 状态
 *     OnPlayerEquip         → 装备武器 → 刷新 UI 状态
 *     OnPlayerAfterMoveItem → 移走武器 → 刷新 UI 状态
 *     OnPlayerLogin/Logout  → 加载/清除
 */

#ifndef PROMOTION_REWARD_MODULE_H
#define PROMOTION_REWARD_MODULE_H

#include "Define.h"
#include "ScriptMgr.h"
#include "Player.h"
#include <string>
#include <unordered_map>
#include <vector>

struct PromotionConfig
{
    uint32      id              = 0;
    bool        enabled         = false;
    bool        debugLog        = false;
    bool        isDefault       = false;

    std::string weaponName      = "宣传武器";
    uint32      weaponEntry     = 0;
    uint32      maxLevel        = 0;
    int256      baseAttrValue   = 0;
    int256      perDayAttrValue = 0;
    std::vector<int256> levelAttrValues;

    uint32      groupId         = 0;
    uint32      requireId       = 0;
    uint32      rewardId        = 0;

    uint32      announceType    = 27;
};

struct PromotionPlayerData
{
    uint32 days = 0;
};

struct PromotionItemCaptureContext
{
    uint64 operationId = 0;
    std::vector<uint32> itemGuids;
    std::unordered_map<uint32, uint32> initialItemCounts;
    bool receiptExact = true;
    std::string receiptError;
};

class PromotionRewardMgr
{
public:
    static PromotionRewardMgr* instance();

    void LoadConfig();
    void LoadAllPlayers();

    PromotionConfig const& GetConfig() const;
    PromotionConfig const* GetConfigForGroup(uint32 groupId) const;
    uint32 GetDefaultGroupId() const { return _defaultGroupId; }
    bool IsEnabled() const { return _enabled; }

    PromotionPlayerData* GetPlayerData(uint32 guid, bool createIfMissing = false);
    PromotionPlayerData* GetPlayerDataForGroup(uint32 guid, uint32 groupId, bool createIfMissing = false);
    void                 SavePlayerData(uint32 guid);
    void                 SavePlayerDataForGroup(uint32 guid, uint32 groupId);

    std::string GenerateUniqueCode();

    // 写入通用宣传 CDK 到 _奖励_兑换码 表
    // 玩家兑换成功时才增加宣传天数并升级武器
    bool IssueCodes(uint32 targetGuid, std::string const& targetName,
                    uint32 count, std::vector<std::string>& outCodes, std::string& errMsg);

    int256 CalcTotalAttr(uint32 days) const;
    int256 CalcTotalAttr(uint32 days, uint32 groupId) const;

    uint32 GetWeaponLevelForDays(uint32 days) const;
    uint32 GetWeaponLevelForDays(uint32 days, uint32 groupId) const;
    uint32 GetWeaponEntryForLevel(uint32 level) const;
    uint32 GetWeaponEntryForLevel(uint32 level, uint32 groupId) const;
    uint32 GetNextWeaponEntry(uint32 days) const;
    uint32 GetNextWeaponEntry(uint32 days, uint32 groupId) const;
    bool   IsPromotionWeaponEntry(uint32 entry) const;
    bool   IsPromotionWeaponEntry(uint32 entry, uint32 groupId) const;
    bool   IsPromotionCodeGroup(uint32 groupId) const;

    bool IsWeaponHeld(Player* player) const;
    bool IsWeaponHeld(Player* player, uint32 groupId) const;
    bool IsPromotionWeaponEquipped(Player* player) const;
    bool IsPromotionWeaponEquipped(Player* player, uint32 groupId) const;
    bool HasPromotionWeaponInBags(Player* player, uint32 groupId) const;

    // 由 mod-redemption-code 在宣传组 CDK 兑换成功前调用。
    // 成功后会增加该角色宣传次数,回收当前活动旧武器,发放下一等级武器。
    bool RedeemPromotionCode(Player* player, uint32 groupId, uint32& outRewardId);

    // 客户端 UI 通信 (Addon Message, prefix=PROMOREWARD)
    void SendAddonMsg(Player* player, std::string const& payload);
    void SendInfoToClient(Player* player);
    void SendOpenUIToClient(Player* player);

    // 给客户端 UI 用的兑换接口(直接走流程,不依赖 mod-redemption-code 命令)
    bool ClientRedeem(Player* player, std::string const& code, std::string& errMsg);

    // 审计发奖/兑换期间捕获 OnPlayerStoreNewItem 产生的物品 GUID。
    bool BeginItemCapture(Player* player, uint64 operationId);
    std::vector<uint32> EndItemCapture(Player* player, uint64 operationId,
        bool* receiptExact = nullptr, std::string* receiptError = nullptr);
    void CancelItemCapture(Player* player, uint64 operationId = 0);
    void RecordCapturedItem(Player* player, Item* item);
    void ClearAllItemCaptures();

private:
    PromotionRewardMgr() = default;
    ~PromotionRewardMgr() = default;
    PromotionRewardMgr(PromotionRewardMgr const&) = delete;
    PromotionRewardMgr& operator=(PromotionRewardMgr const&) = delete;

    static uint64 BuildPlayerKey(uint32 guid, uint32 groupId);

    bool                                             _enabled = false;
    bool                                             _debugLog = false;
    uint32                                           _defaultGroupId = 0;
    PromotionConfig                                  _disabledConfig;
    std::unordered_map<uint32, PromotionConfig>      _configs;
    std::unordered_map<uint64, PromotionPlayerData>  _players;
    std::unordered_map<uint32, PromotionItemCaptureContext> _itemCaptures;
};

#define sPromotionRewardMgr PromotionRewardMgr::instance()

class PromotionReward_WorldScript : public WorldScript
{
public:
    PromotionReward_WorldScript();
    void OnAfterConfigLoad(bool reload) override;
    void OnStartup() override;
    void OnUpdate(uint32 diff) override;
    void OnShutdown() override;

private:
    uint32 _auditTimerMs = 0;
};

class PromotionReward_PlayerScript : public PlayerScript
{
public:
    PromotionReward_PlayerScript();
    void OnPlayerLogin(Player* player) override;
    void OnPlayerLogout(Player* player) override;
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override;
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver) override;
};

#endif // PROMOTION_REWARD_MODULE_H
