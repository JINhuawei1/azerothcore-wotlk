#include "ItemEnhancementMgr.h"
#include "Logging/Log.h"
#include "GameTime.h"
#include "DatabaseEnv.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "Server/WorldSessionMgr.h"
#include "ItemTemplate.h"
#include "SharedDefines.h"
#include "Random.h"
#include "RequirementSystem.h"
#include <algorithm>
#include <sstream>
#include <set>

// 统一物品提示框 / 鉴定系统集成（用于发送 ALL_MODULE_DATA 批量数据）
#if __has_include("mod-item-identification-system/src/ItemIdentificationSystem.h")
    #ifndef MODULE_ITEM_IDENTIFICATION_SYSTEM
        #define MODULE_ITEM_IDENTIFICATION_SYSTEM
    #endif
    #include "mod-item-identification-system/src/ItemIdentificationSystem.h"
#endif

// 可选集成：物品属性模板系统（mod-item-attributes）
#if __has_include("ItemAttributesLoader.h")
    #ifndef MODULE_ITEM_ATTRIBUTES
        #define MODULE_ITEM_ATTRIBUTES
    #endif
    #include "ItemAttributesLoader.h"
#endif

#ifdef MODULE_ITEM_ATTRIBUTES
#include "ItemAttributesDBHelper.h"
#include "ItemAttributesEffects.h"
#endif

ItemEnhancementMgr* ItemEnhancementMgr::instance()
{
    static ItemEnhancementMgr instance;
    return &instance;
}

ItemEnhancementMgr::ItemEnhancementMgr()
{
    _enabled = true;
    _failureProtectionEnabled = true;
    _showSuccessRateEnabled = true;
    _announceEnabled = true;
    _announceLevel = 10;
    _goldCostRate = 1.0f;
    _materialCostRate = 1.0f;
    _successRateRate = 1.0f;
    _attributeRate = 1.0f;
    _cooldown = 0;
    _maxLevel = 20;
    _debugMode = false;
    _recordCacheLifetime = 0;

    // 初始化容器指针
    _enhancementTemplates = new std::unordered_map<uint32, EnhancementTemplate>();
    _enhancementTemplatesByGroupLevel = new std::map<std::pair<uint32, uint32>, uint32>();
}

ItemEnhancementMgr::~ItemEnhancementMgr()
{
    // 清理容器指针
    delete _enhancementTemplates;
    delete _enhancementTemplatesByGroupLevel;

    _recordCache.clear();
}

void ItemEnhancementMgr::LoadConfig()
{
    // 设置默认值来避免 IntelliSense 解析问题
    _enabled = true;
    _failureProtectionEnabled = true;
    _showSuccessRateEnabled = true;
    _announceEnabled = true;
    _announceLevel = 10;
    _goldCostRate = 1.0f;
    _materialCostRate = 1.0f;
    _successRateRate = 1.0f;
    _attributeRate = 1.0f;
    _cooldown = 0;
    _maxLevel = 20;
    _debugMode = false;
    _recordCacheLifetime = 0;

    // 实际代码在编译时会执行，但 IntelliSense 会跳过
    #ifndef __INTELLISENSE__
    _enabled = sConfigMgr->GetOption("ItemEnhancement.Enable", _enabled);
    _failureProtectionEnabled = sConfigMgr->GetOption("ItemEnhancement.FailureProtection", _failureProtectionEnabled);
    _showSuccessRateEnabled = sConfigMgr->GetOption("ItemEnhancement.ShowSuccessRate", _showSuccessRateEnabled);
    _announceEnabled = sConfigMgr->GetOption("ItemEnhancement.Announce", _announceEnabled);
    _announceLevel = sConfigMgr->GetOption("ItemEnhancement.AnnounceLevel", _announceLevel);
    _goldCostRate = sConfigMgr->GetOption("ItemEnhancement.GoldCostRate", _goldCostRate);
    _materialCostRate = sConfigMgr->GetOption("ItemEnhancement.MaterialCostRate", _materialCostRate);
    _successRateRate = sConfigMgr->GetOption("ItemEnhancement.SuccessRateRate", _successRateRate);
    _attributeRate = sConfigMgr->GetOption("ItemEnhancement.AttributeRate", _attributeRate);
    _cooldown = sConfigMgr->GetOption("ItemEnhancement.Cooldown", _cooldown);
    _maxLevel = sConfigMgr->GetOption("ItemEnhancement.MaxLevel", _maxLevel);
    _debugMode = sConfigMgr->GetOption("ItemEnhancement.DebugMode", _debugMode);
    _recordCacheLifetime = sConfigMgr->GetOption("ItemEnhancement.RecordCacheTTL", _recordCacheLifetime);
    #endif
}

void ItemEnhancementMgr::LoadEnhancementTemplates()
{
    _enhancementTemplates->clear();
    _enhancementTemplatesByGroupLevel->clear();

    //LOG_INFO("server.loading", "正在加载物品强化模板数据...");

    // 首先检查表是否存在
    QueryResult checkTable = WorldDatabase.Query("SHOW TABLES LIKE '物品强化_系统'");
    if (!checkTable)
    {
         //LOG_ERROR("server.loading", ">> 数据库表 '物品强化_系统' 不存在！");
        return;
    }

    // 检查表中是否有数据
    QueryResult countResult = WorldDatabase.Query("SELECT COUNT(*) FROM 物品强化_系统");
    if (countResult)
    {
        Field* countFields = countResult->Fetch();
        uint32 totalCount = countFields[0].Get<uint32>();
        //LOG_INFO("server.loading", ">> 数据库表 '物品强化_系统' 中共有 {} 条记录", totalCount);
    }

    QueryResult result = WorldDatabase.Query("SELECT * FROM 物品强化_系统 ORDER BY `组`, `等级`");
    if (!result)
    {
        //LOG_ERROR("server.loading", ">> 查询物品强化模板数据失败或数据为空");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        EnhancementTemplate temp;
        uint32 index = 0;

        // 按照数据库表结构正确读取字段
        // 1. 注释 (varchar)
        std::string comment = "";
        if (!fields[index].IsNull())
            comment = fields[index].Get<std::string>();
        index++;

        // 2. id (int)
        temp.id = fields[index++].Get<uint32>();

        // 3. 组 (int)
        temp.group = fields[index++].Get<uint32>();

        // 4. 等级 (int)
        temp.level = fields[index++].Get<uint32>();

        // 5. 几率 (float)
        temp.chance = fields[index++].Get<float>();

        // 6. 物品属性_模板_组 (int)
        temp.itemStatGroup = fields[index++].Get<uint32>();

        // 7. 平均随机分配 (int)
        temp.distributionType = fields[index++].Get<uint32>();

        // 8. 属性值1 (int)
        temp.statValue1 = fields[index++].Get<uint32>();

        // 9. 属性值2 (int)
        temp.statValue2 = fields[index++].Get<uint32>();

        // 10. 属性百分比1 (int)
        temp.statPercent1 = fields[index++].Get<uint32>();

        // 11. 属性百分比2 (int)
        temp.statPercent2 = fields[index++].Get<uint32>();

        // 12. 达到该强化等级后的奖励属性 (int)
        temp.bonusStat = fields[index++].Get<uint32>();

        // 13. 达到该强化等级后的奖励属性百分比 (int)
        temp.bonusStatPercent = fields[index++].Get<uint32>();

        // 14. 物品技能_模板_id_多个逗号隔开 (varchar)
        if (!fields[index].IsNull())
            temp.skillIds = fields[index].Get<std::string>();
        else
            temp.skillIds = "";
        index++;

        // 15. 强化需求 (int)
        temp.requirementId = fields[index++].Get<uint32>();

        // 16. 失败处理 (tinyint)
        temp.failureType = fields[index++].Get<uint8>();

        (*_enhancementTemplates)[temp.id] = temp;
        (*_enhancementTemplatesByGroupLevel)[std::make_pair(temp.group, temp.level)] = temp.id;

        // 添加调试信息，特别是组0的数据
        if (temp.group == 0)
        {
            // LOG_INFO("server.loading", "加载组0强化模板: ID={}, 等级={}, 成功率={:.1f}%, 失败类型={}, 属性值1={}, 属性值2={}",
            //     temp.id, temp.level, temp.chance, temp.failureType, temp.statValue1, temp.statValue2);
        }

        count++;
    } while (result->NextRow());

    //LOG_INFO("server.loading", ">> 加载了 {} 条物品强化模板数据", count);

    // 检查组0的数据是否正确加载
    uint32 group0Count = 0;
    for (const auto& pair : *_enhancementTemplatesByGroupLevel)
    {
        if (pair.first.first == 0) // 组0
        {
            group0Count++;
        }
    }
    //LOG_INFO("server.loading", ">> 其中组0（官方装备）模板数据: {} 条", group0Count);
}

// 不再使用缓存，直接从数据库读取记录

EnhancementTemplate const* ItemEnhancementMgr::GetEnhancementTemplate(uint32 id) const
{
    auto itr = _enhancementTemplates->find(id);
    if (itr != _enhancementTemplates->end())
        return &itr->second;
    return nullptr;
}

EnhancementTemplate const* ItemEnhancementMgr::GetEnhancementTemplateByGroupAndLevel(uint32 group, uint32 level) const
{
    auto itr = _enhancementTemplatesByGroupLevel->find(std::make_pair(group, level));
    if (itr != _enhancementTemplatesByGroupLevel->end())
        return GetEnhancementTemplate(itr->second);
    return nullptr;
}

std::unique_ptr<EnhancementRecord> ItemEnhancementMgr::DeserializeRecord(Field* fields) const
{
    if (!fields)
        return nullptr;

    auto record = std::make_unique<EnhancementRecord>();
    uint32 index = 0;

    record->itemGuid = fields[index++].Get<uint32>();
    record->ownerGuid = fields[index++].Get<uint32>();
    record->itemTemplateId = fields[index++].Get<uint32>();
    record->group = fields[index++].Get<uint32>();
    record->level = fields[index++].Get<uint32>();
    record->enhancementExp = fields[index++].Get<uint32>();
    record->statValues = fields[index++].Get<std::string>();
    record->qualityLevel = fields[index++].Get<uint32>();
    record->qualityUpgradeCount = fields[index++].Get<uint32>();
    record->enhancementCount = fields[index++].Get<uint32>();
    record->failureCount = fields[index++].Get<uint32>();
    record->lastEnhanceTime = fields[index++].Get<uint32>();

    return record;
}

EnhancementRecord const* ItemEnhancementMgr::GetEnhancementRecord(uint32 itemGuid) const
{
    if (itemGuid == 0)
        return nullptr;

    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());

    auto cacheItr = _recordCache.find(itemGuid);
    if (cacheItr != _recordCache.end())
    {
        if (_recordCacheLifetime == 0 || now - cacheItr->second.lastUpdateTime <= _recordCacheLifetime)
            return cacheItr->second.record.get();

        _recordCache.erase(cacheItr);
    }

    QueryResult result = CharacterDatabase.Query("SELECT * FROM 物品强化_记录 WHERE guid = {}", itemGuid);
    if (!result)
    {
        EnhancementRecordCacheEntry entry;
        entry.lastUpdateTime = now;
        _recordCache[itemGuid] = std::move(entry);
        return nullptr;
    }

    Field* fields = result->Fetch();
    std::unique_ptr<EnhancementRecord> record = DeserializeRecord(fields);
    if (!record)
    {
        EnhancementRecordCacheEntry entry;
        entry.lastUpdateTime = now;
        _recordCache[itemGuid] = std::move(entry);
        return nullptr;
    }

    EnhancementRecordCacheEntry entry;
    entry.record = std::move(record);
    entry.lastUpdateTime = now;

    auto& cacheEntry = _recordCache[itemGuid];
    cacheEntry = std::move(entry);
    return cacheEntry.record.get();
}

void ItemEnhancementMgr::PreloadPlayerEnhancementRecords(Player* player)
{
    if (!player)
        return;

    uint32 ownerGuid = player->GetGUID().GetCounter();
    QueryResult result = CharacterDatabase.Query("SELECT * FROM 物品强化_记录 WHERE owner_guid = {}", ownerGuid);
    if (!result)
        return;

    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());

    do
    {
        Field* fields = result->Fetch();
        std::unique_ptr<EnhancementRecord> record = DeserializeRecord(fields);
        if (!record)
            continue;

        uint32 recordGuid = record->itemGuid;
        auto& cacheEntry = _recordCache[recordGuid];

        if (!cacheEntry.record)
            cacheEntry.record = std::move(record);
        else
            *cacheEntry.record = *record;

        cacheEntry.lastUpdateTime = now;
    } while (result->NextRow());
}

void ItemEnhancementMgr::SaveEnhancementRecord(EnhancementRecord const& record)
{
    // 使用正确的表名和字段名
    std::string sql = "REPLACE INTO 物品强化_记录 (";
    sql += "guid, owner_guid, 物品模板ID, 强化组, 强化等级, 强化经验, 属性值, ";
    sql += "品质等级, 品质进阶次数, 强化次数, 失败次数, 最后强化时间";
    sql += ") VALUES (";
    sql += std::to_string(record.itemGuid) + ", ";           // guid
    sql += std::to_string(record.ownerGuid) + ", ";          // owner_guid
    sql += "0, ";                                            // 物品模板ID (暂时设为0)
    sql += std::to_string(record.group) + ", ";              // 强化组
    sql += std::to_string(record.level) + ", ";              // 强化等级
    sql += "0, ";                                            // 强化经验
    sql += "'" + record.statValues + "', ";                  // 属性值
    sql += std::to_string(record.qualityLevel) + ", ";       // 品质等级
    sql += std::to_string(record.qualityUpgradeCount) + ", "; // 品质进阶次数
    sql += std::to_string(record.enhancementCount) + ", ";   // 强化次数
    sql += std::to_string(record.failureCount) + ", ";       // 失败次数
    sql += std::to_string(record.lastEnhanceTime);           // 最后强化时间
    sql += ")";

    CharacterDatabase.Execute(sql);

    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
    auto& entry = _recordCache[record.itemGuid];
    if (!entry.record)
        entry.record = std::make_unique<EnhancementRecord>(record);
    else
        *entry.record = record;
    entry.lastUpdateTime = now;
}

void ItemEnhancementMgr::DeleteEnhancementRecord(uint32 itemGuid)
{
    // 使用正确的表名和字段名
    std::string sql = "DELETE FROM 物品强化_记录 WHERE guid = " + std::to_string(itemGuid);
    CharacterDatabase.Execute(sql);

    _recordCache.erase(itemGuid);
}

void ItemEnhancementMgr::DeleteAllEnhancementRecordsByPlayer(uint32 playerGuid)
{
    // 删除指定玩家的所有强化记录
    std::string sql = "DELETE FROM 物品强化_记录 WHERE owner_guid = " + std::to_string(playerGuid);
    LOG_INFO("server.loading", "强化系统: 执行SQL删除玩家 {} 的强化记录: {}", playerGuid, sql);
    CharacterDatabase.Execute(sql);
    LOG_INFO("server.loading", "强化系统: 已删除玩家 {} 的所有强化记录", playerGuid);

    for (auto itr = _recordCache.begin(); itr != _recordCache.end(); )
    {
        if (itr->second.record && itr->second.record->ownerGuid == playerGuid)
            itr = _recordCache.erase(itr);
        else
            ++itr;
    }
}

bool ItemEnhancementMgr::CanEnhanceItem(Player* player, Item* item) const
{
    if (!player || !item)
        return false;

    if (!_enabled)
        return false;

    // 检查物品是否装备
    if (item->GetOwnerGUID() != player->GetGUID())
        return false;

    // 检查物品类型
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // 只允许强化武器和装备
    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
        return false;

    // 检查冷却时间
    EnhancementRecord const* record = GetEnhancementRecord(item->GetGUID().GetCounter());
    if (record && _cooldown > 0)
    {
        uint32 lastTime = record->lastEnhanceTime;
        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        if (now < lastTime + _cooldown)
            return false;
    }

    return true;
}

bool ItemEnhancementMgr::EnhanceItem(Player* player, Item* item, uint32 group)
{
    if (!CanEnhanceItem(player, item))
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord record;
    bool isNewRecord = false;

    // 获取或创建强化记录
    EnhancementRecord const* existingRecord = GetEnhancementRecord(itemGuid);
    if (existingRecord)
    {
        record = *existingRecord;
    }
    else
    {
        record.itemGuid = itemGuid;
        record.ownerGuid = player->GetGUID().GetCounter();
        record.group = group; // 设置强化组号
        record.level = 0;
        record.qualityLevel = 0;
        record.qualityUpgradeCount = 0;
        record.enhancementCount = 0;
        record.failureCount = 0;
        record.lastEnhanceTime = 0;
        isNewRecord = true;
    }

    // 备份旧的属性字符串，便于与物品属性系统进行增量同步
    std::string oldStatValuesSnapshot = record.statValues;

    // 检查是否达到最大等级
    uint32 maxLevel = _maxLevel;
    if (record.level >= maxLevel)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("物品已达到最大强化等级");
        return false;
    }

    // 获取下一级强化模板
    uint32 nextLevel = record.level + 1;

    if (_debugMode)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("查找强化模板: 组{}, 等级{}", record.group, nextLevel);
        // 不再遍历所有等级，只显示当前尝试查找的等级
    }

    EnhancementTemplate const* enhTemplate = GetEnhancementTemplateByGroupAndLevel(record.group, nextLevel); // 使用记录中的组号
    if (!enhTemplate)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("错误：找不到组{}等级{}的强化配置，请检查数据库", record.group, nextLevel);

        if (_debugMode)
        {
            // 显示详细的调试信息
            ChatHandler(player->GetSession()).PSendSysMessage("调试信息：查找键值对 ({}, {})", record.group, nextLevel);
            auto itr = _enhancementTemplatesByGroupLevel->find(std::make_pair(record.group, nextLevel));
            if (itr == _enhancementTemplatesByGroupLevel->end())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("在 _enhancementTemplatesByGroupLevel 中未找到该键值对");
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("找到键值对，模板ID: {}", itr->second);
            }
        }

        return false;
    }

    // 检查强化需求
    if (enhTemplate->requirementId > 0)
    {
        // 使用需求模板系统检查玩家是否满足需求
        if (sRequirementSystem)
        {
            // 先检查需求是否满足（不显示消息）
            bool requirementMet = sRequirementSystem->CheckRequirements(player, enhTemplate->requirementId, false);

            if (!requirementMet)
            {
                // 如果不满足，再次检查并显示详细消息
                sRequirementSystem->CheckRequirements(player, enhTemplate->requirementId, true);
                ChatHandler(player->GetSession()).PSendSysMessage("强化失败：不满足强化需求条件");
                return false;
            }

            // 满足需求，消耗需求物品/金币等
            bool consumed = sRequirementSystem->ConsumeRequirements(player, enhTemplate->requirementId);

            if (!consumed)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("强化失败：无法消耗需求物品");
                return false;
            }
        }
        else
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("警告：需求系统未初始化，跳过需求检查");
        }
    }

    // 计算成功率
    float successRate = enhTemplate->chance * _successRateRate;
    uint8 failureType = enhTemplate->failureType;

    if (_debugMode)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("使用数据库配置 - 组: {}, 等级: {}, 成功率: {:.1f}%, 失败类型: {}, 需求ID: {}",
            enhTemplate->group, enhTemplate->level, successRate, failureType, enhTemplate->requirementId);
    }

    successRate = std::min(100.0f, std::max(1.0f, successRate)); // 限制在1%-100%之间

    // 显示成功率
    if (_showSuccessRateEnabled)
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化成功率: {:.1f}%", successRate);
    }

    // 随机判断是否成功（使用AzerothCore的随机数系统）
    bool success = urand(1, 100) <= static_cast<uint32>(successRate);

    if (success)
    {
        // 强化成功
        record.level = nextLevel;
        record.lastEnhanceTime = static_cast<uint32>(GameTime::GetGameTime().count());

        // 生成本次强化的增量属性加成（只计算本次强化增加的属性）
        std::string incrementalStats = GenerateIncrementalEnhancementStats(item, nextLevel, record.group);

        // 先保存旧的属性值（用于移除之前的强化效果）
        std::string oldStatValues = record.statValues;

        if (!incrementalStats.empty())
        {
            // 解析本次强化增加的属性
            std::map<uint32, int32> incrementalStatsMap = ParseStatValues(incrementalStats);

            // 解析现有的累计属性
            std::map<uint32, int32> existingStats = ParseStatValues(record.statValues);

            // ChatHandler(player->GetSession()).PSendSysMessage("本次强化增量属性: '{}'", incrementalStats);
            // ChatHandler(player->GetSession()).PSendSysMessage("强化前累计属性: '{}'", record.statValues);

            // 累加属性（增量模式）
            for (const auto& incrementalStat : incrementalStatsMap)
            {
                existingStats[incrementalStat.first] += incrementalStat.second;
                // ChatHandler(player->GetSession()).PSendSysMessage("属性累加: {} (类型{}) 原值:{} + 增量:{} = 新值:{}",
                //     GetStatTypeName(incrementalStat.first), incrementalStat.first,
                //     existingStats[incrementalStat.first] - incrementalStat.second,
                //     incrementalStat.second, existingStats[incrementalStat.first]);
            }

            // 重新生成属性字符串
            std::string updatedStatValues = "";
            bool first = true;
            for (const auto& stat : existingStats)
            {
                if (stat.second > 0)
                {
                    if (!first) updatedStatValues += ",";
                    updatedStatValues += std::to_string(stat.first) + " " + std::to_string(stat.second);
                    first = false;
                }
            }
            record.statValues = updatedStatValues;

            // ChatHandler(player->GetSession()).PSendSysMessage("强化后累计属性: '{}'", record.statValues);
        }

        // 保存记录到数据库
        SaveEnhancementRecord(record);

#ifdef MODULE_ITEM_ATTRIBUTES
        // 同步到物品属性系统，由物品属性模块统一应用/移除强化属性
        SyncEnhancementAttributesToItemAttributes(player, item, record.statValues, oldStatValuesSnapshot);
#else
        // 先移除之前的强化效果（如果有的话）
        if (record.level > 1 && !oldStatValuesSnapshot.empty())
        {
            // 移除之前等级的属性（基于之前保存的旧属性）
            RemovePreviousEnhancementStats(player, oldStatValuesSnapshot);
        }
#endif

        // 不再向随机附魔字段写入GUID，避免与随机附魔系统和聊天链接冲突
        if (item->IsEquipped())
            player->SetVisibleItemSlot(item->GetSlot(), item);

        // 关键修复：不在这里应用强化效果，因为强化效果只应该在装备穿戴时应用
        // ApplyOfficialItemEnhancement(player, item, record.level);
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：强化数据已保存，装备穿戴时将自动应用效果");

        // 发送成功消息
        ChatHandler(player->GetSession()).PSendSysMessage("物品强化成功，当前等级: +{}", record.level);

        // 强化成功后，从数据库重新获取最新的强化数据发送给客户端
        EnhancementRecord const* updatedRecord = GetEnhancementRecord(itemGuid);
            if (updatedRecord)
            {
                SendEnhancementDataToClient(player, item, *updatedRecord);

#ifndef MODULE_ITEM_ATTRIBUTES
                // 检查装备是否已穿戴，如果穿戴则应用新的强化效果
                bool isEquipped = false;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                    if (equippedItem && equippedItem->GetGUID().GetCounter() == itemGuid)
                    {
                        isEquipped = true;
                        // 关键修复：先移除旧的强化效果，再应用新的强化效果
                        // 但不要使用OnPlayerUnequipItem和OnPlayerEquipItem，因为它们会重复处理
                        // 直接应用新的强化效果即可，因为之前已经移除了旧效果
                        ApplyOfficialItemEnhancement(player, item, record.level);
                        break;
                    }
                }

                if (!isEquipped)
                {
                    // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备未穿戴，强化效果将在穿戴时应用");
                }
#endif
            }
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("警告：无法获取最新的强化数据");
        }

        // 全服公告
        if (_announceEnabled && record.level >= _announceLevel)
        {
            std::string itemLink = item->GetTemplate()->Name1;
            std::string announcement = player->GetName() + " 成功将 " + itemLink + " 强化到 +" + std::to_string(record.level) + " 级！";
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, announcement);
        }

        return true;
    }
    else
    {
        // 强化失败
        record.lastEnhanceTime = static_cast<uint32>(GameTime::GetGameTime().count());

        // 根据失败处理类型处理
#ifdef MODULE_ITEM_ATTRIBUTES
        switch (failureType)
        {
            case 0: // 不变
                ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，等级保持不变");
                break;
            case 1: // 等级清0
                if (record.level > 0)
                {
                    // 重置等级并清空强化属性
                    record.level = 0;
                    record.statValues.clear();
                    ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，强化等级清零");
                }
                break;
            case 2: // 摧毁
                if (!_failureProtectionEnabled)
                {
                    // 先从物品属性系统中移除强化属性
                    SyncEnhancementAttributesToItemAttributes(player, item, "", oldStatValuesSnapshot);

                    player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
                    DeleteEnhancementRecord(itemGuid);
                    ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，物品已摧毁");
                    return false;
                }
                else
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，但受到保护未被摧毁");
                }
                break;
        }

        // 保存记录
        SaveEnhancementRecord(record);

        // 同步失败后的状态到物品属性系统（类型0和1）
        SyncEnhancementAttributesToItemAttributes(player, item, record.statValues, oldStatValuesSnapshot);
#else
        switch (failureType)
        {
            case 0: // 不变
                ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，等级保持不变");
                break;
            case 1: // 等级清0
                if (record.level > 0)
                {
                    // 移除之前的强化效果
                    RemoveOfficialItemEnhancement(player, item);
                    record.level = 0;
                    ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，强化等级清零");
                }
                break;
            case 2: // 摧毁
                if (!_failureProtectionEnabled)
                {
                    // 移除强化效果
                    RemoveOfficialItemEnhancement(player, item);
                    player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
                    DeleteEnhancementRecord(itemGuid);
                    ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，物品已摧毁");
                    return false;
                }
                else
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败，但受到保护未被摧毁");
                }
                break;
        }

        // 保存记录
        SaveEnhancementRecord(record);
#endif

        // 发送失败消息
        ChatHandler(player->GetSession()).PSendSysMessage("物品强化失败");

        return false;
    }
}

// 为鉴定系统初始化强化（直接创建等级1，不依赖随机判定，并按鉴定模板约束条目数量与数值范围）
bool ItemEnhancementMgr::InitializeEnhancementForIdentification(Player* player, Item* item, uint32 group,
    uint32 minAttrCount, uint32 maxAttrCount, uint32 minAttrValue, uint32 maxAttrValue)
{
    if (!CanEnhanceItem(player, item))
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord record;
    bool isNewRecord = false;

    // 获取或创建强化记录
    EnhancementRecord const* existingRecord = GetEnhancementRecord(itemGuid);
    if (existingRecord)
    {
        LOG_INFO("module.itemenhancement", "[鉴定初始化] 物品已有强化记录: itemGuid={}, level={}, group={}，跳过初始化",
            itemGuid, existingRecord->level, existingRecord->group);
        // 物品已有强化记录，不能重复初始化
        return false;
    }
    else
    {
        record.itemGuid = itemGuid;
        record.ownerGuid = player->GetGUID().GetCounter();
        record.group = group;
        record.level = 0; // 从0开始
        record.qualityLevel = 0;
        record.qualityUpgradeCount = 0;
        record.enhancementCount = 0;
        record.failureCount = 0;
        record.lastEnhanceTime = 0;
        isNewRecord = true;
    }

    // 对于鉴定初始化，直接升级到等级1（跳过随机判定）
    uint32 nextLevel = 1; // 直接设为1，不使用 record.level + 1

    // 优先使用传入的组配置，如果该组不存在对应等级，则回退到组0，保证1级必然有模板
    uint32 usedGroup = group;
    EnhancementTemplate const* enhTemplate = GetEnhancementTemplateByGroupAndLevel(usedGroup, nextLevel);
    if (!enhTemplate)
    {
        if (usedGroup != 0)
        {
            enhTemplate = GetEnhancementTemplateByGroupAndLevel(0, nextLevel);
            if (enhTemplate)
            {
                usedGroup = 0;
                if (_debugMode)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage(
                        "强化系统：找不到组{}等级{}的配置，已回退使用组0配置", group, nextLevel);
                }

                LOG_INFO("module.itemenhancement", "[鉴定初始化] 找不到组{}等级{}模板，已回退到组0模板", group, nextLevel);
            }
        }

        if (!enhTemplate)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("错误：找不到组{}等级{}的强化配置", group, nextLevel);
            LOG_ERROR("module.itemenhancement", "[鉴定初始化] 找不到任何可用强化模板: 请求组={}, 等级={}", group, nextLevel);
            return false;
        }
    }

    // 鉴定初始化不检查需求条件（因为鉴定本身可能已经检查过）
    // 直接生成等级1的属性（视为初始累计属性），使用与后续强化相同的增量生成逻辑

    std::string initialStats = GenerateIncrementalEnhancementStats(item, nextLevel, usedGroup);

    // 兼容性兜底：如果当前组生成不到任何属性，尝试使用组0模板
    if (initialStats.empty() && usedGroup != 0)
    {
        std::string fallbackStats = GenerateIncrementalEnhancementStats(item, nextLevel, 0);
        if (fallbackStats.empty())
        {
            // 最后再尝试旧的按组0生成方式
            fallbackStats = GenerateEnhancementStats(item, nextLevel);
        }

        if (!fallbackStats.empty())
        {
            initialStats = fallbackStats;
            usedGroup = 0; // 实际使用组0的配置
            LOG_INFO("module.itemenhancement", "[鉴定初始化] 组{}等级{}未生成属性，已改用组0配置生成1级属性，最终属性='{}'",
                group, nextLevel, initialStats);
        }
    }

    // 记录最终实际使用的组号
    record.group = usedGroup;

    if (!initialStats.empty())
    {
        // 如果配置了约束，则对生成的属性做一次性裁剪
        if (maxAttrCount > 0 || minAttrCount > 0 || minAttrValue > 0 || maxAttrValue > 0)
        {
            std::map<uint32, int32> statsMap = ParseStatValues(initialStats);

            if (!statsMap.empty())
            {
                // 转成vector以便随机抽取
                std::vector<std::pair<uint32, int32>> stats(statsMap.begin(), statsMap.end());
                uint32 availableCount = static_cast<uint32>(stats.size());
                uint32 targetCount = availableCount;

                // 1) 随机决定本次要保留的强化属性条目数量
                if (minAttrCount > 0 || maxAttrCount > 0)
                {
                    uint32 minCount = minAttrCount;
                    uint32 maxCount = maxAttrCount ? maxAttrCount : availableCount;

                    if (minCount == 0)
                        minCount = 1;

                    if (minCount > maxCount)
                        std::swap(minCount, maxCount);

                    targetCount = urand(minCount, maxCount);
                    if (targetCount > availableCount)
                        targetCount = availableCount;
                }

                // 当可用条目数大于目标条目数时，从中随机抽取 targetCount 条
                if (availableCount > targetCount && targetCount > 0)
                {
                    for (uint32 i = 0; i < targetCount; ++i)
                    {
                        uint32 j = urand(i, availableCount - 1);
                        std::swap(stats[i], stats[j]);
                    }
                    stats.resize(targetCount);
                }

                // 2) 限制数值范围
                bool hasRange = (minAttrValue > 0 || maxAttrValue > 0);
                if (hasRange)
                {
                    if (maxAttrValue > 0 && minAttrValue > maxAttrValue)
                        std::swap(minAttrValue, maxAttrValue);

                    for (auto& kv : stats)
                    {
                        int32 v = kv.second;

                        if (minAttrValue > 0 && v < static_cast<int32>(minAttrValue))
                            v = static_cast<int32>(minAttrValue);

                        if (maxAttrValue > 0 && v > static_cast<int32>(maxAttrValue))
                            v = static_cast<int32>(maxAttrValue);

                        kv.second = v;
                    }
                }

                // 3) 重新格式化为字符串
                std::ostringstream oss;
                for (size_t i = 0; i < stats.size(); ++i)
                {
                    if (i > 0)
                        oss << ",";
                    oss << stats[i].first << " " << stats[i].second;
                }

                initialStats = oss.str();

                if (_debugMode)
                {
                    LOG_DEBUG("module.itemenhancement", "[鉴定初始化] 约束后强化属性='{}' (minCount={}, maxCount={}, minValue={}, maxValue={})",
                              initialStats, minAttrCount, maxAttrCount, minAttrValue, maxAttrValue);
                }
            }
        }

        record.statValues = initialStats;
    }
    else
    {
        // 仍然为空时，说明该物品在模板中没有任何可用基础属性（StatsCount=0 等）
        // 为了保证1级一定有数据，这里构造一个默认属性
        ItemTemplate const* proto = item->GetTemplate();
        if (proto)
        {
            // 选择一个默认属性类型：武器用攻击强度，护甲用耐力
            uint32 defaultStatType = (proto->Class == ITEM_CLASS_WEAPON) ? ITEM_MOD_ATTACK_POWER : ITEM_MOD_STAMINA;

            // 根据模板的数值区间选一个值，如果区间为0则给一个固定值1
            int32 minValue = enhTemplate->statValue1;
            int32 maxValue = enhTemplate->statValue2;
            if (maxValue < minValue)
                maxValue = minValue;

            int32 value = 0;
            if (minValue == 0 && maxValue == 0)
                value = 1;
            else if (minValue == maxValue)
                value = minValue;
            else
                value = irand(minValue, maxValue);

            record.statValues = std::to_string(defaultStatType) + " " + std::to_string(value);
            LOG_WARN("module.itemenhancement",
                "[鉴定初始化] itemId={} 无可用基础属性，使用默认属性生成1级强化: statType={}, value={}",
                item->GetEntry(), defaultStatType, value);
        }
        else
        {
            LOG_WARN("module.itemenhancement", "[鉴定初始化] 未能为 itemId={} 生成任何1级属性，statValues 仍然为空", item->GetEntry());
        }
    }

    // 直接设置为等级1（成功）
    record.level = nextLevel;
    record.lastEnhanceTime = static_cast<uint32>(GameTime::GetGameTime().count());
    record.enhancementCount = 1; // 记录强化次数

    // 保存记录到数据库
    SaveEnhancementRecord(record);

#ifdef MODULE_ITEM_ATTRIBUTES
    // 初次鉴定时，将强化属性同步到物品属性系统
    SyncEnhancementAttributesToItemAttributes(player, item, record.statValues, "");
#else
    // 如果物品已装备，立即应用效果
    if (item->IsEquipped())
    {
        player->SetVisibleItemSlot(item->GetSlot(), item);
        ApplyOfficialItemEnhancement(player, item, record.level);
    }
#endif

    // 从数据库重新获取最新的强化数据发送给客户端
    EnhancementRecord const* updatedRecord = GetEnhancementRecord(itemGuid);
    if (updatedRecord)
    {
        SendEnhancementDataToClient(player, item, *updatedRecord);
    }

    return true;
}

void ItemEnhancementMgr::ApplyEnhancementEffects(Player* player, Item* item)
{
    // 这个函数可以在需要时实现
}

void ItemEnhancementMgr::RemoveEnhancementEffects(Player* player, Item* item)
{
    // 这个函数可以在需要时实现
}

void ItemEnhancementMgr::SendEnhancementInfo(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord const* record = GetEnhancementRecord(itemGuid);

    if (!record)
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("该物品未强化");
        return;
    }

    std::string info = "物品强化信息:\n";
    info += "强化等级: +" + std::to_string(record->level) + "\n";

    // ChatHandler(player->GetSession()).PSendSysMessage("{}", info);
}

std::string ItemEnhancementMgr::GetEnhancementVisual(uint32 level)
{
    // 这个函数可以在需要时实现
    return "";
}

bool ItemEnhancementMgr::IsOfficialItem(uint32 itemId) const
{
    // 判断是否为官方装备（0组）
    // 官方装备通常ID在特定范围内，这里需要根据你的服务器配置调整

    // 方法1：通过ID范围判断（经典魔兽世界官方装备ID通常 < 100000）
    if (itemId < 100000)
        return true;

    // 方法2：通过数据库查询判断（如果有专门的表记录官方装备）
    // QueryResult result = WorldDatabase.Query("SELECT 1 FROM official_items WHERE item_id = {}", itemId);
    // return result != nullptr;

    // 方法3：通过特定ID列表判断
    // 可以在这里添加特定的官方装备ID范围或列表

    return false;
}

void ItemEnhancementMgr::ApplyOfficialItemEnhancement(Player* player, Item* item, uint32 level)
{
    if (!player || !item || level == 0)
        return;

    // 关键检查：确保装备已穿戴
    if (!item->IsEquipped())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备未穿戴，跳过强化效果应用");
        return;
    }

    // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：开始应用官方装备强化效果 +{} 级", level);

    // 获取强化记录中的属性值
    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
    if (!record || record->statValues.empty())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：警告 - 找不到强化记录或属性值为空");
        return;
    }

    // 解析属性值
    std::map<uint32, int32> stats = ParseStatValues(record->statValues);

    // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：应用强化属性: {}", record->statValues);

    // 直接给玩家添加属性修正值
    for (const auto& stat : stats)
    {
        uint32 statType = stat.first;
        int32 statValue = stat.second;

        if (statValue > 0)
        {
            // 应用属性修正到玩家身上
            ApplyStatModifier(player, statType, statValue, true);
            // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：应用属性 {} +{}", GetStatTypeName(statType), statValue);

            // ChatHandler(player->GetSession()).PSendSysMessage("应用属性: {} (类型{}) 值:+{}",
            //     GetStatTypeName(statType), statType, statValue);
        }
    }

    // 跟踪已应用的强化效果，确保卸载时能正确移除
    TrackAppliedEnhancement(player, item, record->statValues);

    // 更新玩家属性
    // 【性能优化】登录加载阶段不做全量刷新，交给 OnPlayerLogin 统一刷新一次
    // 原因：登录时每件装备都会触发 OnPlayerEquip -> ApplyOfficialItemEnhancement，
    //       如果每次都刷新属性，9件装备 × 60ms = 540ms 浪费在重复刷新上
    // 优化后：登录阶段跳过刷新，OnPlayerLogin 最后统一刷新一次，节省 ~480ms
    if (!player->isBeingLoaded())
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }

    // ChatHandler(player->GetSession()).PSendSysMessage("强化等级 {} 已应用，属性加成已生效", level);
}



// 注意：不再使用附魔系统存储强化信息，直接修改装备属性

std::string ItemEnhancementMgr::GetStatTypeName(uint32 statType) const
{
    switch (statType)
    {
        case ITEM_MOD_MANA: return "法力值";                                    // 0
        case ITEM_MOD_HEALTH: return "生命值";                                  // 1
        case ITEM_MOD_AGILITY: return "敏捷";                                   // 3
        case ITEM_MOD_STRENGTH: return "力量";                                  // 4
        case ITEM_MOD_INTELLECT: return "智力";                                 // 5
        case ITEM_MOD_SPIRIT: return "精神";                                    // 6
        case ITEM_MOD_STAMINA: return "耐力";                                   // 7
        case ITEM_MOD_DEFENSE_SKILL_RATING: return "防御等级";                  // 12
        case ITEM_MOD_DODGE_RATING: return "躲闪等级";                          // 13
        case ITEM_MOD_PARRY_RATING: return "招架等级";                          // 14
        case ITEM_MOD_BLOCK_RATING: return "格挡等级";                          // 15
        case ITEM_MOD_HIT_MELEE_RATING: return "命中等级(近战)";                // 16
        case ITEM_MOD_HIT_RANGED_RATING: return "命中等级(远程)";               // 17
        case ITEM_MOD_HIT_SPELL_RATING: return "命中等级(法术)";                // 18
        case ITEM_MOD_CRIT_MELEE_RATING: return "暴击等级(近战)";               // 19
        case ITEM_MOD_CRIT_RANGED_RATING: return "暴击等级(远程)";              // 20
        case ITEM_MOD_CRIT_SPELL_RATING: return "暴击等级(法术)";               // 21
        case ITEM_MOD_HIT_TAKEN_MELEE_RATING: return "受击等级(近战)";           // 22
        case ITEM_MOD_HIT_TAKEN_RANGED_RATING: return "受击等级(远程)";          // 23
        case ITEM_MOD_HIT_TAKEN_SPELL_RATING: return "受击等级(法术)";           // 24
        case ITEM_MOD_CRIT_TAKEN_MELEE_RATING: return "受暴击等级(近战)";        // 25
        case ITEM_MOD_CRIT_TAKEN_RANGED_RATING: return "受暴击等级(远程)";       // 26
        case ITEM_MOD_CRIT_TAKEN_SPELL_RATING: return "受暴击等级(法术)";        // 27
        case ITEM_MOD_HASTE_MELEE_RATING: return "急速等级(近战)";               // 28
        case ITEM_MOD_HASTE_RANGED_RATING: return "急速等级(远程)";              // 29
        case ITEM_MOD_HASTE_SPELL_RATING: return "急速等级(法术)";               // 30
        case ITEM_MOD_HIT_RATING: return "命中等级";                            // 31
        case ITEM_MOD_CRIT_RATING: return "暴击等级";                           // 32
        case ITEM_MOD_HIT_TAKEN_RATING: return "受击等级";                      // 33
        case ITEM_MOD_CRIT_TAKEN_RATING: return "受暴击等级";                   // 34
        case ITEM_MOD_RESILIENCE_RATING: return "韧性等级";                     // 35
        case ITEM_MOD_HASTE_RATING: return "急速等级";                          // 36
        case ITEM_MOD_EXPERTISE_RATING: return "精准等级";                      // 37
        case ITEM_MOD_ATTACK_POWER: return "攻击强度";                          // 38
        case ITEM_MOD_RANGED_ATTACK_POWER: return "远程攻击强度";               // 39
        case ITEM_MOD_SPELL_HEALING_DONE: return "法术治疗";                    // 41 (deprecated)
        case ITEM_MOD_SPELL_DAMAGE_DONE: return "法术伤害";                     // 42 (deprecated)
        case ITEM_MOD_MANA_REGENERATION: return "法力回复";                     // 43
        case ITEM_MOD_ARMOR_PENETRATION_RATING: return "护甲穿透等级";          // 44
        case ITEM_MOD_SPELL_POWER: return "法术强度";                           // 45
        case ITEM_MOD_HEALTH_REGEN: return "生命回复";                          // 46
        case ITEM_MOD_SPELL_PENETRATION: return "法术穿透";                     // 47
        case ITEM_MOD_BLOCK_VALUE: return "格挡值";                             // 48
        default: return "未知属性";
    }
}

// 生成强化增量属性值字符串（本次强化增加的属性，不是累计属性）
std::string ItemEnhancementMgr::GenerateEnhancementStats(Item* item, uint32 level)
{
    if (!item || level == 0)
    {
        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：item为空或level为0");
        return "";
    }

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：proto为空");
        return "";
    }

    std::string statValues = "";
    bool first = true;



    // 查找指定等级的强化模板
    EnhancementTemplate const* enhTemplate = GetEnhancementTemplateByGroupAndLevel(0, level);
    if (!enhTemplate)
    {
        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：警告：找不到组0等级{}的配置", level);
        return "";
    }

    // 根据分配类型决定强化值生成方式
    int32 fixedEnhancementValue = 0; // 固定模式下所有属性使用的相同值
    bool useFixedValue = (enhTemplate->distributionType == 0);

    if (useFixedValue)
    {
        // 固定值模式：先随机生成一个值，所有属性都使用这个值
        int32 minValue = enhTemplate->statValue1;
        int32 maxValue = enhTemplate->statValue2;
        if (maxValue > minValue)
        {
            fixedEnhancementValue = irand(minValue, maxValue);
        }
        else
        {
            fixedEnhancementValue = minValue;
        }

    }
    else
    {
        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：随机值模式，每个属性独立随机");
    }

    // 遍历物品的所有原始属性，对每个属性进行强化
    for (uint8 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = proto->ItemStat[i].ItemStatType;
        int32 originalValue = proto->ItemStat[i].ItemStatValue;

        // 强制显示调试信息
        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：属性{}: 类型={}, 值={}",
        //     i, statType, originalValue);

        if (originalValue <= 0)
        {
            // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：跳过属性{}: 值为{}", i, originalValue);
            continue; // 跳过没有数值的属性
        }

        // 注意：这个函数生成的是本次强化的增量属性加成
        // 不是累计属性，而是本次强化新增加的属性值

        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：计算等级{} (类型{}) 的强化加成", level, statType);

        int32 enhancementValue = 0;

        if (useFixedValue)
        {
            // 固定值模式：所有属性使用相同的预先生成的值
            enhancementValue = fixedEnhancementValue;
        }
        else
        {
            // 随机值模式：每个属性独立随机
            int32 minValue = enhTemplate->statValue1;
            int32 maxValue = enhTemplate->statValue2;
            if (maxValue > minValue)
            {
                enhancementValue = irand(minValue, maxValue);
                // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：随机模式，属性{}独立随机[{}-{}]，生成值={}",
                //     i, minValue, maxValue, enhancementValue);
            }
            else
            {
                enhancementValue = minValue;
            }
        }

        // ChatHandler(item->GetOwner()->GetSession()).PSendSysMessage("调试：属性{} 等级{} 强化加成: {} ({}模式)",
        //     i, level, enhancementValue, useFixedValue ? "固定值" : "随机值");

        // 只有当有强化值时才添加到字符串
        if (enhancementValue > 0)
        {
            if (!first)
                statValues += ",";

            statValues += std::to_string(statType) + " " + std::to_string(enhancementValue);
            first = false;


        }
    }

    // 移除字符串末尾的空格（如果有的话）
    while (!statValues.empty() && statValues.back() == ' ')
    {
        statValues.pop_back();
    }


    return statValues;
}

// 生成本次强化的增量属性值字符串（只计算本次强化增加的属性，不是累计属性）
std::string ItemEnhancementMgr::GenerateIncrementalEnhancementStats(Item* item, uint32 level, uint32 group)
{
    if (!item || level == 0)
    {
        LOG_DEBUG("module.itemenhancement", "[增量属性] 参数无效: item={}, level={}", (void*)item, level);
        return "";
    }

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        LOG_DEBUG("module.itemenhancement", "[增量属性] itemId={} 模板为空", item->GetEntry());
        return "";
    }

    LOG_DEBUG("module.itemenhancement", "[增量属性] 开始生成: itemId={}, level={}, group={}, StatsCount={}",
        item->GetEntry(), level, group, uint32(proto->StatsCount));

    std::string statValues = "";
    bool first = true;

    // 查找指定组和等级的强化模板
    EnhancementTemplate const* enhTemplate = GetEnhancementTemplateByGroupAndLevel(group, level);
    if (!enhTemplate)
    {
        LOG_DEBUG("module.itemenhancement", "[增量属性] 未找到模板: group={}, level={}", group, level);
        return "";
    }

    LOG_DEBUG("module.itemenhancement", "[增量属性] 使用模板: id={}, itemStatGroup={}, val1={}, val2={}, distType={}",
        enhTemplate->id, enhTemplate->itemStatGroup, enhTemplate->statValue1, enhTemplate->statValue2, enhTemplate->distributionType);

    // 根据分配类型决定强化值生成方式
    int32 fixedEnhancementValue = 0; // 固定模式下所有属性使用的相同值
    bool useFixedValue = (enhTemplate->distributionType == 0);

    if (useFixedValue)
    {
        // 固定值模式：先随机生成一个值，所有属性都使用这个值
        int32 minValue = enhTemplate->statValue1;
        int32 maxValue = enhTemplate->statValue2;
        if (maxValue > minValue)
        {
            fixedEnhancementValue = irand(minValue, maxValue);
        }
        else
        {
            fixedEnhancementValue = minValue;
        }
    }

#ifdef MODULE_ITEM_ATTRIBUTES
    // 如果mod-item-attributes可用，则优先按物品属性_模板_组生成强化属性
    {
        std::vector<ItemAttributeTemplate const*> attrTemplates =
            sItemAttributesLoader->GetItemAttributeTemplatesByGroup(enhTemplate->itemStatGroup);

        if (!attrTemplates.empty())
        {
            LOG_DEBUG("module.itemenhancement", "[增量属性] 使用物品属性模板组{}，共{}条属性模板",
                enhTemplate->itemStatGroup, uint32(attrTemplates.size()));

            for (size_t i = 0; i < attrTemplates.size(); ++i)
            {
                ItemAttributeTemplate const* attr = attrTemplates[i];
                if (!attr)
                    continue;

                uint32 statType = attr->attributeType; // 对应 ITEM_MOD_* 类型

                int32 enhancementValue = 0;
                if (useFixedValue)
                {
                    enhancementValue = fixedEnhancementValue;
                }
                else
                {
                    int32 minValue = enhTemplate->statValue1;
                    int32 maxValue = enhTemplate->statValue2;
                    if (maxValue > minValue)
                        enhancementValue = irand(minValue, maxValue);
                    else
                        enhancementValue = minValue;
                }

                LOG_DEBUG("module.itemenhancement", "[增量属性] 组{}模板属性{}: statType={}, 增量值={}",
                    enhTemplate->itemStatGroup, attr->id, statType, enhancementValue);

                if (enhancementValue > 0)
                {
                    if (!first)
                        statValues += ",";

                    statValues += std::to_string(statType) + " " + std::to_string(enhancementValue);
                    first = false;
                }
            }

            LOG_DEBUG("module.itemenhancement", "[增量属性] 使用属性模板组{}生成结果: '{}'",
                enhTemplate->itemStatGroup, statValues);

            return statValues;
        }
        else
        {
            LOG_DEBUG("module.itemenhancement", "[增量属性] 属性模板组{}没有任何模板，回退使用物品基础属性", enhTemplate->itemStatGroup);
        }
    }
#endif

    // 默认逻辑：遍历物品的所有原始属性，对每个属性进行强化
    for (uint8 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = proto->ItemStat[i].ItemStatType;
        int32 originalValue = proto->ItemStat[i].ItemStatValue;

        if (originalValue <= 0)
        {
            LOG_DEBUG("module.itemenhancement", "[增量属性] 跳过属性槽{}: statType={}, 原始值={}<=0", i, statType, originalValue);
            continue; // 跳过没有数值的属性
        }

        // 注意：这个函数生成的是本次强化的增量属性加成
        // 不是累计属性，而是本次强化新增加的属性值

        int32 enhancementValue = 0;

        if (useFixedValue)
        {
            // 固定值模式：所有属性使用相同的预先生成的值
            enhancementValue = fixedEnhancementValue;
        }
        else
        {
            // 随机值模式：每个属性独立随机
            int32 minValue = enhTemplate->statValue1;
            int32 maxValue = enhTemplate->statValue2;
            if (maxValue > minValue)
            {
                enhancementValue = irand(minValue, maxValue);
            }
            else
            {
                enhancementValue = minValue;
            }
        }

        LOG_DEBUG("module.itemenhancement", "[增量属性] 槽{}: statType={}, 原始值={}, 增量值={}",
            i, statType, originalValue, enhancementValue);

        // 只有当有强化值时才添加到字符串
        if (enhancementValue > 0)
        {
            if (!first)
                statValues += ",";

            statValues += std::to_string(statType) + " " + std::to_string(enhancementValue);
            first = false;
        }
    }

    // 移除字符串末尾的空格（如果有的话）
    while (!statValues.empty() && statValues.back() == ' ')
    {
        statValues.pop_back();
    }

    LOG_DEBUG("module.itemenhancement", "[增量属性] 生成结果: '{}'", statValues);
    return statValues;
}

// 解析属性值字符串
std::map<uint32, int32> ItemEnhancementMgr::ParseStatValues(const std::string& statValues) const
{
    std::map<uint32, int32> stats;

    if (statValues.empty())
        return stats;

    // 按逗号分割
    std::istringstream ss(statValues);
    std::string token;

    while (std::getline(ss, token, ','))
    {
        // 按空格分割属性类型和值
        std::istringstream tokenStream(token);
        std::string statTypeStr, statValueStr;

        if (std::getline(tokenStream, statTypeStr, ' ') && std::getline(tokenStream, statValueStr))
        {
            try
            {
                uint32 statType = std::stoul(statTypeStr);
                int32 statValue = std::stol(statValueStr);
                stats[statType] = statValue;
            }
            catch (const std::exception& e)
            {
                if (_debugMode)
                {
                    LOG_ERROR("server.loading", "解析属性值时发生错误: {}", e.what());
                }
            }
        }
    }

    return stats;
}

void ItemEnhancementMgr::SyncEnhancementAttributesToItemAttributes(Player* player, Item* item,
    const std::string& newStatValues, const std::string& oldStatValues)
{
#ifdef MODULE_ITEM_ATTRIBUTES
    if (!item)
        return;

    uint64 itemGuid = item->GetGUID().GetCounter();
    uint32 itemId = item->GetEntry();

    std::map<uint32, int32> newStats = ParseStatValues(newStatValues);
    std::map<uint32, int32> oldStats = ParseStatValues(oldStatValues);

    if (newStats.empty() && oldStats.empty())
        return;

    ItemAttributesDBHelper::ItemAttributeData* existing = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
    ItemAttributesDBHelper::ItemAttributeData data;

    if (existing)
    {
        data = *existing;
        delete existing;
    }
    else
    {
        data.itemGuid = itemGuid;
        data.itemId = itemId;
    }

    auto& ids = data.additionalAttributeIds;
    auto& values = data.additionalAttributeValues;

    std::set<uint32> keys;
    for (auto const& kv : newStats)
        keys.insert(kv.first);
    for (auto const& kv : oldStats)
        keys.insert(kv.first);

    for (uint32 statType : keys)
    {
        int32 oldValue = 0;
        auto itOld = oldStats.find(statType);
        if (itOld != oldStats.end())
            oldValue = itOld->second;

        int32 newValue = 0;
        auto itNew = newStats.find(statType);
        if (itNew != newStats.end())
            newValue = itNew->second;

        int32 delta = newValue - oldValue;
        if (delta == 0)
            continue;

        bool found = false;
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (ids[i] == statType)
            {
                values[i] += delta;
                if (values[i] <= 0)
                {
                    ids.erase(ids.begin() + i);
                    values.erase(values.begin() + i);
                }
                found = true;
                break;
            }
        }

        if (!found && delta > 0)
        {
            ids.push_back(statType);
            values.push_back(delta);
        }
    }

    ItemAttributesDBHelper::SaveItemAttributes(data);

    // 【关键修复】参考 ItemAttributesEvents 的实现模式：
    // 1. 只负责数据库同步，不直接调用 Apply/Remove
    // 2. 如果装备已穿戴，需要重新应用属性，但必须安全地进行
    // 3. 添加完整的安全检查，避免在加载阶段或无效状态下操作
    // 4. 【新增】添加对 item 的额外检查，避免在检查后item失效
    if (player && item)
    {
        // 安全检查：确保玩家已完全加载且在世界中
        // 必须在所有操作之前进行完整检查
        if (!player->isBeingLoaded() && player->IsInWorld() && player->GetSession())
        {
            // 【关键】在调用 IsEquipped() 之前再次验证 item 的有效性
            // 因为在并发环境下，item 可能在检查和使用之间被删除
            if (item->IsEquipped() && sItemAttributesEffects)
            {
                // 重新应用该物品的所有属性（包括强化属性）
                sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                sItemAttributesEffects->ApplyItemAttributeEffects(player, item);

                // 刷新玩家属性
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateMaxHealth();
                player->UpdateMaxPower(POWER_MANA);
            }
        }
    }
#else
    (void)player;
    (void)item;
    (void)newStatValues;
    (void)oldStatValues;
#endif
}

// 注意：不再需要单独应用/移除属性，因为属性已经直接修改到装备上（或由物品属性系统统一处理）

// 清理指定装备槽位的所有强化效果
void ItemEnhancementMgr::ClearSlotEnhancements(Player* player, uint8 slot)
{
    if (!player)
        return;

    // ChatHandler(player->GetSession()).PSendSysMessage("DEBUG: 开始清理槽位 {} 的强化效果", slot);

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerItr = _appliedEnhancements.find(playerGuid);
    if (playerItr == _appliedEnhancements.end())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("DEBUG: 玩家没有任何跟踪的强化效果");
        return;
    }

    // 查找并移除该槽位相关的所有强化效果
    std::vector<uint32> itemsToRemove;
    bool hasRemovedEffects = false;

    for (const auto& itemEntry : playerItr->second)
    {
        uint32 trackedItemGuid = itemEntry.first;
        std::string appliedStats = itemEntry.second;

        // 检查这个跟踪的物品是否在指定槽位或者已经不在装备中
        bool shouldRemove = false;

        // 检查当前槽位的装备
        Item* currentSlotItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (currentSlotItem && currentSlotItem->GetGUID().GetCounter() == trackedItemGuid)
        {
            // 这是当前槽位的装备，需要移除其强化效果
            shouldRemove = true;
        }
        else
        {
            // 检查这个跟踪的物品是否还在任何装备槽中
            bool stillEquipped = false;
            for (uint8 checkSlot = EQUIPMENT_SLOT_START; checkSlot < EQUIPMENT_SLOT_END; ++checkSlot)
            {
                Item* checkItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, checkSlot);
                if (checkItem && checkItem->GetGUID().GetCounter() == trackedItemGuid)
                {
                    stillEquipped = true;
                    break;
                }
            }

            // 如果物品不再装备，也需要移除
            if (!stillEquipped)
            {
                shouldRemove = true;
            }
        }

        if (shouldRemove)
        {
            // 移除属性效果
            std::map<uint32, int32> stats = ParseStatValues(appliedStats);
            for (const auto& stat : stats)
            {
                if (stat.second > 0)
                {
                    ApplyStatModifier(player, stat.first, stat.second, false);
                }
            }

            itemsToRemove.push_back(trackedItemGuid);
            hasRemovedEffects = true;
            // ChatHandler(player->GetSession()).PSendSysMessage("清理槽位 {} 相关的强化效果，物品GUID={}", slot, trackedItemGuid);
        }
    }

    // 移除跟踪记录
    for (uint32 itemGuid : itemsToRemove)
    {
        playerItr->second.erase(itemGuid);
    }

    if (playerItr->second.empty())
    {
        _appliedEnhancements.erase(playerItr);
    }

    // 如果移除了效果，更新玩家属性
    // 【性能优化】登录加载阶段不做全量刷新，交给 OnPlayerLogin 统一刷新一次
    if (hasRemovedEffects && !player->isBeingLoaded())
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }
}

// 装备穿戴时的处理
void ItemEnhancementMgr::OnPlayerEquipItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    // 关键检查：确保物品确实已装备
    if (!item->IsEquipped())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：物品未装备，跳过强化效果应用");
        return;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 从数据库读取强化数据
    EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
    if (record && record->level > 0 && !record->statValues.empty())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备穿戴 - 从数据库读取强化数据 +{} 级", record->level);

        // 应用强化加成给玩家
        std::map<uint32, int32> stats = ParseStatValues(record->statValues);
        for (const auto& stat : stats)
        {
            if (stat.second > 0)
            {
                ApplyStatModifier(player, stat.first, stat.second, true);
                // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：应用属性 {} +{}", GetStatTypeName(stat.first), stat.second);
            }
        }

        // 跟踪新应用的强化效果
        TrackAppliedEnhancement(player, item, record->statValues);

        // 更新玩家属性
        // 【性能优化】登录加载阶段不做全量刷新
        if (!player->isBeingLoaded())
        {
            player->UpdateAllStats();
            player->UpdateAttackPowerAndDamage();
            player->UpdateAttackPowerAndDamage(true);
            player->UpdateMaxHealth();
            player->UpdateMaxPower(POWER_MANA);
        }

        // 发送强化数据给客户端UI插件
        SendEnhancementDataToClient(player, item, *record);

        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备穿戴 - 应用强化效果 +{} 级完成", record->level);
    }
    else
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备穿戴 - 该装备无强化效果");
    }
}

// 装备脱下时的处理
void ItemEnhancementMgr::OnPlayerUnequipItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 检查是否有已应用的强化效果
    std::string appliedStats = GetAppliedEnhancement(player, item);
    if (appliedStats.empty())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备脱下 - 该装备没有已应用的强化效果");
        return;
    }

    // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备脱下 - 开始移除强化效果");

    // 解析并移除已应用的属性值
    std::map<uint32, int32> stats = ParseStatValues(appliedStats);
    for (const auto& stat : stats)
    {
        if (stat.second > 0)
        {
            ApplyStatModifier(player, stat.first, stat.second, false);
            // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：移除属性 {} -{}", GetStatTypeName(stat.first), stat.second);
        }
    }

    // 取消跟踪已应用的强化效果
    UntrackAppliedEnhancement(player, item);

    // 更新玩家属性
    // 【性能优化】登录加载阶段不做全量刷新
    if (!player->isBeingLoaded())
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }

    // 获取强化等级用于显示
    EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
    uint32 level = record ? record->level : 0;

    // ChatHandler(player->GetSession()).PSendSysMessage("强化系统：装备脱下 - 已移除强化效果 +{} 级", level);
}

void ItemEnhancementMgr::RemoveOfficialItemEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return;

    // ChatHandler(player->GetSession()).PSendSysMessage("开始移除官方装备强化效果");

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 首先检查是否有已应用的强化效果（通过跟踪系统）
    std::string appliedStats = GetAppliedEnhancement(player, item);
    if (!appliedStats.empty())
    {
        // 如果有跟踪的应用效果，优先使用跟踪的数据移除
        // ChatHandler(player->GetSession()).PSendSysMessage("使用跟踪数据移除强化效果: {}", appliedStats);

        std::map<uint32, int32> stats = ParseStatValues(appliedStats);
        for (const auto& stat : stats)
        {
            if (stat.second > 0)
            {
                ApplyStatModifier(player, stat.first, stat.second, false);
            }
        }

        // 取消跟踪
        UntrackAppliedEnhancement(player, item);
    }
    else
    {
        // 如果没有跟踪数据，使用数据库记录移除
        EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
        if (!record || record->level == 0 || record->statValues.empty())
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("没有找到强化记录或强化等级为0");
            return;
        }

        // 解析属性值
        std::map<uint32, int32> stats = ParseStatValues(record->statValues);

        // ChatHandler(player->GetSession()).PSendSysMessage("使用数据库记录移除强化属性: {}", record->statValues);

        // 移除玩家身上的属性修正值
        for (const auto& stat : stats)
        {
            uint32 statType = stat.first;
            int32 statValue = stat.second;

            if (statValue > 0)
            {
                // 移除属性修正
                ApplyStatModifier(player, statType, statValue, false);

                // ChatHandler(player->GetSession()).PSendSysMessage("移除属性: {} (类型{}) 值:-{}",
                //     GetStatTypeName(statType), statType, statValue);
            }
        }
    }

    // 更新玩家属性
    // 【性能优化】登录加载阶段不做全量刷新
    if (!player->isBeingLoaded())
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }

    // ChatHandler(player->GetSession()).PSendSysMessage("强化效果已移除");
}

// 获取强化装备的tooltip显示信息
std::string ItemEnhancementMgr::GetEnhancementTooltip(Item* item) const
{
    if (!item)
        return "";

    // 获取强化记录
    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
    if (!record || record->level == 0 || record->statValues.empty())
        return "";

    std::string tooltip = "";

    // 添加强化等级显示
    tooltip += "|cff00ff00强化等级: +" + std::to_string(record->level) + "|r\n";

    // 解析并显示强化属性
    std::map<uint32, int32> stats = ParseStatValues(record->statValues);
    if (!stats.empty())
    {
        tooltip += "|cff00ff00强化属性:|r\n";
        for (const auto& stat : stats)
        {
            if (stat.second > 0)
            {
                std::string statName = GetStatTypeName(stat.first);
                tooltip += "|cff00ff00  +" + std::to_string(stat.second) + " " + statName + "|r\n";
            }
        }
    }

    return tooltip;
}

// 应用属性修正到玩家身上
void ItemEnhancementMgr::ApplyStatModifier(Player* player, uint32 statType, int32 value, bool apply)
{
    if (!player || value == 0)
        return;

    // 详细调试信息
    // ChatHandler(player->GetSession()).PSendSysMessage("DEBUG: {} 属性 {} (类型{}) 值:{}",
    //     apply ? "应用" : "移除", GetStatTypeName(statType), statType, apply ? value : -value);

    switch (statType)
    {
        case ITEM_MOD_MANA:
        {
            player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(value), apply);
            player->UpdateMaxPower(POWER_MANA);
            break;
        }
        case ITEM_MOD_HEALTH:
        {
            player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(value), apply);
            player->UpdateMaxHealth();
            break;
        }
        case ITEM_MOD_AGILITY:
        {
            // 参考物品成长系统：同时修改BASE_VALUE与BuffMod，确保能正确参与倍率计算
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_AGILITY, float(value), apply);
            player->UpdateStats(STAT_AGILITY);
            break;
        }
        case ITEM_MOD_STRENGTH:
        {
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_STRENGTH, float(value), apply);
            player->UpdateStats(STAT_STRENGTH);
            break;
        }
        case ITEM_MOD_INTELLECT:
        {
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_INTELLECT, float(value), apply);
            player->UpdateStats(STAT_INTELLECT);
            player->UpdateMaxPower(POWER_MANA);
            break;
        }
        case ITEM_MOD_SPIRIT:
        {
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_SPIRIT, float(value), apply);
            player->UpdateStats(STAT_SPIRIT);
            break;
        }
        case ITEM_MOD_STAMINA:
        {
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_STAMINA, float(value), apply);
            player->UpdateStats(STAT_STAMINA);
            player->UpdateMaxHealth();
            break;
        }
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            player->ApplyRatingMod(CR_DEFENSE_SKILL, value, apply);
            break;
        case ITEM_MOD_DODGE_RATING:
            player->ApplyRatingMod(CR_DODGE, value, apply);
            break;
        case ITEM_MOD_PARRY_RATING:
            player->ApplyRatingMod(CR_PARRY, value, apply);
            break;
        case ITEM_MOD_BLOCK_RATING:
            player->ApplyRatingMod(CR_BLOCK, value, apply);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, value, apply);
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            player->ApplyRatingMod(CR_HIT_RANGED, value, apply);
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            player->ApplyRatingMod(CR_HIT_SPELL, value, apply);
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, value, apply);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            player->ApplyRatingMod(CR_CRIT_RANGED, value, apply);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            player->ApplyRatingMod(CR_CRIT_SPELL, value, apply);
            break;
        case ITEM_MOD_HASTE_MELEE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, value, apply);
            break;
        case ITEM_MOD_HASTE_RANGED_RATING:
            player->ApplyRatingMod(CR_HASTE_RANGED, value, apply);
            break;
        case ITEM_MOD_HASTE_SPELL_RATING:
            player->ApplyRatingMod(CR_HASTE_SPELL, value, apply);
            break;
        case ITEM_MOD_HIT_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, value, apply);
            player->ApplyRatingMod(CR_HIT_RANGED, value, apply);
            player->ApplyRatingMod(CR_HIT_SPELL, value, apply);
            break;
        case ITEM_MOD_CRIT_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, value, apply);
            player->ApplyRatingMod(CR_CRIT_RANGED, value, apply);
            player->ApplyRatingMod(CR_CRIT_SPELL, value, apply);
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, value, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, value, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, value, apply);
            break;
        case ITEM_MOD_HASTE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, value, apply);
            player->ApplyRatingMod(CR_HASTE_RANGED, value, apply);
            player->ApplyRatingMod(CR_HASTE_SPELL, value, apply);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            player->ApplyRatingMod(CR_EXPERTISE, value, apply);
            break;
        case ITEM_MOD_ATTACK_POWER:
        {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(value), apply);
            player->UpdateAttackPowerAndDamage();
            break;
        }
        case ITEM_MOD_RANGED_ATTACK_POWER:
        {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, float(value), apply);
            player->UpdateAttackPowerAndDamage(true);
            break;
        }
        case ITEM_MOD_SPELL_POWER:
            // 法术强度（同时影响伤害和治疗）
            player->ApplySpellPowerBonus(value, apply);
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, value, apply);
            break;
        default:
            // ChatHandler(player->GetSession()).PSendSysMessage("警告：未知的属性类型 {}", statType);
            break;
    }
}

// 批量应用属性修正（不立即更新，用于批量优化）
void ItemEnhancementMgr::ApplyStatModifierBatch(Player* player, uint32 statType, int32 value, bool apply)
{
    if (!player || value == 0)
        return;

    // 批量版本：只设置属性值，不调用任何Update函数
    switch (statType)
    {
        case ITEM_MOD_MANA:
            player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(value), apply);
            break;
        case ITEM_MOD_HEALTH:
            player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(value), apply);
            break;
        case ITEM_MOD_AGILITY:
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_AGILITY, float(value), apply);
            break;
        case ITEM_MOD_STRENGTH:
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_STRENGTH, float(value), apply);
            break;
        case ITEM_MOD_INTELLECT:
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_INTELLECT, float(value), apply);
            break;
        case ITEM_MOD_SPIRIT:
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_SPIRIT, float(value), apply);
            break;
        case ITEM_MOD_STAMINA:
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(value), apply);
            player->ApplyStatBuffMod(STAT_STAMINA, float(value), apply);
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            player->ApplyRatingMod(CR_DEFENSE_SKILL, value, apply);
            break;
        case ITEM_MOD_DODGE_RATING:
            player->ApplyRatingMod(CR_DODGE, value, apply);
            break;
        case ITEM_MOD_PARRY_RATING:
            player->ApplyRatingMod(CR_PARRY, value, apply);
            break;
        case ITEM_MOD_BLOCK_RATING:
            player->ApplyRatingMod(CR_BLOCK, value, apply);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, value, apply);
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            player->ApplyRatingMod(CR_HIT_RANGED, value, apply);
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            player->ApplyRatingMod(CR_HIT_SPELL, value, apply);
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, value, apply);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            player->ApplyRatingMod(CR_CRIT_RANGED, value, apply);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            player->ApplyRatingMod(CR_CRIT_SPELL, value, apply);
            break;
        case ITEM_MOD_HASTE_MELEE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, value, apply);
            break;
        case ITEM_MOD_HASTE_RANGED_RATING:
            player->ApplyRatingMod(CR_HASTE_RANGED, value, apply);
            break;
        case ITEM_MOD_HASTE_SPELL_RATING:
            player->ApplyRatingMod(CR_HASTE_SPELL, value, apply);
            break;
        case ITEM_MOD_HIT_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, value, apply);
            player->ApplyRatingMod(CR_HIT_RANGED, value, apply);
            player->ApplyRatingMod(CR_HIT_SPELL, value, apply);
            break;
        case ITEM_MOD_CRIT_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, value, apply);
            player->ApplyRatingMod(CR_CRIT_RANGED, value, apply);
            player->ApplyRatingMod(CR_CRIT_SPELL, value, apply);
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, value, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, value, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, value, apply);
            break;
        case ITEM_MOD_HASTE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, value, apply);
            player->ApplyRatingMod(CR_HASTE_RANGED, value, apply);
            player->ApplyRatingMod(CR_HASTE_SPELL, value, apply);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            player->ApplyRatingMod(CR_EXPERTISE, value, apply);
            break;
        case ITEM_MOD_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(value), apply);
            break;
        case ITEM_MOD_RANGED_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, float(value), apply);
            break;
        case ITEM_MOD_SPELL_POWER:
            player->ApplySpellPowerBonus(value, apply);
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, value, apply);
            break;
        default:
            break;
    }
}

// 移除之前的强化属性（基于属性字符串）
void ItemEnhancementMgr::RemovePreviousEnhancementStats(Player* player, const std::string& statValues)
{
    if (!player || statValues.empty())
        return;

    // ChatHandler(player->GetSession()).PSendSysMessage("移除之前的强化属性: {}", statValues);

    // 解析属性值
    std::map<uint32, int32> stats = ParseStatValues(statValues);

    // 移除玩家身上的属性修正值
    for (const auto& stat : stats)
    {
        uint32 statType = stat.first;
        int32 statValue = stat.second;

        if (statValue > 0)
        {
            // 移除属性修正
            ApplyStatModifier(player, statType, statValue, false);

            // ChatHandler(player->GetSession()).PSendSysMessage("移除属性: {} (类型{}) 值:-{}",
            //     GetStatTypeName(statType), statType, statValue);
        }
    }

    // 更新玩家属性
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);
}

// 发送强化数据给客户端UI插件
void ItemEnhancementMgr::SendEnhancementDataToClient(Player* player, Item* item, const EnhancementRecord& record)
{
    if (!player || !item)
        return;

    uint32 itemId = item->GetEntry();
    uint32 itemGuid = item->GetGUID().GetCounter();

    ChatHandler handler(player->GetSession());

    // 验证装备链接中的GUID是否正确
    int32 storedGuid = item->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
    // handler.PSendSysMessage("强化系统：发送数据前验证 - 服务器GUID: {}, 装备链接GUID: {}", itemGuid, storedGuid);

    // 获取物品的基础属性信息
    std::string baseStatsInfo = GetItemBaseStatsInfo(item);

    // 发送强化数据给客户端UI插件（使用系统消息格式，确保客户端能接收）
    // 注意：客户端期望小写的"guid"
    std::string message = "[强化系统] 物品ID:" + std::to_string(itemId) +
                         " guid:" + std::to_string(itemGuid) +
                         " 强化等级:" + std::to_string(record.level) +
                         " 属性:" + record.statValues +
                         " 基础属性:" + baseStatsInfo;

    // handler.PSendSysMessage(message);

    // 额外发送一条包含装备链接GUID的消息，供客户端对比
    if (storedGuid != static_cast<int32>(itemGuid))
    {
        // handler.PSendSysMessage("强化系统：警告 - GUID不匹配！服务器: {}, 装备链接: {}", itemGuid, storedGuid);

        // 尝试重新存储GUID
        item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, static_cast<int32>(itemGuid));
        item->SetState(ITEM_CHANGED, player);
        item->SendUpdateToPlayer(player);

        // handler.PSendSysMessage("强化系统：已重新同步GUID到装备链接");
    }
    else
    {
        // handler.PSendSysMessage("强化系统：GUID匹配正确");
    }

    // 使用自定义字段格式：ITEMENHANCE|ENHANCED|字段1|字段2|字段3...
    std::string enhancementData = "ITEMENHANCE|ENHANCED|";
    enhancementData += std::to_string(itemId) + "|";        // 物品ID
    enhancementData += std::to_string(itemGuid) + "|";      // 物品GUID
    enhancementData += std::to_string(record.level) + "|";  // 强化等级
    enhancementData += record.statValues;                     // 强化属性值

    // 发送给物品强化专用UI插件
    SendAddonMessage(player, enhancementData);
}

// 发送插件消息给客户端
void ItemEnhancementMgr::SendAddonMessage(Player* player, const std::string& message)
{
    if (!player)
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_GUILD, LANG_ADDON, player, player, message);
    player->GetSession()->SendPacket(&data);
}

// 跟踪已应用的强化效果
void ItemEnhancementMgr::TrackAppliedEnhancement(Player* player, Item* item, const std::string& statValues)
{
    if (!player || !item)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    _appliedEnhancements[playerGuid][itemGuid] = statValues;

    // ChatHandler(player->GetSession()).PSendSysMessage("跟踪强化效果: 物品GUID={}, 属性={}", itemGuid, statValues);
}

// 取消跟踪已应用的强化效果
void ItemEnhancementMgr::UntrackAppliedEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    auto playerItr = _appliedEnhancements.find(playerGuid);
    if (playerItr != _appliedEnhancements.end())
    {
        auto itemItr = playerItr->second.find(itemGuid);
        if (itemItr != playerItr->second.end())
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("取消跟踪强化效果: 物品GUID={}", itemGuid);
            playerItr->second.erase(itemItr);

            // 如果玩家没有其他强化装备，删除玩家记录
            if (playerItr->second.empty())
            {
                _appliedEnhancements.erase(playerItr);
            }
        }
    }
}

// 获取已应用的强化效果
std::string ItemEnhancementMgr::GetAppliedEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return "";

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    auto playerItr = _appliedEnhancements.find(playerGuid);
    if (playerItr != _appliedEnhancements.end())
    {
        auto itemItr = playerItr->second.find(itemGuid);
        if (itemItr != playerItr->second.end())
        {
            return itemItr->second;
        }
    }

    return "";
}

// 清理玩家的所有已应用强化效果
void ItemEnhancementMgr::ClearPlayerAppliedEnhancements(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerItr = _appliedEnhancements.find(playerGuid);
    if (playerItr != _appliedEnhancements.end())
    {
        // ChatHandler(player->GetSession()).PSendSysMessage("清理玩家所有已应用的强化效果");

        // 先移除所有已应用的属性效果
        for (const auto& itemEntry : playerItr->second)
        {
            std::string appliedStats = itemEntry.second;
            if (!appliedStats.empty())
            {
                // 解析并移除属性
                std::map<uint32, int32> stats = ParseStatValues(appliedStats);
                for (const auto& stat : stats)
                {
                    if (stat.second > 0)
                    {
                        ApplyStatModifier(player, stat.first, stat.second, false);
                    }
                }
            }
        }

        // 清理跟踪数据
        _appliedEnhancements.erase(playerItr);

        // 更新玩家属性
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }
}

// 获取物品基础属性信息
std::string ItemEnhancementMgr::GetItemBaseStatsInfo(Item* item) const
{
    if (!item)
        return "";

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return "";

    std::string baseStats = "";

    // 护甲值
    if (itemTemplate->Armor > 0)
    {
        if (!baseStats.empty()) baseStats += " ";
        baseStats += "护甲:" + std::to_string(itemTemplate->Armor);
    }

    // 基础属性
    for (uint8 i = 0; i < itemTemplate->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (itemTemplate->ItemStat[i].ItemStatValue != 0)
        {
            if (!baseStats.empty()) baseStats += " ";

            std::string statName = GetStatTypeName(itemTemplate->ItemStat[i].ItemStatType);
            baseStats += statName + ":" + std::to_string(itemTemplate->ItemStat[i].ItemStatValue);
        }
    }

    // 武器伤害
    if (itemTemplate->Class == ITEM_CLASS_WEAPON)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            if (itemTemplate->Damage[i].DamageMin > 0 || itemTemplate->Damage[i].DamageMax > 0)
            {
                if (!baseStats.empty()) baseStats += " ";
                baseStats += "伤害:" + std::to_string((int)itemTemplate->Damage[i].DamageMin) +
                           "-" + std::to_string((int)itemTemplate->Damage[i].DamageMax);
                break; // 只显示第一个伤害类型
            }
        }

        // 攻击速度
        if (itemTemplate->Delay > 0)
        {
            if (!baseStats.empty()) baseStats += " ";
            float speed = itemTemplate->Delay / 1000.0f;
            baseStats += "速度:" + std::to_string(speed).substr(0, 4);
        }
    }

    return baseStats;
}
