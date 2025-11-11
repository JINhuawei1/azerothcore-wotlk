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

#include "AchievementMgr.h"
#include "Hyperlinks.h"
#include "ObjectMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <limits>

static constexpr char HYPERLINK_DATA_DELIMITER = ':';

class HyperlinkDataTokenizer
{
    public:
        HyperlinkDataTokenizer(std::string_view str) : _str(str) {}

        template <typename T>
        bool TryConsumeTo(T& val)
        {
            if (IsEmpty())
                return false;

            if (std::size_t off = _str.find(HYPERLINK_DATA_DELIMITER); off != std::string_view::npos)
            {
                if (!Acore::Hyperlinks::LinkTags::base_tag::StoreTo(val, _str.substr(0, off)))
                    return false;
                _str = _str.substr(off+1);
            }
            else
            {
                if (!Acore::Hyperlinks::LinkTags::base_tag::StoreTo(val, _str))
                    return false;
                _str = std::string_view();
            }

            return true;
        }

        bool IsEmpty() { return _str.empty(); }

    private:
        std::string_view _str;
};

bool Acore::Hyperlinks::LinkTags::achievement::StoreTo(AchievementLinkData& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 achievementId;

    if (!t.TryConsumeTo(achievementId))
        return false;

    val.Achievement = sAchievementMgr->GetAchievement(achievementId);

    if (!(val.Achievement && t.TryConsumeTo(val.CharacterId) && t.TryConsumeTo(val.IsFinished) && t.TryConsumeTo(val.Month) && t.TryConsumeTo(val.Day)))
        return false;

    if ((12 < val.Month) || (31 < val.Day))
        return false;

    int8 year;

    if (!t.TryConsumeTo(year))
        return false;

    if (val.IsFinished) // if finished, year must be >= 0
    {
        if (year < 0)
            return false;
        val.Year = static_cast<uint8>(year);
    }
    else
        val.Year = 0;

    return (t.TryConsumeTo(val.Criteria[0]) && t.TryConsumeTo(val.Criteria[1]) && t.TryConsumeTo(val.Criteria[2]) && t.TryConsumeTo(val.Criteria[3]) && t.IsEmpty());
}

bool Acore::Hyperlinks::LinkTags::enchant::StoreTo(SpellInfo const*& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 spellId;

    if (!(t.TryConsumeTo(spellId) && t.IsEmpty()))
        return false;

    return (val = sSpellMgr->GetSpellInfo(spellId)) && val->HasAttribute(SPELL_ATTR0_IS_TRADESKILL);
}

bool Acore::Hyperlinks::LinkTags::glyph::StoreTo(GlyphLinkData& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 slot, prop;

    if (!(t.TryConsumeTo(slot) && t.TryConsumeTo(prop) && t.IsEmpty()))
        return false;

    if (!(val.Slot = sGlyphSlotStore.LookupEntry(slot)))
        return false;

    if (!(val.Glyph = sGlyphPropertiesStore.LookupEntry(prop)))
        return false;

    return true;
}

bool Acore::Hyperlinks::LinkTags::item::StoreTo(ItemLinkData& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 itemId, dummy;

    if (!t.TryConsumeTo(itemId))
        return false;

    val.Item = sObjectMgr->GetItemTemplate(itemId);
    val.IsBuggedInspectLink = false;

    // randomPropertyId is actually a int16 in the client
    // positive values index ItemRandomSuffix.dbc, while negative values index ItemRandomProperties.dbc
    // however, there is also a client bug in inspect packet handling that causes a int16 to be cast to uint16, then int32 (dropping sign extension along the way)
    // this results in the wrong value being sent in the link; DBC lookup clientside fails, so it sends the link without suffix
    // to detect and allow these invalid links, we first read randomPropertyId as a full int32
    int32 randomPropertyId;
    if (!(val.Item && t.TryConsumeTo(val.EnchantId) && t.TryConsumeTo(val.GemEnchantId[0]) && t.TryConsumeTo(val.GemEnchantId[1]) &&
        t.TryConsumeTo(val.GemEnchantId[2]) && t.TryConsumeTo(dummy) && t.TryConsumeTo(randomPropertyId) && t.TryConsumeTo(val.RandomSuffixBaseAmount) &&
        t.TryConsumeTo(val.RenderLevel) && t.IsEmpty() && !dummy))
        return false;

    if ((static_cast<int32>(std::numeric_limits<int16>::max()) < randomPropertyId) && (randomPropertyId <= std::numeric_limits<uint16>::max()))
    { // this is the bug case, the id we received is actually static_cast<uint16>(i16RandomPropertyId)
        randomPropertyId = static_cast<int16>(randomPropertyId);
        val.IsBuggedInspectLink = true;
    }

    if (randomPropertyId < 0)
    {
        if (!val.Item->RandomSuffix)
            return false;

        if (randomPropertyId < -static_cast<int32>(sItemRandomSuffixStore.GetNumRows()))
            return false;

        if (ItemRandomSuffixEntry const* suffixEntry = sItemRandomSuffixStore.LookupEntry(-randomPropertyId))
        {
            val.RandomSuffix = suffixEntry;
            val.RandomProperty = nullptr;
        }
        else
            return false;
    }
    else if (randomPropertyId > 0)
    {
        // 关键修复：优先判断是否是成长装备/强化装备的GUID标识
        // 成长装备使用两种ID格式：
        // 1. 小值范围（1-1000）：直接存储的物品GUID
        // 2. 大值范围（>= 1000000）：GUID + 偏移量（1000000）
        // 标准DBC随机属性ID通常在 1-10000 范围，但有重叠
        
        // 判断是否是成长装备GUID：
        // - 大值（>= 1000000）：明确是 GUID + 偏移量
        // - 小值（< 1000）：可能是 GUID，也可能是 DBC ID
        bool isPotentialGrowthGuid = (randomPropertyId >= 1000000) || (randomPropertyId < 1000);
        
        if (isPotentialGrowthGuid)
        {
            // 成长装备/强化装备的GUID标识
            // 不检查物品模板的 RandomProperty 标志
            // 不检查DBC表
            // 直接允许通过，让成长系统自行处理
            val.RandomSuffix = nullptr;
            val.RandomProperty = nullptr;
            // 验证通过
        }
        else if (ItemRandomPropertiesEntry const* propEntry = sItemRandomPropertiesStore.LookupEntry(randomPropertyId))
        {
            // 标准随机属性：在DBC中找到了，且不在成长GUID范围内
            // 此时检查物品是否支持随机属性
            if (!val.Item->RandomProperty)
                return false;  // 物品不支持随机属性，但链接中有DBC随机属性 → 验证失败
                
            val.RandomSuffix = nullptr;
            val.RandomProperty = propEntry;
        }
        else
        {
            // 不在成长GUID范围，也不在DBC中
            // 可能是其他系统使用的ID，允许通过
            val.RandomSuffix = nullptr;
            val.RandomProperty = nullptr;
        }
    }
    else
    {
        val.RandomSuffix = nullptr;
        val.RandomProperty = nullptr;
    }

    // 修改验证逻辑：允许物品有seed但没有随机后缀（用于成长系统、强化系统等）
    // 原逻辑：RandomSuffix和seed必须同时存在或同时不存在
    // 新逻辑：只检查如果有RandomSuffix则必须有seed，但允许只有seed没有RandomSuffix
    if (val.RandomSuffix && !val.RandomSuffixBaseAmount)
        return false;  // 有随机后缀但没有seed → 验证失败
    // 移除了 (val.RandomSuffixBaseAmount && !val.RandomSuffix) 条件
    // 现在允许：有seed但没有随机后缀 → 验证通过（用于成长/强化装备）

    return true;
}

bool Acore::Hyperlinks::LinkTags::quest::StoreTo(QuestLinkData& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 questId;

    if (!t.TryConsumeTo(questId))
        return false;

    return (val.Quest = sObjectMgr->GetQuestTemplate(questId)) && t.TryConsumeTo(val.QuestLevel) && (val.QuestLevel >= -1) && t.IsEmpty();
}

bool Acore::Hyperlinks::LinkTags::spell::StoreTo(SpellInfo const*& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 spellId;

    if (!(t.TryConsumeTo(spellId) && t.IsEmpty()))
        return false;

    return !!(val = sSpellMgr->GetSpellInfo(spellId));
}

bool Acore::Hyperlinks::LinkTags::talent::StoreTo(TalentLinkData& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 talentId;
    int8 rank; // talent links contain <learned rank>-1, we store <learned rank>

    if (!(t.TryConsumeTo(talentId) && t.TryConsumeTo(rank) && t.IsEmpty()))
        return false;

    if (rank < -1 || rank >= MAX_TALENT_RANK)
        return false;

    val.Talent = sTalentStore.LookupEntry(talentId);
    val.Rank = rank+1;

    if (!val.Talent)
        return false;

    if (val.Rank > 0)
    {
        uint32 const spellId = val.Talent->RankID[val.Rank - 1];
        if (!spellId)
            return false;

        val.Spell = sSpellMgr->GetSpellInfo(spellId);

        if (!val.Spell)
            return false;
    }
    else
    {
        val.Spell = nullptr;
    }

    return true;
}

bool Acore::Hyperlinks::LinkTags::trade::StoreTo(TradeskillLinkData& val, std::string_view text)
{
    HyperlinkDataTokenizer t(text);
    uint32 spellId;

    if (!t.TryConsumeTo(spellId))
        return false;

    val.Spell = sSpellMgr->GetSpellInfo(spellId);

    return (val.Spell && val.Spell->Effects[0].Effect == SPELL_EFFECT_TRADE_SKILL && t.TryConsumeTo(val.CurValue) &&
        t.TryConsumeTo(val.MaxValue) && t.TryConsumeTo(val.Owner) && t.TryConsumeTo(val.KnownRecipes) && t.IsEmpty());
}
