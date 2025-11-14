#include "PluginManager.h"
#include "World.h"
#include "EventProcessor.h"

// 前向声明
void AddPluginManager_CommandsScripts();

PluginManager* PluginManager::_instance = nullptr;

PluginManager* PluginManager::instance()
{
    if (!_instance)
        _instance = new PluginManager();
    return _instance;
}

PluginManager::PluginManager()
    : _enabled(sConfigMgr->GetOption<bool>("PluginManager.Enabled", true)),
      _commandPermissionLevel(sConfigMgr->GetOption<int32>("PluginManager.CommandLevel", SEC_ADMINISTRATOR)),
      _nextPluginId(1)
{
}

void PluginManager::Initialize()
{
    if (!_enabled)
    {
        LOG_INFO("module", ">> 插件管理器模块已禁用");
        return;
    }

    LOG_INFO("module", ">> 正在初始化插件管理器模块...");
    LoadAllPlugins();
    LOG_INFO("module", "   已加载 {} 个插件", static_cast<uint32>(_plugins.size()));
    LOG_INFO("module", ">> 插件管理器模块初始化完成!");
}

void PluginManager::LoadAllPlugins()
{
    _plugins.clear();
    LoadPluginsFromDatabase();
}

void PluginManager::LoadPluginsFromDatabase()
{
    QueryResult result = WorldDatabase.Query("SELECT `插件ID`, `插件名称`, `显示名称`, `插件描述`, `插件类型`, `显示位置`, `X坐标`, `Y坐标`, `宽度`, `高度`, `排序顺序`, `图标路径`, `启用状态` FROM `插件管理器` ORDER BY `排序顺序` ASC");

    if (!result)
    {
        LOG_DEBUG("module", "插件管理器: 没有找到任何插件配置");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        PluginEntry entry;

        entry.pluginId = fields[0].Get<uint32>();
        entry.pluginName = fields[1].Get<std::string>();
        entry.displayName = fields[2].Get<std::string>();
        entry.description = fields[3].Get<std::string>();
        entry.type = static_cast<PluginType>(fields[4].Get<uint32>());
        entry.position = static_cast<PluginPosition>(fields[5].Get<uint32>());
        entry.positionX = fields[6].Get<uint32>();
        entry.positionY = fields[7].Get<uint32>();
        entry.width = fields[8].Get<uint32>();
        entry.height = fields[9].Get<uint32>();
        entry.sortOrder = fields[10].Get<uint32>();
        entry.iconPath = fields[11].Get<std::string>();
        entry.enabled = fields[12].Get<bool>();

        _plugins.push_back(entry);
        _nextPluginId = std::max(_nextPluginId, entry.pluginId + 1);

    } while (result->NextRow());

    LOG_INFO("module", "插件管理器: 成功加载 {} 个插件", static_cast<uint32>(_plugins.size()));
}

PluginEntry const* PluginManager::GetPlugin(uint32 pluginId) const
{
    for (auto const& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
            return &plugin;
    }
    return nullptr;
}

PluginEntry const* PluginManager::GetPluginByName(const std::string& pluginName) const
{
    for (auto const& plugin : _plugins)
    {
        if (plugin.pluginName == pluginName)
            return &plugin;
    }
    return nullptr;
}

std::vector<PluginEntry> PluginManager::GetEnabledPlugins() const
{
    std::vector<PluginEntry> result;
    for (auto const& plugin : _plugins)
    {
        if (plugin.enabled)
            result.push_back(plugin);
    }
    return result;
}

std::vector<PluginEntry> PluginManager::GetPluginsByType(PluginType type) const
{
    std::vector<PluginEntry> result;
    for (auto const& plugin : _plugins)
    {
        if (plugin.type == type)
            result.push_back(plugin);
    }
    return result;
}

std::vector<PluginEntry> PluginManager::GetPluginsByPosition(PluginPosition position) const
{
    std::vector<PluginEntry> result;
    for (auto const& plugin : _plugins)
    {
        if (plugin.position == position)
            result.push_back(plugin);
    }
    return result;
}

uint32 PluginManager::CreatePlugin(
    const std::string& pluginName,
    const std::string& displayName,
    const std::string& description,
    PluginType type,
    PluginPosition position,
    const std::string& iconPath)
{
    if (!_enabled)
        return 0;

    PluginEntry entry;
    entry.pluginId = _nextPluginId++;
    entry.pluginName = pluginName;
    entry.displayName = displayName;
    entry.description = description;
    entry.type = type;
    entry.position = position;
    entry.iconPath = iconPath;
    entry.enabled = true;
    entry.sortOrder = _plugins.size();
    entry.positionX = 0;
    entry.positionY = 0;
    entry.width = 0;
    entry.height = 0;

    SavePluginToDatabase(entry);
    _plugins.push_back(entry);

    LOG_INFO("module", "插件管理器: 成功创建插件 '{}' (ID: {})", pluginName, entry.pluginId);
    return entry.pluginId;
}

bool PluginManager::DeletePlugin(uint32 pluginId)
{
    if (!_enabled)
        return false;

    for (auto it = _plugins.begin(); it != _plugins.end(); ++it)
    {
        if (it->pluginId == pluginId)
        {
            std::string pluginName = it->pluginName;
            _plugins.erase(it);
            DeletePluginFromDatabase(pluginId);
            LOG_INFO("module", "插件管理器: 成功删除插件 '{}' (ID: {})", pluginName, pluginId);
            return true;
        }
    }

    LOG_WARN("module", "插件管理器: 插件ID {} 不存在", pluginId);
    return false;
}

bool PluginManager::EnablePlugin(uint32 pluginId)
{
    return SetPluginEnabled(pluginId, true);
}

bool PluginManager::DisablePlugin(uint32 pluginId)
{
    return SetPluginEnabled(pluginId, false);
}

bool PluginManager::SetPluginEnabled(uint32 pluginId, bool enabled)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.enabled = enabled;
            UpdatePluginInDatabase(pluginId, "启用状态", enabled ? "1" : "0");

            std::string action = enabled ? "启用" : "禁用";
            LOG_INFO("module", "插件管理器: 成功{}插件 '{}' (ID: {})", action, plugin.pluginName, pluginId);
            return true;
        }
    }

    LOG_WARN("module", "插件管理器: 插件ID {} 不存在", pluginId);
    return false;
}

bool PluginManager::IsPluginEnabled(uint32 pluginId) const
{
    PluginEntry const* entry = GetPlugin(pluginId);
    return entry && entry->enabled;
}

bool PluginManager::SetPluginPosition(uint32 pluginId, PluginPosition position)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.position = position;
            UpdatePluginInDatabase(pluginId, "显示位置", std::to_string(static_cast<uint32>(position)));
            LOG_INFO("module", "插件管理器: 成功修改插件 '{}' 显示位置", plugin.pluginName);
            return true;
        }
    }

    return false;
}

bool PluginManager::SetPluginIcon(uint32 pluginId, const std::string& iconPath)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.iconPath = iconPath;
            UpdatePluginInDatabase(pluginId, "图标路径", iconPath);
            LOG_INFO("module", "插件管理器: 成功修改插件 '{}' 图标", plugin.pluginName);
            return true;
        }
    }

    return false;
}

bool PluginManager::SetPluginSortOrder(uint32 pluginId, uint32 sortOrder)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.sortOrder = sortOrder;
            UpdatePluginInDatabase(pluginId, "排序顺序", std::to_string(sortOrder));
            LOG_INFO("module", "插件管理器: 成功修改插件 '{}' 排序顺序", plugin.pluginName);
            return true;
        }
    }

    return false;
}

bool PluginManager::SetPluginDisplayName(uint32 pluginId, const std::string& displayName)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.displayName = displayName;
            UpdatePluginInDatabase(pluginId, "显示名称", displayName);
            LOG_INFO("module", "插件管理器: 成功修改插件 '{}' 显示名称", plugin.pluginName);
            return true;
        }
    }

    return false;
}

bool PluginManager::SetPluginDescription(uint32 pluginId, const std::string& description)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.description = description;
            UpdatePluginInDatabase(pluginId, "插件描述", description);
            return true;
        }
    }

    return false;
}

bool PluginManager::SetPluginCoordinates(uint32 pluginId, uint32 posX, uint32 posY)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.positionX = posX;
            plugin.positionY = posY;
            UpdatePluginInDatabase(pluginId, "X坐标", std::to_string(posX));
            UpdatePluginInDatabase(pluginId, "Y坐标", std::to_string(posY));
            LOG_INFO("module", "插件管理器: 成功修改插件 '{}' 坐标 ({}, {})", plugin.pluginName, posX, posY);
            return true;
        }
    }

    return false;
}

bool PluginManager::SetPluginSize(uint32 pluginId, uint32 width, uint32 height)
{
    if (!_enabled)
        return false;

    for (auto& plugin : _plugins)
    {
        if (plugin.pluginId == pluginId)
        {
            plugin.width = width;
            plugin.height = height;
            UpdatePluginInDatabase(pluginId, "宽度", std::to_string(width));
            UpdatePluginInDatabase(pluginId, "高度", std::to_string(height));
            LOG_INFO("module", "插件管理器: 成功修改插件 '{}' 大小 ({} x {})", plugin.pluginName, width, height);
            return true;
        }
    }

    return false;
}

uint32 PluginManager::GetEnabledPluginCount() const
{
    uint32 count = 0;
    for (auto const& plugin : _plugins)
    {
        if (plugin.enabled)
            count++;
    }
    return count;
}

bool PluginManager::ReorderPlugins(const std::vector<uint32>& pluginIds)
{
    if (!_enabled || pluginIds.empty())
        return false;

    uint32 sortOrder = 0;
    for (uint32 pluginId : pluginIds)
    {
        SetPluginSortOrder(pluginId, sortOrder++);
    }

    std::sort(_plugins.begin(), _plugins.end(), 
        [](const PluginEntry& a, const PluginEntry& b) {
            return a.sortOrder < b.sortOrder;
        });

    return true;
}

// 向玩家发送单个插件配置
void PluginManager::SendPluginConfigToPlayer(Player* player, const std::string& pluginName)
{
    if (!player || !_enabled)
        return;

    // 查找插件
    const PluginEntry* plugin = GetPluginByName(pluginName);
    if (!plugin)
    {
        LOG_ERROR("module", "插件管理器: 找不到插件 '{}'", pluginName);
        return;
    }

    // 构造配置消息
    // 格式: .plugincfg <插件名称> <X坐标> <Y坐标> <宽度> <高度> <启用状态>
    std::string configCmd = ".plugincfg " + plugin->pluginName + " " +
        std::to_string(plugin->positionX) + " " +
        std::to_string(plugin->positionY) + " " +
        std::to_string(plugin->width) + " " +
        std::to_string(plugin->height) + " " +
        std::to_string(plugin->enabled ? 1 : 0);

    // 通过聊天消息发送配置
    ChatHandler(player->GetSession()).PSendSysMessage(configCmd.c_str());

    LOG_DEBUG("module", "插件管理器: 向玩家 {} 发送插件 '{}' 的配置",
        player->GetName(), plugin->pluginName);
}

// 向玩家发送所有插件配置
void PluginManager::SendAllPluginConfigsToPlayer(Player* player)
{
    if (!player || !_enabled)
        return;

    LOG_INFO("module", "【调试】SendAllPluginConfigsToPlayer被调用 - 玩家: {} (总配置数: {})", player->GetName(), _plugins.size());
    LOG_DEBUG("module", "插件管理器: 向玩家 {} 发送所有插件配置", player->GetName());

    // 发送插件配置开始标记
    ChatHandler(player->GetSession()).PSendSysMessage(".plugincfg_start");

    // 遍历所有插件并发送配置
    for (const auto& plugin : _plugins)
    {
        std::string configCmd = ".plugincfg " + plugin.pluginName + " " +
            std::to_string(plugin.positionX) + " " +
            std::to_string(plugin.positionY) + " " +
            std::to_string(plugin.width) + " " +
            std::to_string(plugin.height) + " " +
            std::to_string(plugin.enabled ? 1 : 0);

        ChatHandler(player->GetSession()).PSendSysMessage(configCmd.c_str());
    }

    // 发送插件配置结束标记
    ChatHandler(player->GetSession()).PSendSysMessage(".plugincfg_end");

    LOG_INFO("module", "插件管理器: 已向玩家 {} 发送 {} 个插件配置",
        player->GetName(), _plugins.size());
}

bool PluginManager::SavePluginToDatabase(const PluginEntry& entry)
{
    if (!_enabled)
        return false;

    WorldDatabase.Execute("INSERT INTO `插件管理器` (`插件ID`, `插件名称`, `显示名称`, `插件描述`, `插件类型`, `显示位置`, `X坐标`, `Y坐标`, `宽度`, `高度`, `排序顺序`, `图标路径`, `启用状态`) VALUES ({}, '{}', '{}', '{}', {}, {}, {}, {}, {}, {}, {}, '{}', {}) ON DUPLICATE KEY UPDATE `显示名称` = '{}', `插件描述` = '{}', `插件类型` = {}, `显示位置` = {}, `X坐标` = {}, `Y坐标` = {}, `宽度` = {}, `高度` = {}, `排序顺序` = {}, `图标路径` = '{}', `启用状态` = {}",
        entry.pluginId,
        entry.pluginName, entry.displayName, entry.description,
        static_cast<uint32>(entry.type), static_cast<uint32>(entry.position),
        entry.positionX, entry.positionY, entry.width, entry.height,
        entry.sortOrder, entry.iconPath, entry.enabled ? 1 : 0,
        entry.displayName, entry.description,
        static_cast<uint32>(entry.type), static_cast<uint32>(entry.position),
        entry.positionX, entry.positionY, entry.width, entry.height,
        entry.sortOrder, entry.iconPath, entry.enabled ? 1 : 0);

    return true;
}

bool PluginManager::UpdatePluginInDatabase(uint32 pluginId, const std::string& field, const std::string& value)
{
    if (!_enabled)
        return false;

    WorldDatabase.Execute("UPDATE `插件管理器` SET `{}` = '{}' WHERE `插件ID` = {}", field, value, pluginId);
    return true;
}

bool PluginManager::DeletePluginFromDatabase(uint32 pluginId)
{
    if (!_enabled)
        return false;

    WorldDatabase.Execute("DELETE FROM `插件管理器` WHERE `插件ID` = {}", pluginId);
    return true;
}

PluginManagerLoader::PluginManagerLoader()
    : WorldScript("PluginManagerLoader"), _loaded(false), _updateTimer(0)
{
}

void PluginManagerLoader::OnStartup()
{
    if (!sConfigMgr->GetOption<bool>("PluginManager.Enabled", true))
        return;

    LOG_INFO("module", ">> 正在加载 插件管理器 模块...");
    sPluginManager->Initialize();
    _loaded = true;
}

void PluginManagerLoader::OnUpdate(uint32 diff)
{
    if (!_loaded)
        return;

    _updateTimer += diff;
    if (_updateTimer > 30000)
    {
        _updateTimer = 0;
    }
}

void PluginManagerLoader::OnShutdown()
{
    if (_loaded)
    {
        LOG_INFO("module", ">> 关闭 插件管理器 模块");
        _loaded = false;
    }
}

// ========================================
// 玩家登录脚本实现
// ========================================

// 延迟发送插件配置的事件类
class SendPluginConfigEvent : public BasicEvent
{
public:
    SendPluginConfigEvent(Player* player) : _player(player) {}

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_player && _player->IsInWorld())
        {
            sPluginManager->SendAllPluginConfigsToPlayer(_player);
        }
        return true; // 事件执行完毕后删除
    }

private:
    Player* _player;
};

PluginManagerPlayerScript::PluginManagerPlayerScript()
    : PlayerScript("PluginManagerPlayerScript")
{
}

void PluginManagerPlayerScript::OnPlayerLogin(Player* player)
{
    if (!player)
        return;

    // 插件配置现在仅由客户端请求时发送，删除了服务器自动发送的逻辑
    // 以避免重复发送导致的聊天刷屏问题
}

void AddPluginManagerScripts()
{
    new PluginManagerLoader();
    new PluginManagerPlayerScript();
    AddPluginManager_CommandsScripts();
}