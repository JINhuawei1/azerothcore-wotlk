#ifndef MODULE_REWARD_TEMPLATE_H
#define MODULE_REWARD_TEMPLATE_H

// 定义一个宏，表示奖励模板模块已加载
#define ACORE_WITH_REWARD_TEMPLATE

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "RewardInterface.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <string>

struct RewardItemEntry
{
    uint32 ItemId;
    uint32 Count;
};

struct RewardTemplateEntry
{
    uint32 Id;
    std::string Comment;
    float Chance;
    uint32 Experience;
    uint32 MilitaryRankExp;
    uint32 FightingExp;
    uint32 PeakExp;
    uint32 GrowthExp;
    int64 Money;
    int32 PaoDian;
    int32 JiFen;
    int32 YaoBi;
    int32 MoBi;
    int32 XianBi;
    int32 ShenBi;
    int32 BattlegroundScore;
    int32 HonorPoints;
    int32 AchievementPoints;
    std::vector<RewardItemEntry> RewardItems;
    std::string SkillGroup;
    std::string BuffGroup;
    uint32 TeleportServiceId;
    uint32 VipLevel;
    uint32 TalentPoints;
    uint32 MilitaryRankPoints;
    uint32 FightingPoints;
    uint32 PeakPoints;
    uint32 TitleId;
    std::string GMCommand;
    bool DisableNotification;
    std::string ClientDisplay;
};

class RewardTemplate : public RewardInterface
{
public:
    static RewardTemplate* instance();

    void Initialize();
    void LoadRewardTemplates();

    bool GiveReward(Player* player, uint32 rewardId, bool checkChance = true, bool showNotification = true) override;
    bool GiveRewardWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt,
        bool checkChance = true, bool showNotification = true) override;
    bool GiveRewardWithoutItems(Player* player, uint32 rewardId, bool checkChance = true, bool showNotification = true);
    bool GiveRewardWithoutItemsWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt,
        bool checkChance = true, bool showNotification = true);
    bool GiveRandomReward(Player* player, const std::vector<uint32>& rewardIds) override;
    std::vector<std::string> GetRewardDescription(Player* player, uint32 rewardId) override;

    RewardTemplateEntry const* GetRewardTemplate(uint32 rewardId) const;
    std::vector<RewardTemplateEntry> const& GetAllRewardTemplates() const { return _rewardTemplates; };

    bool IsEnabled() const { return _enabled; }

private:
    RewardTemplate();
    ~RewardTemplate() = default;

    bool GiveRewardInternal(Player* player, uint32 rewardId, bool checkChance, bool showNotification,
        bool processItems, RewardGrantReceipt* receipt = nullptr);
    bool ProcessRewardItem(Player* player, const RewardItemEntry& item, RewardGrantReceipt* receipt = nullptr);
    bool ProcessCurrencyReward(Player* player, const RewardTemplateEntry& reward, bool canNotify = true);
    bool ProcessExperienceReward(Player* player, const RewardTemplateEntry& reward);
    bool ProcessPointsReward(Player* player, const RewardTemplateEntry& reward);
    bool ProcessSpecialReward(Player* player, const RewardTemplateEntry& reward, bool canNotify = true);

    static RewardTemplate* _instance;

    std::vector<RewardTemplateEntry> _rewardTemplates;
    bool _enabled;
    uint32 _commandPermissionLevel;
};

#define sRewardTemplate RewardTemplate::instance()

class RewardTemplatePlayerScript : public PlayerScript
{
public:
    RewardTemplatePlayerScript();

    void OnPlayerLogin(Player* player) override;
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override;
    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override;
};

#endif // MODULE_REWARD_TEMPLATE_H
