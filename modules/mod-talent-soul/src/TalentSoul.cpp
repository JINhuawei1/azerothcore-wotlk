/*
 * 天赋之魂系统 - 主实现文件
 */

#include "TalentSoul.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "SpellMgr.h"

TalentSoulMgr::TalentSoulMgr()
{
}

TalentSoulMgr::~TalentSoulMgr()
{
    _talentSoulData.clear();
    _playerData.clear();
}

TalentSoulMgr* TalentSoulMgr::instance()
{
    static TalentSoulMgr instance;
    return &instance;
}

void TalentSoulMgr::LoadTalentSoulData()
{
    uint32 oldMSTime = getMSTime();

    _talentSoulData.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `职业类型`, `天赋点`, `技能id`, `公共cd`, `公共cd上限`, `技能冷却`, `技能冷却上限`, "
        "`技能消耗`, `技能消耗上限`, `伤害加成`, `伤害加成上限`, `效果描述` FROM `_天赋之魂`");

    if (!result)
    {
        LOG_INFO("server.loading", ">> 天赋之魂: 未找到配置数据");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        TalentSoulData data;
        data.id = fields[0].Get<uint32>();
        data.classType = fields[1].Get<uint32>();
        data.talentPointCost = fields[2].Get<uint32>();
        data.spellId = fields[3].Get<uint32>();
        data.gcdPerLevel = fields[4].Get<float>();
        data.gcdMaxLevel = fields[5].Get<uint32>();
        data.cooldownPerLevel = fields[6].Get<float>();
        data.cooldownMaxLevel = fields[7].Get<uint32>();
        data.costPerLevel = fields[8].Get<float>();
        data.costMaxLevel = fields[9].Get<uint32>();
        data.damagePerLevel = fields[10].Get<float>();
        data.damageMaxLevel = fields[11].Get<uint32>();
        data.description = fields[12].Get<std::string>();

        _talentSoulData[data.spellId] = data;
        ++count;

    } while (result->NextRow());

    LOG_INFO("server.loading", ">> 天赋之魂: 加载 {} 条配置数据，耗时 {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void TalentSoulMgr::SetupIndependentGCDCategories()
{
    // 为配置了GCD减少的技能设置独立的StartRecoveryCategory
    // 这样每个技能都有自己独立的GCD计时器，不会互相影响

    uint32 setupCount = 0;

    for (auto const& pair : _talentSoulData)
    {
        const TalentSoulData& data = pair.second;

        // 只为配置了GCD减少的技能设置独立类别
        if (data.gcdMaxLevel > 0 && data.gcdPerLevel > 0)
        {
            SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(data.spellId));
            if (spellInfo)
            {
                // 使用 10000 + spellId 作为独立类别，确保不与现有类别冲突
                // 原始类别133是大多数技能共用的
                uint32 oldCategory = spellInfo->StartRecoveryCategory;
                uint32 newCategory = 10000 + data.spellId;

                if (oldCategory != newCategory && oldCategory > 0)
                {
                    spellInfo->StartRecoveryCategory = newCategory;
                    ++setupCount;
                }
            }
        }
    }

    if (setupCount > 0)
    {
        LOG_INFO("server.loading", ">> 天赋之魂: 已为 {} 个技能设置独立GCD类别", setupCount);
    }
}

TalentSoulData const* TalentSoulMgr::GetTalentSoulData(uint32 spellId) const
{
    auto itr = _talentSoulData.find(spellId);
    if (itr != _talentSoulData.end())
        return &(itr->second);
    return nullptr;
}

std::vector<TalentSoulData const*> TalentSoulMgr::GetClassTalentSoulData(uint8 playerClass) const
{
    std::vector<TalentSoulData const*> result;

    for (auto const& pair : _talentSoulData)
    {
        // 职业类型0表示全职业可用，或者匹配玩家职业
        if (pair.second.classType == 0 || pair.second.classType == playerClass)
        {
            result.push_back(&pair.second);
        }
    }

    return result;
}

// 解析紧凑格式数据: 技能ID,公共cd等级,冷却等级,消耗等级,伤害等级;...
void TalentSoulMgr::ParseCompactData(const std::string& data, PlayerTalentSoulData& playerData)
{
    playerData.skills.clear();

    if (data.empty())
        return;

    std::stringstream ss(data);
    std::string skillEntry;

    while (std::getline(ss, skillEntry, ';'))
    {
        if (skillEntry.empty())
            continue;

        std::stringstream skillSS(skillEntry);
        std::string value;
        std::vector<uint32> values;

        while (std::getline(skillSS, value, ','))
        {
            try
            {
                values.push_back(static_cast<uint32>(std::stoul(value)));
            }
            catch (...)
            {
                values.push_back(0);
            }
        }

        // 需要5个值: 技能ID,公共cd等级,冷却等级,消耗等级,伤害等级
        if (values.size() >= 5)
        {
            PlayerSkillData skill;
            skill.spellId = values[0];
            skill.gcdLevel = values[1];
            skill.cooldownLevel = values[2];
            skill.costLevel = values[3];
            skill.damageLevel = values[4];

            playerData.skills[skill.spellId] = skill;
        }
    }
}

// 生成紧凑格式数据
std::string TalentSoulMgr::GenerateCompactData(const PlayerTalentSoulData& playerData)
{
    std::stringstream ss;
    bool first = true;

    for (auto const& pair : playerData.skills)
    {
        const PlayerSkillData& skill = pair.second;

        // 跳过没有任何等级的技能
        if (skill.gcdLevel == 0 && skill.cooldownLevel == 0 &&
            skill.costLevel == 0 && skill.damageLevel == 0)
            continue;

        if (!first)
            ss << ";";

        ss << skill.spellId << ","
           << skill.gcdLevel << ","
           << skill.cooldownLevel << ","
           << skill.costLevel << ","
           << skill.damageLevel;

        first = false;
    }

    return ss.str();
}

void TalentSoulMgr::LoadPlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 初始化玩家数据
    _playerData[playerGuid] = PlayerTalentSoulData();

    QueryResult result = CharacterDatabase.Query(
        "SELECT `天赋点`, `技能数据` FROM `_天赋之魂_玩家数据` WHERE `角色id` = {}", playerGuid);

    if (!result)
        return;

    Field* fields = result->Fetch();

    _playerData[playerGuid].usedTalentPoints = fields[0].Get<uint32>();
    std::string compactData = fields[1].Get<std::string>();

    ParseCompactData(compactData, _playerData[playerGuid]);

    LOG_DEBUG("module", "天赋之魂: 为玩家 {} 加载了 {} 条技能数据，已使用天赋点: {}",
        player->GetName(), _playerData[playerGuid].skills.size(), _playerData[playerGuid].usedTalentPoints);
}

void TalentSoulMgr::SavePlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return;

    std::string compactData = GenerateCompactData(itr->second);

    // 转义字符串以防止SQL注入
    CharacterDatabase.Execute(
        "REPLACE INTO `_天赋之魂_玩家数据` (`角色id`, `天赋点`, `技能数据`) VALUES ({}, {}, '{}')",
        playerGuid, itr->second.usedTalentPoints, compactData);

    LOG_DEBUG("module", "天赋之魂: 为玩家 {} 保存数据，紧凑格式: {}",
        player->GetName(), compactData);
}

void TalentSoulMgr::OnPlayerLogout(uint32 playerGuid)
{
    _playerData.erase(playerGuid);
}

PlayerTalentSoulData* TalentSoulMgr::GetPlayerData(uint32 playerGuid)
{
    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return nullptr;
    return &(itr->second);
}

PlayerSkillData* TalentSoulMgr::GetPlayerSkillData(uint32 playerGuid, uint32 spellId)
{
    auto playerItr = _playerData.find(playerGuid);
    if (playerItr == _playerData.end())
        return nullptr;

    auto skillItr = playerItr->second.skills.find(spellId);
    if (skillItr == playerItr->second.skills.end())
        return nullptr;

    return &(skillItr->second);
}

bool TalentSoulMgr::CanUpgradeSpell(Player* player, uint32 spellId, std::string& errorMsg) const
{
    if (!player)
    {
        errorMsg = "玩家无效";
        return false;
    }

    // 检查技能配置是否存在
    TalentSoulData const* config = GetTalentSoulData(spellId);
    if (!config)
    {
        errorMsg = "该技能没有配置天赋之魂效果";
        return false;
    }

    // 检查职业限制
    if (config->classType != 0 && config->classType != player->getClass())
    {
        errorMsg = "该技能不适用于你的职业";
        return false;
    }

    // 检查天赋点需求
    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerItr = _playerData.find(playerGuid);

    // 检查玩家是否已经升级过这个技能
    bool hasSkill = false;
    if (playerItr != _playerData.end())
    {
        hasSkill = playerItr->second.skills.find(spellId) != playerItr->second.skills.end();
    }

    // 计算本次升级需要的天赋点
    uint32 requiredPoints = 1;  // 每次升级消耗1点
    if (!hasSkill && config->talentPointCost > 0)
    {
        // 新技能首次解锁需要额外的前置天赋点
        requiredPoints = config->talentPointCost;
    }

    // 检查是否有足够天赋点
    uint32 availablePoints = GetPlayerAvailableTalentPoints(const_cast<Player*>(player));
    if (availablePoints < requiredPoints)
    {
        errorMsg = "天赋点不足，需要 " + std::to_string(requiredPoints) + " 点";
        return false;
    }

    return true;
}

bool TalentSoulMgr::UpgradePlayerSpell(Player* player, uint32 spellId, TalentSoulUpgradeType upgradeType)
{
    if (!player || upgradeType >= TALENT_SOUL_UPGRADE_MAX)
        return false;

    std::string errorMsg;
    if (!CanUpgradeSpell(player, spellId, errorMsg))
        return false;

    TalentSoulData const* config = GetTalentSoulData(spellId);
    if (!config)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 确保玩家数据存在
    if (_playerData.find(playerGuid) == _playerData.end())
    {
        _playerData[playerGuid] = PlayerTalentSoulData();
    }

    PlayerTalentSoulData& playerData = _playerData[playerGuid];

    // 获取或创建技能数据
    bool isNewSkill = playerData.skills.find(spellId) == playerData.skills.end();
    if (isNewSkill)
    {
        PlayerSkillData newSkill;
        newSkill.spellId = spellId;
        playerData.skills[spellId] = newSkill;

        // 扣除天赋点
        if (config->talentPointCost > 0)
        {
            playerData.usedTalentPoints += config->talentPointCost;
        }
    }

    PlayerSkillData& skill = playerData.skills[spellId];

    // 根据类型升级
    bool success = false;
    switch (upgradeType)
    {
        case TALENT_SOUL_UPGRADE_GCD:
            if (skill.gcdLevel < config->gcdMaxLevel)
            {
                skill.gcdLevel++;
                success = true;
            }
            break;
        case TALENT_SOUL_UPGRADE_COOLDOWN:
            if (skill.cooldownLevel < config->cooldownMaxLevel)
            {
                skill.cooldownLevel++;
                success = true;
            }
            break;
        case TALENT_SOUL_UPGRADE_COST:
            if (skill.costLevel < config->costMaxLevel)
            {
                skill.costLevel++;
                success = true;
            }
            break;
        case TALENT_SOUL_UPGRADE_DAMAGE:
            if (skill.damageLevel < config->damageMaxLevel)
            {
                skill.damageLevel++;
                success = true;
            }
            break;
        default:
            break;
    }

    if (success)
    {
        // 每次升级消耗1点天赋点（新技能首次解锁时已在上面扣除）
        if (!isNewSkill)
        {
            playerData.usedTalentPoints += 1;
        }

        // 立即保存到数据库
        std::string compactData = GenerateCompactData(playerData);
        CharacterDatabase.Execute(
            "REPLACE INTO `_天赋之魂_玩家数据` (`角色id`, `天赋点`, `技能数据`) VALUES ({}, {}, '{}')",
            playerGuid, playerData.usedTalentPoints, compactData);
    }

    return success;
}

uint32 TalentSoulMgr::GetPlayerUsedTalentPoints(uint32 playerGuid) const
{
    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return 0;
    return itr->second.usedTalentPoints;
}

uint32 TalentSoulMgr::GetPlayerAvailableTalentPoints(Player* player) const
{
    if (!player)
        return 0;

    // 可用天赋点 = 总天赋点(含倍率) - 已使用的天赋点
    uint32 totalPoints = player->CalculateTalentsPoints();
    uint32 usedPoints = GetPlayerUsedTalentPoints(player->GetGUID().GetCounter());

    return totalPoints > usedPoints ? totalPoints - usedPoints : 0;
}

float TalentSoulMgr::GetPlayerGCDReduction(uint32 playerGuid, uint32 spellId) const
{
    TalentSoulData const* config = GetTalentSoulData(spellId);
    if (!config)
        return 0.0f;

    auto playerItr = _playerData.find(playerGuid);
    if (playerItr == _playerData.end())
        return 0.0f;

    auto skillItr = playerItr->second.skills.find(spellId);
    if (skillItr == playerItr->second.skills.end())
        return 0.0f;

    return config->gcdPerLevel * skillItr->second.gcdLevel;
}

float TalentSoulMgr::GetPlayerCooldownReduction(uint32 playerGuid, uint32 spellId) const
{
    TalentSoulData const* config = GetTalentSoulData(spellId);
    if (!config)
        return 0.0f;

    auto playerItr = _playerData.find(playerGuid);
    if (playerItr == _playerData.end())
        return 0.0f;

    auto skillItr = playerItr->second.skills.find(spellId);
    if (skillItr == playerItr->second.skills.end())
        return 0.0f;

    return config->cooldownPerLevel * skillItr->second.cooldownLevel;
}

float TalentSoulMgr::GetPlayerCostReduction(uint32 playerGuid, uint32 spellId) const
{
    TalentSoulData const* config = GetTalentSoulData(spellId);
    if (!config)
        return 0.0f;

    auto playerItr = _playerData.find(playerGuid);
    if (playerItr == _playerData.end())
        return 0.0f;

    auto skillItr = playerItr->second.skills.find(spellId);
    if (skillItr == playerItr->second.skills.end())
        return 0.0f;

    return config->costPerLevel * skillItr->second.costLevel;
}

float TalentSoulMgr::GetPlayerDamageBonus(uint32 playerGuid, uint32 spellId) const
{
    TalentSoulData const* config = GetTalentSoulData(spellId);
    if (!config)
        return 0.0f;

    auto playerItr = _playerData.find(playerGuid);
    if (playerItr == _playerData.end())
        return 0.0f;

    auto skillItr = playerItr->second.skills.find(spellId);
    if (skillItr == playerItr->second.skills.end())
        return 0.0f;

    return config->damagePerLevel * skillItr->second.damageLevel;
}

void TalentSoulMgr::ApplyGCDReduction(Player* player, uint32 spellId, int32& gcdReduction) const
{
    if (!player)
        return;

    float reductionPercent = GetPlayerGCDReduction(player->GetGUID().GetCounter(), spellId);
    if (reductionPercent > 0)
    {
        // 默认GCD是1500毫秒
        int32 baseGCD = 1500;
        int32 reducedAmount = static_cast<int32>(baseGCD * reductionPercent / 100.0f);
        gcdReduction = -reducedAmount;  // 负值表示减少
    }
}

void TalentSoulMgr::ApplyCooldownReduction(Player* player, uint32 spellId, int32& cooldownReduction) const
{
    if (!player)
        return;

    float reductionPercent = GetPlayerCooldownReduction(player->GetGUID().GetCounter(), spellId);
    if (reductionPercent > 0)
    {
        // 获取技能的基础冷却时间
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        int32 baseCooldown = spellInfo->RecoveryTime;
        if (baseCooldown <= 0)
            baseCooldown = spellInfo->CategoryRecoveryTime;

        if (baseCooldown > 0)
        {
            int32 reducedAmount = static_cast<int32>(baseCooldown * reductionPercent / 100.0f);
            cooldownReduction = -reducedAmount;  // 负值表示减少
        }
    }
}

void TalentSoulMgr::ApplyCostReduction(Player* player, uint32 spellId, int32& cost) const
{
    if (!player || cost <= 0)
        return;

    float reductionPercent = GetPlayerCostReduction(player->GetGUID().GetCounter(), spellId);
    if (reductionPercent > 0)
    {
        int32 originalCost = cost;
        int32 reducedAmount = static_cast<int32>(cost * reductionPercent / 100.0f);
        cost -= reducedAmount;

        if (cost < 0)
            cost = 0;
    }
}

void TalentSoulMgr::ApplyDamageBonus(Unit* attacker, uint32 spellId, int32& damage) const
{
    if (!attacker || damage <= 0)
        return;

    Player* player = attacker->ToPlayer();
    if (!player)
        return;

    float bonusPercent = GetPlayerDamageBonus(player->GetGUID().GetCounter(), spellId);
    if (bonusPercent > 0)
    {
        int32 bonusAmount = static_cast<int32>(damage * bonusPercent / 100.0f);
        damage += bonusAmount;
    }
}
