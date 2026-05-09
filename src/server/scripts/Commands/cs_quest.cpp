/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: quest_commandscript
%Complete: 100
Comment: All quest related commands
Category: commandscripts
EndScriptData */

#include "Chat.h"
#include "CommandScript.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReputationMgr.h"
#include <limits>

using namespace Acore::ChatCommands;

class quest_commandscript : public CommandScript
{
public:
    quest_commandscript() : CommandScript("quest_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable questCommandTable =
        {
            { "add",      HandleQuestAdd,      SEC_GAMEMASTER, Console::Yes },
            { "complete", HandleQuestComplete, SEC_GAMEMASTER, Console::Yes },
            { "remove",   HandleQuestRemove,   SEC_GAMEMASTER, Console::Yes },
            { "reward",   HandleQuestReward,   SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "quest", questCommandTable },
        };
        return commandTable;
    }

    static bool HandleQuestAdd(ChatHandler* handler, Quest const* quest, Optional<PlayerIdentifier> playerTarget)
    {
        if (!playerTarget)
        {
            playerTarget = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!playerTarget)
        {
            handler->SendErrorMessage(LANG_PLAYER_NOT_FOUND);
            return false;
        }

        uint32 entry = quest->GetQuestId();

        // check item starting quest (it can work incorrectly if added without item in inventory)
        ItemTemplateContainer const* itc = sObjectMgr->GetItemTemplateStore();
        ItemTemplateContainer::const_iterator result = find_if(itc->begin(), itc->end(), Finder<uint32, ItemTemplate>(entry, &ItemTemplate::StartQuest));

        if (result != itc->end())
        {
            handler->SendErrorMessage(LANG_COMMAND_QUEST_STARTFROMITEM, entry, result->second.ItemId);
            return false;
        }

        if (Player* player = playerTarget->GetConnectedPlayer())
        {
            if (player->IsActiveQuest(entry))
            {
                handler->SendErrorMessage(LANG_COMMAND_QUEST_ACTIVE, quest->GetTitle().c_str(), entry);
                return false;
            }

            // ok, normal (creature/GO starting) quest
            if (player->CanAddQuest(quest, true))
            {
                player->RemoveRewardedQuest(entry, false);
                player->AddQuestAndCheckCompletion(quest, nullptr);
            }
        }
        else
        {
            ObjectGuid::LowType guid = playerTarget->GetGUID().GetCounter();
            QueryResult result = CharacterDatabase.Query("SELECT 1 FROM character_queststatus WHERE guid = {} AND quest = {}", guid, entry);

            if (result)
            {
                handler->SendErrorMessage(LANG_COMMAND_QUEST_ACTIVE, quest->GetTitle().c_str(), entry);
                return false;
            }

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST);
            stmt->SetData(0, guid);
            stmt->SetData(1, entry);
            CharacterDatabase.Execute(stmt);

            uint8 index = 0;

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHAR_QUESTSTATUS);
            stmt->SetData(index++, guid);
            stmt->SetData(index++, entry);
            stmt->SetData(index++, 1);
            stmt->SetData(index++, false);
            stmt->SetData(index++, 0);

            for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
            {
                stmt->SetData(index++, 0);
            }

            for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
            {
                stmt->SetData(index++, 0);
            }

            stmt->SetData(index, 0);

            CharacterDatabase.Execute(stmt);
        }

        handler->PSendSysMessage(LANG_COMMAND_QUEST_ADD, quest->GetTitle(), entry);
        handler->SetSentErrorMessage(false);
        return true;
    }

    static bool HandleQuestRemove(ChatHandler* handler, Quest const* quest, Optional<PlayerIdentifier> playerTarget)
    {
        if (!playerTarget)
        {
            playerTarget = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!playerTarget)
        {
            handler->SendErrorMessage(LANG_PLAYER_NOT_FOUND);
            return false;
        }

        uint32 entry = quest->GetQuestId();

        if (!quest)
        {
            handler->SendErrorMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            return false;
        }

        if (Player* player = playerTarget->GetConnectedPlayer())
        {
            // remove all quest entries for 'entry' from quest log
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 logQuest = player->GetQuestSlotQuestId(slot);
                if (logQuest == entry)
                {
                    player->SetQuestSlot(slot, 0);
                    player->SendQuestSlotUpdate(slot);

                    // we ignore unequippable quest items in this case, its' still be equipped
                    player->TakeQuestSourceItem(logQuest, false);

                    if (quest->HasFlag(QUEST_FLAGS_FLAGS_PVP))
                    {
                        player->pvpInfo.IsHostile = player->pvpInfo.IsInHostileArea || player->HasPvPForcingQuest();
                        player->UpdatePvPState();
                    }
                }
            }

            player->RemoveRewardedQuest(entry, false);
            player->RemoveActiveQuest(entry);
        }
        else
        {
            ObjectGuid::LowType guid = playerTarget->GetGUID().GetCounter();
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST);
            stmt->SetData(0, guid);
            stmt->SetData(1, entry);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_BY_QUEST);
            stmt->SetData(0, guid);
            stmt->SetData(1, entry);
            trans->Append(stmt);

            for (uint32 const& requiredItem : quest->RequiredItemId)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_INVENTORY_ITEM_BY_ENTRY_AND_OWNER);
                stmt->SetData(0, requiredItem);
                stmt->SetData(1, guid);

                PreparedQueryResult result = CharacterDatabase.Query(stmt);

                if (result)
                {
                    Field* fields = result->Fetch();

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
                    stmt->SetData(0, fields[0].Get<uint32>());
                    trans->Append(stmt);

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
                    stmt->SetData(0, fields[0].Get<uint32>());
                    trans->Append(stmt);
                }
            }

            CharacterDatabase.CommitTransaction(trans);
        }

        handler->PSendSysMessage(LANG_COMMAND_QUEST_REMOVED, quest->GetTitle(), entry);
        handler->SetSentErrorMessage(false);
        return true;
    }

    static bool CanQuestCompleteSupplyRequiredItem(Quest const* quest, ItemTemplate const* itemTemplate, uint32 itemId)
    {
        if (!quest || !itemTemplate || !itemId)
            return false;

        if (quest->GetSrcItemId() == itemId)
            return false;

        if (itemTemplate->Class != ITEM_CLASS_QUEST)
            return false;

        if (itemTemplate->SellPrice > 0 || itemTemplate->InventoryType != INVTYPE_NON_EQUIP)
            return false;

        return true;
    }

    static bool HandleQuestComplete(ChatHandler* handler, Quest const* quest, Optional<PlayerIdentifier> playerTarget)
    {
        if (!playerTarget)
        {
            playerTarget = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!playerTarget)
        {
            handler->SendErrorMessage(LANG_PLAYER_NOT_FOUND);
            return false;
        }

        uint32 entry = quest->GetQuestId();

        // 黑名单: 禁止 .quest complete 直接完成 modules/物品数据 模块下的任务.
        // 这些任务 (远古战袍 70001-71000, 远古衬衫 71001-72000) 需求量极大且物品可出售换钱,
        // 用 .quest complete 一键完成会被反复利用刷金币 (接取->complete->卖出->放弃->再接取).
        // 如需新增其他黑名单区段, 在下方数组追加 {start, end} 即可.
        static constexpr struct { uint32 start; uint32 end; } kBlockedQuestRanges[] = {
            { 70001, 72000 }, // modules/物品数据 (远古战袍/衬衫任务框架)
        };
        for (auto const& range : kBlockedQuestRanges)
        {
            if (entry >= range.start && entry <= range.end)
            {
                handler->PSendSysMessage("[.quest complete] 任务 [{}] (entry {}) 属于受保护区段 [{}, {}], 命令拒绝执行. 请让玩家正常完成任务.",
                    quest->GetTitle(), entry, range.start, range.end);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        if (Player* player = playerTarget->GetConnectedPlayer())
        {
            // If player doesn't have the quest
            if (!player->IsActiveQuest(entry))
            {
                handler->SendErrorMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
                return false;
            }

            // Add quest items strictly based on the quest's RequiredItemCount.
            // 严格按照任务 RequiredItemCount 补足玩家持有量, 不依赖物品堆叠上限,
            // 防止玩家通过 接取->.quest complete->出售->放弃->再接取 的循环刷金币.
            bool canComplete = true;

            for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
            {
                uint32 id    = quest->RequiredItemId[x];
                uint32 count = quest->RequiredItemCount[x];
                if (!id || !count)
                {
                    continue;
                }

                // 校验物品模板, 缺失的物品直接告警跳过, 避免 CreateItem 失败后吞掉错误.
                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(id);
                if (!itemTemplate)
                {
                    handler->PSendSysMessage("[.quest complete] 任务 {} 物品 entry={}: 物品模板不存在, 已拒绝完成.",
                        entry, id);
                    canComplete = false;
                    continue;
                }

                if (!CanQuestCompleteSupplyRequiredItem(quest, itemTemplate, id))
                {
                    handler->PSendSysMessage("[.quest complete] 任务 {} 物品 [{}] (entry {}): 不是安全任务物品, 已拒绝完成. 请通过正常任务流程获取.",
                        entry, itemTemplate->Name1, id);
                    canComplete = false;
                    continue;
                }

                // 修复 "秒任务后任务品丢失 + .quest reward 报 NOTFOUND":
                // 旧逻辑 "curItemCount >= count 则 continue" 在玩家恰好已凑齐任务品时, 跳过补发,
                // 但随后 RewardQuest::DestroyItemCount 仍会按 RequiredItemCount 扣物品, 直接吃掉玩家私人库存,
                // 玩家感知是 GM 秒任务后任务品凭空消失. 与此同时 CompleteQuest 已经把 m_QuestStatus 设成 COMPLETE,
                // 中间任一钩子 (例如 mod-abyss-cultivation::HandleQuestCompletion) 触发额外路径就可能让客户端
                // 任务栏与服务端状态分歧, 触发 "需要小退才刷新" 现象.
                //
                // 改为强制覆盖: 先把同 entry 任务品全部销毁, 再补满 count 个.
                // 因为 CanQuestCompleteSupplyRequiredItem 已经把这条路径限制在
                // ITEM_CLASS_QUEST + SellPrice == 0 + INVTYPE_NON_EQUIP, 这类物品本身不可出售/装备/交易,
                // 销毁玩家原有库存不会造成经济损失, 也彻底切断 "complete -> sell -> abandon -> add" 刷金循环.
                uint32 curItemCount = player->GetItemCount(id, true);
                if (curItemCount > 0)
                {
                    player->DestroyItemCount(id, curItemCount, true);
                    handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 已先清空原持有 {} 个, 改为按任务需求重发 {} 个.",
                        itemTemplate->Name1, id, curItemCount, count);
                }

                uint32 needCount = count;

                // 受 item_template.MaxCount (单人持有上限) 约束: <= 0 表示无限制.
                // 销毁后 curItemCount 已清零, 只需校验 count 本身是否在上限内.
                if (itemTemplate->MaxCount > 0)
                {
                    uint32 ownerLimit = static_cast<uint32>(itemTemplate->MaxCount);
                    if (needCount > ownerLimit)
                    {
                        handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 任务需求 {} 超过单人持有上限 MaxCount={}, 已拒绝完成.",
                            itemTemplate->Name1, id, needCount, ownerLimit);
                        canComplete = false;
                        continue;
                    }
                }

                // 尝试占位; no_space_count 反馈出无法放下的数量, 便于做部分补充.
                ItemPosCountVec dest;
                uint32 noSpaceCount = 0;
                InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, needCount, &noSpaceCount);

                if (msg != EQUIP_ERR_OK)
                {
                    // 完全放不下: 尝试只补可放下的部分, 仍放不下则报错放弃该物品.
                    if (noSpaceCount >= needCount)
                    {
                        handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 玩家背包/银行已满, 无法补充 {} 个 (错误码 {}), 已拒绝完成.",
                            itemTemplate->Name1, id, needCount, uint32(msg));
                        canComplete = false;
                        continue;
                    }

                    uint32 fitCount = needCount - noSpaceCount;
                    dest.clear();
                    msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, fitCount);
                    if (msg != EQUIP_ERR_OK || fitCount == 0)
                    {
                        handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 无法存储任意数量 (错误码 {}), 已拒绝完成.",
                            itemTemplate->Name1, id, uint32(msg));
                        canComplete = false;
                        continue;
                    }

                    if (Item* item = player->StoreNewItem(dest, id, true))
                    {
                        player->SendNewItem(item, fitCount, true, false);
                        handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 只能部分补充 {}/{} (剩余 {} 个空间不足), 已拒绝完成.",
                            itemTemplate->Name1, id, fitCount, needCount, noSpaceCount);
                    }
                    canComplete = false;
                    continue;
                }

                if (Item* item = player->StoreNewItem(dest, id, true))
                {
                    player->SendNewItem(item, needCount, true, false);
                    handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 已补充 {} 个 (持有 {}/{}).",
                        itemTemplate->Name1, id, needCount, needCount, count);
                }
                else
                {
                    handler->PSendSysMessage("[.quest complete] 物品 [{}] (entry {}): 创建失败, 已拒绝完成.",
                        itemTemplate->Name1, id);
                    canComplete = false;
                }
            }

            if (!canComplete)
            {
                handler->SetSentErrorMessage(true);
                return false;
            }

            // All creature/GO slain/casted (not required, but otherwise it will display "Creature slain 0/10")
            for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            {
                int32  creature      = quest->RequiredNpcOrGo[i];
                uint32 creatureCount = quest->RequiredNpcOrGoCount[i];

                if (creature > 0)
                {
                    if (CreatureTemplate const* creatureInfo = sObjectMgr->GetCreatureTemplate(creature))
                    {
                        for (uint16 z = 0; z < creatureCount; ++z)
                        {
                            player->KilledMonster(creatureInfo, ObjectGuid::Empty);
                        }
                    }
                }
                else if (creature < 0)
                {
                    for (uint16 z = 0; z < creatureCount; ++z)
                    {
                        player->KillCreditGO(creature);
                    }
                }
            }

            // player kills
            if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_PLAYER_KILL))
            {
                if (uint32 reqPlayers = quest->GetPlayersSlain())
                {
                    player->KilledPlayerCreditForQuest(reqPlayers, quest);
                }
            }

            // If the quest requires reputation to complete
            if (uint32 repFaction = quest->GetRepObjectiveFaction())
            {
                uint32 repValue = quest->GetRepObjectiveValue();
                uint32 curRep   = player->GetReputationMgr().GetReputation(repFaction);
                if (curRep < repValue)
                {
                    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(repFaction))
                    {
                        player->GetReputationMgr().SetReputation(factionEntry, static_cast<float>(repValue));
                    }
                }
            }

            // If the quest requires a SECOND reputation to complete
            if (uint32 repFaction = quest->GetRepObjectiveFaction2())
            {
                uint32 repValue2 = quest->GetRepObjectiveValue2();
                uint32 curRep    = player->GetReputationMgr().GetReputation(repFaction);
                if (curRep < repValue2)
                {
                    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(repFaction))
                    {
                        player->GetReputationMgr().SetReputation(factionEntry, static_cast<float>(repValue2));
                    }
                }
            }

            // 修复刷金 bug:
            // 旧代码在 RewardMoney < 0 (任务需要玩家"付钱"才能交) 时,
            // 反而调用 ModifyMoney(-ReqOrRewMoney) 把这笔钱"赠送"给玩家,
            // 导致 .quest complete 9211 之类的命令可被反复使用刷金币.
            // 这里直接移除该分支: GM 秒任务命令不再发放/扣除任何金币,
            // 真正的金币奖励/扣除由正常的任务接受/交付流程处理.

            player->RemoveRewardedQuest(entry, false);
            player->CompleteQuest(entry);
        }
        else
        {
            ObjectGuid::LowType guid = playerTarget->GetGUID().GetCounter();
            QueryResult result = CharacterDatabase.Query("SELECT 1 FROM character_queststatus WHERE guid = {} AND quest = {}", guid, entry);

            if (!result)
            {
                handler->SendErrorMessage(LANG_COMMAND_QUEST_NOT_FOUND_IN_LOG, quest->GetTitle(), entry);
                return false;
            }

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST);
            stmt->SetData(0, guid);
            stmt->SetData(1, entry);
            trans->Append(stmt);

            typedef std::pair<uint32, uint32> items;
            std::vector<items> questItems;
            bool canComplete = true;

            for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
            {
                uint32 id    = quest->RequiredItemId[x];
                uint32 count = quest->RequiredItemCount[x];
                if (!id || !count)
                {
                    continue;
                }

                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(id);
                if (!itemTemplate)
                {
                    handler->PSendSysMessage("[.quest complete] (离线) 任务 {} 物品 entry={}: 物品模板不存在, 已拒绝完成.",
                        entry, id);
                    canComplete = false;
                    continue;
                }

                if (!CanQuestCompleteSupplyRequiredItem(quest, itemTemplate, id))
                {
                    handler->PSendSysMessage("[.quest complete] (离线) 任务 {} 物品 [{}] (entry {}): 不是安全任务物品, 已拒绝完成. 请通过正常任务流程获取.",
                        entry, itemTemplate->Name1, id);
                    canComplete = false;
                    continue;
                }

                // 离线场景下查 character_inventory + item_instance, 统计玩家已持有的同类物品总量.
                // 仅按差额补发, 防止反复执行后角色登录时累积过多物品被卖出换金.
                uint32 curItemCount = 0;
                if (QueryResult invResult = CharacterDatabase.Query(
                    "SELECT CAST(COALESCE(SUM(ii.count), 0) AS UNSIGNED) FROM character_inventory ci "
                    "INNER JOIN item_instance ii ON ii.guid = ci.item "
                    "WHERE ii.itemEntry = {} AND ii.owner_guid = {}", id, guid))
                {
                    Field* fields = invResult->Fetch();
                    uint64 raw = fields[0].Get<uint64>();
                    curItemCount = raw > std::numeric_limits<uint32>::max()
                        ? std::numeric_limits<uint32>::max()
                        : static_cast<uint32>(raw);
                }

                if (curItemCount >= count)
                {
                    continue;
                }

                uint32 needCount = count - curItemCount;

                // 受 item_template.MaxCount 单人持有上限约束.
                if (itemTemplate->MaxCount > 0)
                {
                    uint32 ownerLimit = static_cast<uint32>(itemTemplate->MaxCount);
                    if (curItemCount >= ownerLimit)
                    {
                        handler->PSendSysMessage("[.quest complete] (离线) 物品 [{}] (entry {}): 已达单人持有上限 MaxCount={}, 已拒绝完成.",
                            itemTemplate->Name1, id, ownerLimit);
                        canComplete = false;
                        continue;
                    }
                    uint32 allowedAdd = ownerLimit - curItemCount;
                    if (needCount > allowedAdd)
                    {
                        handler->PSendSysMessage("[.quest complete] (离线) 物品 [{}] (entry {}): 受 MaxCount={} 限制, 差额 {} 但只能补 {}, 已拒绝完成.",
                            itemTemplate->Name1, id, ownerLimit, needCount, allowedAdd);
                        canComplete = false;
                        continue;
                    }
                }

                questItems.emplace_back(id, needCount);
                handler->PSendSysMessage("[.quest complete] (离线) 物品 [{}] (entry {}): 准备邮件补发 {} 个 (已有 {}/{}).",
                    itemTemplate->Name1, id, needCount, curItemCount, count);
            }

            if (!canComplete)
            {
                handler->SetSentErrorMessage(true);
                return false;
            }

            if (!questItems.empty())
            {
                MailSender sender(MAIL_NORMAL, guid, MAIL_STATIONERY_GM);
                // fill mail
                MailDraft draft(quest->GetTitle(), std::string());
                bool mailItemsCreated = true;

                for (auto const& itr : questItems)
                {
                    if (Item* item = Item::CreateItem(itr.first, itr.second))
                    {
                        item->SaveToDB(trans);
                        draft.AddItem(item);
                    }
                    else
                    {
                        mailItemsCreated = false;
                    }
                }

                if (!mailItemsCreated)
                {
                    handler->PSendSysMessage("[.quest complete] (离线) 任务 {}: 邮件补发任务物品失败, 已拒绝完成.", entry);
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                draft.SendMailTo(trans, MailReceiver(nullptr, guid), sender);
            }

            uint8 index = 0;

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHAR_QUESTSTATUS);
            stmt->SetData(index++, guid);
            stmt->SetData(index++, entry);
            stmt->SetData(index++, 1);
            stmt->SetData(index++, quest->HasFlag(QUEST_FLAGS_EXPLORATION));
            stmt->SetData(index++, 0);

            for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
            {
                stmt->SetData(index++, quest->RequiredNpcOrGoCount[i]);
            }

            for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
            {
                // Will be updated once they loot the items from the mailbox.
                stmt->SetData(index++, 0);
            }

            stmt->SetData(index, 0);

            trans->Append(stmt);

            // If the quest requires reputation to complete, set the player rep to the required amount.
            if (uint32 repFaction = quest->GetRepObjectiveFaction())
            {
                uint32 repValue = quest->GetRepObjectiveValue();

                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_REP_BY_FACTION);
                stmt->SetData(0, repFaction);
                stmt->SetData(1, guid);
                PreparedQueryResult result = CharacterDatabase.Query(stmt);

                if (result)
                {
                    Field* fields = result->Fetch();
                    uint32 curRep = fields[0].Get<uint32>();

                    if (curRep < repValue)
                    {
                        if (sFactionStore.LookupEntry(repFaction))
                        {
                            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_REP_FACTION_CHANGE);
                            stmt->SetData(0, repFaction);
                            stmt->SetData(1, repValue);
                            stmt->SetData(2, repFaction);
                            stmt->SetData(3, guid);
                            trans->Append(stmt);
                        }
                    }
                }
            }

            // If the quest requires reputation to complete, set the player rep to the required amount.
            if (uint32 repFaction = quest->GetRepObjectiveFaction2())
            {
                uint32 repValue = quest->GetRepObjectiveValue();

                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_REP_BY_FACTION);
                stmt->SetData(0, repFaction);
                stmt->SetData(1, guid);
                PreparedQueryResult result = CharacterDatabase.Query(stmt);

                if (result)
                {
                    Field* fields = result->Fetch();
                    uint32 curRep = fields[0].Get<uint32>();

                    if (curRep < repValue)
                    {
                        if (sFactionStore.LookupEntry(repFaction))
                        {
                            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_REP_FACTION_CHANGE);
                            stmt->SetData(0, repFaction);
                            stmt->SetData(1, repValue);
                            stmt->SetData(2, repFaction);
                            stmt->SetData(3, guid);
                            trans->Append(stmt);
                        }
                    }
                }
            }

            CharacterDatabase.CommitTransaction(trans);
        }

        // check if Quest Tracker is enabled
        if (sWorld->getBoolConfig(CONFIG_QUEST_ENABLE_QUEST_TRACKER))
        {
            // prepare Quest Tracker datas
            auto stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_QUEST_TRACK_GM_COMPLETE);
            stmt->SetData(0, entry);
            stmt->SetData(1, playerTarget->GetGUID().GetCounter());

            // add to Quest Tracker
            CharacterDatabase.Execute(stmt);
        }

        handler->PSendSysMessage(LANG_COMMAND_QUEST_COMPLETE, quest->GetTitle(), entry);
        handler->SetSentErrorMessage(false);
        return true;
    }

    static bool HandleQuestReward(ChatHandler* handler, Quest const* quest, Optional<PlayerIdentifier> playerTarget)
    {
        if (!playerTarget)
        {
            playerTarget = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!playerTarget)
        {
            handler->SendErrorMessage(LANG_PLAYER_NOT_FOUND);
            return false;
        }

        uint32 entry = quest->GetQuestId();

        if (Player* player = playerTarget->GetConnectedPlayer())
        {
            // If player doesn't have the quest
            if (player->GetQuestStatus(entry) != QUEST_STATUS_COMPLETE)
            {
                handler->SendErrorMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
                return false;
            }

            player->RewardQuest(quest, 0, player);
        }
        else
        {
            // Achievement criteria updates correctly the next time a quest is rewarded.
            // Titles are already awarded correctly the next time they login (only one quest awards title - 11549).
            // Rewarded talent points (Death Knights) and spells (e.g Druid forms) are also granted on login.
            // No reputation gains - too troublesome to calculate them when the player is offline.

            ObjectGuid::LowType guid = playerTarget->GetGUID().GetCounter();
            uint8 charLevel = sCharacterCache->GetCharacterLevelByGuid(ObjectGuid(HighGuid::Player, guid));
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            CharacterDatabasePreparedStatement* stmt;

            QueryResult result = CharacterDatabase.Query("SELECT 1 FROM character_queststatus WHERE guid = {} AND quest = {} AND status = 1", guid, entry);

            if (!result)
            {
                handler->SendErrorMessage(LANG_COMMAND_QUEST_NOT_COMPLETE);
                return false;
            }

            for (uint32 const& requiredItem : quest->RequiredItemId)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_INVENTORY_ITEM_BY_ENTRY_AND_OWNER);
                stmt->SetData(0, requiredItem);
                stmt->SetData(1, guid);

                PreparedQueryResult result = CharacterDatabase.Query(stmt);

                if (result)
                {
                    Field* fields = result->Fetch();

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
                    stmt->SetData(0, fields[0].Get<uint32>());
                    trans->Append(stmt);

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
                    stmt->SetData(0, fields[0].Get<uint32>());
                    trans->Append(stmt);
                }
            }

            for (uint32 const& sourceItem : quest->ItemDrop)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_INVENTORY_ITEM_BY_ENTRY_AND_OWNER);
                stmt->SetData(0, sourceItem);
                stmt->SetData(1, guid);

                PreparedQueryResult result = CharacterDatabase.Query(stmt);

                if (result)
                {
                    Field* fields = result->Fetch();

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
                    stmt->SetData(0, fields[0].Get<uint32>());
                    trans->Append(stmt);

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
                    stmt->SetData(0, fields[0].Get<uint32>());
                    trans->Append(stmt);
                }
            }

            typedef std::pair<uint32, uint32> items;
            std::vector<items> questRewardItems;

            if (quest->GetRewChoiceItemsCount())
            {
                for (uint32 const& itemId : quest->RewardChoiceItemId)
                {
                    uint8 index = 0;
                    questRewardItems.emplace_back(itemId, quest->RewardChoiceItemCount[index++]);
                }
            }

            if (quest->GetRewItemsCount())
            {
                for (uint32 const& itemId : quest->RewardItemId)
                {
                    uint8 index = 0;
                    questRewardItems.emplace_back(itemId, quest->RewardItemIdCount[index++]);
                }
            }

            if (!questRewardItems.empty())
            {
                MailSender sender(MAIL_NORMAL, guid, MAIL_STATIONERY_GM);
                // fill mail
                MailDraft draft(quest->GetTitle(), "This quest has been manually rewarded to you. This mail contains your quest rewards.");

                for (auto const& itr : questRewardItems)
                {
                    if (!itr.first || !itr.second)
                    {
                        continue;
                    }

                    // Skip invalid items.
                    if (!sObjectMgr->GetItemTemplate(itr.first))
                    {
                        continue;
                    }

                    if (Item* item = Item::CreateItem(itr.first, itr.second))
                    {
                        item->SaveToDB(trans);
                        draft.AddItem(item);
                    }
                }

                draft.SendMailTo(trans, MailReceiver(nullptr, guid), sender);
            }

            // Send quest giver mail, if any.
            if (uint32 mail_template_id = quest->GetRewMailTemplateId())
            {
                if (quest->GetRewMailSenderEntry() != 0)
                {
                    MailDraft(mail_template_id).SendMailTo(trans, MailReceiver(nullptr, guid), quest->GetRewMailSenderEntry(), MAIL_CHECK_MASK_HAS_BODY, quest->GetRewMailDelaySecs());
                }
            }

            if (quest->IsDaily() || quest->IsDFQuest())
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_DAILYQUESTSTATUS);
                stmt->SetData(0, guid);
                stmt->SetData(1, entry);
                stmt->SetData(2, GameTime::GetGameTime().count());
                trans->Append(stmt);
            }
            else if (quest->IsWeekly())
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_WEEKLYQUESTSTATUS);
                stmt->SetData(0, guid);
                stmt->SetData(1, entry);
                trans->Append(stmt);
            }
            else if (quest->IsMonthly())
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MONTHLYQUESTSTATUS);
                stmt->SetData(0, guid);
                stmt->SetData(1, entry);
                trans->Append(stmt);
            }
            else if (quest->IsSeasonal())
            {
                // We can't know which event is the quest linked to, so we can't do anything about this.
                /* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_SEASONALQUESTSTATUS);
                stmt->SetData(0, guid);
                stmt->SetData(1, entry);
                stmt->SetData(2, event_id);
                trans->Append(stmt);*/
            }

            if (uint32 honor = quest->CalculateHonorGain(charLevel))
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_HONOR_POINTS_ACCUMULATIVE);
                stmt->SetData(0, honor);
                stmt->SetData(1, guid);
                trans->Append(stmt);
            }

            if (quest->GetRewArenaPoints())
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_ARENA_POINTS_ACCUMULATIVE);
                stmt->SetData(0, quest->GetRewArenaPoints());
                stmt->SetData(1, guid);
                trans->Append(stmt);
            }

            int64 rewMoney = 0;

            if (charLevel >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
            {
                rewMoney = quest->GetRewMoneyMaxLevel();
            }
            else
            {
                // Some experience might get lost on level up.
                uint32 xp = uint32(quest->XPValue(charLevel) * sWorld->getRate(RATE_XP_QUEST));
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_XP_ACCUMULATIVE);
                stmt->SetData(0, xp);
                stmt->SetData(1, guid);
                trans->Append(stmt);
            }

            if (int64 rewOrReqMoney = quest->GetRewOrReqMoney(charLevel))
            {
                rewMoney += rewOrReqMoney;
            }

            // Only reward money, don't subtract, let's not cause an overflow...
            if (rewMoney > 0)
            {
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_MONEY_ACCUMULATIVE);
                stmt->SetData(0, rewMoney);
                stmt->SetData(1, guid);
                trans->Append(stmt);
            }

            if (!quest->IsDFQuest() && !quest->IsDailyOrWeekly() && !quest->IsMonthly())
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_QUESTSTATUS_REWARDED);
                stmt->SetData(0, guid);
                stmt->SetData(1, entry);
                trans->Append(stmt);
            }

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_BY_QUEST);
            stmt->SetData(0, guid);
            stmt->SetData(1, entry);
            trans->Append(stmt);

            CharacterDatabase.CommitTransaction(trans);
        }

        handler->PSendSysMessage(LANG_COMMAND_QUEST_REWARDED, quest->GetTitle(), entry);
        handler->SetSentErrorMessage(false);
        return true;
    }
};

void AddSC_quest_commandscript()
{
    new quest_commandscript();
}
