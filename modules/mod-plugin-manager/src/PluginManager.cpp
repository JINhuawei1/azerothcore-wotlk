#include "PluginManager.h"
#include "AddonThrottle.h"
#include "World.h"
#include <algorithm>
#include <limits>
#include <sstream>

// 前向声明
void AddPluginManager_CommandsScripts();

namespace
{
constexpr char PLUGIN_MANAGER_ADDON_PREFIX[] = "PLUGMGR";
constexpr char PLUGIN_MANAGER_REQUEST_ALL[] = "REQ_ALL";
constexpr char PLUGIN_MANAGER_SAVE_POS_PREFIX[] = "SAVE_POS:";
constexpr char PLUGIN_MANAGER_SYNC_START[] = ".plugincfg_start";
constexpr char PLUGIN_MANAGER_SYNC_END[] = ".plugincfg_end";

std::vector<std::string> SplitFields(std::string const& text, char delimiter)
{
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string part;

    while (std::getline(stream, part, delimiter))
        fields.push_back(part);

    if (!text.empty() && text.back() == delimiter)
        fields.emplace_back();

    return fields;
}

bool TryParseInt32(std::string const& text, int32& value)
{
    if (text.empty())
        return false;

    try
    {
        long long parsed = std::stoll(text);
        if (parsed < std::numeric_limits<int32>::min() || parsed > std::numeric_limits<int32>::max())
            return false;

        value = static_cast<int32>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool TryParseUInt32(std::string const& text, uint32& value)
{
    if (text.empty())
        return false;

    try
    {
        unsigned long long parsed = std::stoull(text);
        if (parsed > std::numeric_limits<uint32>::max())
            return false;

        value = static_cast<uint32>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
}

PluginManager* PluginManager::_instance = nullptr;

PluginManager* PluginManager::instance()
{
    if (!_instance)
        _instance = new PluginManager();
    return _instance;
}

PluginManager::PluginManager()
    : _enabled(sConfigMgr->GetOption<bool>("PluginManager.Enabled", true)),
      _loadPlayerConfig(sConfigMgr->GetOption<bool>("PluginManager.LoadPlayerConfig", true)),
      _autoSavePlayerConfig(sConfigMgr->GetOption<bool>("PluginManager.AutoSavePlayerConfig", true)),
      _allowCustomPositioning(sConfigMgr->GetOption<bool>("PluginManager.AllowCustomPositioning", true)),
      _commandPermissionLevel(sConfigMgr->GetOption<int32>("PluginManager.CommandLevel", SEC_ADMINISTRATOR)),
      _nextPluginId(1)
{
}

void PluginManager::Initialize()
{
    if (!_enabled)
    {
        return;
    }

    LoadAllPlugins();
}

void PluginManager::LoadAllPlugins()
{
    _plugins.clear();
    _nextPluginId = 1;
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
}

void PluginManager::LoadPlayerLayouts(uint32 playerGuid)
{
    if (!_loadPlayerConfig)
        return;

    if (_loadedPlayerLayouts.find(playerGuid) != _loadedPlayerLayouts.end())
        return;

    std::unordered_map<uint32, PlayerPluginLayout> layouts;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `插件ID`, `X坐标`, `Y坐标`, `宽度`, `高度` "
        "FROM `_插件管理玩家坐标` WHERE `玩家GUID` = {}",
        playerGuid);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            PlayerPluginLayout layout;
            layout.positionX = fields[1].Get<int32>();
            layout.positionY = fields[2].Get<int32>();
            layout.width = fields[3].Get<uint32>();
            layout.height = fields[4].Get<uint32>();

            layouts[fields[0].Get<uint32>()] = layout;
        } while (result->NextRow());
    }

    _playerLayouts[playerGuid] = std::move(layouts);
    _loadedPlayerLayouts.insert(playerGuid);
}

void PluginManager::UnloadPlayerLayouts(uint32 playerGuid)
{
    _playerLayouts.erase(playerGuid);
    _loadedPlayerLayouts.erase(playerGuid);
}

void PluginManager::DeletePlayerLayouts(uint32 playerGuid)
{
    CharacterDatabase.Execute("DELETE FROM `_插件管理玩家坐标` WHERE `玩家GUID` = {}", playerGuid);
    UnloadPlayerLayouts(playerGuid);
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

std::vector<PluginEntry> PluginManager::GetVisiblePlugins() const
{
    std::vector<PluginEntry> result;
    for (auto const& plugin : _plugins)
    {
        if (plugin.enabled && plugin.position != POSITION_HIDDEN)
            result.push_back(plugin);
    }
    return result;
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
        [](const PluginEntry& a, const PluginEntry& b)
        {
            return a.sortOrder < b.sortOrder;
        });

    return true;
}

bool PluginManager::SavePlayerLayout(uint32 playerGuid, uint32 pluginId, int32 posX, int32 posY, uint32 width, uint32 height)
{
    if (!_allowCustomPositioning || !_autoSavePlayerConfig)
        return false;

    if (_loadedPlayerLayouts.find(playerGuid) == _loadedPlayerLayouts.end())
        LoadPlayerLayouts(playerGuid);

    PlayerPluginLayout layout;
    layout.positionX = posX;
    layout.positionY = posY;
    layout.width = width;
    layout.height = height;

    _playerLayouts[playerGuid][pluginId] = layout;

    CharacterDatabase.Execute(
        "INSERT INTO `_插件管理玩家坐标` (`玩家GUID`, `插件ID`, `X坐标`, `Y坐标`, `宽度`, `高度`) "
        "VALUES ({}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "`X坐标` = VALUES(`X坐标`), "
        "`Y坐标` = VALUES(`Y坐标`), "
        "`宽度` = VALUES(`宽度`), "
        "`高度` = VALUES(`高度`)",
        playerGuid, pluginId, posX, posY, width, height);

    return true;
}

PlayerPluginLayout const* PluginManager::GetPlayerLayout(uint32 playerGuid, uint32 pluginId) const
{
    auto playerIt = _playerLayouts.find(playerGuid);
    if (playerIt == _playerLayouts.end())
        return nullptr;

    auto layoutIt = playerIt->second.find(pluginId);
    if (layoutIt == playerIt->second.end())
        return nullptr;

    return &layoutIt->second;
}

void PluginManager::SendAddonPayload(Player* player, std::string const& payload) const
{
    if (!player || payload.empty())
        return;

    std::string fullMessage = std::string(PLUGIN_MANAGER_ADDON_PREFIX) + '\t' + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
}

// 向玩家发送单个插件配置
void PluginManager::SendPluginConfigToPlayer(Player* player, const std::string& pluginName)
{
    if (!player || !_enabled)
        return;

    PluginEntry const* plugin = GetPluginByName(pluginName);
    if (!plugin)
    {
        LOG_ERROR("module", "插件管理器: 找不到插件 '{}'", pluginName);
        return;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (_loadPlayerConfig && _loadedPlayerLayouts.find(playerGuid) == _loadedPlayerLayouts.end())
        LoadPlayerLayouts(playerGuid);

    int32 posX = static_cast<int32>(plugin->positionX);
    int32 posY = static_cast<int32>(plugin->positionY);
    uint32 width = plugin->width;
    uint32 height = plugin->height;

    if (PlayerPluginLayout const* layout = GetPlayerLayout(playerGuid, plugin->pluginId))
    {
        posX = layout->positionX;
        posY = layout->positionY;
        if (layout->width > 0)
            width = layout->width;
        if (layout->height > 0)
            height = layout->height;
    }

    bool visible = plugin->enabled && plugin->position != POSITION_HIDDEN;

    std::string configCmd = ".plugincfg " + plugin->pluginName + " " +
        std::to_string(posX) + " " +
        std::to_string(posY) + " " +
        std::to_string(width) + " " +
        std::to_string(height) + " " +
        std::to_string(visible ? 1 : 0);

    SendAddonPayload(player, configCmd);

    LOG_DEBUG("module", "插件管理器: 向玩家 {} 发送插件 '{}' 的配置", player->GetName(), plugin->pluginName);
}

// 向玩家发送所有插件配置
void PluginManager::SendAllPluginConfigsToPlayer(Player* player)
{
    if (!player || !_enabled)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (_loadPlayerConfig && _loadedPlayerLayouts.find(playerGuid) == _loadedPlayerLayouts.end())
        LoadPlayerLayouts(playerGuid);

    SendAddonPayload(player, PLUGIN_MANAGER_SYNC_START);

    for (PluginEntry const& plugin : _plugins)
        SendPluginConfigToPlayer(player, plugin.pluginName);

    SendAddonPayload(player, PLUGIN_MANAGER_SYNC_END);
}

bool PluginManager::HandleAddonMessage(Player* player, std::string const& payload)
{
    if (!player || !_enabled || payload.empty())
        return false;

    if (payload == PLUGIN_MANAGER_REQUEST_ALL)
    {
        SendAllPluginConfigsToPlayer(player);
        return true;
    }

    if (payload.rfind(PLUGIN_MANAGER_SAVE_POS_PREFIX, 0) == 0)
    {
        if (!_allowCustomPositioning)
            return true;

        std::vector<std::string> fields = SplitFields(payload, ':');
        if (fields.size() < 4)
        {
            LOG_WARN("module", "插件管理器: 收到无效的坐标保存请求 '{}'", payload);
            return true;
        }

        PluginEntry const* plugin = GetPluginByName(fields[1]);
        if (!plugin)
        {
            LOG_WARN("module", "插件管理器: 坐标保存失败，插件 '{}' 不存在", fields[1]);
            return true;
        }

        int32 posX = 0;
        int32 posY = 0;
        uint32 width = plugin->width;
        uint32 height = plugin->height;

        if (!TryParseInt32(fields[2], posX) || !TryParseInt32(fields[3], posY))
        {
            LOG_WARN("module", "插件管理器: 插件 '{}' 坐标解析失败", plugin->pluginName);
            return true;
        }

        if (fields.size() >= 5)
        {
            uint32 parsedWidth = 0;
            if (TryParseUInt32(fields[4], parsedWidth) && parsedWidth > 0)
                width = parsedWidth;
        }

        if (fields.size() >= 6)
        {
            uint32 parsedHeight = 0;
            if (TryParseUInt32(fields[5], parsedHeight) && parsedHeight > 0)
                height = parsedHeight;
        }

        if (SavePlayerLayout(player->GetGUID().GetCounter(), plugin->pluginId, posX, posY, width, height))
        {
            LOG_DEBUG("module", "插件管理器: 已保存玩家 {} 的插件 '{}' 坐标({}, {})",
                player->GetName(), plugin->pluginName, posX, posY);
        }

        return true;
    }

    return false;
}

bool PluginManager::SavePluginToDatabase(const PluginEntry& entry)
{
    if (!_enabled)
        return false;

    std::string pluginName = entry.pluginName;
    std::string displayName = entry.displayName;
    std::string description = entry.description;
    std::string iconPath = entry.iconPath;

    WorldDatabase.EscapeString(pluginName);
    WorldDatabase.EscapeString(displayName);
    WorldDatabase.EscapeString(description);
    WorldDatabase.EscapeString(iconPath);

    WorldDatabase.Execute(
        "INSERT INTO `插件管理器` (`插件ID`, `插件名称`, `显示名称`, `插件描述`, `插件类型`, `显示位置`, `X坐标`, `Y坐标`, `宽度`, `高度`, `排序顺序`, `图标路径`, `启用状态`) "
        "VALUES ({}, '{}', '{}', '{}', {}, {}, {}, {}, {}, {}, {}, '{}', {}) "
        "ON DUPLICATE KEY UPDATE "
        "`显示名称` = '{}', `插件描述` = '{}', `插件类型` = {}, `显示位置` = {}, `X坐标` = {}, `Y坐标` = {}, `宽度` = {}, `高度` = {}, `排序顺序` = {}, `图标路径` = '{}', `启用状态` = {}",
        entry.pluginId, pluginName, displayName, description,
        static_cast<uint32>(entry.type), static_cast<uint32>(entry.position),
        entry.positionX, entry.positionY, entry.width, entry.height,
        entry.sortOrder, iconPath, entry.enabled ? 1 : 0,
        displayName, description,
        static_cast<uint32>(entry.type), static_cast<uint32>(entry.position),
        entry.positionX, entry.positionY, entry.width, entry.height,
        entry.sortOrder, iconPath, entry.enabled ? 1 : 0);

    return true;
}

bool PluginManager::UpdatePluginInDatabase(uint32 pluginId, const std::string& field, const std::string& value)
{
    if (!_enabled)
        return false;

    std::string escapedValue = value;
    WorldDatabase.EscapeString(escapedValue);

    WorldDatabase.Execute("UPDATE `插件管理器` SET `{}` = '{}' WHERE `插件ID` = {}", field, escapedValue, pluginId);
    return true;
}

bool PluginManager::DeletePluginFromDatabase(uint32 pluginId)
{
    if (!_enabled)
        return false;

    WorldDatabase.Execute("DELETE FROM `插件管理器` WHERE `插件ID` = {}", pluginId);
    CharacterDatabase.Execute("DELETE FROM `_插件管理玩家坐标` WHERE `插件ID` = {}", pluginId);
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

    sPluginManager->Initialize();
    _loaded = true;
    LOG_INFO("server.loading", "→插件管理器系统√");
}

void PluginManagerLoader::OnUpdate(uint32 diff)
{
    if (!_loaded)
        return;

    _updateTimer += diff;
    if (_updateTimer > 30000)
        _updateTimer = 0;
}

void PluginManagerLoader::OnShutdown()
{
    if (_loaded)
    {
        LOG_INFO("module", ">> 关闭 插件管理器 模块");
        _loaded = false;
    }
}

PluginManagerPlayerScript::PluginManagerPlayerScript()
    : PlayerScript("PluginManagerPlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER
    })
{
}

void PluginManagerPlayerScript::OnPlayerLogin(Player* player)
{
    if (!player || !sPluginManager->IsSystemEnabled())
        return;

    sPluginManager->LoadPlayerLayouts(player->GetGUID().GetCounter());
    sPluginManager->SendAllPluginConfigsToPlayer(player);
}

void PluginManagerPlayerScript::OnPlayerLogout(Player* player)
{
    if (!player || !sPluginManager->IsSystemEnabled())
        return;

    sPluginManager->UnloadPlayerLayouts(player->GetGUID().GetCounter());
}

void PluginManagerPlayerScript::OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/)
{
    if (!sPluginManager->IsSystemEnabled())
        return;

    sPluginManager->DeletePlayerLayouts(guid.GetCounter());
}

void PluginManagerPlayerScript::OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver)
{
    if (!sPluginManager->IsSystemEnabled() || !player || !receiver)
        return;

    if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        return;

    if (receiver->GetGUID() != player->GetGUID())
        return;

    std::size_t tabPos = msg.find('\t');
    if (tabPos == std::string::npos)
        return;

    std::string prefix = msg.substr(0, tabPos);
    if (prefix != PLUGIN_MANAGER_ADDON_PREFIX)
        return;

    // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
    if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "PLUGINMGR"))
        return;

    std::string payload = msg.substr(tabPos + 1);
    sPluginManager->HandleAddonMessage(player, payload);
}

void AddPluginManagerScripts()
{
    new PluginManagerLoader();
    new PluginManagerPlayerScript();
    AddPluginManager_CommandsScripts();
}
