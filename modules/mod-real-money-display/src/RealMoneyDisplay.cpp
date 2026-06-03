#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "Util.h"
#include "WorldPacket.h"

namespace
{
    constexpr char const* REAL_MONEY_ADDON_PREFIX = "REALMONEY";

    int128 NormalizeMoney(int128 value)
    {
        if (value <= 0)
            return 0;

        int128 const maxMoney = Acore::Number::GetDecimal65SignedMax();
        return value > maxMoney ? maxMoney : value;
    }

    void SendAddonMessage(Player* player, std::string const& payload)
    {
        if (!player || !player->GetSession() || payload.empty())
            return;

        std::string full = std::string(REAL_MONEY_ADDON_PREFIX) + '\t' + payload;

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, full, 0);
        player->SendDirectMessage(&data);
    }

    void SendMoney(Player* player)
    {
        if (!player)
            return;

        SendAddonMessage(player, "MONEY:" + Acore::ToString(NormalizeMoney(player->GetMoney())));
    }

    void SendMoney(Player* player, int128 const& money)
    {
        SendAddonMessage(player, "MONEY:" + Acore::ToString(NormalizeMoney(money)));
    }
}

class RealMoneyDisplay_PlayerScript : public PlayerScript
{
public:
    RealMoneyDisplay_PlayerScript() : PlayerScript("RealMoneyDisplay_PlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        SendMoney(player);
    }

    void OnPlayerMoneyChanged(Player* player, int64& amount) override
    {
        if (!player)
            return;

        SendMoney(player, player->GetMoney() + static_cast<int128>(amount));
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        std::size_t tab = msg.find('\t');
        if (tab == std::string::npos)
            return;

        if (msg.substr(0, tab) != REAL_MONEY_ADDON_PREFIX)
            return;

        std::string cmd = msg.substr(tab + 1);
        if (cmd == "REQ" || cmd == "REQ_MONEY")
            SendMoney(player);
    }
};

void AddRealMoneyDisplayScripts()
{
    new RealMoneyDisplay_PlayerScript();
}
