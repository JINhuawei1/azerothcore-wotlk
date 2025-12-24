/*
 * 飞升系统 - AscensionSystem.cpp
 * 允许玩家在原有装备槽位基础上额外装备第二套装备
 */

#include "AscensionSystem.h"
#include "Logging/Log.h"
#include "World.h"
#include "Mail.h"
#include <sstream>

// 需求模板系统集成：通过模块管理器访问统一的需求接口
#include "ModuleManager.h"

// 包含幻境系统头文件（用于应用鉴定系统的自定义属性）
#if __has_include("HuanJingSystem.h")
    #ifndef MODULE_HUANJING_SYSTEM
        #define MODULE_HUANJING_SYSTEM
    #endif
    #include "HuanJingSystem.h"
#endif

using namespace Acore::ChatCommands;

// 获取需求模块接口（通过模块管理器）
static RequirementInterface* GetRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    if (!mgr)
        return nullptr;
    return mgr->GetRequirementModule();
}

//=============================================================================
// AscensionConfig 实现
//=============================================================================

AscensionConfig* AscensionConfig::instance()
{
    static AscensionConfig instance;
    return &instance;
}

bool AscensionConfig::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("飞升系统.启用", true);
    _debugMode = sConfigMgr->GetOption<bool>("飞升系统.调试模式", false);
    _statMultiplier = sConfigMgr->GetOption<float>("飞升系统.属性倍率", 1.0f);
    _requiredLevel = sConfigMgr->GetOption<uint32>("飞升系统.需要等级", 1);
    _showNotification = sConfigMgr->GetOption<bool>("飞升系统.显示提示", true);
    _autoUnlock = sConfigMgr->GetOption<bool>("飞升系统.自动解锁", false);

    if (_debugMode)
    {
        LOG_INFO("server.loading", "飞升系统: 模块已{}", _enabled ? "启用" : "禁用");
        LOG_INFO("server.loading", "飞升系统: 调试模式已{}", _debugMode ? "启用" : "禁用");
        LOG_INFO("server.loading", "飞升系统: 属性倍率 = {}", _statMultiplier);
        LOG_INFO("server.loading", "飞升系统: 需要等级 = {}", _requiredLevel);
    }

    return true;
}

//=============================================================================
// AscensionManager 实现
//=============================================================================

AscensionManager* AscensionManager::instance()
{
    static AscensionManager instance;
    return &instance;
}

bool AscensionManager::Initialize()
{
    if (!sAscensionConfig->IsEnabled())
        return false;

    LoadSlotControls();
    return true;
}

void AscensionManager::LoadSlotControls()
{
    _slotControls.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `槽位`, `槽位名称`, `启用`, `解锁需求`, `属性倍率` FROM `_飞升系统_控制` ORDER BY `槽位`");

    if (!result)
    {
        // 创建默认槽位配置
        const char* defaultNames[] = {"头部", "颈部", "肩部", "衬衣", "胸甲", "腰带", "腿部", "脚部",
                                       "手腕", "手套", "戒指1", "戒指2", "饰品1", "饰品2", "披风", "主手", "副手", "远程"};
        for (uint8 i = 0; i < ASCENSION_SLOT_COUNT; ++i)
        {
            AscensionSlotControl ctrl;
            ctrl.slot = i;
            ctrl.slotName = defaultNames[i];
            ctrl.enabled = true;
            ctrl.unlockRequirement = 0;
            ctrl.statMultiplier = 1.0f;
            _slotControls[i] = ctrl;
        }
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        AscensionSlotControl ctrl;
        ctrl.slot = fields[0].Get<uint8>();
        ctrl.slotName = fields[1].Get<std::string>();
        ctrl.enabled = fields[2].Get<bool>();
        ctrl.unlockRequirement = fields[3].Get<uint32>();
        ctrl.statMultiplier = fields[4].Get<float>();

        _slotControls[ctrl.slot] = ctrl;
        count++;
    } while (result->NextRow());
}

void AscensionManager::LoadPlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 清除旧数据
    _playerStatus.erase(playerGuid);

    PlayerAscensionStatus status;
    status.playerGuid = playerGuid;

    // 从合并表加载数据（已解锁槽位和装备数据）
    QueryResult result = CharacterDatabase.Query(
        "SELECT `已解锁槽位`, `装备数据` FROM `_飞升系统_数据` WHERE `玩家GUID` = {}", playerGuid);

    if (result)
    {
        Field* fields = result->Fetch();

        // 解析已解锁槽位（逗号分隔的字符串）
        std::string unlockedStr = fields[0].Get<std::string>();
        if (!unlockedStr.empty())
        {
            std::stringstream ss(unlockedStr);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                if (!token.empty())
                {
                    try {
                        uint8 slot = static_cast<uint8>(std::stoi(token));
                        if (slot < ASCENSION_SLOT_COUNT)
                        {
                            status.unlockedSlots.insert(slot);
                        }
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR("module", "飞升系统: 解析槽位失败: {}", e.what());
                    }
                }
            }
        }

        // 解析装备数据（槽位:物品ID:物品GUID,槽位:物品ID:物品GUID,...）
        std::string equipStr = fields[1].Get<std::string>();
        if (!equipStr.empty())
        {
            std::stringstream ss(equipStr);
            std::string slotInfo;
            while (std::getline(ss, slotInfo, ','))
            {
                if (!slotInfo.empty())
                {
                    std::stringstream slotSS(slotInfo);
                    std::string part;
                    std::vector<std::string> parts;
                    while (std::getline(slotSS, part, ':'))
                    {
                        parts.push_back(part);
                    }
                    if (parts.size() >= 3)
                    {
                        uint8 slot = static_cast<uint8>(std::stoi(parts[0]));
                        if (slot < ASCENSION_SLOT_COUNT)
                        {
                            AscensionSlotData slotData;
                            slotData.itemId = static_cast<uint32>(std::stoul(parts[1]));
                            slotData.itemGuid = static_cast<uint32>(std::stoul(parts[2]));
                            slotData.itemPtr = nullptr;  // 稍后在加载物品实例时设置
                            status.slots[slot] = slotData;
                        }
                    }
                }
            }
        }
    }

    // 如果启用自动解锁，解锁所有槽位
    if (sAscensionConfig->IsAutoUnlock())
    {
        for (uint8 i = 0; i < ASCENSION_SLOT_COUNT; ++i)
        {
            status.unlockedSlots.insert(i);
        }
    }

    _playerStatus[playerGuid] = status;

    // 加载物品实例
    LoadAscensionItems(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 数据加载完成，已解锁 {} 个槽位，已装备 {} 件",
            player->GetName(), status.unlockedSlots.size(), status.slots.size());
    }
}

void AscensionManager::SavePlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 构建已解锁槽位字符串
    std::ostringstream unlockStr;
    bool first = true;
    for (uint8 s : status->unlockedSlots)
    {
        if (!first) unlockStr << ",";
        unlockStr << (int)s;
        first = false;
    }

    // 构建装备数据字符串
    std::ostringstream equipStr;
    first = true;
    for (const auto& pair : status->slots)
    {
        if (!first) equipStr << ",";
        equipStr << (int)pair.first << ":" << pair.second.itemId << ":" << pair.second.itemGuid;
        first = false;
    }

    // 保存到合并表
    CharacterDatabase.Execute(
        "REPLACE INTO `_飞升系统_数据` (`玩家GUID`, `已解锁槽位`, `装备数据`) VALUES ({}, '{}', '{}')",
        playerGuid, unlockStr.str(), equipStr.str());

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 数据已保存", player->GetName());
    }
}

void AscensionManager::ClearPlayerData(uint32 playerGuid)
{
    // 清理内存缓存，释放物品实例（不删除数据库数据）
    auto it = _playerStatus.find(playerGuid);
    if (it != _playerStatus.end())
    {
        // 释放所有物品实例的内存
        for (auto& pair : it->second.slots)
        {
            if (pair.second.itemPtr)
            {
                delete pair.second.itemPtr;
                pair.second.itemPtr = nullptr;
            }
        }
        _playerStatus.erase(it);
    }
}

void AscensionManager::DeletePlayerData(uint32 playerGuid)
{
    // 清理内存并删除数据库数据（仅在角色删除时调用）
    // 先释放物品内存
    auto it = _playerStatus.find(playerGuid);
    if (it != _playerStatus.end())
    {
        for (auto& pair : it->second.slots)
        {
            if (pair.second.itemPtr)
            {
                // 删除物品实例数据
                CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", pair.second.itemGuid);
                delete pair.second.itemPtr;
                pair.second.itemPtr = nullptr;
            }
        }
        _playerStatus.erase(it);
    }
    CharacterDatabase.Execute("DELETE FROM `_飞升系统_数据` WHERE `玩家GUID` = {}", playerGuid);
}

void AscensionManager::SaveAscensionItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto& pair : status->slots)
    {
        Item* item = pair.second.itemPtr;
        if (item)
        {
            // 确保物品状态为 ITEM_CHANGED，这样 SaveToDB 会保存物品
            item->SetState(ITEM_CHANGED, player);
            // 保存物品到数据库
            item->SaveToDB(trans);

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 保存飞升物品 槽位={} 物品ID={} GUID={}",
                    pair.first, pair.second.itemId, pair.second.itemGuid);
            }
        }
    }

    CharacterDatabase.CommitTransaction(trans);
}

void AscensionManager::LoadAscensionItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    std::vector<uint8> slotsToRemove;

    for (auto& pair : status->slots)
    {
        uint8 slot = pair.first;
        AscensionSlotData& slotData = pair.second;

        // 从 item_instance 表加载物品（使用与核心相同的查询字段）
        QueryResult result = CharacterDatabase.Query(
            "SELECT creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, "
            "randomPropertyId, durability, playedTime, text, itemEntry "
            "FROM item_instance WHERE guid = {}", slotData.itemGuid);

        if (!result)
        {
            // 物品实例不存在
            LOG_ERROR("module", "飞升系统: 物品实例不存在 GUID={}", slotData.itemGuid);
            slotsToRemove.push_back(slot);
            continue;
        }

        Field* fields = result->Fetch();
        uint32 itemEntry = fields[11].Get<uint32>();  // itemEntry 在最后一列

        // 验证物品ID是否匹配
        if (itemEntry != slotData.itemId)
        {
            LOG_ERROR("module", "飞升系统: 物品ID不匹配 期望={} 实际={}", slotData.itemId, itemEntry);
            slotsToRemove.push_back(slot);
            continue;
        }

        // 创建新的物品对象
        Item* item = NewItemOrBag(sObjectMgr->GetItemTemplate(slotData.itemId));
        if (!item)
        {
            LOG_ERROR("module", "飞升系统: 无法创建物品实例 itemId={}", slotData.itemId);
            slotsToRemove.push_back(slot);
            continue;
        }

        // 从数据库加载完整物品数据
        if (!item->LoadFromDB(slotData.itemGuid, player->GetGUID(), fields, slotData.itemId))
        {
            LOG_ERROR("module", "飞升系统: 从数据库加载物品失败 GUID={}", slotData.itemGuid);
            delete item;
            slotsToRemove.push_back(slot);
            continue;
        }

        // 保存物品指针
        slotData.itemPtr = item;

        if (sAscensionConfig->IsDebugMode())
        {
            LOG_INFO("module", "飞升系统: 加载飞升物品 槽位={} 物品ID={} GUID={}",
                slot, slotData.itemId, slotData.itemGuid);
        }
    }

    // 移除无效的槽位
    for (uint8 slot : slotsToRemove)
    {
        status->slots.erase(slot);
    }

    // 如果有移除，保存数据
    if (!slotsToRemove.empty())
    {
        SavePlayerData(player);
    }
}

void AscensionManager::ValidateEquippedItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    std::vector<uint8> slotsToRemove;
    bool needSave = false;

    for (auto& pair : status->slots)
    {
        uint8 slot = pair.first;
        AscensionSlotData& slotData = pair.second;

        // 检查物品指针是否有效
        if (!slotData.itemPtr)
        {
            // 物品指针无效，可能是加载失败
            slotsToRemove.push_back(slot);
            needSave = true;

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 的物品指针无效，移除记录",
                    player->GetName(), slot);
            }
            continue;
        }

        // 验证物品实例是否还存在于数据库
        QueryResult result = CharacterDatabase.Query(
            "SELECT guid FROM item_instance WHERE guid = {}", slotData.itemGuid);

        if (!result)
        {
            // 物品已被删除
            slotsToRemove.push_back(slot);
            needSave = true;

            // 释放物品内存
            delete slotData.itemPtr;
            slotData.itemPtr = nullptr;

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 的物品(GUID:{}) 已被删除，移除记录",
                    player->GetName(), slot, slotData.itemGuid);
            }
        }
    }

    // 移除无效的装备记录
    for (uint8 slot : slotsToRemove)
    {
        status->slots.erase(slot);
    }

    // 如果有变动，保存数据
    if (needSave)
    {
        SavePlayerData(player);
    }
}

bool AscensionManager::IsSlotUnlocked(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    // 检查槽位是否启用
    auto ctrlIt = _slotControls.find(slot);
    if (ctrlIt == _slotControls.end() || !ctrlIt->second.enabled)
        return false;

    // 自动解锁模式
    if (sAscensionConfig->IsAutoUnlock())
        return true;

    // 检查玩家是否已解锁此槽位（从数据库记录判断）
    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return false;

    return status->unlockedSlots.find(slot) != status->unlockedSlots.end();
}

bool AscensionManager::UnlockSlot(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    if (IsSlotUnlocked(player, slot))
    {
        ChatHandler(player->GetSession()).SendSysMessage("该槽位已经解锁。");
        return false;
    }

    // 检查解锁需求
    if (!CheckSlotUnlockRequirement(player, slot))
    {
        ChatHandler(player->GetSession()).SendSysMessage("不满足解锁条件。");
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
    {
        LoadPlayerData(player);
        status = GetPlayerStatus(playerGuid);
    }

    status->unlockedSlots.insert(slot);

    // 保存到合并表（使用SavePlayerData统一保存）
    SavePlayerData(player);

    std::string slotName = GetSlotName(slot);
    ChatHandler(player->GetSession()).PSendSysMessage("成功解锁飞升槽位: {}", slotName.c_str());

    // 发送更新数据到客户端
    SendAscensionDataToClient(player);

    return true;
}

bool AscensionManager::CheckSlotUnlockRequirement(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    auto ctrlIt = _slotControls.find(slot);
    if (ctrlIt == _slotControls.end())
        return false;

    uint32 requirementId = ctrlIt->second.unlockRequirement;
    if (requirementId == 0)
        return true; // 无需求

    // 通过模块管理器获取需求模块接口
    RequirementInterface* reqModule = GetRequirementModule();
    if (!reqModule)
    {
        LOG_ERROR("module", "飞升系统: 需求模板系统未初始化，无法检查解锁需求");
        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[飞升系统]|r 需求模板系统未加载，无法解锁槽位");
        return false;
    }

    // 先静默检查是否满足需求
    bool meetsRequirements = reqModule->CheckRequirements(player, requirementId, false);
    if (!meetsRequirements)
    {
        // 不满足条件，显示需求详情
        ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[飞升系统]|r 不满足解锁条件，请查看需求详情：");
        reqModule->CheckRequirements(player, requirementId, true);
        return false;
    }

    // 满足条件，消耗需求资源（扣除物品、金币等）
    if (!reqModule->ConsumeRequirements(player, requirementId))
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[飞升系统]|r 消耗需求资源失败！");
        return false;
    }

    return true;
}

bool AscensionManager::EquipItem(Player* player, uint8 slot, uint32 itemId, uint32 itemGuid)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    // 检查等级要求
    if (player->GetLevel() < sAscensionConfig->GetRequiredLevel())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("需要达到 {} 级才能使用飞升系统。", sAscensionConfig->GetRequiredLevel());
        return false;
    }

    // 检查槽位是否解锁
    if (!IsSlotUnlocked(player, slot))
    {
        ChatHandler(player->GetSession()).SendSysMessage("该槽位尚未解锁。");
        return false;
    }

    // 检查物品是否可以装备到该槽位
    if (!CanEquipItemInSlot(player, slot, itemId))
    {
        ChatHandler(player->GetSession()).SendSysMessage("该物品无法装备到此槽位。");
        return false;
    }

    // 检查物品是否在背包中
    Item* item = FindItemInBags(player, itemGuid);
    if (!item || item->GetEntry() != itemId)
    {
        ChatHandler(player->GetSession()).SendSysMessage("背包中未找到该物品。");
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
    {
        LoadPlayerData(player);
        status = GetPlayerStatus(playerGuid);
    }

    // 检查该槽位是否已有装备
    if (status->slots.find(slot) != status->slots.end())
    {
        // 先卸下旧装备
        UnequipItem(player, slot);
    }

    // 检查该物品是否已在其他槽位
    for (const auto& pair : status->slots)
    {
        if (pair.second.itemGuid == itemGuid)
        {
            ChatHandler(player->GetSession()).SendSysMessage("该物品已装备在其他飞升槽位。");
            return false;
        }
    }

    // 记录物品背包位置（用于从背包移除）
    uint8 bagSlot = item->GetBagSlot();
    uint8 itemSlot = item->GetSlot();

    // 装备物品 - 从背包移除但保留物品实例
    AscensionSlotData slotData;
    slotData.itemId = itemId;
    slotData.itemGuid = itemGuid;
    slotData.itemPtr = item;
    status->slots[slot] = slotData;

    // 从背包移除物品（但不删除物品实例）
    player->MoveItemFromInventory(bagSlot, itemSlot, true);

    // 从数据库的 character_inventory 中删除（物品不再属于背包）
    // 同时立即保存物品实例，确保物品数据不会丢失
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    item->DeleteFromInventoryDB(trans);
    // 确保物品状态为 ITEM_CHANGED，这样 SaveToDB 会保存物品到 item_instance
    item->SetState(ITEM_CHANGED, player);
    item->SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    // 应用属性
    ApplyItemEffect(player, itemId, slot, true);

    // 保存数据
    SavePlayerData(player);

    if (sAscensionConfig->ShowNotification())
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        std::string itemName = proto ? proto->Name1 : "未知物品";
        std::string slotName = GetSlotName(slot);
        ChatHandler(player->GetSession()).PSendSysMessage("飞升装备: {} 已装备到 {}", itemName.c_str(), slotName.c_str());
    }

    // 发送数据到客户端
    SendAscensionDataToClient(player);

    return true;
}

bool AscensionManager::UnequipItem(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return false;

    auto it = status->slots.find(slot);
    if (it == status->slots.end())
    {
        ChatHandler(player->GetSession()).SendSysMessage("该槽位没有装备。");
        return false;
    }

    uint32 itemId = it->second.itemId;
    uint32 itemGuid = it->second.itemGuid;
    Item* item = it->second.itemPtr;

    // 移除属性
    ApplyItemEffect(player, itemId, slot, false);

    // 将物品放回背包
    if (item)
    {
        // 查找空闲背包位置
        ItemPosCountVec dest;
        InventoryResult result = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (result == EQUIP_ERR_OK)
        {
            // 存入背包
            player->StoreItem(dest, item, true);

            // 保存到数据库
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            item->SaveToDB(trans);
            CharacterDatabase.CommitTransaction(trans);

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 物品 {} (GUID:{}) 已放回背包", itemId, itemGuid);
            }
        }
        else
        {
            // 背包已满，发送邮件
            MailDraft draft("飞升系统", "您的背包已满，飞升装备已通过邮件返还。");
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            draft.AddItem(item);
            draft.SendMailTo(trans, MailReceiver(player, playerGuid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
            CharacterDatabase.CommitTransaction(trans);

            ChatHandler(player->GetSession()).SendSysMessage("背包已满，物品已通过邮件返还。");

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 背包已满，物品 {} (GUID:{}) 通过邮件返还", itemId, itemGuid);
            }
        }
    }
    else
    {
        // 物品指针无效，尝试从数据库恢复
        ChatHandler(player->GetSession()).SendSysMessage("物品数据异常，已清除飞升装备记录。");

        if (sAscensionConfig->IsDebugMode())
        {
            LOG_ERROR("module", "飞升系统: 卸下装备时物品指针无效 itemId={} itemGuid={}", itemId, itemGuid);
        }
    }

    // 移除装备记录
    status->slots.erase(it);

    // 保存数据
    SavePlayerData(player);

    if (sAscensionConfig->ShowNotification())
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        std::string itemName = proto ? proto->Name1 : "未知物品";
        std::string slotName = GetSlotName(slot);
        ChatHandler(player->GetSession()).PSendSysMessage("飞升装备: {} 已从 {} 卸下", itemName.c_str(), slotName.c_str());
    }

    // 发送数据到客户端
    SendAscensionDataToClient(player);

    return true;
}

void AscensionManager::UnequipAllItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 移除所有属性
    RemoveAllEffects(player);

    // 将所有物品放回背包
    std::vector<Item*> itemsToMail;
    for (auto& pair : status->slots)
    {
        Item* item = pair.second.itemPtr;
        if (!item)
            continue;

        // 查找空闲背包位置
        ItemPosCountVec dest;
        InventoryResult result = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (result == EQUIP_ERR_OK)
        {
            player->StoreItem(dest, item, true);

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            item->SaveToDB(trans);
            CharacterDatabase.CommitTransaction(trans);
        }
        else
        {
            // 背包已满，收集待邮寄物品
            itemsToMail.push_back(item);
        }
    }

    // 邮寄无法放入背包的物品
    if (!itemsToMail.empty())
    {
        MailDraft draft("飞升系统", "您的背包已满，飞升装备已通过邮件返还。");
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        for (Item* item : itemsToMail)
        {
            draft.AddItem(item);
        }
        draft.SendMailTo(trans, MailReceiver(player, playerGuid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
        CharacterDatabase.CommitTransaction(trans);

        ChatHandler(player->GetSession()).PSendSysMessage("背包已满，{} 件物品已通过邮件返还。", itemsToMail.size());
    }

    // 清空装备
    status->slots.clear();

    // 保存数据
    SavePlayerData(player);

    if (sAscensionConfig->ShowNotification())
    {
        ChatHandler(player->GetSession()).SendSysMessage("已卸下所有飞升装备。");
    }

    // 发送数据到客户端
    SendAscensionDataToClient(player);
}

void AscensionManager::ApplyAllEffects(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    for (const auto& pair : status->slots)
    {
        ApplyItemEffect(player, pair.second.itemId, pair.first, true);
    }

    UpdatePlayerStats(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 应用了 {} 件飞升装备的属性", player->GetName(), status->slots.size());
    }
}

void AscensionManager::RemoveAllEffects(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 移除所有已应用的法术
    for (uint32 spellId : status->appliedSpells)
    {
        player->RemoveAurasDueToSpell(spellId);
    }
    status->appliedSpells.clear();

    // 移除所有已装备物品的效果
    for (const auto& pair : status->slots)
    {
        ApplyItemEffect(player, pair.second.itemId, pair.first, false);
    }
    status->appliedStats.clear();

    UpdatePlayerStats(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 移除了所有飞升装备属性", player->GetName());
    }
}

void AscensionManager::RefreshEffects(Player* player)
{
    if (!player)
        return;

    RemoveAllEffects(player);
    ApplyAllEffects(player);
}

void AscensionManager::ApplyItemEffect(Player* player, uint32 itemId, uint8 slot, bool apply)
{
    if (!player)
        return;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 获取槽位数据和物品指针（用于应用鉴定系统的自定义属性）
    Item* item = nullptr;
    auto slotIt = status->slots.find(slot);
    if (slotIt != status->slots.end())
    {
        item = slotIt->second.itemPtr;
    }

    // 获取槽位倍率
    float slotMultiplier = 1.0f;
    auto ctrlIt = _slotControls.find(slot);
    if (ctrlIt != _slotControls.end())
    {
        slotMultiplier = ctrlIt->second.statMultiplier;
    }

    // 全局倍率
    float globalMultiplier = sAscensionConfig->GetStatMultiplier();
    float totalMultiplier = slotMultiplier * globalMultiplier;

    // 应用物品属性 - 使用正确的属性类型映射
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (i >= proto->StatsCount)
            break;

        if (proto->ItemStat[i].ItemStatValue != 0)
        {
            int32 val = int32(proto->ItemStat[i].ItemStatValue * totalMultiplier);
            uint32 statType = proto->ItemStat[i].ItemStatType;

            // 根据属性类型正确应用
            switch (statType)
            {
                case ITEM_MOD_MANA:
                    player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_HEALTH:
                    player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_AGILITY:
                    player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
                    break;
                case ITEM_MOD_STRENGTH:
                    player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
                    break;
                case ITEM_MOD_INTELLECT:
                    player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
                    break;
                case ITEM_MOD_SPIRIT:
                    player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
                    break;
                case ITEM_MOD_STAMINA:
                    player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
                    break;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                    player->ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
                    break;
                case ITEM_MOD_DODGE_RATING:
                    player->ApplyRatingMod(CR_DODGE, int32(val), apply);
                    break;
                case ITEM_MOD_PARRY_RATING:
                    player->ApplyRatingMod(CR_PARRY, int32(val), apply);
                    break;
                case ITEM_MOD_BLOCK_RATING:
                    player->ApplyRatingMod(CR_BLOCK, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_MELEE_RATING:
                    player->ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_RANGED_RATING:
                    player->ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_SPELL_RATING:
                    player->ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_MELEE_RATING:
                    player->ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_RANGED_RATING:
                    player->ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_SPELL_RATING:
                    player->ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_HASTE_MELEE_RATING:
                    player->ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                    break;
                case ITEM_MOD_HASTE_RANGED_RATING:
                    player->ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                    break;
                case ITEM_MOD_HASTE_SPELL_RATING:
                    player->ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_RATING:
                    player->ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                    player->ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                    player->ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_RATING:
                    player->ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                    player->ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                    player->ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                    player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                    player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_RATING:
                case ITEM_MOD_RESILIENCE_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_HASTE_RATING:
                    player->ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                    player->ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                    player->ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                    break;
                case ITEM_MOD_EXPERTISE_RATING:
                    player->ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
                    break;
                case ITEM_MOD_ATTACK_POWER:
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), apply);
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_RANGED_ATTACK_POWER:
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_MANA_REGENERATION:
                    player->ApplyManaRegenBonus(int32(val), apply);
                    break;
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    player->ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), apply);
                    break;
                case ITEM_MOD_SPELL_POWER:
                    player->ApplySpellPowerBonus(int32(val), apply);
                    break;
                case ITEM_MOD_HEALTH_REGEN:
                    player->ApplyHealthRegenBonus(int32(val), apply);
                    break;
                case ITEM_MOD_SPELL_PENETRATION:
                    player->ApplySpellPenetrationBonus(val, apply);
                    break;
                case ITEM_MOD_BLOCK_VALUE:
                    player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), apply);
                    break;
                default:
                    break;
            }

            if (apply)
            {
                AppliedStatEffect effect;
                effect.statType = statType;
                effect.statValue = val;
                status->appliedStats.push_back(effect);
            }
        }
    }

    // 应用物品法术效果
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId <= 0)
            continue;

        if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[i].SpellId);
            if (!spellInfo)
                continue;

            if (apply)
            {
                player->CastSpell(player, spellInfo, true);
                status->appliedSpells.push_back(proto->Spells[i].SpellId);
            }
            else
            {
                player->RemoveAurasDueToSpell(proto->Spells[i].SpellId);
            }
        }
    }

    // 应用鉴定系统的自定义属性（通过幻境系统）
    // 包括：基础属性、追加属性、成长属性、强化属性等，以及倍率加成
#ifdef MODULE_HUANJING_SYSTEM
    if (item)
    {
        if (apply)
        {
            // 应用鉴定系统的自定义属性（包含倍率计算）
            sHuanJingSystem->ApplyHuanJingEnhancement(player, item, true);

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 应用了鉴定系统自定义属性 (物品GUID: {})",
                    player->GetName(), slot, item->GetGUID().GetCounter());
            }
        }
        else
        {
            // 移除鉴定系统的自定义属性
            sHuanJingSystem->RemoveHuanJingEnhancement(player, item);

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 移除了鉴定系统自定义属性 (物品GUID: {})",
                    player->GetName(), slot, item->GetGUID().GetCounter());
            }
        }
    }
#endif

    // 更新玩家属性
    UpdatePlayerStats(player);
}

bool AscensionManager::CanEquipItemInSlot(Player* player, uint8 slot, uint32 itemId)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
    {
        LOG_ERROR("module", "飞升系统: CanEquipItemInSlot 物品模板未找到 itemId={}", itemId);
        return false;
    }

    // 检查物品等级要求
    if (proto->RequiredLevel > player->GetLevel())
        return false;

    // 检查物品是否是装备类型 (InventoryType = 0 表示不是可装备物品)
    if (proto->InventoryType == 0)
        return false;

    // 获取物品应该装备的槽位
    uint8 expectedSlot = GetSlotForItemClass(proto->Class, proto->SubClass, proto->InventoryType);

    // 无效槽位
    if (expectedSlot == 0xFF)
        return false;

    // 特殊处理：戒指和饰品可以装备到两个槽位
    if (slot == ASCENSION_SLOT_FINGER1 || slot == ASCENSION_SLOT_FINGER2)
    {
        return expectedSlot == ASCENSION_SLOT_FINGER1 || expectedSlot == ASCENSION_SLOT_FINGER2;
    }
    if (slot == ASCENSION_SLOT_TRINKET1 || slot == ASCENSION_SLOT_TRINKET2)
    {
        return expectedSlot == ASCENSION_SLOT_TRINKET1 || expectedSlot == ASCENSION_SLOT_TRINKET2;
    }

    return expectedSlot == slot;
}

uint8 AscensionManager::GetSlotForItemClass(uint32 itemClass, uint32 itemSubClass, uint32 inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:          return ASCENSION_SLOT_HEAD;
        case INVTYPE_NECK:          return ASCENSION_SLOT_NECK;
        case INVTYPE_SHOULDERS:     return ASCENSION_SLOT_SHOULDERS;
        case INVTYPE_BODY:          return ASCENSION_SLOT_BODY;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:          return ASCENSION_SLOT_CHEST;
        case INVTYPE_WAIST:         return ASCENSION_SLOT_WAIST;
        case INVTYPE_LEGS:          return ASCENSION_SLOT_LEGS;
        case INVTYPE_FEET:          return ASCENSION_SLOT_FEET;
        case INVTYPE_WRISTS:        return ASCENSION_SLOT_WRISTS;
        case INVTYPE_HANDS:         return ASCENSION_SLOT_HANDS;
        case INVTYPE_FINGER:        return ASCENSION_SLOT_FINGER1;
        case INVTYPE_TRINKET:       return ASCENSION_SLOT_TRINKET1;
        case INVTYPE_CLOAK:         return ASCENSION_SLOT_BACK;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_2HWEAPON:      return ASCENSION_SLOT_MAINHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:      return ASCENSION_SLOT_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:   return ASCENSION_SLOT_RANGED;
        default:                    return 0xFF; // 无效槽位
    }
}

PlayerAscensionStatus* AscensionManager::GetPlayerStatus(uint32 playerGuid)
{
    auto it = _playerStatus.find(playerGuid);
    if (it != _playerStatus.end())
        return &it->second;
    return nullptr;
}

const AscensionSlotControl* AscensionManager::GetSlotControl(uint8 slot) const
{
    auto it = _slotControls.find(slot);
    if (it != _slotControls.end())
        return &it->second;
    return nullptr;
}

std::string AscensionManager::GetSlotName(uint8 slot) const
{
    auto it = _slotControls.find(slot);
    if (it != _slotControls.end())
        return it->second.slotName;

    static const char* defaultNames[] = {"头部", "颈部", "肩部", "衬衣", "胸甲", "腰带", "腿部", "脚部",
                                          "手腕", "手套", "戒指1", "戒指2", "饰品1", "饰品2", "披风", "主手", "副手", "远程"};
    if (slot < ASCENSION_SLOT_COUNT)
        return defaultNames[slot];
    return "未知";
}

void AscensionManager::UpdatePlayerStats(Player* player)
{
    if (!player)
        return;

    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);
}

Item* AscensionManager::FindItemInBags(Player* player, uint32 itemGuid)
{
    if (!player)
        return nullptr;

    // 在主背包中查找
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
            return item;
    }

    // 在其他背包中查找
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetGUID().GetCounter() == itemGuid)
                    return item;
            }
        }
    }

    return nullptr;
}

bool AscensionManager::IsItemEquippedInAscension(Player* player, uint32 itemGuid)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return false;

    for (const auto& pair : status->slots)
    {
        if (pair.second.itemGuid == itemGuid)
            return true;
    }
    return false;
}

int8 AscensionManager::GetAscensionSlotByItemGuid(Player* player, uint32 itemGuid)
{
    if (!player)
        return -1;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return -1;

    for (const auto& pair : status->slots)
    {
        if (pair.second.itemGuid == itemGuid)
            return static_cast<int8>(pair.first);
    }
    return -1;
}

void AscensionManager::SendAscensionDataToClient(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 构建数据字符串: ASCENSION_DATA:槽位:物品ID:物品GUID:解锁状态;...
    // 注意：使用分号;作为槽位分隔符，因为|在WoW客户端会被当作颜色代码
    std::ostringstream ss;
    ss << "ASCENSION_DATA:";

    bool first = true;
    for (uint8 slot = 0; slot < ASCENSION_SLOT_COUNT; ++slot)
    {
        if (!first)
            ss << ";";  // 使用分号代替竖线

        bool unlocked = IsSlotUnlocked(player, slot);
        auto it = status->slots.find(slot);

        if (it != status->slots.end())
        {
            // 有装备
            ss << (int)slot << ":" << it->second.itemId << ":" << it->second.itemGuid << ":" << (unlocked ? 1 : 0);
        }
        else
        {
            // 无装备
            ss << (int)slot << ":0:0:" << (unlocked ? 1 : 0);
        }
        first = false;
    }

    // 发送隐藏消息到客户端
    std::string hiddenMsg = "ASCENSION_HIDDEN:" + ss.str();
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, nullptr, nullptr, hiddenMsg);
    player->GetSession()->SendPacket(&data);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 发送数据到客户端 - {}", ss.str());
    }
}

//=============================================================================
// AscensionWorldScript 实现
//=============================================================================

AscensionWorldScript::AscensionWorldScript() : WorldScript("AscensionWorldScript")
{
    _initialized = false;
    _loadTimer = 0;
}

void AscensionWorldScript::OnAfterConfigLoad(bool reload)
{
    sAscensionConfig->LoadConfig();

    if (reload && _initialized)
    {
        sAscensionManager->LoadSlotControls();
        LOG_INFO("module", "飞升系统: 配置已重新加载");
    }
}

void AscensionWorldScript::OnUpdate(uint32 diff)
{
    if (_initialized)
        return;

    _loadTimer += diff;
    if (_loadTimer >= 1000) // 1秒延迟
    {
        if (sAscensionConfig->IsEnabled())
        {
            sAscensionManager->Initialize();
            LOG_INFO("server.loading", "→飞升系统加载√");
        }
        _initialized = true;
    }
}

//=============================================================================
// AscensionPlayerScript 实现
//=============================================================================

AscensionPlayerScript::AscensionPlayerScript() : PlayerScript("AscensionPlayerScript", {
    PLAYERHOOK_ON_LOGIN,
    PLAYERHOOK_ON_LOGOUT,
    PLAYERHOOK_ON_DELETE
})
{
}

void AscensionPlayerScript::OnPlayerLogin(Player* player)
{
    if (!sAscensionConfig->IsEnabled() || !player)
        return;

    // 加载玩家数据
    sAscensionManager->LoadPlayerData(player);

    // 验证飞升装备是否还在背包中
    sAscensionManager->ValidateEquippedItems(player);

    // 应用所有飞升装备效果
    sAscensionManager->ApplyAllEffects(player);

    // 发送数据到客户端
    sAscensionManager->SendAscensionDataToClient(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 登录，已加载飞升数据", player->GetName());
    }
}

void AscensionPlayerScript::OnPlayerLogout(Player* player)
{
    if (!sAscensionConfig->IsEnabled() || !player)
        return;

    // 移除所有效果
    sAscensionManager->RemoveAllEffects(player);

    // 保存飞升物品到数据库
    sAscensionManager->SaveAscensionItems(player);

    // 保存数据
    sAscensionManager->SavePlayerData(player);

    // 清理内存（包括释放物品实例）
    uint32 playerGuid = player->GetGUID().GetCounter();
    sAscensionManager->ClearPlayerData(playerGuid);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 登出，已保存飞升数据", player->GetName());
    }
}

void AscensionPlayerScript::OnPlayerDelete(ObjectGuid guid, uint32 accountId)
{
    if (!sAscensionConfig->IsEnabled())
        return;

    uint32 playerGuid = guid.GetCounter();
    sAscensionManager->DeletePlayerData(playerGuid);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 角色删除，清理玩家GUID:{} 的飞升数据", playerGuid);
    }
}

//=============================================================================
// AscensionCommandScript 实现
//=============================================================================

AscensionCommandScript::AscensionCommandScript() : CommandScript("AscensionCommandScript")
{
}

Acore::ChatCommands::ChatCommandTable AscensionCommandScript::GetCommands() const
{
    static ChatCommandTable ascensionCommandTable =
    {
        { "查看",   HandleAscensionView,    SEC_PLAYER,        Console::No },
        { "装备",   HandleAscensionEquip,   SEC_PLAYER,        Console::No },
        { "卸下",   HandleAscensionUnequip, SEC_PLAYER,        Console::No },
        { "清空",   HandleAscensionClear,   SEC_PLAYER,        Console::No },
        { "刷新",   HandleAscensionRefresh, SEC_PLAYER,        Console::No },
        { "解锁",   HandleAscensionUnlock,  SEC_PLAYER,        Console::No },
        { "重载",   HandleAscensionReload,  SEC_ADMINISTRATOR, Console::No }
    };

    static ChatCommandTable commandTable =
    {
        { "飞升", ascensionCommandTable }
    };

    return commandTable;
}

bool AscensionCommandScript::HandleAscensionView(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = sAscensionManager->GetPlayerStatus(playerGuid);

    handler->SendSysMessage("|cFFFFD700=== 飞升装备 ===|r");

    if (!status || status->slots.empty())
    {
        handler->SendSysMessage("当前没有装备任何飞升物品。");
    }
    else
    {
        for (uint8 i = 0; i < ASCENSION_SLOT_COUNT; ++i)
        {
            std::string slotName = sAscensionManager->GetSlotName(i);
            bool unlocked = sAscensionManager->IsSlotUnlocked(player, i);

            auto it = status->slots.find(i);
            if (it != status->slots.end())
            {
                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(it->second.itemId);
                std::string itemName = proto ? proto->Name1 : "未知物品";
                handler->PSendSysMessage("|cFF00FF00[{}]|r {}: {}", i, slotName.c_str(), itemName.c_str());
            }
            else if (unlocked)
            {
                handler->PSendSysMessage("|cFF808080[{}]|r {}: 空", i, slotName.c_str());
            }
            else
            {
                handler->PSendSysMessage("|cFFFF0000[{}]|r {}: 未解锁", i, slotName.c_str());
            }
        }
    }

    // 发送数据到客户端UI
    sAscensionManager->SendAscensionDataToClient(player);

    return true;
}

bool AscensionCommandScript::HandleAscensionEquip(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!*args)
    {
        handler->SendSysMessage("用法: .飞升 装备 <槽位> <背包ID> <背包槽位>");
        handler->SendSysMessage("槽位: 0=头部, 1=颈部, 2=肩部, 3=衬衣, 4=胸甲, 5=腰带, 6=腿部, 7=脚部");
        handler->SendSysMessage("      8=手腕, 9=手套, 10=戒指1, 11=戒指2, 12=饰品1, 13=饰品2");
        handler->SendSysMessage("      14=披风, 15=主手, 16=副手, 17=远程");
        return true;
    }

    // 使用int类型读取参数，避免uint8的内存覆盖问题
    int ascensionSlotInt = 0;
    int bagIdInt = 0;
    int bagSlotInt = 0;

    std::istringstream iss(args);
    if (!(iss >> ascensionSlotInt >> bagIdInt >> bagSlotInt))
    {
        handler->SendSysMessage("参数错误。用法: .飞升 装备 <槽位> <背包ID> <背包槽位>");
        return true;
    }

    uint8 ascensionSlot = static_cast<uint8>(ascensionSlotInt);
    uint8 bagId = static_cast<uint8>(bagIdInt);
    uint8 bagSlot = static_cast<uint8>(bagSlotInt);

    // 通过背包位置获取物品
    // 客户端: bag=0 是主背包，bag=1-4 是额外背包
    // 客户端: slot 从1开始
    Item* item = nullptr;
    if (bagId == 0)
    {
        // 主背包 (客户端bag=0 对应 INVENTORY_SLOT_BAG_0)
        // 客户端slot从1开始，所以 slot-1 得到0-based索引
        // 主背包物品从 INVENTORY_SLOT_ITEM_START (23) 开始
        uint8 serverSlot = INVENTORY_SLOT_ITEM_START + bagSlot - 1;
        item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
    }
    else
    {
        // 其他背包 (客户端bag=1-4 对应背包槽位 INVENTORY_SLOT_BAG_START + bagId - 1)
        uint8 bagSlotIndex = INVENTORY_SLOT_BAG_START + bagId - 1;
        Bag* bag = player->GetBagByPos(bagSlotIndex);
        if (bag)
        {
            // 客户端slot从1开始，所以 slot-1 得到0-based索引
            item = bag->GetItemByPos(bagSlot - 1);
        }
    }

    if (!item)
    {
        handler->SendSysMessage("背包中未找到该物品。");
        return true;
    }

    uint32 itemId = item->GetEntry();
    uint32 itemGuid = item->GetGUID().GetCounter();

    sAscensionManager->EquipItem(player, ascensionSlot, itemId, itemGuid);
    return true;
}

bool AscensionCommandScript::HandleAscensionUnequip(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!*args)
    {
        handler->SendSysMessage("用法: .飞升 卸下 <槽位>");
        return true;
    }

    uint8 slot = atoi(args);
    sAscensionManager->UnequipItem(player, slot);
    return true;
}

bool AscensionCommandScript::HandleAscensionClear(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    sAscensionManager->UnequipAllItems(player);
    return true;
}

bool AscensionCommandScript::HandleAscensionRefresh(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    sAscensionManager->RefreshEffects(player);
    handler->SendSysMessage("飞升装备属性已刷新。");
    return true;
}

bool AscensionCommandScript::HandleAscensionUnlock(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!*args)
    {
        handler->SendSysMessage("用法: .飞升 解锁 <槽位>");
        return true;
    }

    uint8 slot = atoi(args);
    sAscensionManager->UnlockSlot(player, slot);
    return true;
}

bool AscensionCommandScript::HandleAscensionReload(ChatHandler* handler, const char* args)
{
    sAscensionManager->LoadSlotControls();
    handler->SendSysMessage("飞升系统配置已重新加载。");
    return true;
}

//=============================================================================
// 脚本加载函数
//=============================================================================

void AddAscensionSystemScripts()
{
    new AscensionWorldScript();
    new AscensionPlayerScript();
    new AscensionCommandScript();
}

