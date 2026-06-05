#include "ScriptMgr.h"

class DbConfigManagerWorldScript : public WorldScript
{
public:
    DbConfigManagerWorldScript() : WorldScript("DbConfigManagerWorldScript") { }
};

void AddDbConfigManagerScripts()
{
    new DbConfigManagerWorldScript();
}
