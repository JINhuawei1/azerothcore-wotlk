#include "AddonThrottle.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "RequirementSystem.h"
#include "RewardTemplate.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr char SYNTHESIS_ADDON_PREFIX[] = "SYNTHSYS";

struct SynthesisEntry
{
    uint32 itemId = 0;
    uint32 upgradeLevel = 0;
    uint32 requirementId = 0;
    uint32 rewardId = 0;
    float successChance = 100.0f;
    uint32 boosterItemId = 0;
    float boosterChance = 0.0f;
    bool destroyOnFail = false;
};

std::string TrimAddonText(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '|' || ch == '^' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }
    return value;
}

std::string FormatChance(float value)
{
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(value == static_cast<uint32>(value) ? 0 : 1);
    ss << value;
    return ss.str();
}

std::string GetItemName(uint32 itemId)
{
    if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId))
        return TrimAddonText(itemTemplate->Name1);

    return "未知物品";
}

std::string GetItemIconPath(uint32 /*itemId*/)
{
    return "Interface\\Icons\\INV_Misc_QuestionMark";
}

void SendSynthesisPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    std::string fullMessage = std::string(SYNTHESIS_ADDON_PREFIX) + '\t' + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
}

std::vector<std::string> SplitFields(std::string const& text, char delimiter)
{
    std::vector<std::string> fields;
    std::string current;
    std::istringstream ss(text);
    while (std::getline(ss, current, delimiter))
        fields.push_back(current);
    return fields;
}

uint32 ToUInt32(std::string const& value)
{
    return static_cast<uint32>(std::strtoul(value.c_str(), nullptr, 10));
}

class SynthesisSystemMgr
{
public:
    static SynthesisSystemMgr* instance()
    {
        static SynthesisSystemMgr instance;
        return &instance;
    }

    void LoadConfig(bool /*reload*/)
    {
        _enabled = sConfigMgr->GetOption<bool>("SynthesisSystem.Enable", true);
        _announceStartup = sConfigMgr->GetOption<bool>("SynthesisSystem.AnnounceStartup", true);
        _throttleKey = sConfigMgr->GetOption<std::string>("SynthesisSystem.AddonThrottleKey", "SYNTHSYS");
    }

    bool IsEnabled() const { return _enabled; }

    uint32 LoadEntries()
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _entries.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `物品id`, `升级等级`, `需求id`, `升级成功奖励id`, `成功几率`, "
            "`合成几率物品id`, `合成几率提升`, `失败是否摧毁` "
            "FROM `_物品合成` ORDER BY `物品id`, `升级等级`");

        if (!result)
            return 0;

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            SynthesisEntry entry;
            entry.itemId = fields[0].Get<uint32>();
            entry.upgradeLevel = fields[1].Get<uint32>();
            entry.requirementId = fields[2].Get<uint32>();
            entry.rewardId = fields[3].Get<uint32>();
            entry.successChance = std::clamp(fields[4].Get<float>(), 0.0f, 100.0f);
            entry.boosterItemId = fields[5].Get<uint32>();
            entry.boosterChance = std::clamp(fields[6].Get<float>(), 0.0f, 100.0f);
            entry.destroyOnFail = fields[7].Get<uint8>() != 0;

            if (!sObjectMgr->GetItemTemplate(entry.itemId))
            {
                LOG_WARN("server.loading", "合成系统: 物品 {} 不存在，跳过配置", entry.itemId);
                continue;
            }

            if (entry.boosterItemId && !sObjectMgr->GetItemTemplate(entry.boosterItemId))
            {
                LOG_WARN("server.loading", "合成系统: 几率提升物品 {} 不存在，跳过配置 {}-{}",
                    entry.boosterItemId, entry.itemId, entry.upgradeLevel);
                continue;
            }

            _entries[{ entry.itemId, entry.upgradeLevel }] = entry;
            ++count;
        } while (result->NextRow());

        return count;
    }

    void SendList(Player* player)
    {
        if (!player)
            return;

        std::vector<SynthesisEntry> entries;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            for (auto const& pair : _entries)
                entries.push_back(pair.second);
        }

        SendSynthesisPayload(player, "LIST_BEGIN");

        for (SynthesisEntry const& entry : entries)
        {
            uint32 sourceCount = player->GetItemCount(entry.itemId, true);
            uint32 boosterCount = entry.boosterItemId ? player->GetItemCount(entry.boosterItemId, true) : 0;
            bool requirementOk = true;
            if (entry.requirementId != 0)
            {
                RequirementSystem* requirementSystem = sRequirementSystem;
                requirementOk = requirementSystem && requirementSystem->CheckRequirements(player, entry.requirementId, false);
            }

            std::vector<std::string> rewardDescriptions;
            if (entry.rewardId != 0 && sRewardTemplate->IsEnabled())
                rewardDescriptions = sRewardTemplate->GetRewardDescription(player, entry.rewardId);

            std::string rewardText = "无奖励";
            if (!rewardDescriptions.empty())
            {
                rewardText.clear();
                for (size_t i = 0; i < rewardDescriptions.size(); ++i)
                {
                    if (i)
                        rewardText += " / ";
                    rewardText += rewardDescriptions[i];
                }
            }

            std::ostringstream payload;
            payload << "ENTRY:"
                    << entry.itemId << '|'
                    << entry.upgradeLevel << '|'
                    << entry.requirementId << '|'
                    << entry.rewardId << '|'
                    << FormatChance(entry.successChance) << '|'
                    << entry.boosterItemId << '|'
                    << FormatChance(entry.boosterChance) << '|'
                    << (entry.destroyOnFail ? 1 : 0) << '|'
                    << TrimAddonText(GetItemName(entry.itemId)) << '|'
                    << GetItemIconPath(entry.itemId) << '|'
                    << TrimAddonText(rewardText) << '|'
                    << (requirementOk ? 1 : 0) << '|'
                    << sourceCount << '|'
                    << boosterCount;
            SendSynthesisPayload(player, payload.str());
        }

        SendSynthesisPayload(player, "LIST_DONE");
    }

    void TrySynthesize(Player* player, uint32 itemId, uint32 upgradeLevel, bool useBooster)
    {
        if (!player)
            return;

        SynthesisEntry entry;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto itr = _entries.find({ itemId, upgradeLevel });
            if (itr == _entries.end())
            {
                SendResult(player, false, "未找到该合成配置");
                return;
            }
            entry = itr->second;
        }

        if (player->GetItemCount(entry.itemId, true) == 0)
        {
            SendResult(player, false, "背包中没有需要合成的物品");
            return;
        }

        if (entry.rewardId == 0 || !sRewardTemplate->IsEnabled() || !sRewardTemplate->GetRewardTemplate(entry.rewardId))
        {
            SendResult(player, false, "合成奖励模板不存在或奖励系统未启用");
            return;
        }

        if (entry.requirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            if (!requirementSystem)
            {
                SendResult(player, false, "需求系统未初始化");
                return;
            }

            if (!requirementSystem->CheckRequirements(player, entry.requirementId, true))
            {
                SendResult(player, false, "未满足合成需求");
                return;
            }
        }

        if (useBooster)
        {
            if (entry.boosterItemId == 0 || entry.boosterChance <= 0.0f)
            {
                SendResult(player, false, "该配置没有可用的几率提升物品");
                return;
            }

            if (player->GetItemCount(entry.boosterItemId, true) == 0)
            {
                SendResult(player, false, "缺少几率提升物品");
                return;
            }
        }

        if (entry.requirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            if (!requirementSystem || !requirementSystem->ConsumeRequirements(player, entry.requirementId))
            {
                SendResult(player, false, "消耗合成需求失败");
                return;
            }
        }

        if (useBooster && entry.boosterItemId != 0)
            player->DestroyItemCount(entry.boosterItemId, 1, true);

        float finalChance = std::clamp(entry.successChance + (useBooster ? entry.boosterChance : 0.0f), 0.0f, 100.0f);
        bool success = roll_chance_f(finalChance);

        if (success)
        {
            player->DestroyItemCount(entry.itemId, 1, true);
            bool rewarded = sRewardTemplate->GiveReward(player, entry.rewardId, false, true);
            if (rewarded)
                SendResult(player, true, "合成成功");
            else
                SendResult(player, false, "合成成功但发放奖励失败，请检查奖励模板或背包空间");
        }
        else
        {
            if (entry.destroyOnFail)
                player->DestroyItemCount(entry.itemId, 1, true);
            SendResult(player, false, entry.destroyOnFail ? "合成失败，物品已摧毁" : "合成失败，物品未摧毁");
        }

        SendList(player);
    }

    bool AllowAddon(Player* player) const
    {
        if (!player)
            return false;
        return ModuleAddon::Throttle::Allow(player->GetGUID(), _throttleKey);
    }

    bool ShouldAnnounceStartup() const { return _announceStartup; }

private:
    void SendResult(Player* player, bool success, std::string const& message)
    {
        std::ostringstream payload;
        payload << "RESULT:" << (success ? 1 : 0) << '|' << TrimAddonText(message);
        SendSynthesisPayload(player, payload.str());
    }

    bool _enabled = true;
    bool _announceStartup = true;
    std::string _throttleKey = "SYNTHSYS";
    std::map<std::pair<uint32, uint32>, SynthesisEntry> _entries;
    std::mutex _mutex;
};

#define sSynthesisSystemMgr SynthesisSystemMgr::instance()

class SynthesisSystemWorldScript : public WorldScript
{
public:
    SynthesisSystemWorldScript() : WorldScript("SynthesisSystemWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sSynthesisSystemMgr->LoadConfig(reload);
    }

    void OnStartup() override
    {
        if (!sSynthesisSystemMgr->IsEnabled())
        {
            LOG_INFO("server.loading", "→合成系统已禁用");
            return;
        }

        uint32 count = sSynthesisSystemMgr->LoadEntries();
        if (sSynthesisSystemMgr->ShouldAnnounceStartup())
            LOG_INFO("server.loading", "→合成系统√ 已加载 {} 条配置", count);
    }
};

class SynthesisSystemPlayerScript : public PlayerScript
{
public:
    SynthesisSystemPlayerScript()
        : PlayerScript("SynthesisSystemPlayerScript", {
            PLAYERHOOK_ON_CHAT
        })
    {
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !sSynthesisSystemMgr->IsEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != SYNTHESIS_ADDON_PREFIX)
            return;

        if (!sSynthesisSystemMgr->AllowAddon(player))
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_LIST" || command == "REQ_ALL")
        {
            sSynthesisSystemMgr->SendList(player);
            return;
        }

        if (command.rfind("DO:", 0) == 0)
        {
            std::vector<std::string> fields = SplitFields(command.substr(3), '|');
            if (fields.size() < 3)
            {
                SendSynthesisPayload(player, "RESULT:0|合成请求格式错误");
                return;
            }

            uint32 itemId = ToUInt32(fields[0]);
            uint32 upgradeLevel = ToUInt32(fields[1]);
            bool useBooster = ToUInt32(fields[2]) != 0;
            sSynthesisSystemMgr->TrySynthesize(player, itemId, upgradeLevel, useBooster);
            return;
        }
    }
};
}

void AddSC_mod_synthesis_system()
{
    new SynthesisSystemWorldScript();
    new SynthesisSystemPlayerScript();
}
