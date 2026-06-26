#include "Log.h"
#include "ScriptMgr.h"

void AddHermesBridgePacketBridgeScripts();

class HermesBridgeWorldScript : public WorldScript
{
public:
    HermesBridgeWorldScript() : WorldScript("HermesBridgeWorldScript",
        {
            WORLDHOOK_ON_STARTUP
        })
    {
    }

    void OnStartup() override
    {
        LOG_INFO("server.loading", "HermesBridge: module loaded, custom opcodes CMSG_HERMES_BRIDGE/SMSG_HERMES_BRIDGE are available.");
        LOG_INFO("server.loading", "HermesBridge: client auto injection is launcher-controlled; worldserver will not start local injectors.");
    }
};

void AddHermesBridgeScripts()
{
    new HermesBridgeWorldScript();
    AddHermesBridgePacketBridgeScripts();
}
