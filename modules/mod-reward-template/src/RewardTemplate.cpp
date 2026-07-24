#include "RewardTemplate.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "BoundaryMgr.h"
#include "Config.h"
#include "Chat.h"
#include "CurrencySystem.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Logging/Log.h"
#include "Utilities/StringFormat.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Item.h"
#include "Configuration/Config.h"
#include <vector>
#include <string>
#include <sstream>
#include <fmt/format.h>
#include <random>
#include <limits>
#include <array>

namespace
{
int64 SignedCurrencyDelta(uint64 before, uint64 after)
{
    if (after >= before)
    {
        uint64 delta = after - before;
        return delta > static_cast<uint64>(std::numeric_limits<int64>::max())
            ? std::numeric_limits<int64>::max()
            : static_cast<int64>(delta);
    }

    uint64 delta = before - after;
    uint64 negativeLimit = static_cast<uint64>(std::numeric_limits<int64>::max()) + 1;
    if (delta >= negativeLimit)
        return std::numeric_limits<int64>::min();

    return -static_cast<int64>(delta);
}
}

RewardTemplate* RewardTemplate::_instance = nullptr;

RewardTemplate* RewardTemplate::instance()
{
    if (!_instance)
    {
        _instance = new RewardTemplate();
    }
    return _instance;
}

RewardTemplate::RewardTemplate()
{
    _enabled = false;
    _commandPermissionLevel = 1;
}

void RewardTemplate::Initialize()
{
    // 从配置文件获取值
    bool enabled = sConfigMgr->GetOption<bool>("RewardTemplate.Enable", true); // 默认启用
    _enabled = enabled;

    if (!_enabled)
        return;

    uint32 permLevel = sConfigMgr->GetOption<uint32>("RewardTemplate.CommandPermissionLevel", 1); // 默认权限级别
    _commandPermissionLevel = permLevel;

    LoadRewardTemplates();
}

void RewardTemplate::LoadRewardTemplates()
{
    _rewardTemplates.clear();

    // 明确指定字段顺序，避免依赖表结构变更
    QueryResult result = WorldDatabase.Query(
        "SELECT `注释`, `id`, `几率`, `经验`, `军衔经验`, `斗气经验`, `巅峰经验`, `成长经验`, "
        "`金币`, `泡点`, `积分`, `妖币`, `魔币`, `仙币`, `神币`, `战场分数`, `荣誉点数`, `成就点数x`, "
        "`奖励物品`, `技能组`, `Buff组`, `综合传送服务id`, `会员等级`, `天赋点数`, `军衔点数`, "
        "`斗气点数`, `巅峰点数`, `头衔id`, `GM命令`, `是否关闭提示`, `客户端显示` "
        "FROM `_模板_奖励` ORDER BY `id`");
    if (!result)
    {
        LOG_WARN("server.loading", ">> 未找到奖励模板数据");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        RewardTemplateEntry entry;
        // 按照SQL文件中的字段顺序获取数据
        entry.Comment = fields[0].Get<std::string>();              // 注释
        entry.Id = fields[1].Get<uint32>();                        // id
        entry.Chance = fields[2].Get<float>();                     // 几率
        entry.Experience = fields[3].Get<uint32>();                // 经验
        entry.MilitaryRankExp = fields[4].Get<uint32>();           // 军衔经验
        entry.FightingExp = fields[5].Get<uint32>();               // 斗气经验
        entry.PeakExp = fields[6].Get<uint32>();                   // 巅峰经验
        entry.GrowthExp = fields[7].Get<uint32>();                 // 成长经验
        entry.Money = fields[8].Get<int64>();                      // 金币
        entry.PaoDian = fields[9].Get<int32>();                    // 泡点
        entry.JiFen = fields[10].Get<int32>();                     // 积分
        entry.YaoBi = fields[11].Get<int32>();                     // 妖币
        entry.MoBi = fields[12].Get<int32>();                      // 魔币
        entry.XianBi = fields[13].Get<int32>();                    // 仙币
        entry.ShenBi = fields[14].Get<int32>();                    // 神币
        entry.BattlegroundScore = fields[15].Get<int32>();         // 战场分数
        entry.HonorPoints = fields[16].Get<int32>();               // 荣誉点数
        entry.AchievementPoints = fields[17].Get<int32>();         // 成就点数x

        // 解析奖励物品 (字段名: 奖励物品)
        std::string itemsStr = fields[18].Get<std::string>();
        if (!itemsStr.empty() && itemsStr != "0")
        {
            std::istringstream ss(itemsStr);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                // 跳过空白token
                if (token.empty())
                    continue;

                std::istringstream itemSS(token);
                uint32 itemId = 0, count = 0;

                // 校验解析结果
                if (!(itemSS >> itemId) || !(itemSS >> count))
                {
                    LOG_WARN("module", "奖励模板ID {} 物品解析失败，token: {}", entry.Id, token);
                    continue;
                }

                if (itemId > 0 && count > 0)
                {
                    RewardItemEntry item;
                    item.ItemId = itemId;
                    item.Count = count;
                    entry.RewardItems.push_back(item);
                }
            }
        }

        entry.SkillGroup = fields[19].Get<std::string>();           // 技能组
        entry.BuffGroup = fields[20].Get<std::string>();            // Buff组
        entry.TeleportServiceId = fields[21].Get<uint32>();         // 综合传送服务id
        entry.VipLevel = fields[22].Get<uint32>();                  // 会员等级
        entry.TalentPoints = fields[23].Get<uint32>();              // 天赋点数
        entry.MilitaryRankPoints = fields[24].Get<uint32>();        // 军衔点数
        entry.FightingPoints = fields[25].Get<uint32>();            // 斗气点数
        entry.PeakPoints = fields[26].Get<uint32>();                // 巅峰点数
        entry.TitleId = fields[27].Get<uint32>();                   // 头衔id
        entry.GMCommand = fields[28].Get<std::string>();            // GM命令
        entry.DisableNotification = fields[29].Get<bool>();         // 是否关闭提示
        entry.ClientDisplay = fields[30].Get<std::string>();        // 客户端显示

        _rewardTemplates.push_back(entry);
        count++;
    } while (result->NextRow());


}

RewardTemplateEntry const* RewardTemplate::GetRewardTemplate(uint32 rewardId) const
{
    for (auto& entry : _rewardTemplates)
    {
        if (entry.Id == rewardId)
            return &entry;
    }
    return nullptr;
}

bool RewardTemplate::GiveReward(Player* player, uint32 rewardId, bool checkChance, bool showNotification)
{
    return GiveRewardInternal(player, rewardId, checkChance, showNotification, true, nullptr);
}

bool RewardTemplate::GiveRewardWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt,
    bool checkChance, bool showNotification)
{
    receipt = {};
    return GiveRewardInternal(player, rewardId, checkChance, showNotification, true, &receipt);
}

bool RewardTemplate::GiveRewardWithoutItems(Player* player, uint32 rewardId, bool checkChance, bool showNotification)
{
    return GiveRewardInternal(player, rewardId, checkChance, showNotification, false, nullptr);
}

bool RewardTemplate::GiveRewardInternal(Player* player, uint32 rewardId, bool checkChance, bool showNotification,
    bool processItems, RewardGrantReceipt* receipt)
{
    if (!player || !_enabled)
        return false;

    RewardTemplateEntry const* reward = GetRewardTemplate(rewardId);
    if (!reward)
    {
        LOG_ERROR("module", "奖励模板ID {} 不存在", rewardId);
        return false;
    }

    // 是否显示提示：showNotification参数优先级最高，其次是模板配置
    // 【审计修复】机器人等无会话玩家 GetSession() 为空，ChatHandler 会无条件解引用 session，判空后再通知
    bool canNotify = showNotification && !reward->DisableNotification && player->GetSession();

    // 检查几率
    if (checkChance)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 100);
        float roll = dis(gen);

        if (roll > reward->Chance)
        {
            if (canNotify)
                ChatHandler(player->GetSession()).PSendSysMessage("未能获得奖励，运气不佳！");
            return false;
        }
    }

    int256 moneyBefore = player->GetMoney();
    static constexpr std::array<char const*, 6> currencyNames = { "泡点", "积分", "妖币", "魔币", "仙币", "神币" };
    std::array<uint64, currencyNames.size()> currenciesBefore{};
    if (receipt)
    {
        for (std::size_t i = 0; i < currencyNames.size(); ++i)
            currenciesBefore[i] = sCurrencySystem->GetCurrency(player, currencyNames[i]);
    }

    // 处理金币奖励
    if (reward->Money != 0)
    {
        if (reward->Money > 0)
        {
            player->ModifyMoney(reward->Money);
            if (canNotify)
                ChatHandler(player->GetSession()).PSendSysMessage("获得 {} 铜币", reward->Money);
        }
        else
        {
            uint64 moneyCost = reward->Money == std::numeric_limits<int64>::min() ? static_cast<uint64>(std::numeric_limits<int64>::max()) + 1 : static_cast<uint64>(-reward->Money);
            if (player->GetMoney() < moneyCost)
            {
                if (canNotify)
                    ChatHandler(player->GetSession()).PSendSysMessage("金币不足，无法扣除 {} 铜币", moneyCost);
                return false;
            }
            player->ModifyMoney(reward->Money);
            if (canNotify)
                ChatHandler(player->GetSession()).PSendSysMessage("失去 {} 铜币", moneyCost);
        }
    }

    // 处理经验奖励
    if (reward->Experience > 0 && player->GetLevel() < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
    {
        player->GiveXP(reward->Experience, nullptr);
        if (canNotify)
            ChatHandler(player->GetSession()).PSendSysMessage("获得 {} 经验值", reward->Experience);
    }

    // 处理荣誉点数奖励
    if (reward->HonorPoints != 0)
    {
        player->ModifyHonorPoints(reward->HonorPoints);
        if (canNotify)
            ChatHandler(player->GetSession()).PSendSysMessage("获得 {} 荣誉点数", reward->HonorPoints);
    }

    if (processItems)
    {
        // 处理物品奖励
        for (const auto& item : reward->RewardItems)
        {
            if (!ProcessRewardItem(player, item, receipt))
            {
                if (canNotify)
                    ChatHandler(player->GetSession()).PSendSysMessage("无法添加物品 {}，背包可能已满", item.ItemId);
            }
        }
    }

    // 处理货币奖励
    ProcessCurrencyReward(player, *reward, canNotify);

    // 处理境界经验/点数奖励
    // 【半截实现补齐】这两个函数此前从未被调用——8 个经验/点数字段配置后完全不发放
    ProcessExperienceReward(player, *reward);
    ProcessPointsReward(player, *reward);

    // 处理特殊奖励
    ProcessSpecialReward(player, *reward, canNotify);

    // 显示客户端提示
    if (!reward->ClientDisplay.empty() && canNotify)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("{}", reward->ClientDisplay);
    }

    if (receipt)
    {
        receipt->moneyDelta = Acore::Number::ToInt64Saturated(player->GetMoney() - moneyBefore);
        for (std::size_t i = 0; i < currencyNames.size(); ++i)
        {
            uint64 currencyAfter = sCurrencySystem->GetCurrency(player, currencyNames[i]);
            receipt->resourceDeltas[currencyNames[i]] = SignedCurrencyDelta(currenciesBefore[i], currencyAfter);
        }
    }

    return true;
}

bool RewardTemplate::GiveRandomReward(Player* player, const std::vector<uint32>& rewardIds)
{
    if (!player || !_enabled || rewardIds.empty())
        return false;

    std::vector<std::pair<uint32, float>> weightedRewards;
    float totalWeight = 0.0f;

    // 收集所有有效的奖励及其权重
    for (uint32 rewardId : rewardIds)
    {
        RewardTemplateEntry const* reward = GetRewardTemplate(rewardId);
        if (reward)
        {
            weightedRewards.push_back(std::make_pair(rewardId, reward->Chance));
            totalWeight += reward->Chance;
        }
    }

    if (weightedRewards.empty())
        return false;

    // 随机选择一个奖励
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, totalWeight);
    float roll = dis(gen);

    float currentWeight = 0.0f;
    for (const auto& pair : weightedRewards)
    {
        currentWeight += pair.second;
        if (roll <= currentWeight)
        {
            return GiveReward(player, pair.first, false); // 已经基于权重选择，不需要再次检查几率
        }
    }

    // 如果没有选中任何奖励，选择第一个
    return GiveReward(player, weightedRewards[0].first, false);
}

std::vector<std::string> RewardTemplate::GetRewardDescription(Player* player, uint32 rewardId)
{
    std::vector<std::string> descriptions;

    RewardTemplateEntry const* reward = GetRewardTemplate(rewardId);
    if (!reward)
    {
        descriptions.push_back("奖励模板不存在");
        return descriptions;
    }

    // 金币
    if (reward->Money != 0)
    {
        uint64 absValue = reward->Money > 0 ? static_cast<uint64>(reward->Money) : (reward->Money == std::numeric_limits<int64>::min() ? static_cast<uint64>(std::numeric_limits<int64>::max()) + 1 : static_cast<uint64>(-reward->Money));
        uint64 gold = absValue / 10000;
        uint64 silver = (absValue % 10000) / 100;
        uint64 copper = absValue % 100;
        std::string moneyStr = "";
        if (gold > 0) moneyStr += std::to_string(gold) + "金";
        if (silver > 0) moneyStr += std::to_string(silver) + "银";
        if (copper > 0) moneyStr += std::to_string(copper) + "铜";
        if (!moneyStr.empty())
        {
            if (reward->Money > 0)
                descriptions.push_back("获得金币: " + moneyStr);
            else
                descriptions.push_back("扣除金币: " + moneyStr);
        }
    }

    // 经验
    if (reward->Experience > 0)
        descriptions.push_back("经验: " + std::to_string(reward->Experience));

    // 荣誉
    if (reward->HonorPoints != 0)
    {
        if (reward->HonorPoints > 0)
            descriptions.push_back("获得荣誉点数: " + std::to_string(reward->HonorPoints));
        else
            descriptions.push_back("扣除荣誉点数: " + std::to_string(-reward->HonorPoints));
    }

    // 物品奖励 - 生成可点击的物品链接
    for (const auto& item : reward->RewardItems)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.ItemId);
        if (itemTemplate)
        {
            // 生成物品链接格式: |cFFFFFFFF|Hitem:itemId:0:0:0:0:0:0:0:0|h[物品名称]|h|r
            std::string qualityColor;
            switch (itemTemplate->Quality)
            {
                case ITEM_QUALITY_POOR:      qualityColor = "|cff9d9d9d"; break;  // 灰色
                case ITEM_QUALITY_NORMAL:    qualityColor = "|cffffffff"; break;  // 白色
                case ITEM_QUALITY_UNCOMMON:  qualityColor = "|cff1eff00"; break;  // 绿色
                case ITEM_QUALITY_RARE:      qualityColor = "|cff0070dd"; break;  // 蓝色
                case ITEM_QUALITY_EPIC:      qualityColor = "|cffa335ee"; break;  // 紫色
                case ITEM_QUALITY_LEGENDARY: qualityColor = "|cffff8000"; break;  // 橙色
                case ITEM_QUALITY_ARTIFACT:  qualityColor = "|cffe6cc80"; break;  // 金色
                case ITEM_QUALITY_HEIRLOOM:  qualityColor = "|cffe6cc80"; break;  // 传家宝
                default:                     qualityColor = "|cffffffff"; break;
            }

            std::string itemLink = qualityColor + "|Hitem:" + std::to_string(item.ItemId) +
                ":0:0:0:0:0:0:0:0|h[" + itemTemplate->Name1 + "]|h|r";
            descriptions.push_back("物品: " + itemLink + " x" + std::to_string(item.Count));
        }
        else
        {
            descriptions.push_back("物品ID: " + std::to_string(item.ItemId) + " x" + std::to_string(item.Count));
        }
    }

    // 各种货币（支持正负数显示）
    if (reward->PaoDian != 0)
    {
        if (reward->PaoDian > 0)
            descriptions.push_back("获得泡点: " + std::to_string(reward->PaoDian));
        else
            descriptions.push_back("扣除泡点: " + std::to_string(-reward->PaoDian));
    }
    if (reward->JiFen != 0)
    {
        if (reward->JiFen > 0)
            descriptions.push_back("获得积分: " + std::to_string(reward->JiFen));
        else
            descriptions.push_back("扣除积分: " + std::to_string(-reward->JiFen));
    }
    if (reward->YaoBi != 0)
    {
        if (reward->YaoBi > 0)
            descriptions.push_back("获得妖币: " + std::to_string(reward->YaoBi));
        else
            descriptions.push_back("扣除妖币: " + std::to_string(-reward->YaoBi));
    }
    if (reward->MoBi != 0)
    {
        if (reward->MoBi > 0)
            descriptions.push_back("获得魔币: " + std::to_string(reward->MoBi));
        else
            descriptions.push_back("扣除魔币: " + std::to_string(-reward->MoBi));
    }
    if (reward->XianBi != 0)
    {
        if (reward->XianBi > 0)
            descriptions.push_back("获得仙币: " + std::to_string(reward->XianBi));
        else
            descriptions.push_back("扣除仙币: " + std::to_string(-reward->XianBi));
    }
    if (reward->ShenBi != 0)
    {
        if (reward->ShenBi > 0)
            descriptions.push_back("获得神币: " + std::to_string(reward->ShenBi));
        else
            descriptions.push_back("扣除神币: " + std::to_string(-reward->ShenBi));
    }

    // 各种经验
    if (reward->MilitaryRankExp > 0)
        descriptions.push_back("军衔经验: " + std::to_string(reward->MilitaryRankExp));
    if (reward->FightingExp > 0)
        descriptions.push_back("斗气经验: " + std::to_string(reward->FightingExp));
    if (reward->PeakExp > 0)
        descriptions.push_back("巅峰经验: " + std::to_string(reward->PeakExp));
    if (reward->GrowthExp > 0)
        descriptions.push_back("成长经验: " + std::to_string(reward->GrowthExp));

    // 各种点数
    if (reward->TalentPoints > 0)
        descriptions.push_back("天赋点数: " + std::to_string(reward->TalentPoints));
    if (reward->MilitaryRankPoints > 0)
        descriptions.push_back("军衔点数: " + std::to_string(reward->MilitaryRankPoints));
    if (reward->FightingPoints > 0)
        descriptions.push_back("斗气点数: " + std::to_string(reward->FightingPoints));
    if (reward->PeakPoints > 0)
        descriptions.push_back("巅峰点数: " + std::to_string(reward->PeakPoints));

    // 头衔
    if (reward->TitleId > 0)
    {
        CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(reward->TitleId);
        if (titleInfo)
            descriptions.push_back("头衔: " + std::string(titleInfo->nameMale[0]));
        else
            descriptions.push_back("头衔ID: " + std::to_string(reward->TitleId));
    }

    // 如果没有任何奖励内容
    if (descriptions.empty())
        descriptions.push_back("无奖励内容");

    return descriptions;
}

bool RewardTemplate::ProcessRewardItem(Player* player, const RewardItemEntry& item, RewardGrantReceipt* receipt)
{
    if (!player)
        return false;

    // 检查物品是否存在
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.ItemId);
    if (!itemTemplate)
    {
        LOG_ERROR("module", "物品ID {} 不存在", item.ItemId);
        return false;
    }

    // 创建物品并添加到玩家背包
    uint32 noSpaceForCount = 0;
    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.ItemId, item.Count, &noSpaceForCount);

    if (msg != EQUIP_ERR_OK)
        return false;

    Item* newItem = player->StoreNewItem(dest, item.ItemId, true, Item::GenerateItemRandomPropertyId(item.ItemId));
    if (!newItem)
        return false;

    if (receipt)
        receipt->itemGuids.push_back(newItem->GetGUID().GetCounter());

    player->SendNewItem(newItem, item.Count, true, false);
    return true;
}

bool RewardTemplate::ProcessCurrencyReward(Player* player, const RewardTemplateEntry& reward, bool canNotify /*= true*/)
{
    if (!player)
        return false;

    // 【半截实现补齐】此前 6 种货币的 ModifyCurrency 调用全部被注释，
    // 只发"获得 X 泡点"假提示而不实际发放——配置了货币奖励的运营以为生效实际无效。
    // 现对接 mod-currency-system 真实发放；ModifyCurrency 自带获得/失去提示，
    // 这里不再重复发消息（canNotify/DisableNotification 不再作用于货币行——提示与发放保持一致）。
    (void)canNotify;

    if (!sCurrencySystem->IsEnabled())
    {
        if (reward.PaoDian != 0 || reward.JiFen != 0 || reward.YaoBi != 0 ||
            reward.MoBi != 0 || reward.XianBi != 0 || reward.ShenBi != 0)
        {
            LOG_WARN("module", "奖励模板 {} 配置了货币奖励，但货币系统模块未启用", reward.Id);
            return false;
        }

        return true;
    }

    bool success = true;

    auto giveCurrency = [&](char const* currencyName, int64 amount)
    {
        if (amount == 0)
            return;

        if (!sCurrencySystem->ModifyCurrency(player, currencyName, amount))
        {
            LOG_WARN("module", "奖励模板 {}: 玩家 {} 发放 {} {} 失败", reward.Id, player->GetName(), amount, currencyName);
            success = false;
        }
    };

    giveCurrency("泡点", reward.PaoDian);
    giveCurrency("积分", reward.JiFen);
    giveCurrency("妖币", reward.YaoBi);
    giveCurrency("魔币", reward.MoBi);
    giveCurrency("仙币", reward.XianBi);
    giveCurrency("神币", reward.ShenBi);

    return success;
}

bool RewardTemplate::ProcessExperienceReward(Player* player, const RewardTemplateEntry& reward)
{
    if (!player)
        return false;

    bool success = true;

    // 【半截实现补齐】军衔/斗气/巅峰经验对接境界系统共享经验池
    // （AddBoundaryExp 自带获得提示，这里不重复发消息）。
    // 此前只发"获得 X 军衔经验"假提示而不实际发放。
    if (reward.MilitaryRankExp > 0)
        success = sBoundaryMgr->AddBoundaryExp(player, BOUNDARY_MILITARY, reward.MilitaryRankExp) && success;

    if (reward.FightingExp > 0)
        success = sBoundaryMgr->AddBoundaryExp(player, BOUNDARY_FIGHTING, reward.FightingExp) && success;

    if (reward.PeakExp > 0)
        success = sBoundaryMgr->AddBoundaryExp(player, BOUNDARY_PEAK, reward.PeakExp) && success;

    // 成长经验需要目标物品（装备成长系统按物品记经验），奖励模板无法表达目标——
    // 不发放也不发假提示，仅告警提醒运营该字段无效
    if (reward.GrowthExp > 0)
    {
        LOG_WARN("module", "奖励模板 {}: 成长经验字段({})未实现（装备成长经验需要目标物品，模板无法表达），该配置不会发放",
            reward.Id, reward.GrowthExp);
    }

    return success;
}

bool RewardTemplate::ProcessPointsReward(Player* player, const RewardTemplateEntry& reward)
{
    if (!player)
        return false;

    // 【假提示清除】军衔/斗气/巅峰/天赋点数没有对应的点数系统实现，
    // 此前只发"获得 X 点数"假提示而不实际发放。改为仅告警，待对应系统落地后再接入。
    if (reward.MilitaryRankPoints > 0 || reward.FightingPoints > 0 || reward.PeakPoints > 0 || reward.TalentPoints > 0)
    {
        LOG_WARN("module", "奖励模板 {}: 点数字段(军衔:{} 斗气:{} 巅峰:{} 天赋:{})未实现，对应系统不存在，该配置不会发放",
            reward.Id, reward.MilitaryRankPoints, reward.FightingPoints, reward.PeakPoints, reward.TalentPoints);
    }

    return true;
}

bool RewardTemplate::ProcessSpecialReward(Player* player, const RewardTemplateEntry& reward, bool canNotify /*= true*/)
{
    if (!player)
        return false;

    bool success = true;
    bool shouldNotify = canNotify && !reward.DisableNotification && player->GetSession(); // 【审计修复】无会话玩家判空，610 行还会取 GetSessionDbcLocale

    // 处理头衔奖励
    if (reward.TitleId > 0)
    {
        CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(reward.TitleId);
        if (titleInfo)
        {
            player->SetTitle(titleInfo);
            if (shouldNotify)
                ChatHandler(player->GetSession()).PSendSysMessage("获得头衔: {}", titleInfo->nameMale[player->GetSession()->GetSessionDbcLocale()]);
        }
        else
        {
            LOG_ERROR("module", "头衔ID {} 不存在", reward.TitleId);
            success = false;
        }
    }

    // 处理GM命令
    if (!reward.GMCommand.empty())
    {
        // 这里需要谨慎处理，可能需要额外的安全检查
        // 由于没有具体实现，这里只记录日志
        LOG_INFO("module", "为玩家 {} 执行GM命令: {}", player->GetName(), reward.GMCommand);
    }

    // 处理技能组
    if (reward.SkillGroup != "0" && !reward.SkillGroup.empty())
    {
        // 解析技能ID列表
        std::istringstream ss(reward.SkillGroup);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            // 跳过空白token
            if (token.empty())
                continue;

            // 去除前后空白
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start == std::string::npos)
                continue;
            token = token.substr(start, end - start + 1);

            try
            {
                uint32 skillId = std::stoul(token);
                if (skillId > 0)
                {
                    // 这里需要调用相应的API来学习技能
                    // 由于没有具体实现，这里只记录日志
                    LOG_INFO("module", "玩家 {} 学习技能ID: {}", player->GetName(), skillId);
                    if (shouldNotify)
                        ChatHandler(player->GetSession()).PSendSysMessage("学习技能ID: {}", skillId);
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("module", "技能组解析失败，无效的技能ID: {}，错误: {}", token, e.what());
            }
        }
    }

    // 处理Buff组
    if (reward.BuffGroup != "0" && !reward.BuffGroup.empty())
    {
        // 解析Buff ID列表
        std::istringstream ss(reward.BuffGroup);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            // 跳过空白token
            if (token.empty())
                continue;

            // 去除前后空白
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start == std::string::npos)
                continue;
            token = token.substr(start, end - start + 1);

            try
            {
                uint32 spellId = std::stoul(token);
                if (spellId > 0)
                {
                    player->CastSpell(player, spellId, true);
                    if (shouldNotify)
                        ChatHandler(player->GetSession()).PSendSysMessage("获得Buff: {}", spellId);
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("module", "Buff组解析失败，无效的法术ID: {}，错误: {}", token, e.what());
            }
        }
    }

    return success;
}

// 玩家脚本实现
RewardTemplatePlayerScript::RewardTemplatePlayerScript() : PlayerScript("RewardTemplatePlayerScript") {}

void RewardTemplatePlayerScript::OnPlayerLogin(Player* player)
{
    if (!player || !sRewardTemplate->IsEnabled())
        return;

    // 这里可以实现登录奖励逻辑
    // 例如，每日首次登录奖励
    // sRewardTemplate->GiveReward(player, 2); // 假设ID 2是每日登录奖励
}

void RewardTemplatePlayerScript::OnPlayerLevelChanged(Player* player, uint8 oldLevel)
{
    if (!player || !sRewardTemplate->IsEnabled())
        return;

    // 这里可以实现升级奖励逻辑
    // 例如，特定等级的奖励
    // if (player->GetLevel() == 10 || player->GetLevel() == 20 || player->GetLevel() == 30)
    // {
    //     sRewardTemplate->GiveReward(player, 1); // 假设ID 1是升级奖励
    // }
}

void RewardTemplatePlayerScript::OnPlayerKilledByCreature(Creature* killer, Player* killed)
{
    if (!killer || !killed || !sRewardTemplate->IsEnabled())
        return;

    // 这里可以实现击杀奖励逻辑
    // 例如，击杀特定BOSS的奖励
    // if (killer->GetEntry() == 12345) // 假设12345是某个BOSS的ID
    // {
    //     sRewardTemplate->GiveReward(killed, 3); // 假设ID 3是击杀BOSS奖励
    // }
}
