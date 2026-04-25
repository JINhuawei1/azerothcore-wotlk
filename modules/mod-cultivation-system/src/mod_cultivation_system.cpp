/*
 * 修仙系统 (mod-cultivation-system)
 *
 * 10大境界×10小级=100级，每突破大境界解锁一个仙术
 * 所有材料消耗通过需求系统（mod-requirement-template）处理
 */

#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "World.h"
#include "Player.h"
#include "Unit.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "GossipDef.h"
#include "ScriptedGossip.h"
#include "ObjectAccessor.h"
#include "Creature.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "SpellMgr.h"
#include "Object/Updates/UpdateFields.h"
#include "ModuleManager.h"
#include "RequirementInterface.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>

using namespace Acore::ChatCommands;

// Addon消息通信常量
static constexpr const char* CULTIVATION_ADDON_PREFIX = "CULT_SYS";
static constexpr size_t CULTIVATION_MAX_ADDON_PAYLOAD = 220;

// ============================================
// 数据结构
// ============================================

struct CultivationRealmConfig
{
    uint32 level = 0;
    std::string name;
    uint8 majorRealm = 0;
    uint8 minorRealm = 0;
    float statBonus = 0.0f;
    uint32 upgradeRequireId = 0;
    bool needTribulation = false;
    uint32 tribRequireId = 0;
    uint32 tribBossEntry = 0;
};

struct CultivationSkillConfig
{
    uint32 skillId = 0;
    std::string name;
    uint32 unlockLevel = 0;
    uint32 spellId = 0;
    uint8 skillType = 0;
    std::string description;
};

struct PlayerCultivationData
{
    uint32 level = 0;
    uint32 tribCooldown = 0;
};

// ============================================
// 大境界名称映射
// ============================================

static const char* GetMajorRealmName(uint8 majorRealm)
{
    switch (majorRealm)
    {
        case 1: return "炼气期";
        case 2: return "筑基期";
        case 3: return "金丹期";
        case 4: return "元婴期";
        case 5: return "化神期";
        case 6: return "炼虚期";
        case 7: return "合体期";
        case 8: return "大乘期";
        case 9: return "渡劫期";
        case 10: return "仙人境";
        default: return "凡人";
    }
}

// ============================================
// 获取需求模块接口
// ============================================

static RequirementInterface* GetRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    if (!mgr)
        return nullptr;
    return mgr->GetRequirementModule();
}

// ============================================
// CultivationMgr 单例
// ============================================

class CultivationMgr
{
public:
    static CultivationMgr* instance()
    {
        static CultivationMgr inst;
        return &inst;
    }

    // 加载境界配置
    void LoadRealmConfigs()
    {
        uint32 oldMSTime = getMSTime();
        _realmConfigs.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `境界等级`, `境界名称`, `大境界`, `小境界`, `属性加成百分比`, "
            "`升级需求ID`, `是否需要渡劫`, `渡劫需求ID`, `渡劫Boss入口` "
            "FROM `_修仙境界配置` ORDER BY `境界等级`");

        if (!result)
        {
            LOG_WARN("module", "修仙系统: 未找到境界配置数据!");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            CultivationRealmConfig config;
            config.level = fields[0].Get<uint32>();
            config.name = fields[1].Get<std::string>();
            config.majorRealm = fields[2].Get<uint8>();
            config.minorRealm = fields[3].Get<uint8>();
            config.statBonus = fields[4].Get<float>();
            config.upgradeRequireId = fields[5].Get<uint32>();
            config.needTribulation = fields[6].Get<uint8>() != 0;
            config.tribRequireId = fields[7].Get<uint32>();
            config.tribBossEntry = fields[8].Get<uint32>();
            _realmConfigs[config.level] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> 修仙系统: 加载 {} 条境界配置，耗时 {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    }

    // 加载技能配置
    void LoadSkillConfigs()
    {
        uint32 oldMSTime = getMSTime();
        _skillConfigs.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `技能ID`, `技能名称`, `解锁境界等级`, `法术ID`, `技能类型`, `技能描述` "
            "FROM `_修仙技能配置` ORDER BY `解锁境界等级`");

        if (!result)
        {
            LOG_WARN("module", "修仙系统: 未找到技能配置数据!");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            CultivationSkillConfig skill;
            skill.skillId = fields[0].Get<uint32>();
            skill.name = fields[1].Get<std::string>();
            skill.unlockLevel = fields[2].Get<uint32>();
            skill.spellId = fields[3].Get<uint32>();
            skill.skillType = fields[4].Get<uint8>();
            skill.description = fields[5].Get<std::string>();
            _skillConfigs.push_back(skill);
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> 修仙系统: 加载 {} 条技能配置，耗时 {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    }

    // 加载玩家数据
    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        _playerData[guid] = PlayerCultivationData();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `修仙等级`, `渡劫冷却时间` FROM `_玩家修仙数据` WHERE `角色id` = {}", guid);

        if (!result)
            return;

        Field* fields = result->Fetch();
        _playerData[guid].level = fields[0].Get<uint32>();
        _playerData[guid].tribCooldown = fields[1].Get<uint32>();

        LOG_DEBUG("module", "修仙系统: 加载玩家 {} 数据，修仙等级: {}",
            player->GetName(), _playerData[guid].level);
    }

    // 保存玩家数据
    void SavePlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return;

        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家修仙数据` (`角色id`, `修仙等级`, `渡劫冷却时间`) VALUES ({}, {}, {})",
            guid, itr->second.level, itr->second.tribCooldown);
    }

    // 获取玩家属性加成百分比（累计）
    float GetPlayerStatBonus(uint32 guid) const
    {
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end() || itr->second.level == 0)
            return 0.0f;

        float total = 0.0f;
        for (uint32 i = 1; i <= itr->second.level; ++i)
        {
            auto cfgItr = _realmConfigs.find(i);
            if (cfgItr != _realmConfigs.end())
                total += cfgItr->second.statBonus;
        }
        return total;
    }

    // 获取玩家修仙等级
    uint32 GetPlayerLevel(uint32 guid) const
    {
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return 0;
        return itr->second.level;
    }

    // 获取境界配置
    CultivationRealmConfig const* GetRealmConfig(uint32 level) const
    {
        auto itr = _realmConfigs.find(level);
        if (itr != _realmConfigs.end())
            return &itr->second;
        return nullptr;
    }

    // 获取技能列表
    std::vector<CultivationSkillConfig> const& GetSkillConfigs() const
    {
        return _skillConfigs;
    }

    // 获取全部境界配置
    std::unordered_map<uint32, CultivationRealmConfig> const& GetAllRealmConfigs() const
    {
        return _realmConfigs;
    }

    // 获取玩家数据
    PlayerCultivationData const* GetPlayerData(uint32 guid) const
    {
        auto itr = _playerData.find(guid);
        if (itr != _playerData.end())
            return &itr->second;
        return nullptr;
    }

    // 入门修仙（从0级变1级）
    bool StartCultivation(Player* player)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
        {
            _playerData[guid] = PlayerCultivationData();
            itr = _playerData.find(guid);
        }

        if (itr->second.level > 0)
            return false;

        itr->second.level = 1;
        SavePlayerData(player);
        player->UpdateAllStats();

        return true;
    }

    // 尝试升级
    bool TryUpgrade(Player* player)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end() || itr->second.level == 0)
            return false;

        uint32 currentLevel = itr->second.level;
        if (currentLevel >= 100)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000你已达到修仙最高境界！|r");
            return false;
        }

        // 检查当前等级是否需要渡劫才能继续
        auto cfgItr = _realmConfigs.find(currentLevel);
        if (cfgItr != _realmConfigs.end() && cfgItr->second.needTribulation)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000当前境界已圆满，需要渡劫才能突破！|r");
            return false;
        }

        // 获取下一等级的升级需求
        uint32 nextLevel = currentLevel + 1;
        auto nextCfgItr = _realmConfigs.find(nextLevel);
        if (nextCfgItr == _realmConfigs.end())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000未找到下一境界的配置！|r");
            return false;
        }

        // 检查和消耗需求
        if (nextCfgItr->second.upgradeRequireId > 0)
        {
            RequirementInterface* reqModule = GetRequirementModule();
            if (!reqModule)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000需求系统未加载！|r");
                return false;
            }

            if (!reqModule->CheckRequirements(player, nextCfgItr->second.upgradeRequireId, true))
                return false;

            if (!reqModule->ConsumeRequirements(player, nextCfgItr->second.upgradeRequireId))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000消耗升级材料失败！|r");
                return false;
            }
        }

        // 升级成功
        itr->second.level = nextLevel;
        SavePlayerData(player);
        player->UpdateAllStats();
        player->UpdateAllRatings();

        // 发送通知
        float totalBonus = GetPlayerStatBonus(guid);
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[修仙系统]|r 修炼成功！当前境界: |cffffd700{}|r，全属性加成: |cff00ffff{:.1f}%|r",
            nextCfgItr->second.name, totalBonus);

        // 检查是否解锁了新技能
        LearnAvailableSkills(player);

        return true;
    }

    // 开始渡劫
    bool StartTribulation(Player* player)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end() || itr->second.level == 0)
            return false;

        uint32 currentLevel = itr->second.level;

        // 检查当前等级是否需要渡劫
        auto cfgItr = _realmConfigs.find(currentLevel);
        if (cfgItr == _realmConfigs.end() || !cfgItr->second.needTribulation)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000当前境界无需渡劫！|r");
            return false;
        }

        // 检查冷却
        uint32 now = static_cast<uint32>(time(nullptr));
        if (itr->second.tribCooldown > now)
        {
            uint32 remaining = itr->second.tribCooldown - now;
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffff0000渡劫冷却中，剩余 {} 秒|r", remaining);
            return false;
        }

        // 检查和消耗渡劫需求
        if (cfgItr->second.tribRequireId > 0)
        {
            RequirementInterface* reqModule = GetRequirementModule();
            if (!reqModule)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000需求系统未加载！|r");
                return false;
            }

            if (!reqModule->CheckRequirements(player, cfgItr->second.tribRequireId, true))
                return false;

            if (!reqModule->ConsumeRequirements(player, cfgItr->second.tribRequireId))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000消耗渡劫材料失败！|r");
                return false;
            }
        }

        // 召唤渡劫Boss
        if (cfgItr->second.tribBossEntry > 0)
        {
            float x, y, z, o;
            player->GetPosition(x, y, z, o);

            // 在玩家前方5码处召唤Boss
            x += 5.0f * cos(o);
            y += 5.0f * sin(o);

            if (Creature* boss = player->SummonCreature(cfgItr->second.tribBossEntry, x, y, z, o + M_PI,
                TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300000)) // 5分钟后消失
            {
                boss->SetInCombatWith(player);
                boss->AddThreat(player, 1000.0f);
                _tribBosses[boss->GetGUID().GetCounter()] = guid;

                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffff8800[修仙系统]|r 天劫降临！击败劫兽方可突破！");
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffff0000召唤渡劫Boss失败！|r");
                return false;
            }
        }
        else
        {
            // 没有配置Boss，直接突破
            return CompleteTribulation(player);
        }

        // 设置冷却
        uint32 cooldown = sConfigMgr->GetOption("Cultivation.TribulationCooldown", 600u);
        itr->second.tribCooldown = now + cooldown;
        SavePlayerData(player);

        return true;
    }

    // 渡劫成功（击杀Boss后调用）
    bool CompleteTribulation(Player* player)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return false;

        uint32 currentLevel = itr->second.level;
        uint32 nextLevel = currentLevel + 1;

        if (nextLevel > 100)
            return false;

        auto nextCfgItr = _realmConfigs.find(nextLevel);
        if (nextCfgItr == _realmConfigs.end())
            return false;

        // 提升等级
        itr->second.level = nextLevel;
        SavePlayerData(player);
        player->UpdateAllStats();
        player->UpdateAllRatings();

        float totalBonus = GetPlayerStatBonus(guid);
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[修仙系统]|r 渡劫成功！突破至: |cffffd700{}|r，全属性加成: |cff00ffff{:.1f}%|r",
            nextCfgItr->second.name, totalBonus);

        // 学习新技能
        LearnAvailableSkills(player);

        return true;
    }

    // 渡劫Boss击杀判定
    void OnCreatureKilled(Player* player, Creature* creature)
    {
        if (!player || !creature)
            return;

        uint32 bossGuid = creature->GetGUID().GetCounter();
        auto itr = _tribBosses.find(bossGuid);
        if (itr == _tribBosses.end())
            return;

        uint32 playerGuid = itr->second;
        _tribBosses.erase(itr);

        if (player->GetGUID().GetCounter() != playerGuid)
            return;

        CompleteTribulation(player);
    }

    // 渡劫失败判定
    void OnPlayerDeath(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();

        // 检查是否在渡劫中（通过Boss映射反查）
        for (auto itr = _tribBosses.begin(); itr != _tribBosses.end(); ++itr)
        {
            if (itr->second == guid)
            {
                _tribBosses.erase(itr);
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffff0000[修仙系统]|r 渡劫失败！修为未损，待冷却后可再次挑战。");
                break;
            }
        }
    }

    // 学习已解锁的仙术
    void LearnAvailableSkills(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 playerLevel = GetPlayerLevel(guid);

        for (auto const& skill : _skillConfigs)
        {
            if (skill.spellId > 0 && playerLevel >= skill.unlockLevel)
            {
                if (!player->HasSpell(skill.spellId))
                {
                    player->learnSpell(skill.spellId);
                    ChatHandler(player->GetSession()).PSendSysMessage(
                        "|cff00ff00[修仙系统]|r 领悟仙术: |cffffd700{}|r - {}",
                        skill.name, skill.description);
                }
            }
        }
    }

    // GM设置玩家等级
    bool SetPlayerLevel(Player* player, uint32 level)
    {
        if (!player || level > 100)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
        {
            _playerData[guid] = PlayerCultivationData();
            itr = _playerData.find(guid);
        }

        itr->second.level = level;
        SavePlayerData(player);
        player->UpdateAllStats();
        player->UpdateAllRatings();

        if (level > 0)
            LearnAvailableSkills(player);

        return true;
    }

    // 玩家登出
    void OnPlayerLogout(uint32 guid)
    {
        _playerData.erase(guid);
    }

    // 删除玩家数据
    void DeletePlayerData(uint32 guid)
    {
        _playerData.erase(guid);
        CharacterDatabase.Execute("DELETE FROM `_玩家修仙数据` WHERE `角色id` = {}", guid);
    }

private:
    CultivationMgr() = default;
    ~CultivationMgr() = default;

    std::unordered_map<uint32, CultivationRealmConfig> _realmConfigs;
    std::vector<CultivationSkillConfig> _skillConfigs;
    std::unordered_map<uint32, PlayerCultivationData> _playerData;
    std::unordered_map<uint32, uint32> _tribBosses; // bossGuid → playerGuid
};

#define sCultivationMgr CultivationMgr::instance()

// ============================================
// Addon消息发送函数
// ============================================

void SendCultivationPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= CULTIVATION_MAX_ADDON_PAYLOAD)
    {
        std::string fullMessage = std::string(CULTIVATION_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
        return;
    }

    // 分块发送
    size_t totalChunks = (payload.length() + CULTIVATION_MAX_ADDON_PAYLOAD - 1) / CULTIVATION_MAX_ADDON_PAYLOAD;
    for (size_t i = 0; i < totalChunks; ++i)
    {
        size_t start = i * CULTIVATION_MAX_ADDON_PAYLOAD;
        size_t len = std::min(CULTIVATION_MAX_ADDON_PAYLOAD, payload.length() - start);
        std::string chunk = payload.substr(start, len);

        std::ostringstream chunkMessage;
        chunkMessage << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << chunk;

        std::string fullMessage = std::string(CULTIVATION_ADDON_PREFIX) + '\t' + chunkMessage.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendCultivationOpenUI(Player* player)
{
    SendCultivationPayload(player, "CS_OPEN");
}

void SendCultivationRealmList(Player* player)
{
    if (!player)
        return;

    auto const& configs = sCultivationMgr->GetAllRealmConfigs();

    // 按等级排序
    std::vector<CultivationRealmConfig const*> sorted;
    sorted.reserve(configs.size());
    for (auto const& pair : configs)
        sorted.push_back(&pair.second);

    std::sort(sorted.begin(), sorted.end(), [](CultivationRealmConfig const* a, CultivationRealmConfig const* b)
    {
        return a->level < b->level;
    });

    std::ostringstream payload;
    payload << "CS_REALMS:";

    bool first = true;
    for (auto const* cfg : sorted)
    {
        if (!first)
            payload << '~';
        first = false;

        payload << cfg->level << '^'
                << cfg->name << '^'
                << static_cast<uint32>(cfg->majorRealm) << '^'
                << static_cast<uint32>(cfg->minorRealm) << '^'
                << cfg->statBonus << '^'
                << cfg->upgradeRequireId << '^'
                << (cfg->needTribulation ? 1 : 0) << '^'
                << cfg->tribRequireId << '^'
                << cfg->tribBossEntry;
    }

    SendCultivationPayload(player, payload.str());
}

void SendCultivationSkillList(Player* player)
{
    if (!player)
        return;

    auto const& skills = sCultivationMgr->GetSkillConfigs();

    std::ostringstream payload;
    payload << "CS_SKILLS:";

    bool first = true;
    for (auto const& skill : skills)
    {
        if (!first)
            payload << '~';
        first = false;

        payload << skill.skillId << '^'
                << skill.name << '^'
                << skill.unlockLevel << '^'
                << skill.spellId << '^'
                << static_cast<uint32>(skill.skillType) << '^'
                << skill.description;
    }

    SendCultivationPayload(player, payload.str());
}

void SendCultivationState(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    uint32 pLevel = sCultivationMgr->GetPlayerLevel(guid);
    float statBonus = sCultivationMgr->GetPlayerStatBonus(guid);

    CultivationRealmConfig const* curConfig = sCultivationMgr->GetRealmConfig(pLevel);
    std::string realmName = curConfig ? curConfig->name : "凡人";
    int32 majorRealm = curConfig ? curConfig->majorRealm : 0;

    PlayerCultivationData const* pData = sCultivationMgr->GetPlayerData(guid);
    uint32 tribCooldown = pData ? pData->tribCooldown : 0;

    // 已解锁技能列表
    std::ostringstream skillIds;
    bool firstSkill = true;
    for (auto const& skill : sCultivationMgr->GetSkillConfigs())
    {
        if (pLevel >= skill.unlockLevel)
        {
            if (!firstSkill)
                skillIds << ',';
            firstSkill = false;
            skillIds << skill.spellId;
        }
    }

    std::ostringstream payload;
    payload << "CS_STATE:" << pLevel << '|'
            << realmName << '|'
            << majorRealm << '|'
            << statBonus << '|'
            << tribCooldown << '|'
            << skillIds.str();

    SendCultivationPayload(player, payload.str());
}

void SendCultivationResult(Player* player, std::string const& action, bool success, std::string const& message)
{
    std::ostringstream payload;
    payload << "CS_RESULT:" << action << '^' << (success ? 1 : 0) << '^' << message;
    SendCultivationPayload(player, payload.str());
}

void SendCultivationAllData(Player* player)
{
    SendCultivationRealmList(player);
    SendCultivationSkillList(player);
    SendCultivationState(player);
}

// 技能ID宏定义
#define CULT_SPELL_YUFENG       371001
#define CULT_SPELL_LINGREN      371002
#define CULT_SPELL_JINDAN       371003
#define CULT_SPELL_YUANYING     371004
#define CULT_SPELL_TIANLEI      371005
#define CULT_SPELL_SHUNYING     371006
#define CULT_SPELL_WANJIAN      371007
#define CULT_SPELL_TUNTIAN      371008
#define CULT_SPELL_JIUTIAN      371009
#define CULT_SPELL_XIANSHEN     371010

// 元婴出窍 - 分身GUID映射（前向声明，供PlayerScript使用）
#define CULTIVATION_MIRROR_IMAGE_ENTRY 31216
static std::unordered_map<uint32, ObjectGuid> _mirrorImageGuids;

// ============================================
// WorldScript - 延迟加载
// ============================================

class CultivationWorldScript : public WorldScript
{
public:
    CultivationWorldScript() : WorldScript("CultivationWorldScript"), _loaded(false), _updateTimer(0) { }

    void OnUpdate(uint32 diff) override
    {
        if (_loaded)
            return;

        _updateTimer += diff;
        if (_updateTimer >= 1000)
        {
            bool enabled = sConfigMgr->GetOption("Cultivation.Enable", true);
            if (!enabled)
            {
                LOG_INFO("server.loading", ">> 修仙系统模块已禁用");
                _loaded = true;
                return;
            }

            sCultivationMgr->LoadRealmConfigs();
            sCultivationMgr->LoadSkillConfigs();

            LOG_INFO("server.loading", "→修仙系统加载成功√");
            _loaded = true;
        }
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload && _loaded)
        {
            bool enabled = sConfigMgr->GetOption("Cultivation.Enable", true);
            if (!enabled)
            {
                LOG_INFO("module", "修仙系统模块已禁用");
                return;
            }

            sCultivationMgr->LoadRealmConfigs();
            sCultivationMgr->LoadSkillConfigs();

            LOG_INFO("module", "┌───────────────────────────────────────┐");
            LOG_INFO("module", "│          修仙系统配置已重载           │");
            LOG_INFO("module", "└───────────────────────────────────────┘");
        }
    }

private:
    bool _loaded;
    uint32 _updateTimer;
};

// ============================================
// PlayerScript - 属性钩子 + 事件
// ============================================

class CultivationPlayerScript : public PlayerScript
{
public:
    CultivationPlayerScript() : PlayerScript("CultivationPlayerScript") { }

    // 加载玩家数据
    void OnPlayerLoadFromDB(Player* player) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;
        sCultivationMgr->LoadPlayerData(player);
    }

    // 登录时刷新属性 + 学习技能
    void OnPlayerLogin(Player* player) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 level = sCultivationMgr->GetPlayerLevel(guid);

        player->UpdateAllStats();
        player->UpdateAllRatings();

        if (level > 0)
        {
            sCultivationMgr->LearnAvailableSkills(player);

            auto const* config = sCultivationMgr->GetRealmConfig(level);
            float bonus = sCultivationMgr->GetPlayerStatBonus(guid);
            std::string realmName = config ? config->name : "未知";

            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[修仙系统]|r 当前境界: |cffffd700{}|r，全属性加成: |cff00ffff{:.1f}%|r",
                realmName, bonus);
        }
    }

    // 登出时保存 + 清理分身
    void OnPlayerLogout(Player* player) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        // 清理元婴分身
        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _mirrorImageGuids.find(guid);
        if (itr != _mirrorImageGuids.end())
        {
            Creature* mirror = ObjectAccessor::GetCreature(*player, itr->second);
            if (mirror && mirror->IsAlive())
                mirror->DespawnOrUnsummon();
            _mirrorImageGuids.erase(itr);
        }

        sCultivationMgr->SavePlayerData(player);
        sCultivationMgr->OnPlayerLogout(guid);
    }

    // 删除角色时清理数据
    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        sCultivationMgr->DeletePlayerData(guid.GetCounter());
    }

    // 击杀生物（渡劫Boss判定）
    void OnPlayerCreatureKill(Player* player, Creature* creature) override
    {
        if (!player || !creature || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;
        sCultivationMgr->OnCreatureKilled(player, creature);
    }

    // 被生物击杀（渡劫失败判定）
    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* player) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;
        sCultivationMgr->OnPlayerDeath(player);
    }

    // Addon消息处理（客户端UI通信）
    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true) || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != CULTIVATION_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);

        if (command == "REQ_ALL")
        {
            SendCultivationAllData(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            sCultivationMgr->LoadPlayerData(player);
            SendCultivationState(player);
            return;
        }

        if (command == "OPEN")
        {
            SendCultivationOpenUI(player);
            return;
        }

        if (command == "UPG")
        {
            uint32 guid = player->GetGUID().GetCounter();
            uint32 oldLevel = sCultivationMgr->GetPlayerLevel(guid);

            bool success = sCultivationMgr->TryUpgrade(player);

            if (success)
            {
                uint32 newLevel = sCultivationMgr->GetPlayerLevel(guid);
                auto const* newConfig = sCultivationMgr->GetRealmConfig(newLevel);
                std::string newName = newConfig ? newConfig->name : "未知";
                float bonus = sCultivationMgr->GetPlayerStatBonus(guid);

                std::ostringstream resultMsg;
                resultMsg << "修炼成功！突破至【" << newName << "】，全属性加成 +" << bonus << "%";
                SendCultivationResult(player, "UPG", true, resultMsg.str());
            }
            else
            {
                SendCultivationResult(player, "UPG", false, "修炼失败，请检查材料或条件");
            }

            SendCultivationState(player);
            return;
        }

        if (command == "TRIB")
        {
            bool success = sCultivationMgr->StartTribulation(player);
            if (success)
            {
                SendCultivationResult(player, "TRIB", true, "天劫降临！击败劫兽方可突破！");
            }
            else
            {
                SendCultivationResult(player, "TRIB", false, "渡劫失败，请检查条件");
            }
            SendCultivationState(player);
            return;
        }

        if (command == "ENTER")
        {
            bool success = sCultivationMgr->StartCultivation(player);
            if (success)
            {
                SendCultivationResult(player, "ENTER", true, "恭喜踏入修仙之路！你已成为炼气一层修士");
                SendCultivationAllData(player);
            }
            else
            {
                uint32 pLevel = sCultivationMgr->GetPlayerLevel(player->GetGUID().GetCounter());
                if (pLevel > 0)
                    SendCultivationResult(player, "ENTER", false, "你已经踏入修仙之路");
                else
                    SendCultivationResult(player, "ENTER", false, "入门失败");
            }
            return;
        }
    }

    // ============================================
    // 属性加成钩子
    // ============================================

    // 五维属性（力量、敏捷、耐力、智力、精神）
    void OnPlayerAfterUpdateStat(Player* player, Stats stat, float& value) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            value *= (1.0f + bonus / 100.0f);

        // 仙神降世额外+100%
        if (player->HasAura(371010))
            value *= 2.0f;
    }

    // 生命上限
    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            value *= (1.0f + bonus / 100.0f);

        if (player->HasAura(371010))
            value *= 2.0f;
    }

    // 法力上限
    void OnPlayerAfterUpdateMaxPower(Player* player, Powers& power, float& value) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            value *= (1.0f + bonus / 100.0f);

        if (player->HasAura(371010))
            value *= 2.0f;
    }

    // 攻击强度
    void OnPlayerAfterUpdateAttackPowerAndDamage(Player* player, float& level, float& base_attPower, float& attPowerMod, float& attPowerMultiplier, bool ranged) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            attPowerMultiplier += bonus / 100.0f;

        if (player->HasAura(371010))
            attPowerMultiplier += 1.0f;
    }

    // 护甲
    void OnPlayerAfterUpdateArmor(Player* player, float& value) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            value *= (1.0f + bonus / 100.0f);

        if (player->HasAura(371010))
            value *= 2.0f;
    }

    // 法术强度和治疗强度
    void OnPlayerAfterUpdateSpellDamageAndHealing(Player* player, int32& healingBonus, int32 spellDamage[7]) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        bool hasXianshen = player->HasAura(371010);

        if (bonus > 0 || hasXianshen)
        {
            float multiplier = 1.0f;
            if (bonus > 0) multiplier += bonus / 100.0f;
            if (hasXianshen) multiplier *= 2.0f;

            if (healingBonus > 0)
                healingBonus = int32(healingBonus * multiplier);

            for (int i = 1; i < 7; ++i)
            {
                if (spellDamage[i] > 0)
                    spellDamage[i] = int32(spellDamage[i] * multiplier);
            }
        }
    }

    // 物理暴击
    void OnPlayerAfterUpdateCritPercentage(Player* player, WeaponAttackType attType, float& value) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            value += bonus * 0.1f;
    }

    // 法术暴击
    void OnPlayerAfterUpdateSpellCritChance(Player* player, uint32 school, float& value) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0)
            value += bonus * 0.1f;
    }

    // 评级属性（命中、急速、精准等）
    void OnPlayerAfterUpdateRating(Player* player, CombatRating cr, int32& amount) override
    {
        if (!player || !sConfigMgr->GetOption("Cultivation.Enable", true))
            return;

        float bonus = sCultivationMgr->GetPlayerStatBonus(player->GetGUID().GetCounter());
        if (bonus > 0 && amount > 0)
            amount = int32(amount * (1.0f + bonus / 100.0f));
    }
};

// ============================================
// CreatureScript - NPC 修仙导师
// ============================================

enum CultivationGossipActions
{
    CULT_ACTION_INFO         = 1000,
    CULT_ACTION_UPGRADE      = 1001,
    CULT_ACTION_TRIBULATION  = 1002,
    CULT_ACTION_SKILLS       = 1003,
    CULT_ACTION_START        = 1004,
    CULT_ACTION_BACK         = 1005,
};

class CultivationNPCScript : public CreatureScript
{
public:
    CultivationNPCScript() : CreatureScript("npc_cultivation_master") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
            return false;

        ShowMainMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
            return false;

        player->PlayerTalkClass->ClearMenus();

        switch (action)
        {
            case CULT_ACTION_INFO:
                ShowInfo(player, creature);
                break;
            case CULT_ACTION_UPGRADE:
                sCultivationMgr->TryUpgrade(player);
                ShowMainMenu(player, creature);
                break;
            case CULT_ACTION_TRIBULATION:
                sCultivationMgr->StartTribulation(player);
                CloseGossipMenuFor(player);
                break;
            case CULT_ACTION_SKILLS:
                ShowSkills(player, creature);
                break;
            case CULT_ACTION_START:
                if (sCultivationMgr->StartCultivation(player))
                {
                    ChatHandler(player->GetSession()).PSendSysMessage(
                        "|cff00ff00[修仙系统]|r 恭喜踏入修仙之路！当前境界: |cffffd700炼气一层|r");
                }
                ShowMainMenu(player, creature);
                break;
            case CULT_ACTION_BACK:
                ShowMainMenu(player, creature);
                break;
            default:
                ShowMainMenu(player, creature);
                break;
        }
        return true;
    }

private:
    void ShowMainMenu(Player* player, Creature* creature)
    {
        player->PlayerTalkClass->ClearMenus();

        uint32 guid = player->GetGUID().GetCounter();
        uint32 level = sCultivationMgr->GetPlayerLevel(guid);

        // 查看境界信息
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "|TInterface\\Icons\\INV_Misc_Book_09:30:30|t 查看修仙境界",
            GOSSIP_SENDER_MAIN, CULT_ACTION_INFO);

        if (level == 0)
        {
            // 未入门，显示入门选项
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "|TInterface\\Icons\\Spell_Holy_SurgeOfLight:30:30|t 踏入修仙之路",
                GOSSIP_SENDER_MAIN, CULT_ACTION_START);
        }
        else
        {
            // 已入门
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                "|TInterface\\Icons\\Spell_Holy_MagicalSentry:30:30|t 修炼升级",
                GOSSIP_SENDER_MAIN, CULT_ACTION_UPGRADE);

            // 检查是否需要渡劫
            auto const* config = sCultivationMgr->GetRealmConfig(level);
            if (config && config->needTribulation)
            {
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                    "|TInterface\\Icons\\Spell_Shadow_SummonInfernal:30:30|t 突破渡劫",
                    GOSSIP_SENDER_MAIN, CULT_ACTION_TRIBULATION);
            }

            // 仙术一览
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_2,
                "|TInterface\\Icons\\Spell_Arcane_TeleportStormwind:30:30|t 仙术一览",
                GOSSIP_SENDER_MAIN, CULT_ACTION_SKILLS);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ShowInfo(Player* player, Creature* creature)
    {
        player->PlayerTalkClass->ClearMenus();

        uint32 guid = player->GetGUID().GetCounter();
        uint32 level = sCultivationMgr->GetPlayerLevel(guid);

        if (level == 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[修仙系统]|r 你尚未踏入修仙之路，当前为 |cff888888凡人|r");
        }
        else
        {
            auto const* config = sCultivationMgr->GetRealmConfig(level);
            float bonus = sCultivationMgr->GetPlayerStatBonus(guid);
            std::string realmName = config ? config->name : "未知";
            std::string majorName = config ? GetMajorRealmName(config->majorRealm) : "未知";

            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[修仙系统]|r 修仙信息:");
            ChatHandler(player->GetSession()).PSendSysMessage(
                "  境界: |cffffd700{}|r ({})", realmName, majorName);
            ChatHandler(player->GetSession()).PSendSysMessage(
                "  等级: |cffffd700{}/100|r", level);
            ChatHandler(player->GetSession()).PSendSysMessage(
                "  属性加成: |cff00ffff{:.1f}%|r", bonus);

            if (config && config->needTribulation)
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "  状态: |cffff8800境界圆满，需渡劫突破|r");
            }
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "|TInterface\\Icons\\Misc_ArrowLeft:30:30|t 返回",
            GOSSIP_SENDER_MAIN, CULT_ACTION_BACK);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ShowSkills(Player* player, Creature* creature)
    {
        player->PlayerTalkClass->ClearMenus();

        uint32 guid = player->GetGUID().GetCounter();
        uint32 playerLevel = sCultivationMgr->GetPlayerLevel(guid);

        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[修仙系统]|r 仙术一览:");

        auto const& skills = sCultivationMgr->GetSkillConfigs();
        for (auto const& skill : skills)
        {
            const char* typeStr = "";
            switch (skill.skillType)
            {
                case 1: typeStr = "主动"; break;
                case 2: typeStr = "被动"; break;
                case 3: typeStr = "辅助"; break;
            }

            std::string statusStr;
            if (playerLevel >= skill.unlockLevel)
                statusStr = "|cff00ff00已领悟|r";
            else
                statusStr = Acore::StringFormat("|cff888888{}级解锁|r", skill.unlockLevel);

            ChatHandler(player->GetSession()).PSendSysMessage(
                "  |cffffd700{}|r [{}] - {} ({})",
                skill.name, typeStr, skill.description, statusStr);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "|TInterface\\Icons\\Misc_ArrowLeft:30:30|t 返回",
            GOSSIP_SENDER_MAIN, CULT_ACTION_BACK);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
};

// ============================================
// CommandScript - .修仙 命令
// ============================================

class CultivationCommandScript : public CommandScript
{
public:
    CultivationCommandScript() : CommandScript("CultivationCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subTable =
        {
            { "信息", HandleInfoCommand,        SEC_PLAYER,     Console::No },
            { "升级", HandleUpgradeCommand,     SEC_PLAYER,     Console::No },
            { "渡劫", HandleTribulationCommand, SEC_PLAYER,     Console::No },
            { "技能", HandleSkillsCommand,      SEC_PLAYER,     Console::No },
            { "入门", HandleStartCommand,       SEC_PLAYER,     Console::No },
            { "界面", HandleOpenUICommand,      SEC_PLAYER,     Console::No },
            { "重载", HandleReloadCommand,      SEC_GAMEMASTER, Console::Yes },
            { "设置", HandleSetCommand,         SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable rootTable =
        {
            { "修仙", subTable },
            { "修仙系统", subTable },
        };

        return rootTable;
    }

    // .修仙 信息
    static bool HandleInfoCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000修仙系统已禁用|r");
            return true;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 level = sCultivationMgr->GetPlayerLevel(guid);

        if (level == 0)
        {
            handler->PSendSysMessage("|cff00ff00[修仙系统]|r 你尚未踏入修仙之路，使用 .修仙 入门 开始修炼");
            return true;
        }

        auto const* config = sCultivationMgr->GetRealmConfig(level);
        float bonus = sCultivationMgr->GetPlayerStatBonus(guid);
        std::string realmName = config ? config->name : "未知";
        std::string majorName = config ? GetMajorRealmName(config->majorRealm) : "未知";

        handler->PSendSysMessage("|cff00ff00[修仙系统]|r 修仙信息:");
        handler->PSendSysMessage("  境界: |cffffd700{}|r ({})", realmName, majorName);
        handler->PSendSysMessage("  等级: |cffffd700{}/100|r", level);
        handler->PSendSysMessage("  属性加成: |cff00ffff{:.1f}%|r", bonus);

        if (config && config->needTribulation)
            handler->PSendSysMessage("  状态: |cffff8800境界圆满，需渡劫突破|r");

        return true;
    }

    // .修仙 升级
    static bool HandleUpgradeCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000修仙系统已禁用|r");
            return true;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        uint32 level = sCultivationMgr->GetPlayerLevel(player->GetGUID().GetCounter());
        if (level == 0)
        {
            handler->PSendSysMessage("|cffff0000请先使用 .修仙 入门 踏入修仙之路|r");
            return true;
        }

        sCultivationMgr->TryUpgrade(player);
        return true;
    }

    // .修仙 渡劫
    static bool HandleTribulationCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000修仙系统已禁用|r");
            return true;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        uint32 level = sCultivationMgr->GetPlayerLevel(player->GetGUID().GetCounter());
        if (level == 0)
        {
            handler->PSendSysMessage("|cffff0000请先使用 .修仙 入门 踏入修仙之路|r");
            return true;
        }

        sCultivationMgr->StartTribulation(player);
        return true;
    }

    // .修仙 技能
    static bool HandleSkillsCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000修仙系统已禁用|r");
            return true;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 playerLevel = sCultivationMgr->GetPlayerLevel(guid);

        handler->PSendSysMessage("|cff00ff00[修仙系统]|r 仙术一览:");

        auto const& skills = sCultivationMgr->GetSkillConfigs();
        for (auto const& skill : skills)
        {
            const char* typeStr = "";
            switch (skill.skillType)
            {
                case 1: typeStr = "主动"; break;
                case 2: typeStr = "被动"; break;
                case 3: typeStr = "辅助"; break;
            }

            std::string statusStr;
            if (playerLevel >= skill.unlockLevel)
                statusStr = "|cff00ff00已领悟|r";
            else
                statusStr = Acore::StringFormat("|cff888888{}级解锁|r", skill.unlockLevel);

            handler->PSendSysMessage("  |cffffd700{}|r [{}] - {} ({})",
                skill.name, typeStr, skill.description, statusStr);
        }

        return true;
    }

    // .修仙 入门
    static bool HandleStartCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!sConfigMgr->GetOption("Cultivation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000修仙系统已禁用|r");
            return true;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        uint32 level = sCultivationMgr->GetPlayerLevel(player->GetGUID().GetCounter());
        if (level > 0)
        {
            handler->PSendSysMessage("|cffff0000你已踏入修仙之路，无需重复入门|r");
            return true;
        }

        if (sCultivationMgr->StartCultivation(player))
        {
            handler->PSendSysMessage("|cff00ff00[修仙系统]|r 恭喜踏入修仙之路！当前境界: |cffffd700炼气一层|r");
            handler->PSendSysMessage("  使用 |cffffd700.修仙 升级|r 进行修炼");
            handler->PSendSysMessage("  使用 |cffffd700.修仙 信息|r 查看境界信息");
        }
        return true;
    }

    // .修仙 重载 (GM)
    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sCultivationMgr->LoadRealmConfigs();
        sCultivationMgr->LoadSkillConfigs();
        handler->PSendSysMessage("|cff00ff00[修仙系统]|r 配置已重载");
        return true;
    }

    // .修仙 设置 <等级> (GM)
    static bool HandleSetCommand(ChatHandler* handler, Optional<uint32> levelOpt)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!levelOpt)
        {
            handler->PSendSysMessage("用法: .修仙 设置 <等级(0-100)>");
            return true;
        }

        uint32 level = *levelOpt;
        if (level > 100)
        {
            handler->PSendSysMessage("|cffff0000等级不能超过100|r");
            return true;
        }

        // 获取目标玩家（如果选中了目标）
        Player* target = player->GetSelectedPlayer();
        if (!target)
            target = player;

        if (sCultivationMgr->SetPlayerLevel(target, level))
        {
            auto const* config = sCultivationMgr->GetRealmConfig(level);
            float bonus = sCultivationMgr->GetPlayerStatBonus(target->GetGUID().GetCounter());
            std::string realmName = (level == 0) ? "凡人" : (config ? config->name : "未知");

            handler->PSendSysMessage("|cff00ff00[修仙系统]|r 已将 {} 的修仙等级设置为 {} ({}，加成: {:.1f}%)",
                target->GetName(), level, realmName, bonus);

            if (target != player)
            {
                ChatHandler(target->GetSession()).PSendSysMessage(
                    "|cff00ff00[修仙系统]|r GM已将你的修仙等级设置为 {} ({})", level, realmName);
            }
        }
        return true;
    }

    // .修仙 界面
    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        SendCultivationOpenUI(player);
        SendCultivationAllData(player);
        return true;
    }
};

// ============================================
// 辅助函数：搜索范围内可攻击敌人
// ============================================

static void GetHostileUnitsInRange(Unit* caster, std::list<Unit*>& targets, float range)
{
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, targets, check);
    Cell::VisitAllObjects(caster, searcher, range);
}

// 工具函数：获取玩家攻击力或法强中较高者
// ============================================

static double GetPlayerHighestDamageValue(Unit* caster)
{
    if (!caster)
        return 0.0f;

    double ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
    double sp = 0.0f;

    if (Player* player = caster->ToPlayer())
    {
        ap = player->GetExtendedTotalAttackPowerValue(BASE_ATTACK);
        double rangedAP = player->GetExtendedTotalAttackPowerValue(RANGED_ATTACK);
        if (rangedAP > ap) ap = rangedAP;

        sp = player->GetExtendedSpellDamageBonus();
    }

    return std::max(ap, sp);
}

static double GetCultivationAttackPower(Unit* caster)
{
    if (!caster)
        return 0.0;

    if (Player* player = caster->ToPlayer())
        return player->GetExtendedTotalAttackPowerValue(BASE_ATTACK);

    return caster->GetTotalAttackPowerValue(BASE_ATTACK);
}

static void SetShunyingControlImmunity(Unit* unit, bool apply)
{
    if (!unit)
        return;

    static uint32 const mechanics[] =
    {
        MECHANIC_CHARM,
        MECHANIC_DISORIENTED,
        MECHANIC_FEAR,
        MECHANIC_ROOT,
        MECHANIC_SLEEP,
        MECHANIC_SNARE,
        MECHANIC_FREEZE,
        MECHANIC_KNOCKOUT,
        MECHANIC_POLYMORPH,
        MECHANIC_BANISH,
        MECHANIC_SHACKLE,
        MECHANIC_TURN,
        MECHANIC_HORROR,
        MECHANIC_INTERRUPT,
        MECHANIC_DAZE,
        MECHANIC_STUN
    };

    for (uint32 mechanic : mechanics)
        unit->ApplySpellImmune(0, IMMUNITY_MECHANIC, mechanic, apply);

    unit->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, apply);
    unit->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST, apply);
}

// ============================================
// 371002 灵刃术 - 穿透直线伤害
// 攻击力/法强(取高)×150%，穿透直线所有敌人
// ============================================

class spell_cultivation_lingren : public SpellScript
{
    PrepareSpellScript(spell_cultivation_lingren);

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        double highValue = GetPlayerHighestDamageValue(caster);
        int64 damage = static_cast<int64>(highValue * 1.5);
        if (damage < 1) damage = 1;
        SetHitDamage(damage);
    }

    void HandleAfterHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float angle = caster->GetAngle(target);
        double highValue = GetPlayerHighestDamageValue(caster);
        int64 damage = static_cast<int64>(highValue * 1.5);
        if (damage < 1) damage = 1;

        std::list<Unit*> targets;
        GetHostileUnitsInRange(caster, targets, 30.0f);

        for (Unit* u : targets)
        {
            if (u == target || u == caster)
                continue;

            float unitAngle = caster->GetAngle(u);
            float angleDiff = std::abs(unitAngle - angle);
            if (angleDiff > M_PI) angleDiff = 2.0f * M_PI - angleDiff;

            // 约±8度的角度容差，模拟直线穿透
            if (angleDiff < 0.15f)
            {
                caster->DealDamage(caster, u, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_ARCANE);
            }
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_cultivation_lingren::HandleOnHit);
        AfterHit += SpellHitFn(spell_cultivation_lingren::HandleAfterHit);
    }
};

// ============================================
// 371003 金丹爆 - 12码AOE + 击退
// 攻击力×200%
// ============================================

class spell_cultivation_jindanbao : public SpellScript
{
    PrepareSpellScript(spell_cultivation_jindanbao);

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        double ap = GetCultivationAttackPower(caster);
        int64 damage = static_cast<int64>(ap * 2.0);
        if (damage < 1) damage = 1;
        SetHitDamage(damage);

        // 击退
        float x = caster->GetPositionX();
        float y = caster->GetPositionY();
        target->KnockbackFrom(x, y, 10.0f, 5.0f);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_cultivation_jindanbao::HandleOnHit);
    }
};

// ============================================
// 371004 元婴出窍 - 召唤分身
// 60%属性的永久分身，同时只能1个
// ============================================

class spell_cultivation_yuanying : public SpellScript
{
    PrepareSpellScript(spell_cultivation_yuanying);

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->ToPlayer())
            return SPELL_FAILED_BAD_TARGETS;

        uint32 guid = caster->GetGUID().GetCounter();
        auto itr = _mirrorImageGuids.find(guid);
        if (itr != _mirrorImageGuids.end())
        {
            Creature* oldMirror = ObjectAccessor::GetCreature(*caster, itr->second);
            if (oldMirror && oldMirror->IsAlive())
                oldMirror->DespawnOrUnsummon();
            _mirrorImageGuids.erase(itr);
        }

        return SPELL_CAST_OK;
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        float x, y, z, o;
        player->GetPosition(x, y, z, o);
        x += 3.0f * cos(o);
        y += 3.0f * sin(o);

        if (Creature* mirror = player->SummonCreature(CULTIVATION_MIRROR_IMAGE_ENTRY, x, y, z, o,
            TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            mirror->SetOwnerGUID(player->GetGUID());
            mirror->SetCreatorGUID(player->GetGUID());
            mirror->SetFaction(player->GetFaction());
            mirror->SetLevel(player->GetLevel());
            mirror->SetDisplayId(player->GetDisplayId());
            mirror->SetNativeDisplayId(player->GetDisplayId());
            mirror->SetObjectScale(player->GetObjectScale());
            mirror->SetReactState(REACT_AGGRESSIVE);

            float healthPct = 0.6f;
            uint32 playerHP = player->GetMaxHealth();
            mirror->SetMaxHealth(static_cast<uint32>(playerHP * healthPct));
            mirror->SetHealth(static_cast<uint32>(playerHP * healthPct));

            float ap = player->GetTotalAttackPowerValue(BASE_ATTACK) * healthPct;
            float minDmg = ap / 14.0f * 2.0f;
            float maxDmg = minDmg * 1.2f;
            mirror->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, minDmg);
            mirror->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, maxDmg);
            mirror->UpdateDamagePhysical(BASE_ATTACK);

            if (Unit* playerTarget = player->GetSelectedUnit())
            {
                if (playerTarget->IsHostileTo(player))
                {
                    mirror->AI()->AttackStart(playerTarget);
                    mirror->Attack(playerTarget, true);
                    mirror->GetMotionMaster()->MoveChase(playerTarget);
                }
            }

            _mirrorImageGuids[player->GetGUID().GetCounter()] = mirror->GetGUID();

            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[修仙系统]|r 元婴出窍！分身已召唤。");
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_cultivation_yuanying::CheckCast);
        AfterCast += SpellCastFn(spell_cultivation_yuanying::HandleAfterCast);
    }
};

// ============================================
// 371005 天雷诀 - 15码AOE + 眩晕1.5秒
// 法强×250%
// ============================================

class spell_cultivation_tianlei : public SpellScript
{
    PrepareSpellScript(spell_cultivation_tianlei);

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        double highValue = GetPlayerHighestDamageValue(caster);
        int64 damage = static_cast<int64>(highValue * 2.5);
        if (damage < 1) damage = 1;
        SetHitDamage(damage);

    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_cultivation_tianlei::HandleOnHit);
    }
};

// ============================================
// 371006 瞬影步 - 瞬移+落点AOE+减速+免控
// ============================================

class spell_cultivation_shunying : public SpellScript
{
    PrepareSpellScript(spell_cultivation_shunying);

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target)
            return;

        float x, y, z;
        target->GetContactPoint(caster, x, y, z);
        caster->NearTeleportTo(x, y, z, caster->GetAngle(target));
        SetShunyingControlImmunity(caster, true);

        if (Player* player = caster->ToPlayer())
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid]()
            {
                if (Player* owner = ObjectAccessor::FindPlayer(playerGuid))
                    SetShunyingControlImmunity(owner, false);
            }, 3s);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_cultivation_shunying::HandleAfterCast);
    }
};

// ============================================
// 371007 万剑归宗 - 15码环绕每秒伤害持续10秒
// 每秒攻击力×120%
// ============================================

class spell_cultivation_wanjian_aura : public AuraScript
{
    PrepareAuraScript(spell_cultivation_wanjian_aura);

    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        double ap = GetCultivationAttackPower(caster);
        int64 damage = static_cast<int64>(ap * 1.2);
        if (damage < 1) damage = 1;

        std::list<Unit*> targets;
        GetHostileUnitsInRange(caster, targets, 15.0f);
        for (Unit* u : targets)
        {
            if (u == caster)
                continue;
            caster->DealDamage(caster, u, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_cultivation_wanjian_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// ============================================
// 371008 吞天噬地 - 30码锥形 + 吸血20%
// 法强×350%
// ============================================

class spell_cultivation_tuntian : public SpellScript
{
    PrepareSpellScript(spell_cultivation_tuntian);

    int64 _totalDamage = 0;

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        targets.remove_if([caster](WorldObject* obj)
        {
            return !obj || obj->GetGUID() == caster->GetGUID();
        });
    }

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        if (target == caster)
        {
            SetHitDamage(0);
            return;
        }

        double highValue = GetPlayerHighestDamageValue(caster);
        int64 damage = static_cast<int64>(highValue * 3.5);
        if (damage < 1) damage = 1;
        SetHitDamage(damage);
        _totalDamage += damage;

    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (!caster || _totalDamage <= 0)
            return;

        // 吸血20%
        int64 healAmount = _totalDamage / 5;
        if (healAmount > 0)
        {
            caster->ModifyHealth(healAmount);

            if (Player* player = caster->ToPlayer())
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00ff00[吞天噬地]|r 吸取生命力，回复 |cff00ff00{}|r 点生命值", healAmount);
            }
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_cultivation_tuntian::FilterTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_24);
        OnHit += SpellHitFn(spell_cultivation_tuntian::HandleOnHit);
        AfterCast += SpellCastFn(spell_cultivation_tuntian::HandleAfterCast);
    }
};

// ============================================
// 371009 九天神雷 - 50码范围每秒伤害+减速
// 每秒法强×200%
// ============================================

class spell_cultivation_jiutian_aura : public AuraScript
{
    PrepareAuraScript(spell_cultivation_jiutian_aura);

    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        double highValue = GetPlayerHighestDamageValue(caster);
        int64 damage = static_cast<int64>(highValue * 2.0);
        if (damage < 1) damage = 1;

        std::list<Unit*> targets;
        GetHostileUnitsInRange(caster, targets, 50.0f);
        for (Unit* u : targets)
        {
            if (u == caster)
                continue;
            caster->DealDamage(caster, u, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NATURE);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_cultivation_jiutian_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// ============================================
// 371010 仙神降世 - 变身30秒
// 全属性+100%、免控、每秒回血5%、击杀续时
// ============================================

class spell_cultivation_xianshen_aura : public AuraScript
{
    PrepareAuraScript(spell_cultivation_xianshen_aura);

    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        // 每秒回复5%最大生命值
        int64 healAmount = target->CountPctFromMaxHealth(5);
        if (healAmount > 0)
            target->ModifyHealth(healAmount);
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        if (Player* player = target->ToPlayer())
        {
            player->UpdateAllStats();
            player->SetFullHealth();

            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffd700[仙神降世]|r 仙神附体！全属性翻倍，生命瞬间回满，免疫控制，每秒回血！");
        }
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        if (Player* player = target->ToPlayer())
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff888888[仙神降世]|r 仙神离体，变身效果结束。");
            player->UpdateAllStats();
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_cultivation_xianshen_aura::OnPeriodic, EFFECT_0, SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE);
        OnEffectApply += AuraEffectApplyFn(spell_cultivation_xianshen_aura::OnApply, EFFECT_0, SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_cultivation_xianshen_aura::OnRemove, EFFECT_0, SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 仙神降世 - 击杀续时
class CultivationXianshenKillScript : public PlayerScript
{
public:
    CultivationXianshenKillScript() : PlayerScript("CultivationXianshenKillScript") { }

    void OnPlayerCreatureKill(Player* player, Creature* /*creature*/) override
    {
        if (!player)
            return;

        Aura* xianshenAura = player->GetAura(CULT_SPELL_XIANSHEN);
        if (!xianshenAura)
            return;

        int32 currentDuration = xianshenAura->GetDuration();
        int32 maxDuration = 60 * IN_MILLISECONDS;

        if (currentDuration < maxDuration)
        {
            int32 newDuration = std::min(currentDuration + 3 * IN_MILLISECONDS, maxDuration);
            xianshenAura->SetDuration(newDuration);
            xianshenAura->SetMaxDuration(std::max(xianshenAura->GetMaxDuration(), newDuration));

            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffd700[仙神降世]|r 击杀敌人！持续时间延长3秒（剩余: {}秒）",
                newDuration / IN_MILLISECONDS);
        }
    }
};

// ============================================
// 注册脚本
// ============================================

void AddSC_mod_cultivation_system()
{
    new CultivationWorldScript();
    new CultivationPlayerScript();
    new CultivationNPCScript();
    new CultivationCommandScript();

    // 仙术技能脚本
    RegisterSpellScript(spell_cultivation_lingren);       // 371002 灵刃术
    RegisterSpellScript(spell_cultivation_jindanbao);      // 371003 金丹爆
    RegisterSpellScript(spell_cultivation_yuanying);       // 371004 元婴出窍
    RegisterSpellScript(spell_cultivation_tianlei);        // 371005 天雷诀
    RegisterSpellScript(spell_cultivation_shunying);       // 371006 瞬影步
    RegisterSpellScript(spell_cultivation_wanjian_aura);   // 371007 万剑归宗
    RegisterSpellScript(spell_cultivation_tuntian);        // 371008 吞天噬地
    RegisterSpellScript(spell_cultivation_jiutian_aura);   // 371009 九天神雷
    RegisterSpellScript(spell_cultivation_xianshen_aura);  // 371010 仙神降世
    new CultivationXianshenKillScript();                   // 仙神降世击杀续时
}
