#include "Log.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "HermesBridgeAddonApi.h"
#include "HermesBridgeReadyGate.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Bag.h"
#include "Item.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include "Util.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include "../../mod-twelve-zodiac/src/TwelveZodiacHermesApi.h"
#include "../../mod-boundary/src/BoundaryMgr.h"
#include "../../mod-breakthrough/src/BreakthroughSystem.h"
#include "../../mod-breakthrough/src/BreakthroughSkillSystem.h"
#include "../../mod-item-identification-system/src/ItemIdentificationSystem.h"
#include "../../mod-mall-system/src/MallSystem.h"
#include "../../mod-requirement-template/src/RequirementSystem.h"
#include "../../mod-reward-template/src/RewardTemplate.h"
#include "../../mod-wear-control/src/WearControl.h"

#include <json/json.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32 HERMES_BRIDGE_MAX_PAYLOAD_SIZE = 64 * 1024;

    constexpr uint16 HERMES_BRIDGE_FRAME_MAGIC = 0x4842;
    constexpr uint8 HERMES_BRIDGE_FRAME_VERSION = 2;
    constexpr uint8 HERMES_BRIDGE_FRAME_HEADER_SIZE = 24;
    constexpr bool HERMES_BRIDGE_TRACE_PACKETS = false;
    constexpr bool HERMES_BRIDGE_DEBUG_COMM = false;
    constexpr bool HERMES_BRIDGE_TRACE_SERVER_COMM = false;
    constexpr uint32 HERMES_SERVER_OUTBOUND_QUEUE_CAPACITY = 4096;
    constexpr uint8 HERMES_LANE_COUNT = 6;

    enum HermesLane : uint8
    {
        HERMES_LANE_CONTROL = 0,
        HERMES_LANE_RPC = 1,
        HERMES_LANE_EVENT = 2,
        HERMES_LANE_SNAPSHOT = 3,
        HERMES_LANE_BULK = 4,
        HERMES_LANE_DEBUG = 5
    };

    enum HermesMessageType : uint8
    {
        HERMES_MESSAGE_REQUEST = 1,
        HERMES_MESSAGE_RESPONSE = 2,
        HERMES_MESSAGE_EVENT = 3,
        HERMES_MESSAGE_ERROR = 4,
        HERMES_MESSAGE_ACK = 5,
        HERMES_MESSAGE_CHUNK = 6
    };

    enum HermesCodec : uint8
    {
        HERMES_CODEC_JSON = 1,
        HERMES_CODEC_MSGPACK = 2,
        HERMES_CODEC_BINARY = 3
    };

    enum HermesMethodId : uint16
    {
        HERMES_METHOD_UNKNOWN = 0,
        HERMES_METHOD_HELLO = 1,
        HERMES_METHOD_PING = 2,
        HERMES_METHOD_DEBUG_EMIT = 3,
        HERMES_METHOD_GET_SCHEMA_REGISTRY = 4,
        HERMES_METHOD_GET_METHOD_REGISTRY = 5,
        HERMES_METHOD_DEBUG_BINARY_EMIT = 6,
        HERMES_METHOD_DEBUG_VITALS_BINARY_EMIT = 7,
        HERMES_METHOD_DEBUG_BULK_CHUNK_EMIT = 8,
        HERMES_METHOD_SERVER_GET_STATUS = 10,
        HERMES_METHOD_ADDON_DISPATCH = 20,
        HERMES_METHOD_ADDON_MESSAGE = 21,
        HERMES_METHOD_PLAYER_GET_BASIC_INFO = 100,
        HERMES_METHOD_PLAYER_GET_POSITION = 101,
        HERMES_METHOD_PLAYER_GET_VITALS = 102,
        HERMES_METHOD_PLAYER_GET_SNAPSHOT = 103,
        HERMES_METHOD_SERVER_GET_NUMERIC_LIMITS = 104,
        HERMES_METHOD_PLAYER_GET_ATTRIBUTES = 105,
        HERMES_METHOD_PLAYER_GET_TARGET_SNAPSHOT = 106,
        HERMES_METHOD_PLAYER_EMIT_DAMAGE_EVENT = 107,
        HERMES_METHOD_PLAYER_EMIT_VITALS_BINARY = 108,
        HERMES_METHOD_PLAYER_EMIT_SNAPSHOT_BULK = 109,
        HERMES_METHOD_UI_GET_DASHBOARD = 200,
        HERMES_METHOD_UI_GET_MODULE_STATUS = 201,
        HERMES_METHOD_ABYSS_GET_EQUIPMENT_PAGE = 300,
        HERMES_METHOD_ABYSS_GET_SET_OVERVIEW = 301,
        HERMES_METHOD_ABYSS_GET_SET_BONUSES = 302,
        HERMES_METHOD_ABYSS_GET_RELICS = 303,
        HERMES_METHOD_CULTIVATION_GET_ALL = 320,
        HERMES_METHOD_CULTIVATION_GET_STATE = 321,
        HERMES_METHOD_BOUNDARY_GET_ALL = 340,
        HERMES_METHOD_BOUNDARY_GET_DETAIL = 341,
        HERMES_METHOD_BREAKTHROUGH_GET_ALL = 360,
        HERMES_METHOD_BREAKTHROUGH_GET_SYSTEM_DATA = 361,
        HERMES_METHOD_BREAKTHROUGH_GET_INFO = 362,
        HERMES_METHOD_BREAKTHROUGH_GET_SKILLS = 363,
        HERMES_METHOD_BREAKTHROUGH_GET_SKILL_DETAIL = 364,
        HERMES_METHOD_BREAKTHROUGH_GET_LEADERBOARD = 365,
        HERMES_METHOD_BREAKTHROUGH_GET_EXP_SOURCES = 366,
        HERMES_METHOD_BREAKTHROUGH_UPGRADE = 367,
        HERMES_METHOD_BREAKTHROUGH_LEARN_SKILL = 368,
        HERMES_METHOD_BREAKTHROUGH_UPGRADE_SKILL = 369,
        HERMES_METHOD_BREAKTHROUGH_RESET_SKILLS = 370,
        HERMES_METHOD_SYNTHESIS_LIST = 380,
        HERMES_METHOD_SYNTHESIS_DO = 381,
        HERMES_METHOD_TOOLTIP_QUERY = 500,
        HERMES_METHOD_TOOLTIP_QUERY_TEMPLATE = 501,
        HERMES_METHOD_TOOLTIP_INSPECT_ITEM_GUID = 502,
        HERMES_METHOD_TOOLTIP_LIST_PENDING = 503,
        HERMES_METHOD_MALL_GET_CATEGORIES = 520,
        HERMES_METHOD_MALL_GET_ITEMS = 521,
        HERMES_METHOD_MALL_PURCHASE = 522,
        HERMES_METHOD_ZODIAC_GET_STATE = 540,
        HERMES_METHOD_ZODIAC_GET_DETAIL = 541,
        HERMES_METHOD_ZODIAC_EQUIP = 542,
        HERMES_METHOD_ZODIAC_UNEQUIP = 543
    };

    enum HermesSchemaId : uint16
    {
        HERMES_SCHEMA_JSON_RPC = 0,
        HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1 = 1000,
        HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1 = 1001,
        HERMES_SCHEMA_BULK_CHUNK_V1 = 1002
    };

    constexpr uint8 HERMES_FLAG_COMPRESSED = 0x01;
    constexpr uint8 HERMES_FLAG_FRAGMENTED = 0x02;
    constexpr uint64 HERMES_JSON_SAFE_UINT_MAX = 9007199254740991ULL;

    enum HermesVitalsFlags : uint16
    {
        HERMES_VITALS_FLAG_ALIVE = 0x0001,
        HERMES_VITALS_FLAG_IN_COMBAT = 0x0002,
        HERMES_VITALS_FLAG_HEALTH_EXACT = 0x0004,
        HERMES_VITALS_FLAG_MAX_HEALTH_EXACT = 0x0008,
        HERMES_VITALS_FLAG_POWER_EXACT = 0x0010,
        HERMES_VITALS_FLAG_MAX_POWER_EXACT = 0x0020
    };

    struct HermesFrameV2
    {
        uint8 Lane = 0;
        uint8 MessageType = 0;
        uint8 Codec = 0;
        uint8 Flags = 0;
        uint16 SchemaId = 0;
        uint16 MethodId = 0;
        uint32 RequestId = 0;
        uint32 Sequence = 0;
        uint32 PayloadSize = 0;
        std::string Payload;
    };

    struct JsonRpcDispatchResult
    {
        bool IsError = false;
        std::string Payload;
    };

    using HermesMethodHandler = JsonRpcDispatchResult (*)(WorldSession&, HermesFrameV2&, Json::Value const&, std::string const&, std::string const&);

    JsonRpcDispatchResult HandleHermesHello(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesPing(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesDebugEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesGetSchemaRegistry(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesGetMethodRegistry(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesDebugBinaryEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesDebugVitalsBinaryEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleHermesDebugBulkChunkEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleServerGetStatus(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleAddonDispatch(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleServerGetNumericLimits(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerGetBasicInfo(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerGetPosition(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerGetVitals(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerGetSnapshot(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerGetAttributes(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerGetTargetSnapshot(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerEmitDamageEvent(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerEmitVitalsBinary(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandlePlayerEmitSnapshotBulk(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleUiGetDashboard(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleUiGetModuleStatus(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleAbyssGetEquipmentPage(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleAbyssGetSetOverview(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleAbyssGetSetBonuses(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleAbyssGetRelics(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleCultivationGetAll(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleCultivationGetState(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBoundaryGetAll(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBoundaryGetDetail(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetAll(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetSystemData(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetInfo(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetSkills(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetSkillDetail(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetLeaderboard(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughGetExpSources(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughUpgrade(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughLearnSkill(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughUpgradeSkill(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleBreakthroughResetSkills(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleSynthesisList(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleSynthesisDo(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleTooltipQuery(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleTooltipQueryTemplate(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleTooltipInspectItemGuid(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleTooltipListPending(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleMallGetCategories(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleMallGetItems(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleMallPurchase(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleZodiacGetState(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleZodiacGetDetail(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleZodiacEquip(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);
    JsonRpcDispatchResult HandleZodiacUnequip(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method);

    struct HermesMethodDescriptor
    {
        char const* Name;
        uint16 MethodId;
        uint8 Lane;
        uint8 Codec;
        char const* Status;
        AccountTypes MinSecurity;
        uint16 RateLimitPerSecond;
        HermesMethodHandler Handler;
    };

    constexpr HermesMethodDescriptor HERMES_METHOD_REGISTRY[] =
    {
        { "hermes.hello", HERMES_METHOD_HELLO, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleHermesHello },
        { "hermes.ping", HERMES_METHOD_PING, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 64, HandleHermesPing },
        { "hermes.debugEmit", HERMES_METHOD_DEBUG_EMIT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_GAMEMASTER, 4, HandleHermesDebugEmit },
        { "hermes.getSchemaRegistry", HERMES_METHOD_GET_SCHEMA_REGISTRY, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleHermesGetSchemaRegistry },
        { "hermes.getMethodRegistry", HERMES_METHOD_GET_METHOD_REGISTRY, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleHermesGetMethodRegistry },
        { "hermes.debugBinaryEmit", HERMES_METHOD_DEBUG_BINARY_EMIT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "smoke", SEC_GAMEMASTER, 4, HandleHermesDebugBinaryEmit },
        { "hermes.debugVitalsBinaryEmit", HERMES_METHOD_DEBUG_VITALS_BINARY_EMIT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "smoke", SEC_GAMEMASTER, 4, HandleHermesDebugVitalsBinaryEmit },
        { "hermes.debugBulkChunkEmit", HERMES_METHOD_DEBUG_BULK_CHUNK_EMIT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "smoke", SEC_GAMEMASTER, 4, HandleHermesDebugBulkChunkEmit },
        { "server.getStatus", HERMES_METHOD_SERVER_GET_STATUS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleServerGetStatus },
        { "addon.dispatch", HERMES_METHOD_ADDON_DISPATCH, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 64, HandleAddonDispatch },
        { "server.getNumericLimits", HERMES_METHOD_SERVER_GET_NUMERIC_LIMITS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleServerGetNumericLimits },
        { "player.getBasicInfo", HERMES_METHOD_PLAYER_GET_BASIC_INFO, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 32, HandlePlayerGetBasicInfo },
        { "player.getPosition", HERMES_METHOD_PLAYER_GET_POSITION, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 32, HandlePlayerGetPosition },
        { "player.getVitals", HERMES_METHOD_PLAYER_GET_VITALS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 32, HandlePlayerGetVitals },
        { "player.getSnapshot", HERMES_METHOD_PLAYER_GET_SNAPSHOT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandlePlayerGetSnapshot },
        { "player.getAttributes", HERMES_METHOD_PLAYER_GET_ATTRIBUTES, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandlePlayerGetAttributes },
        { "player.getTargetSnapshot", HERMES_METHOD_PLAYER_GET_TARGET_SNAPSHOT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandlePlayerGetTargetSnapshot },
        { "player.emitDamageEvent", HERMES_METHOD_PLAYER_EMIT_DAMAGE_EVENT, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_GAMEMASTER, 4, HandlePlayerEmitDamageEvent },
        { "player.emitVitalsBinary", HERMES_METHOD_PLAYER_EMIT_VITALS_BINARY, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_GAMEMASTER, 4, HandlePlayerEmitVitalsBinary },
        { "player.emitSnapshotBulk", HERMES_METHOD_PLAYER_EMIT_SNAPSHOT_BULK, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_GAMEMASTER, 4, HandlePlayerEmitSnapshotBulk },
        { "ui.getDashboard", HERMES_METHOD_UI_GET_DASHBOARD, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleUiGetDashboard },
        { "ui.getModuleStatus", HERMES_METHOD_UI_GET_MODULE_STATUS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleUiGetModuleStatus },
        { "abyss.getEquipmentPage", HERMES_METHOD_ABYSS_GET_EQUIPMENT_PAGE, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 24, HandleAbyssGetEquipmentPage },
        { "abyss.getSetOverview", HERMES_METHOD_ABYSS_GET_SET_OVERVIEW, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleAbyssGetSetOverview },
        { "abyss.getSetBonuses", HERMES_METHOD_ABYSS_GET_SET_BONUSES, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleAbyssGetSetBonuses },
        { "abyss.getRelics", HERMES_METHOD_ABYSS_GET_RELICS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleAbyssGetRelics },
        { "cultivation.getAll", HERMES_METHOD_CULTIVATION_GET_ALL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleCultivationGetAll },
        { "cultivation.getState", HERMES_METHOD_CULTIVATION_GET_STATE, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleCultivationGetState },
        { "boundary.getAll", HERMES_METHOD_BOUNDARY_GET_ALL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleBoundaryGetAll },
        { "boundary.getDetail", HERMES_METHOD_BOUNDARY_GET_DETAIL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleBoundaryGetDetail },
        { "breakthrough.getAll", HERMES_METHOD_BREAKTHROUGH_GET_ALL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleBreakthroughGetAll },
        { "breakthrough.getSystemData", HERMES_METHOD_BREAKTHROUGH_GET_SYSTEM_DATA, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleBreakthroughGetSystemData },
        { "breakthrough.getInfo", HERMES_METHOD_BREAKTHROUGH_GET_INFO, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleBreakthroughGetInfo },
        { "breakthrough.getSkills", HERMES_METHOD_BREAKTHROUGH_GET_SKILLS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleBreakthroughGetSkills },
        { "breakthrough.getSkillDetail", HERMES_METHOD_BREAKTHROUGH_GET_SKILL_DETAIL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 32, HandleBreakthroughGetSkillDetail },
        { "breakthrough.getLeaderboard", HERMES_METHOD_BREAKTHROUGH_GET_LEADERBOARD, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleBreakthroughGetLeaderboard },
        { "breakthrough.getExpSources", HERMES_METHOD_BREAKTHROUGH_GET_EXP_SOURCES, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleBreakthroughGetExpSources },
        { "breakthrough.upgrade", HERMES_METHOD_BREAKTHROUGH_UPGRADE, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 4, HandleBreakthroughUpgrade },
        { "breakthrough.learnSkill", HERMES_METHOD_BREAKTHROUGH_LEARN_SKILL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleBreakthroughLearnSkill },
        { "breakthrough.upgradeSkill", HERMES_METHOD_BREAKTHROUGH_UPGRADE_SKILL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleBreakthroughUpgradeSkill },
        { "breakthrough.resetSkills", HERMES_METHOD_BREAKTHROUGH_RESET_SKILLS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 4, HandleBreakthroughResetSkills },
        { "synthesis.list", HERMES_METHOD_SYNTHESIS_LIST, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 4, HandleSynthesisList },
        { "synthesis.do", HERMES_METHOD_SYNTHESIS_DO, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 2, HandleSynthesisDo },
        { "tooltip.query", HERMES_METHOD_TOOLTIP_QUERY, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 64, HandleTooltipQuery },
        { "tooltip.queryTemplate", HERMES_METHOD_TOOLTIP_QUERY_TEMPLATE, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 64, HandleTooltipQueryTemplate },
        { "tooltip.inspectItemGuid", HERMES_METHOD_TOOLTIP_INSPECT_ITEM_GUID, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 64, HandleTooltipInspectItemGuid },
        { "tooltip.listPending", HERMES_METHOD_TOOLTIP_LIST_PENDING, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleTooltipListPending },
        { "mall.getCategories", HERMES_METHOD_MALL_GET_CATEGORIES, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleMallGetCategories },
        { "mall.getItems", HERMES_METHOD_MALL_GET_ITEMS, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 32, HandleMallGetItems },
        { "mall.purchase", HERMES_METHOD_MALL_PURCHASE, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleMallPurchase },
        { "zodiac.getState", HERMES_METHOD_ZODIAC_GET_STATE, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 16, HandleZodiacGetState },
        { "zodiac.getDetail", HERMES_METHOD_ZODIAC_GET_DETAIL, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 32, HandleZodiacGetDetail },
        { "zodiac.equip", HERMES_METHOD_ZODIAC_EQUIP, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleZodiacEquip },
        { "zodiac.unequip", HERMES_METHOD_ZODIAC_UNEQUIP, HERMES_LANE_RPC, HERMES_CODEC_JSON, "active", SEC_PLAYER, 8, HandleZodiacUnequip }
    };

    struct HermesRateLimitBucket
    {
        time_t WindowSecond = 0;
        uint16 Count = 0;
    };

    std::unordered_map<uint64, HermesRateLimitBucket> g_HermesRateLimitBuckets;

    struct HermesOutboundLaneStats
    {
        uint32 Depth = 0;
        uint32 HighWatermark = 0;
        uint64 Enqueued = 0;
        uint64 Sent = 0;
        uint64 Dropped = 0;
        uint64 Coalesced = 0;
    };

    struct HermesOutboundQueueStats
    {
        uint32 Depth = 0;
        uint32 HighWatermark = 0;
        uint64 Enqueued = 0;
        uint64 Sent = 0;
        uint64 Dropped = 0;
        uint64 Coalesced = 0;
        HermesOutboundLaneStats Lanes[HERMES_LANE_COUNT];
    };

    HermesOutboundQueueStats g_HermesOutboundQueueStats;

    struct HermesOutboundFrame
    {
        WorldSession* Session = nullptr;
        // 【审计修复】记录会话对应的账号 ID，flush 发送前用于验证会话是否仍然存活，避免 use-after-free
        uint32 AccountId = 0;
        uint8 Lane = HERMES_LANE_RPC;
        uint8 MessageType = HERMES_MESSAGE_RESPONSE;
        uint8 Codec = HERMES_CODEC_JSON;
        uint16 SchemaId = HERMES_SCHEMA_JSON_RPC;
        uint16 MethodId = HERMES_METHOD_UNKNOWN;
        uint32 RequestId = 0;
        uint32 Sequence = 0;
        std::string Payload;
    };

    // 【审计修复】g_HermesStateMutex 保护出站队列/队列统计/分片缓冲/限速桶/调试统计等跨线程共享容器
    // （世界线程 opcode 处理、地图线程 CanPacketSend、跨模块 API 均会并发访问）。
    // 约定：持有该锁期间禁止调用 session->SendPacket / 查库，发送必须先将帧 swap 到局部容器再解锁执行。
    std::mutex g_HermesStateMutex;
    // 【审计修复】g_HermesReadyMutex（读多写少）保护插件就绪表 g_HermesAddonReadyPlayers / g_HermesAddonReadyPrefixes
    std::shared_mutex g_HermesReadyMutex;

    std::deque<HermesOutboundFrame> g_HermesOutboundQueue;
    // 【审计修复】批量守卫仅用于"同线程批量处理期间暂缓 flush"，改为 thread_local 使语义线程内自洽，
    // 避免跨线程非原子 ++/-- 导致计数错乱、队列滞留
    thread_local uint32 g_HermesOutboundBatchDepth = 0;
    std::atomic<uint32> g_HermesServerEventSequence{1};
    std::atomic<uint64> g_HermesAddonTakeoverBlockedLegacySmsg{0};
    std::atomic<int> g_HermesDebugRecvLogBudget{200};
    std::atomic<int> g_HermesDebugDispatchLogBudget{200};
    std::atomic<int> g_HermesDebugAddonLogBudget{200};
    std::atomic<int> g_HermesDebugSendLogBudget{200};
    std::atomic<int> g_HermesTraceLogBudget{160};
    std::atomic<int> g_HermesTraceReadyLogBudget{20};
    std::atomic<int> g_HermesTraceRecvFrameLogBudget{40};
    std::atomic<int> g_HermesTracePingLogBudget{20};
    std::atomic<int> g_HermesTraceAddonEventOutLogBudget{60};
    std::atomic<int> g_HermesTraceAddonDispatchLogBudget{40};
    std::atomic<int> g_HermesTraceAddonNotReadyLogBudget{20};
    std::atomic<int> g_HermesTraceLegacyBypassLogBudget{20};

    bool ConsumeHermesDebugBudget(std::atomic<int>& budget)
    {
        return HERMES_BRIDGE_DEBUG_COMM && budget.fetch_sub(1, std::memory_order_relaxed) > 0;
    }

    bool ConsumeHermesTraceBudget()
    {
        return HERMES_BRIDGE_TRACE_SERVER_COMM && g_HermesTraceLogBudget.fetch_sub(1, std::memory_order_relaxed) > 0;
    }

    bool ConsumeHermesTraceBudget(std::atomic<int>& budget)
    {
        return HERMES_BRIDGE_TRACE_SERVER_COMM && budget.fetch_sub(1, std::memory_order_relaxed) > 0 && ConsumeHermesTraceBudget();
    }

    bool IsHermesTracePayload(std::string const& payload)
    {
        return payload.rfind("trace ", 0) == 0 || payload.rfind("addon-compat ", 0) == 0;
    }

    bool ShouldTraceHermesServerMethod(uint16 methodId)
    {
        switch (methodId)
        {
            case HERMES_METHOD_HELLO:
            case HERMES_METHOD_ADDON_DISPATCH:
            case HERMES_METHOD_ADDON_MESSAGE:
            case HERMES_METHOD_PLAYER_GET_SNAPSHOT:
            case HERMES_METHOD_PLAYER_GET_ATTRIBUTES:
            case HERMES_METHOD_ABYSS_GET_EQUIPMENT_PAGE:
            case HERMES_METHOD_ABYSS_GET_SET_OVERVIEW:
            case HERMES_METHOD_ABYSS_GET_SET_BONUSES:
            case HERMES_METHOD_ABYSS_GET_RELICS:
            case HERMES_METHOD_TOOLTIP_QUERY:
            case HERMES_METHOD_TOOLTIP_QUERY_TEMPLATE:
            case HERMES_METHOD_TOOLTIP_INSPECT_ITEM_GUID:
            case HERMES_METHOD_TOOLTIP_LIST_PENDING:
            case HERMES_METHOD_MALL_GET_CATEGORIES:
            case HERMES_METHOD_MALL_GET_ITEMS:
            case HERMES_METHOD_SYNTHESIS_LIST:
            case HERMES_METHOD_SYNTHESIS_DO:
            case HERMES_METHOD_MALL_PURCHASE:
                return true;
            default:
                return false;
        }
    }

    char const* HexDigits()
    {
        return "0123456789ABCDEF";
    }

    std::string HermesByteHex(unsigned char value)
    {
        std::string out;
        out.reserve(4);
        out += "0x";
        out += HexDigits()[(value >> 4) & 0x0F];
        out += HexDigits()[value & 0x0F];
        return out;
    }

    bool IsHermesUtf8Continuation(unsigned char value)
    {
        return (value & 0xC0) == 0x80;
    }

    struct HermesUtf8Check
    {
        bool Valid = true;
        std::size_t InvalidOffset = 0;
        unsigned char InvalidByte = 0;
        uint32 Replacements = 0;
    };

    bool TryHermesUtf8Sequence(std::string const& text, std::size_t offset, std::size_t& length)
    {
        unsigned char const lead = static_cast<unsigned char>(text[offset]);
        std::size_t const remaining = text.size() - offset;

        length = 1;
        if (lead <= 0x7F)
            return true;

        if (lead >= 0xC2 && lead <= 0xDF)
        {
            length = 2;
            return remaining >= 2 && IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 1]));
        }

        if (lead == 0xE0)
        {
            length = 3;
            return remaining >= 3 &&
                static_cast<unsigned char>(text[offset + 1]) >= 0xA0 &&
                static_cast<unsigned char>(text[offset + 1]) <= 0xBF &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 2]));
        }

        if ((lead >= 0xE1 && lead <= 0xEC) || (lead >= 0xEE && lead <= 0xEF))
        {
            length = 3;
            return remaining >= 3 &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 1])) &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 2]));
        }

        if (lead == 0xED)
        {
            length = 3;
            return remaining >= 3 &&
                static_cast<unsigned char>(text[offset + 1]) >= 0x80 &&
                static_cast<unsigned char>(text[offset + 1]) <= 0x9F &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 2]));
        }

        if (lead == 0xF0)
        {
            length = 4;
            return remaining >= 4 &&
                static_cast<unsigned char>(text[offset + 1]) >= 0x90 &&
                static_cast<unsigned char>(text[offset + 1]) <= 0xBF &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 2])) &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 3]));
        }

        if (lead >= 0xF1 && lead <= 0xF3)
        {
            length = 4;
            return remaining >= 4 &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 1])) &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 2])) &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 3]));
        }

        if (lead == 0xF4)
        {
            length = 4;
            return remaining >= 4 &&
                static_cast<unsigned char>(text[offset + 1]) >= 0x80 &&
                static_cast<unsigned char>(text[offset + 1]) <= 0x8F &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 2])) &&
                IsHermesUtf8Continuation(static_cast<unsigned char>(text[offset + 3]));
        }

        return false;
    }

    HermesUtf8Check CheckHermesUtf8(std::string const& text)
    {
        HermesUtf8Check result;

        for (std::size_t offset = 0; offset < text.size();)
        {
            std::size_t length = 1;
            if (TryHermesUtf8Sequence(text, offset, length))
            {
                offset += length;
                continue;
            }

            if (result.Valid)
            {
                result.Valid = false;
                result.InvalidOffset = offset;
                result.InvalidByte = static_cast<unsigned char>(text[offset]);
            }

            ++result.Replacements;
            ++offset;
        }

        return result;
    }

    std::string HermesUtf8OffsetText(HermesUtf8Check const& check)
    {
        return check.Valid ? "-" : std::to_string(check.InvalidOffset);
    }

    std::string HermesUtf8ByteText(HermesUtf8Check const& check)
    {
        return check.Valid ? "-" : HermesByteHex(check.InvalidByte);
    }

    std::string HermesLogPreview(std::string const& text, std::size_t maxLen = 96)
    {
        std::string preview;
        preview.reserve(std::min(text.size() * 4, maxLen + 3));

        for (char ch : text)
        {
            if (preview.size() >= maxLen)
            {
                preview += "...";
                break;
            }

            unsigned char c = static_cast<unsigned char>(ch);
            if (c >= 32 && c < 127)
            {
                preview += ch;
                continue;
            }

            if (preview.size() + 4 > maxLen)
            {
                preview += "...";
                break;
            }

            preview += "\\x";
            preview += HexDigits()[(c >> 4) & 0x0F];
            preview += HexDigits()[c & 0x0F];
        }

        return preview;
    }

    struct HermesOutboundDebugBucket
    {
        uint8 Lane = 0;
        uint8 MessageType = 0;
        uint8 Codec = 0;
        uint16 SchemaId = 0;
        uint16 MethodId = 0;
        uint64 Sent = 0;
        uint64 SentBytes = 0;
        uint64 Suppressed = 0;
        uint64 SuppressedBytes = 0;
    };

    struct HermesOutboundDebugStats
    {
        uint32 WindowStartMs = 0;
        uint64 Sent = 0;
        uint64 SentBytes = 0;
        uint64 Suppressed = 0;
        uint64 SuppressedBytes = 0;
        std::unordered_map<uint64, HermesOutboundDebugBucket> Buckets;
    };

    struct HermesOutboundRateBucket
    {
        uint32 WindowStartMs = 0;
        uint32 LastSentMs = 0;
        uint32 Count = 0;
        uint64 Bytes = 0;
    };

    HermesOutboundDebugStats g_HermesOutboundDebugStats;
    std::unordered_map<uint32, HermesOutboundRateBucket> g_HermesOutboundRateBuckets;
    std::unordered_set<uint32> g_HermesAddonReadyPlayers;
    std::unordered_map<uint32, std::unordered_set<std::string>> g_HermesAddonReadyPrefixes;

    struct HermesAddonChunkBuffer
    {
        uint32 Total = 0;
        uint32 Received = 0;
        std::vector<std::string> Parts;
    };

    std::unordered_map<std::string, HermesAddonChunkBuffer> g_HermesAddonChunkBuffers;

    struct HermesSchemaDescriptor
    {
        uint16 SchemaId;
        char const* Name;
        uint8 Lane;
        uint8 MessageType;
        uint8 Codec;
        char const* Status;
        char const* Description;
    };

    constexpr HermesSchemaDescriptor HERMES_SCHEMA_REGISTRY[] =
    {
        { HERMES_SCHEMA_JSON_RPC, "json-rpc-2.0", HERMES_LANE_RPC, HERMES_MESSAGE_REQUEST, HERMES_CODEC_JSON, "active", "JSON-RPC control and low-frequency RPC envelope" },
        { HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1, "custom.damage.v1", HERMES_LANE_EVENT, HERMES_MESSAGE_EVENT, HERMES_CODEC_BINARY, "active", "Binary custom damage event frame" },
        { HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1, "unit.vitals.snapshot.v1", HERMES_LANE_SNAPSHOT, HERMES_MESSAGE_EVENT, HERMES_CODEC_BINARY, "active", "Binary unit vitals snapshot frame" },
        { HERMES_SCHEMA_BULK_CHUNK_V1, "bulk.chunk.v1", HERMES_LANE_BULK, HERMES_MESSAGE_CHUNK, HERMES_CODEC_BINARY, "active", "Chunked large payload transfer frame" }
    };

    struct HermesPanelStatDef
    {
        uint32 Id;
        Stats Stat;
    };

    constexpr HermesPanelStatDef HERMES_PANEL_PRIMARY_STATS[] =
    {
        { 4, STAT_STRENGTH  },
        { 3, STAT_AGILITY   },
        { 7, STAT_STAMINA   },
        { 5, STAT_INTELLECT },
        { 6, STAT_SPIRIT    }
    };

    using HermesAttributeFields = std::vector<std::pair<std::string, std::string>>;

    bool ParseJsonPayload(std::string const& payload, Json::Value& request, std::string& error)
    {
        try
        {
            Json::Reader reader;
            if (reader.parse(payload.data(), payload.data() + payload.size(), request))
                return true;

            error = reader.getFormattedErrorMessages();
            return false;
        }
        catch (std::exception const& ex)
        {
            error = ex.what();
            return false;
        }
        catch (...)
        {
            error = "unknown exception";
            return false;
        }
    }

    std::string EscapeJsonString(std::string const& value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 8);

        for (std::size_t offset = 0; offset < value.size();)
        {
            unsigned char const c = static_cast<unsigned char>(value[offset]);
            if (c >= 0x80)
            {
                std::size_t length = 1;
                if (TryHermesUtf8Sequence(value, offset, length))
                {
                    escaped.append(value, offset, length);
                    offset += length;
                    continue;
                }

                escaped += '?';
                ++offset;
                continue;
            }

            char const ch = value[offset++];
            switch (ch)
            {
                case '"':
                    escaped += "\\\"";
                    break;
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\b':
                    escaped += "\\b";
                    break;
                case '\f':
                    escaped += "\\f";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20)
                        escaped += ' ';
                    else
                        escaped += ch;
                    break;
            }
        }

        return escaped;
    }

    std::string SanitizeAddonPayloadText(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '|' || ch == '^' || ch == '~' || ch == '\t' || ch == '\r' || ch == '\n')
                ch = '/';
        }

        return value;
    }

    std::vector<std::string> SplitHermesPayloadFields(std::string const& text, char delimiter)
    {
        std::vector<std::string> fields;
        std::stringstream stream(text);
        std::string token;
        while (std::getline(stream, token, delimiter))
            fields.push_back(token);

        if (!text.empty() && text.back() == delimiter)
            fields.emplace_back();

        return fields;
    }

    uint32 ParseHermesUInt(std::string const& text, uint32 defaultValue = 0)
    {
        if (text.empty())
            return defaultValue;

        char* end = nullptr;
        unsigned long value = std::strtoul(text.c_str(), &end, 10);
        if (end == text.c_str())
            return defaultValue;

        return static_cast<uint32>(value);
    }

    std::string BuildHermesAddonIconPath(char const* inventoryIcon)
    {
        if (!inventoryIcon || !*inventoryIcon)
            return "Interface\\Icons\\INV_Misc_QuestionMark";

        std::string icon(inventoryIcon);
        if (icon.find("Interface\\") == 0 || icon.find("interface\\") == 0)
            return icon;

        return "Interface\\Icons\\" + icon;
    }

    std::string GetHermesItemIconPathForAddon(uint32 itemId, uint32 fallbackDisplayId = 0)
    {
        uint32 displayId = fallbackDisplayId;
        if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId))
            displayId = itemTemplate->DisplayInfoID;

        if (displayId == 0)
            return "Interface\\Icons\\INV_Misc_QuestionMark";

        if (ItemDisplayInfoEntry const* displayInfo = sItemDisplayInfoStore.LookupEntry(displayId))
            return BuildHermesAddonIconPath(displayInfo->inventoryIcon);

        return "Interface\\Icons\\INV_Misc_QuestionMark";
    }

    void EnsureHermesMallDataLoaded()
    {
        if (!sMallSystem || !sMallSystem->IsMallSystemEnable())
            return;

        if (sMallSystem->GetMallItems().empty())
            sMallSystem->LoadMallSystemData();
    }

    std::string LimitHermesMallText(std::string value, size_t maxLength)
    {
        value = SanitizeAddonPayloadText(value);
        if (value.length() <= maxLength)
            return value;

        if (maxLength <= 3)
            return value.substr(0, maxLength);

        size_t end = maxLength - 3;
        while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80)
            --end;

        return value.substr(0, end) + "...";
    }

    std::string GetHermesMallItemName(MallSystemInfo const& info)
    {
        if (info.displayItemEntry > 0)
        {
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(info.displayItemEntry))
                return itemTemplate->Name1;
        }

        if (!info.comment.empty())
            return info.comment;

        return "商品 #" + std::to_string(info.id);
    }

    std::string GetHermesMallIconPath(MallSystemInfo const& info)
    {
        if (info.displayItemEntry > 0)
            return GetHermesItemIconPathForAddon(info.displayItemEntry);

        if (!info.categoryIcon.empty())
            return info.categoryIcon;

        return "Interface\\Icons\\INV_Misc_QuestionMark";
    }

    std::string BuildHermesMallCategoriesPayload()
    {
        EnsureHermesMallDataLoaded();

        std::vector<MallSystemInfo const*> categories;
        for (auto const& mallItem : sMallSystem->GetMallItems())
        {
            MallSystemInfo const& info = mallItem.second;
            if (info.categoryName.empty())
                continue;

            bool exists = false;
            for (MallSystemInfo const* category : categories)
            {
                if (category->categoryIndex == info.categoryIndex || category->categoryName == info.categoryName)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
                categories.push_back(&info);
        }

        std::sort(categories.begin(), categories.end(), [](MallSystemInfo const* left, MallSystemInfo const* right)
        {
            if (left->categoryIndex != right->categoryIndex)
                return left->categoryIndex < right->categoryIndex;
            return left->categoryName < right->categoryName;
        });

        std::ostringstream payload;
        payload << "CATEGORIES|" << categories.size();
        for (MallSystemInfo const* category : categories)
        {
            payload << '|' << category->categoryIndex
                    << '^' << LimitHermesMallText(category->categoryName, 48)
                    << '^' << LimitHermesMallText(category->categoryIcon, 96)
                    << '^' << category->categoryIndex;
        }

        return payload.str();
    }

    std::string BuildHermesMallItemsPayload(uint32 categoryId)
    {
        EnsureHermesMallDataLoaded();

        std::vector<MallSystemInfo const*> items;
        for (auto const& mallItem : sMallSystem->GetMallItems())
        {
            MallSystemInfo const& info = mallItem.second;
            if (categoryId != 0 && info.categoryIndex != categoryId)
                continue;

            items.push_back(&info);
        }

        std::sort(items.begin(), items.end(), [](MallSystemInfo const* left, MallSystemInfo const* right)
        {
            if (left->categoryIndex != right->categoryIndex)
                return left->categoryIndex < right->categoryIndex;
            if (left->pageIndex != right->pageIndex)
                return left->pageIndex < right->pageIndex;
            return left->id < right->id;
        });

        std::ostringstream payload;
        payload << "ITEMS|" << categoryId << '|' << items.size();
        for (MallSystemInfo const* info : items)
        {
            payload << '|' << info->id
                    << '^' << LimitHermesMallText(GetHermesMallItemName(*info), 64)
                    << '^' << LimitHermesMallText(GetHermesMallIconPath(*info), 96)
                    << '^' << info->displayItemEntry
                    << '^' << info->tryOnItemEntry
                    << '^' << (info->isNew ? 1 : 0)
                    << '^' << info->discount
                    << '^' << LimitHermesMallText(info->categoryName, 48)
                    << '^' << info->categoryIndex
                    << '^' << info->pageIndex;
        }

        return payload.str();
    }

    std::string BuildHermesMallStatusPayload(bool success, std::string const& message)
    {
        std::ostringstream payload;
        payload << (success ? "OK" : "FAIL") << '|' << LimitHermesMallText(message, 120);
        return payload.str();
    }

    struct HermesSynthesisEntry
    {
        uint32 ItemId = 0;
        uint32 UpgradeLevel = 0;
        uint8 ClassType = 0;
        uint32 RequirementId = 0;
        uint32 RewardId = 0;
        float SuccessChance = 100.0f;
        uint32 BoosterItemId = 0;
        float BoosterChance = 0.0f;
        bool DestroyOnFail = false;
        uint32 UnlockWearLevel = 0;
    };

    std::string TrimHermesSynthesisText(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '|' || ch == '^' || ch == '~' || ch == '\t' || ch == '\r' || ch == '\n')
                ch = ' ';
        }
        return value;
    }

    std::string LimitHermesSynthesisText(std::string value, size_t maxLength)
    {
        value = TrimHermesSynthesisText(value);
        if (value.length() <= maxLength)
            return value;

        if (maxLength <= 3)
            return value.substr(0, maxLength);

        size_t end = maxLength - 3;
        while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80)
            --end;

        return value.substr(0, end) + "...";
    }

    std::string FormatHermesSynthesisChance(float value)
    {
        std::ostringstream ss;
        ss.setf(std::ios::fixed);
        ss.precision(value == static_cast<uint32>(value) ? 0 : 1);
        ss << value;
        return ss.str();
    }

    std::string GetHermesSynthesisItemName(uint32 itemId)
    {
        if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId))
            return TrimHermesSynthesisText(itemTemplate->Name1);

        return "未知物品";
    }

    std::string FormatHermesSynthesisMoney(uint64 copper)
    {
        if (!copper)
            return "";

        std::ostringstream ss;
        ss << "金币 " << (copper / 10000);
        uint64 silver = (copper % 10000) / 100;
        uint64 remainCopper = copper % 100;
        if (silver)
            ss << "金" << silver << "银";
        if (remainCopper)
            ss << remainCopper << "铜";
        return ss.str();
    }

    void AppendHermesSynthesisRequirementPart(std::vector<std::string>& parts, std::string const& label, uint64 value)
    {
        if (value == 0)
            return;

        std::ostringstream ss;
        ss << label << " " << value;
        parts.push_back(ss.str());
    }

    std::string BuildHermesSynthesisRequirementSummary(uint32 requirementId)
    {
        if (requirementId == 0)
            return "无额外材料";

        QueryResult result = WorldDatabase.Query(
            "SELECT `需要人物等级`, `消耗金币`, `消耗泡点`, `消耗积分`, `消耗妖币`, `消耗魔币`, "
            "`消耗仙币`, `消耗神币`, `消耗战场分数`, `消耗荣誉点数`, `消耗成就点数`, `是否消耗物品`, "
            "`消耗物品` FROM `_模板_需求` WHERE `id` = {}", requirementId);

        if (!result)
            return "需求模板未找到";

        Field* fields = result->Fetch();
        std::vector<std::string> parts;

        std::string level = fields[0].Get<std::string>();
        if (!level.empty() && level != "0")
            parts.push_back("角色等级 " + level);

        std::string money = FormatHermesSynthesisMoney(fields[1].Get<uint64>());
        if (!money.empty())
            parts.push_back(money);

        AppendHermesSynthesisRequirementPart(parts, "泡点", fields[2].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "积分", fields[3].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "妖币", fields[4].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "魔币", fields[5].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "仙币", fields[6].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "神币", fields[7].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "战场分数", fields[8].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "荣誉点数", fields[9].Get<uint32>());
        AppendHermesSynthesisRequirementPart(parts, "成就点数", fields[10].Get<uint32>());

        std::string items = TrimHermesSynthesisText(fields[12].Get<std::string>());
        if (!items.empty())
        {
            std::vector<std::string> itemParts;
            std::istringstream itemPairs(items);
            std::string itemPair;
            while (std::getline(itemPairs, itemPair, ','))
            {
                std::istringstream itemStream(itemPair);
                uint32 itemId = 0;
                uint32 count = 0;
                itemStream >> itemId >> count;
                if (itemId)
                {
                    std::ostringstream itemText;
                    itemText << GetHermesSynthesisItemName(itemId) << " x" << (count ? count : 1);
                    itemParts.push_back(itemText.str());
                }
            }

            if (!itemParts.empty())
            {
                std::ostringstream ss;
                ss << (fields[11].Get<uint32>() == 1 ? "持有" : "消耗") << "物品 ";
                for (size_t i = 0; i < itemParts.size(); ++i)
                {
                    if (i)
                        ss << ", ";
                    ss << itemParts[i];
                }
                parts.push_back(ss.str());
            }
        }

        if (parts.empty())
            return "无额外材料";

        std::ostringstream summary;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i)
                summary << " / ";
            summary << parts[i];
        }
        return summary.str();
    }

    std::string BuildHermesSynthesisRequirementItems(Player& player, uint32 requirementId)
    {
        if (requirementId == 0)
            return "";

        QueryResult result = WorldDatabase.Query(
            "SELECT `消耗物品` FROM `_模板_需求` WHERE `id` = {}", requirementId);
        if (!result)
            return "";

        std::string items = TrimHermesSynthesisText((*result)[0].Get<std::string>());
        if (items.empty())
            return "";

        std::ostringstream details;
        bool first = true;
        std::istringstream itemPairs(items);
        std::string itemPair;
        while (std::getline(itemPairs, itemPair, ','))
        {
            std::istringstream itemStream(itemPair);
            uint32 itemId = 0;
            uint32 count = 0;
            itemStream >> itemId >> count;
            if (!itemId)
                continue;

            if (!first)
                details << ';';
            first = false;

            details << itemId << ':'
                    << (count ? count : 1) << ':'
                    << player.GetItemCount(itemId, true) << ':'
                    << LimitHermesSynthesisText(GetHermesSynthesisItemName(itemId), 52) << ':'
                    << GetHermesItemIconPathForAddon(itemId);
        }

        return details.str();
    }

    std::string GetHermesSynthesisRewardItemList(uint32 rewardId)
    {
        if (rewardId == 0)
            return "";

        QueryResult result = WorldDatabase.Query("SELECT `奖励物品` FROM `_模板_奖励` WHERE `id` = {}", rewardId);
        if (!result)
            return "";

        return TrimHermesSynthesisText((*result)[0].Get<std::string>());
    }

    std::pair<uint32, uint32> ParseHermesSynthesisFirstRewardItem(std::string const& rewardItems)
    {
        std::istringstream ss(rewardItems);
        uint32 itemId = 0;
        uint32 count = 0;
        ss >> itemId >> count;
        return { itemId, count ? count : 1 };
    }

    std::vector<HermesSynthesisEntry> LoadHermesSynthesisEntries()
    {
        std::vector<HermesSynthesisEntry> entries;
        QueryResult result = WorldDatabase.Query(
            "SELECT `物品id`, `升级等级`, `职业类型`, `需求id`, `升级成功奖励id`, `成功几率`, "
            "`合成几率物品id`, `合成几率提升`, `失败是否摧毁`, `解锁穿戴等级` "
            "FROM `_物品合成` ORDER BY `物品id`, `升级等级`");

        if (!result)
            return entries;

        do
        {
            Field* fields = result->Fetch();
            HermesSynthesisEntry entry;
            entry.ItemId = fields[0].Get<uint32>();
            entry.UpgradeLevel = fields[1].Get<uint32>();
            entry.ClassType = fields[2].Get<uint8>();
            entry.RequirementId = fields[3].Get<uint32>();
            entry.RewardId = fields[4].Get<uint32>();
            entry.SuccessChance = std::clamp(fields[5].Get<float>(), 0.0f, 100.0f);
            entry.BoosterItemId = fields[6].Get<uint32>();
            entry.BoosterChance = std::clamp(fields[7].Get<float>(), 0.0f, 100.0f);
            entry.DestroyOnFail = fields[8].Get<uint8>() != 0;
            entry.UnlockWearLevel = fields[9].Get<uint32>();

            if (!sObjectMgr->GetItemTemplate(entry.ItemId))
                continue;
            if (entry.BoosterItemId && !sObjectMgr->GetItemTemplate(entry.BoosterItemId))
                continue;

            entries.push_back(entry);
        } while (result->NextRow());

        return entries;
    }

    std::string BuildHermesSynthesisRecord(Player& player, HermesSynthesisEntry const& entry)
    {
        uint32 sourceCount = player.GetItemCount(entry.ItemId, true);
        uint32 boosterCount = entry.BoosterItemId ? player.GetItemCount(entry.BoosterItemId, true) : 0;
        bool requirementOk = true;
        if (entry.RequirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            requirementOk = requirementSystem && requirementSystem->CheckRequirements(&player, entry.RequirementId, false);
        }

        std::vector<std::string> rewardDescriptions;
        if (entry.RewardId != 0 && sRewardTemplate->IsEnabled())
            rewardDescriptions = sRewardTemplate->GetRewardDescription(&player, entry.RewardId);

        std::string rewardText = "无奖励";
        if (!rewardDescriptions.empty())
        {
            rewardText.clear();
            for (size_t i = 0; i < rewardDescriptions.size(); ++i)
            {
                if (i)
                    rewardText += " / ";
                rewardText += rewardDescriptions[i];
            }
        }

        std::string rewardItems = GetHermesSynthesisRewardItemList(entry.RewardId);
        auto [nextItemId, nextItemCount] = ParseHermesSynthesisFirstRewardItem(rewardItems);
        std::string nextItemName = nextItemId ? GetHermesSynthesisItemName(nextItemId) : rewardText;
        std::string requirementSummary = BuildHermesSynthesisRequirementSummary(entry.RequirementId);
        std::string requirementItems = BuildHermesSynthesisRequirementItems(player, entry.RequirementId);

        std::ostringstream record;
        record << entry.ItemId << '^'
               << entry.UpgradeLevel << '^'
               << entry.RequirementId << '^'
               << entry.RewardId << '^'
               << FormatHermesSynthesisChance(entry.SuccessChance) << '^'
               << entry.BoosterItemId << '^'
               << FormatHermesSynthesisChance(entry.BoosterChance) << '^'
               << (entry.DestroyOnFail ? 1 : 0) << '^'
               << LimitHermesSynthesisText(GetHermesSynthesisItemName(entry.ItemId), 52) << '^'
               << GetHermesItemIconPathForAddon(entry.ItemId) << '^'
               << nextItemId << '^'
               << LimitHermesSynthesisText(nextItemName, 52) << '^'
               << GetHermesItemIconPathForAddon(nextItemId) << '^'
               << nextItemCount << '^'
               << (requirementOk ? 1 : 0) << '^'
               << sourceCount << '^'
               << boosterCount << '^'
               << LimitHermesSynthesisText(requirementSummary, 80) << '^'
               << LimitHermesSynthesisText(rewardText, 70) << '^'
               << static_cast<uint32>(entry.ClassType) << '^'
               << requirementItems << '^'
               << entry.UnlockWearLevel;
        return record.str();
    }

    bool UnlockHermesSynthesisWearSlots(Player& player, HermesSynthesisEntry const& entry, std::string& message)
    {
        std::string rewardItems = GetHermesSynthesisRewardItemList(entry.RewardId);
        uint32 const unlockItemId = ParseHermesSynthesisFirstRewardItem(rewardItems).first;
        uint8 const limitType = WearControl::GetExclusiveLimit(unlockItemId);
        std::vector<uint8> const slots = WearControl::GetItemWearSlots(unlockItemId, limitType);

        if (entry.UnlockWearLevel == 0)
            return true;

        if (limitType == WEAR_LIMIT_NONE || slots.empty())
        {
            message = "合成成功，但产物没有配置可解锁的专属槽位";
            return false;
        }

        for (uint8 slotPosition : slots)
        {
            bool const unlocked = WearControl::UnlockPlayerWearLevel(&player, limitType, slotPosition, entry.UnlockWearLevel, false);
            if (!unlocked)
            {
                message = "合成成功，但相关槽位穿戴权限解锁失败，请检查 `_穿戴等级权限` 表结构";
                return false;
            }
        }

        message = "合成成功，已解锁仙器穿戴权限";
        return true;
    }

    bool CanUnlockHermesSynthesisWearSlots(Player& player, HermesSynthesisEntry const& entry, std::string& message)
    {
        if (entry.UnlockWearLevel == 0)
            return true;

        std::string rewardItems = GetHermesSynthesisRewardItemList(entry.RewardId);
        uint32 const unlockItemId = ParseHermesSynthesisFirstRewardItem(rewardItems).first;
        uint8 const limitType = WearControl::GetExclusiveLimit(unlockItemId);
        std::vector<uint8> const slots = WearControl::GetItemWearSlots(unlockItemId, limitType);
        if (limitType == WEAR_LIMIT_NONE || slots.empty())
        {
            message = "产物没有配置可解锁的专属槽位";
            return false;
        }

        for (uint8 slotPosition : slots)
            if (!WearControl::CanUnlockPlayerWearLevel(&player, limitType, slotPosition, entry.UnlockWearLevel, &message))
                return false;

        return true;
    }

    std::string BuildHermesSynthesisListPayload(Player& player, uint32 offset, uint32 limit, uint32 classType)
    {
        std::vector<HermesSynthesisEntry> entries = LoadHermesSynthesisEntries();
        std::vector<HermesSynthesisEntry const*> filteredEntries;
        filteredEntries.reserve(entries.size());

        for (HermesSynthesisEntry const& entry : entries)
        {
            if (classType <= 10 && entry.ClassType != classType)
                continue;

            filteredEntries.push_back(&entry);
        }

        uint32 total = static_cast<uint32>(filteredEntries.size());
        if (limit == 0)
            limit = 8;
        limit = std::min<uint32>(limit, 12);
        offset = std::min<uint32>(offset, total);

        std::ostringstream payload;
        payload << "SS_LIST_PAGE:" << offset << '^' << limit << '^' << total << '^';

        bool first = true;
        uint32 end = std::min<uint32>(total, offset + limit);
        for (uint32 index = offset; index < end; ++index)
        {
            if (!first)
                payload << '~';
            first = false;
            payload << BuildHermesSynthesisRecord(player, *filteredEntries[index]);
        }

        return payload.str();
    }

    std::string BuildHermesSynthesisResultPayload(bool success, uint32 itemId, uint32 upgradeLevel, std::string const& message)
    {
        std::ostringstream payload;
        payload << "SS_RESULT:DO^" << (success ? "OK" : "FAIL") << '^'
                << itemId << '^'
                << upgradeLevel << '^'
                << LimitHermesSynthesisText(message, 120);
        return payload.str();
    }

    bool TryFindHermesSynthesisEntry(uint32 itemId, uint32 upgradeLevel, HermesSynthesisEntry& outEntry)
    {
        for (HermesSynthesisEntry const& entry : LoadHermesSynthesisEntries())
        {
            if (entry.ItemId == itemId && entry.UpgradeLevel == upgradeLevel)
            {
                outEntry = entry;
                return true;
            }
        }
        return false;
    }

    std::string ExecuteHermesSynthesis(Player& player, uint32 itemId, uint32 upgradeLevel, bool useBooster)
    {
        HermesSynthesisEntry entry;
        if (!TryFindHermesSynthesisEntry(itemId, upgradeLevel, entry))
            return BuildHermesSynthesisResultPayload(false, itemId, upgradeLevel, "未找到该合成配置");

        if (player.GetItemCount(entry.ItemId, true) == 0)
            return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "背包中没有需要合成的物品");

        if (entry.RewardId == 0 || !sRewardTemplate->IsEnabled() || !sRewardTemplate->GetRewardTemplate(entry.RewardId))
            return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "合成奖励模板不存在或奖励系统未启用");

        if (entry.UnlockWearLevel > 0)
        {
            std::string prerequisiteError;
            if (!CanUnlockHermesSynthesisWearSlots(player, entry, prerequisiteError))
                return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, prerequisiteError);
        }

        if (entry.RequirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            if (!requirementSystem)
                return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "需求系统未初始化");

            if (!requirementSystem->CheckRequirements(&player, entry.RequirementId, true))
                return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "未满足合成需求");
        }

        if (useBooster)
        {
            if (entry.BoosterItemId == 0 || entry.BoosterChance <= 0.0f)
                return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "该配置没有可用的几率提升物品");

            if (player.GetItemCount(entry.BoosterItemId, true) == 0)
                return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "缺少几率提升物品");
        }

        if (entry.RequirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            if (!requirementSystem || !requirementSystem->ConsumeRequirements(&player, entry.RequirementId))
                return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "消耗合成需求失败");
        }

        float finalChance = std::clamp(entry.SuccessChance + (useBooster ? entry.BoosterChance : 0.0f), 0.0f, 100.0f);
        bool success = roll_chance_f(finalChance);

        if (success)
        {
            bool rewarded = sRewardTemplate->GiveReward(&player, entry.RewardId, false, true);
            if (rewarded)
            {
                std::string resultMessage = "合成成功";
                if (entry.UnlockWearLevel > 0)
                    UnlockHermesSynthesisWearSlots(player, entry, resultMessage);
                return BuildHermesSynthesisResultPayload(true, entry.ItemId, entry.UpgradeLevel, resultMessage);
            }

            return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "合成成功但发放奖励失败，请检查奖励模板或背包空间");
        }

        return BuildHermesSynthesisResultPayload(false, entry.ItemId, entry.UpgradeLevel, "合成失败");
    }

    char const* const HERMES_ABYSS_LOAD_CHAPTERS_SQL =
        "SELECT * FROM `_\xE6\xB7\xB1\xE6\xB8\x8A\xE7\xAB\xA0\xE8\x8A\x82\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE7\xAB\xA0\xE8\x8A\x82ID`";

    char const* const HERMES_ABYSS_LOAD_EQUIPMENT_SQL =
        "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE8\xA3\x85\xE5\xA4\x87\xE6\xA8\xA1\xE6\x9D\xBF` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE6\x9D\xA5\xE6\xBA\x90\xE7\xAB\xA0\xE8\x8A\x82`, `\xE6\x9D\xA5\xE6\xBA\x90\xE6\xA8\xA1\xE5\xBC\x8F`, `\xE7\x89\xA9\xE5\x93\x81\xE6\xA8\xA1\xE6\x9D\xBFID`";

    char const* const HERMES_ABYSS_LOAD_SET_BONUSES_SQL =
        "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE5\xA5\x97\xE8\xA3\x85\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE5\xA5\x97\xE8\xA3\x85ID`";

    char const* const HERMES_ABYSS_LOAD_RELICS_SQL =
        "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE9\x81\x97\xE7\x89\xA9\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY 3, 4, 1";

    char const* const HERMES_ABYSS_LOAD_PLAYER_CURRENT_CHAPTER_SQL =
        "SELECT `\xE5\xBD\x93\xE5\x89\x8D\xE7\xAB\xA0\xE8\x8A\x82` FROM `_\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE4\xB8\xBB\xE6\x95\xB0\xE6\x8D\xAE` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

    char const* const HERMES_ABYSS_LOAD_PLAYER_RELIC_STATE_SQL =
        "SELECT * FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE4\xB8\xBB\xE6\x95\xB0\xE6\x8D\xAE` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

    char const* const HERMES_ABYSS_LOAD_PLAYER_COLLECTIONS_SQL =
        "SELECT * FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE6\x94\xB6\xE8\x97\x8F` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

    struct HermesAbyssChapter
    {
        uint16 ChapterId = 0;
        uint8 ActId = 0;
        std::string Name;
    };

    struct HermesAbyssEquipment
    {
        uint32 TemplateId = 0;
        uint32 ItemId = 0;
        std::string ItemName;
        uint8 EquipmentType = 0;
        uint16 SourceChapter = 0;
        uint8 SourceMode = 0;
        uint32 SlotMask = 0;
        uint8 ActId = 0;
        uint16 BaseItemLevel = 0;
        bool FromCacheBoss = false;
        bool RequiresFragments = false;
        std::string FlavorText;
        uint32 SetId = 0;
    };

    struct HermesAbyssSetBonus
    {
        uint32 SetId = 0;
        std::string SetName;
        uint8 ActId = 0;
        uint8 SourceMode = 0;
        std::string TwoPieceDesc;
        std::string FourPieceDesc;
        std::string SixPieceDesc;
        std::string EightPieceDesc;
    };

    struct HermesAbyssRelic
    {
        uint32 ItemId = 0;
        std::string Name;
        uint8 RelicType = 0;
        uint8 ActId = 0;
        uint16 RelatedChapterId = 0;
        uint8 ActiveRule = 0;
        uint32 ExclusiveGroup = 0;
        uint8 RecommendedSlot = 0;
        float SubSlotScale = 0.0f;
        std::string BriefDescription;
    };

    struct HermesAbyssPlayerRelicState
    {
        uint32 MainRelic = 0;
        uint32 SubRelic1 = 0;
        uint32 SubRelic2 = 0;
        uint32 SubRelic3 = 0;
        uint32 SubRelic4 = 0;
        uint32 SubRelic5 = 0;
        uint32 PhaseArtifact = 0;
        uint32 UltimateArtifact = 0;
    };

    std::unordered_map<uint16, HermesAbyssChapter> LoadHermesAbyssChapters()
    {
        std::unordered_map<uint16, HermesAbyssChapter> chapters;
        QueryResult result = WorldDatabase.Query(HERMES_ABYSS_LOAD_CHAPTERS_SQL);
        if (!result)
            return chapters;

        do
        {
            Field* fields = result->Fetch();
            HermesAbyssChapter chapter;
            chapter.ChapterId = fields[0].Get<uint16>();
            chapter.ActId = fields[1].Get<uint8>();
            chapter.Name = fields[2].Get<std::string>();
            chapters[chapter.ChapterId] = chapter;
        } while (result->NextRow());

        return chapters;
    }

    std::vector<HermesAbyssEquipment> LoadHermesAbyssEquipment()
    {
        std::vector<HermesAbyssEquipment> equipmentList;
        QueryResult result = WorldDatabase.Query(HERMES_ABYSS_LOAD_EQUIPMENT_SQL);
        if (!result)
            return equipmentList;

        do
        {
            Field* fields = result->Fetch();
            HermesAbyssEquipment equipment;
            equipment.TemplateId = fields[0].Get<uint32>();
            equipment.ItemId = fields[1].Get<uint32>();
            equipment.ItemName = fields[2].Get<std::string>();
            equipment.EquipmentType = fields[3].Get<uint8>();
            equipment.SourceChapter = fields[4].Get<uint16>();
            equipment.SourceMode = fields[5].Get<uint8>();
            equipment.SlotMask = fields[6].Get<uint32>();
            equipment.ActId = fields[7].Get<uint8>();
            equipment.BaseItemLevel = fields[8].Get<uint16>();
            equipment.FromCacheBoss = fields[19].Get<bool>();
            equipment.RequiresFragments = fields[20].Get<bool>();
            equipment.FlavorText = fields[21].Get<std::string>();
            equipment.SetId = fields[22].Get<uint32>();
            equipmentList.push_back(equipment);
        } while (result->NextRow());

        return equipmentList;
    }

    std::vector<HermesAbyssSetBonus> LoadHermesAbyssSetBonuses()
    {
        std::vector<HermesAbyssSetBonus> bonuses;
        QueryResult result = WorldDatabase.Query(HERMES_ABYSS_LOAD_SET_BONUSES_SQL);
        if (!result)
            return bonuses;

        do
        {
            Field* fields = result->Fetch();
            HermesAbyssSetBonus bonus;
            bonus.SetId = fields[0].Get<uint32>();
            bonus.SetName = fields[1].Get<std::string>();
            bonus.ActId = fields[2].Get<uint8>();
            bonus.SourceMode = fields[3].Get<uint8>();
            bonus.TwoPieceDesc = fields[4].Get<std::string>();
            bonus.FourPieceDesc = fields[10].Get<std::string>();
            bonus.SixPieceDesc = fields[16].Get<std::string>();
            bonus.EightPieceDesc = fields[22].Get<std::string>();
            bonuses.push_back(bonus);
        } while (result->NextRow());

        return bonuses;
    }

    std::vector<HermesAbyssRelic> LoadHermesAbyssRelics()
    {
        std::vector<HermesAbyssRelic> relics;
        QueryResult result = WorldDatabase.Query(HERMES_ABYSS_LOAD_RELICS_SQL);
        if (!result)
            return relics;

        do
        {
            Field* fields = result->Fetch();
            HermesAbyssRelic relic;
            relic.ItemId = fields[0].Get<uint32>();
            relic.Name = fields[1].Get<std::string>();
            relic.RelicType = fields[2].Get<uint8>();
            relic.ActId = fields[3].Get<uint8>();
            relic.RelatedChapterId = fields[4].Get<uint16>();
            relic.ActiveRule = fields[6].Get<uint8>();
            relic.ExclusiveGroup = fields[7].Get<uint32>();
            relic.RecommendedSlot = fields[9].Get<uint8>();
            relic.SubSlotScale = fields[11].Get<float>();
            relic.BriefDescription = fields[13].Get<std::string>();

            if (relic.RelicType == 2)
                relic.RecommendedSlot = 7;
            else if (relic.RelicType == 3)
                relic.RecommendedSlot = 8;

            relics.push_back(relic);
        } while (result->NextRow());

        return relics;
    }

    HermesAbyssPlayerRelicState LoadHermesAbyssPlayerRelicState(uint32 guidLow)
    {
        HermesAbyssPlayerRelicState state;
        QueryResult result = CharacterDatabase.Query(Acore::StringFormat(HERMES_ABYSS_LOAD_PLAYER_RELIC_STATE_SQL, guidLow));
        if (!result)
            return state;

        Field* fields = result->Fetch();
        state.MainRelic = fields[7].Get<uint32>();
        state.SubRelic1 = fields[8].Get<uint32>();
        state.SubRelic2 = fields[9].Get<uint32>();
        state.SubRelic3 = fields[10].Get<uint32>();
        state.SubRelic4 = fields[11].Get<uint32>();
        state.SubRelic5 = fields[12].Get<uint32>();
        state.PhaseArtifact = fields[13].Get<uint32>();
        state.UltimateArtifact = fields[14].Get<uint32>();
        return state;
    }

    std::vector<uint32> LoadHermesAbyssPlayerCollectedRelics(uint32 guidLow)
    {
        std::vector<uint32> collected;
        QueryResult result = CharacterDatabase.Query(Acore::StringFormat(HERMES_ABYSS_LOAD_PLAYER_COLLECTIONS_SQL, guidLow));
        if (!result)
            return collected;

        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[2].Get<uint32>();
            if (itemId != 0 && std::find(collected.begin(), collected.end(), itemId) == collected.end())
                collected.push_back(itemId);
        } while (result->NextRow());

        return collected;
    }

    uint8 GetHermesAbyssActiveRelicSlot(HermesAbyssPlayerRelicState const& state, uint32 itemId)
    {
        if (itemId == 0)
            return 0;
        if (state.MainRelic == itemId)
            return 1;
        if (state.SubRelic1 == itemId)
            return 2;
        if (state.SubRelic2 == itemId)
            return 3;
        if (state.SubRelic3 == itemId)
            return 4;
        if (state.SubRelic4 == itemId)
            return 5;
        if (state.SubRelic5 == itemId)
            return 6;
        if (state.PhaseArtifact == itemId)
            return 7;
        if (state.UltimateArtifact == itemId)
            return 8;
        return 0;
    }

    std::string GetHermesAbyssChapterName(std::unordered_map<uint16, HermesAbyssChapter> const& chapters, uint16 chapterId)
    {
        auto itr = chapters.find(chapterId);
        return itr != chapters.end() ? SanitizeAddonPayloadText(itr->second.Name) : "";
    }

    uint32 GetHermesAbyssCurrentActId(Player& player, std::unordered_map<uint16, HermesAbyssChapter> const& chapters)
    {
        QueryResult result = WorldDatabase.Query(Acore::StringFormat(HERMES_ABYSS_LOAD_PLAYER_CURRENT_CHAPTER_SQL, player.GetGUID().GetCounter()));
        if (!result)
            return 0;

        uint16 currentChapter = result->Fetch()[0].Get<uint16>();
        auto itr = chapters.find(currentChapter);
        return itr != chapters.end() ? itr->second.ActId : 0;
    }

    HermesAbyssSetBonus const* FindHermesAbyssSetBonus(std::vector<HermesAbyssSetBonus> const& bonuses, uint8 sourceMode, uint8 actId)
    {
        HermesAbyssSetBonus const* best = nullptr;
        for (HermesAbyssSetBonus const& bonus : bonuses)
        {
            if (bonus.SourceMode != sourceMode || bonus.ActId != actId)
                continue;

            if (!best || bonus.SetId < best->SetId)
                best = &bonus;
        }

        return best;
    }

    std::string HermesAbyssOptionalText(std::string const& text)
    {
        std::string sanitized = SanitizeAddonPayloadText(text);
        return sanitized.empty() ? " " : sanitized;
    }

    std::string GetHermesAbyssSlotName(uint32 slotMask)
    {
        switch (slotMask)
        {
            case 1:   return "\xE6\xAD\xA6\xE5\x99\xA8";
            case 2:   return "\xE5\xA4\xB4\xE9\x83\xA8";
            case 4:   return "\xE8\x83\xB8\xE7\x94\xB2";
            case 16:  return "\xE8\x85\xB0\xE5\xB8\xA6";
            case 32:  return "\xE9\x9D\xB4\xE5\xAD\x90";
            case 64:  return "\xE6\x88\x92\xE6\x8C\x87";
            case 128: return "\xE9\xA5\xB0\xE5\x93\x81";
            case 256: return "\xE6\x8A\xAB\xE9\xA3\x8E";
            case 512: return "\xE6\xB3\x95\xE5\x99\xA8";
            default:  break;
        }

        std::ostringstream out;
        out << "\xE9\x83\xA8\xE4\xBD\x8D#" << slotMask;
        return out.str();
    }

    char const* const HERMES_CULTIVATION_LOAD_REALMS_SQL =
        "SELECT `\xE5\xA2\x83\xE7\x95\x8C\xE7\xAD\x89\xE7\xBA\xA7`, `\xE5\xA2\x83\xE7\x95\x8C\xE5\x90\x8D\xE7\xA7\xB0`, `\xE5\xA4\xA7\xE5\xA2\x83\xE7\x95\x8C`, `\xE5\xB0\x8F\xE5\xA2\x83\xE7\x95\x8C`, `\xE5\xB1\x9E\xE6\x80\xA7\xE5\x8A\xA0\xE6\x88\x90\xE7\x99\xBE\xE5\x88\x86\xE6\xAF\x94`, "
        "`\xE5\x8D\x87\xE7\xBA\xA7\xE9\x9C\x80\xE6\xB1\x82\x49\x44`, `\xE6\x98\xAF\xE5\x90\xA6\xE9\x9C\x80\xE8\xA6\x81\xE6\xB8\xA1\xE5\x8A\xAB`, `\xE6\xB8\xA1\xE5\x8A\xAB\xE9\x9C\x80\xE6\xB1\x82\x49\x44`, `\xE6\xB8\xA1\xE5\x8A\xAB\x42\x6F\x73\x73\xE5\x85\xA5\xE5\x8F\xA3` "
        "FROM `\x5F\xE4\xBF\xAE\xE4\xBB\x99\xE5\xA2\x83\xE7\x95\x8C\xE9\x85\x8D\xE7\xBD\xAE` ORDER BY `\xE5\xA2\x83\xE7\x95\x8C\xE7\xAD\x89\xE7\xBA\xA7`";

    char const* const HERMES_CULTIVATION_LOAD_SKILLS_SQL =
        "SELECT `\xE6\x8A\x80\xE8\x83\xBD\x49\x44`, `\xE6\x8A\x80\xE8\x83\xBD\xE5\x90\x8D\xE7\xA7\xB0`, `\xE8\xA7\xA3\xE9\x94\x81\xE5\xA2\x83\xE7\x95\x8C\xE7\xAD\x89\xE7\xBA\xA7`, `\xE6\xB3\x95\xE6\x9C\xAF\x49\x44`, `\xE6\x8A\x80\xE8\x83\xBD\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE6\x8A\x80\xE8\x83\xBD\xE6\x8F\x8F\xE8\xBF\xB0` "
        "FROM `\x5F\xE4\xBF\xAE\xE4\xBB\x99\xE6\x8A\x80\xE8\x83\xBD\xE9\x85\x8D\xE7\xBD\xAE` ORDER BY `\xE8\xA7\xA3\xE9\x94\x81\xE5\xA2\x83\xE7\x95\x8C\xE7\xAD\x89\xE7\xBA\xA7`";

    char const* const HERMES_CULTIVATION_LOAD_PLAYER_SQL =
        "SELECT `\xE4\xBF\xAE\xE4\xBB\x99\xE7\xAD\x89\xE7\xBA\xA7`, `\xE6\xB8\xA1\xE5\x8A\xAB\xE5\x86\xB7\xE5\x8D\xB4\xE6\x97\xB6\xE9\x97\xB4`, `\xE6\xB8\xA1\xE5\x8A\xAB\xE9\x80\x9A\xE8\xBF\x87\xE7\xAD\x89\xE7\xBA\xA7` "
        "FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE4\xBF\xAE\xE4\xBB\x99\xE6\x95\xB0\xE6\x8D\xAE` WHERE `\xE8\xA7\x92\xE8\x89\xB2\x69\x64` = {}";

    struct HermesCultivationRealm
    {
        uint32 Level = 0;
        std::string Name;
        uint8 MajorRealm = 0;
        uint8 MinorRealm = 0;
        float StatBonus = 0.0f;
        uint32 UpgradeRequireId = 0;
        bool NeedTribulation = false;
        uint32 TribRequireId = 0;
        uint32 TribBossEntry = 0;
    };

    struct HermesCultivationSkill
    {
        uint32 SkillId = 0;
        std::string Name;
        uint32 UnlockLevel = 0;
        uint32 SpellId = 0;
        uint8 SkillType = 0;
        std::string Description;
    };

    struct HermesCultivationPlayerData
    {
        uint32 Level = 0;
        uint32 TribCooldown = 0;
    };

    std::vector<HermesCultivationRealm> LoadHermesCultivationRealms()
    {
        std::vector<HermesCultivationRealm> realms;
        QueryResult result = WorldDatabase.Query(HERMES_CULTIVATION_LOAD_REALMS_SQL);
        if (!result)
            return realms;

        do
        {
            Field* fields = result->Fetch();
            HermesCultivationRealm realm;
            realm.Level = fields[0].Get<uint32>();
            realm.Name = fields[1].Get<std::string>();
            realm.MajorRealm = fields[2].Get<uint8>();
            realm.MinorRealm = fields[3].Get<uint8>();
            realm.StatBonus = fields[4].Get<float>();
            realm.UpgradeRequireId = fields[5].Get<uint32>();
            realm.NeedTribulation = fields[6].Get<uint8>() != 0;
            realm.TribRequireId = fields[7].Get<uint32>();
            realm.TribBossEntry = fields[8].Get<uint32>();
            realms.push_back(realm);
        } while (result->NextRow());

        return realms;
    }

    std::vector<HermesCultivationSkill> LoadHermesCultivationSkills()
    {
        std::vector<HermesCultivationSkill> skills;
        QueryResult result = WorldDatabase.Query(HERMES_CULTIVATION_LOAD_SKILLS_SQL);
        if (!result)
            return skills;

        do
        {
            Field* fields = result->Fetch();
            HermesCultivationSkill skill;
            skill.SkillId = fields[0].Get<uint32>();
            skill.Name = fields[1].Get<std::string>();
            skill.UnlockLevel = fields[2].Get<uint32>();
            skill.SpellId = fields[3].Get<uint32>();
            skill.SkillType = fields[4].Get<uint8>();
            skill.Description = fields[5].Get<std::string>();
            skills.push_back(skill);
        } while (result->NextRow());

        return skills;
    }

    HermesCultivationPlayerData LoadHermesCultivationPlayerData(uint32 guidLow)
    {
        HermesCultivationPlayerData playerData;
        QueryResult result = CharacterDatabase.Query(Acore::StringFormat(HERMES_CULTIVATION_LOAD_PLAYER_SQL, guidLow));
        if (!result)
            return playerData;

        Field* fields = result->Fetch();
        playerData.Level = fields[0].Get<uint32>();
        playerData.TribCooldown = fields[1].Get<uint32>();
        return playerData;
    }

    std::string BuildHermesCultivationRealmsPayload(std::vector<HermesCultivationRealm> const& realms)
    {
        std::ostringstream payload;
        payload << "CS_REALMS:";

        bool first = true;
        for (HermesCultivationRealm const& realm : realms)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << realm.Level << '^'
                    << SanitizeAddonPayloadText(realm.Name) << '^'
                    << static_cast<uint32>(realm.MajorRealm) << '^'
                    << static_cast<uint32>(realm.MinorRealm) << '^'
                    << realm.StatBonus << '^'
                    << realm.UpgradeRequireId << '^'
                    << (realm.NeedTribulation ? 1 : 0) << '^'
                    << realm.TribRequireId << '^'
                    << realm.TribBossEntry;
        }

        return payload.str();
    }

    std::string BuildHermesCultivationSkillsPayload(std::vector<HermesCultivationSkill> const& skills)
    {
        std::ostringstream payload;
        payload << "CS_SKILLS:";

        bool first = true;
        for (HermesCultivationSkill const& skill : skills)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << skill.SkillId << '^'
                    << SanitizeAddonPayloadText(skill.Name) << '^'
                    << skill.UnlockLevel << '^'
                    << skill.SpellId << '^'
                    << static_cast<uint32>(skill.SkillType) << '^'
                    << SanitizeAddonPayloadText(skill.Description);
        }

        return payload.str();
    }

    std::string BuildHermesCultivationStatePayload(uint32 guidLow, std::vector<HermesCultivationRealm> const& realms, std::vector<HermesCultivationSkill> const& skills)
    {
        HermesCultivationPlayerData playerData = LoadHermesCultivationPlayerData(guidLow);

        HermesCultivationRealm const* currentRealm = nullptr;
        float totalStatBonus = 0.0f;
        for (HermesCultivationRealm const& realm : realms)
        {
            if (realm.Level <= playerData.Level)
                totalStatBonus += realm.StatBonus;
            if (realm.Level == playerData.Level)
                currentRealm = &realm;
        }

        std::ostringstream skillIds;
        bool firstSkill = true;
        for (HermesCultivationSkill const& skill : skills)
        {
            if (playerData.Level < skill.UnlockLevel)
                continue;

            if (!firstSkill)
                skillIds << ',';
            firstSkill = false;
            skillIds << skill.SpellId;
        }

        std::string realmName = currentRealm ? currentRealm->Name : "\xE5\x87\xA1\xE4\xBA\xBA";
        uint32 majorRealm = currentRealm ? static_cast<uint32>(currentRealm->MajorRealm) : 0;

        std::ostringstream payload;
        payload << "CS_STATE:" << playerData.Level << '|'
                << SanitizeAddonPayloadText(realmName) << '|'
                << majorRealm << '|'
                << totalStatBonus << '|'
                << playerData.TribCooldown << '|'
                << skillIds.str();

        return payload.str();
    }

    std::string BuildHermesBoundaryRecord(Player& player, uint8 type, uint32 requestedLevel = std::numeric_limits<uint32>::max())
    {
        uint32 level = requestedLevel == std::numeric_limits<uint32>::max()
            ? sBoundaryMgr->GetBoundaryLevel(&player, type)
            : requestedLevel;

        BoundaryInfo const* info = sBoundaryMgr->GetBoundaryInfo(type, level);
        uint32 expRequired = info ? info->expRequired : 1000;

        std::ostringstream payload;
        payload << static_cast<uint32>(type) << '^'
                << level << '^'
                << expRequired << '^';

        if (info)
        {
            payload << SanitizeAddonPayloadText(info->title) << '^'
                    << SanitizeAddonPayloadText(info->description) << '^'
                    << SanitizeAddonPayloadText(info->auras) << '^'
                    << info->strBonus << '^'
                    << info->agiBonus << '^'
                    << info->staBonus << '^'
                    << info->intBonus << '^'
                    << info->spiBonus << '^'
                    << info->healthBonus << '^'
                    << info->manaBonus;
        }
        else
        {
            payload << "^" << "^" << "^0^0^0^0^0^0^0";
        }

        return payload.str();
    }

    std::string BuildHermesBoundaryAllPayload(Player& player)
    {
        std::ostringstream payload;
        payload << "BOUNDARY_ALL:" << sBoundaryMgr->GetBoundaryTotalExp(&player) << '|';

        bool first = true;
        for (uint8 type = 1; type <= 6; ++type)
        {
            if (!first)
                payload << '~';
            first = false;
            payload << BuildHermesBoundaryRecord(player, type);
        }

        return payload.str();
    }

    std::string BuildHermesBoundaryDetailPayload(Player& player, uint8 type, uint32 requestedLevel = std::numeric_limits<uint32>::max())
    {
        std::ostringstream payload;
        payload << "BOUNDARY_DETAIL:" << sBoundaryMgr->GetBoundaryTotalExp(&player) << '|'
                << BuildHermesBoundaryRecord(player, type, requestedLevel);
        return payload.str();
    }

    std::string SanitizeBreakthroughField(std::string value, char delimiter)
    {
        for (char& ch : value)
        {
            if (ch == delimiter || ch == '|' || ch == '\t' || ch == '\r' || ch == '\n')
                ch = '/';
        }

        return value;
    }

    std::string BreakthroughTaggedPayload(char const* tag, std::string const& data)
    {
        std::ostringstream out;
        out << "[突破系统] [" << tag << "] " << data;
        return out.str();
    }

    void AppendBreakthroughPayloadLine(std::ostringstream& out, std::string const& line)
    {
        if (out.tellp() != std::streampos(0))
            out << '\n';
        out << line;
    }

    bool EnsureHermesBreakthroughPlayerData(Player& player)
    {
        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult result = CharacterDatabase.Query("SELECT 1 FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid);
        if (result)
            return true;

        CharacterDatabase.DirectExecute(
            "INSERT INTO `_突破系统_玩家数据` (`角色编号`, `账号编号`, `名称`, `职业`, `突破ID`, `突破等级`, `突破点数`, `突破经验`, `下级所需`, `技能点数`) "
            "VALUES ({}, {}, '{}', {}, 0, 1, 0, 0, 1000, 0)",
            guid, player.GetSession()->GetAccountId(), player.GetName(), player.getClass());

        return !!CharacterDatabase.Query("SELECT 1 FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid);
    }

    std::string ResolveBreakthroughSkillNames(std::string const& idsText)
    {
        if (idsText.empty())
            return "";

        std::ostringstream names;
        for (std::string const& rawId : SplitHermesPayloadFields(idsText, ','))
        {
            uint32 const id = ParseHermesUInt(rawId, 0);
            if (!id)
                continue;

            QueryResult result = WorldDatabase.Query("SELECT `名称` FROM `_突破系统_技能` WHERE `编号` = {}", id);
            if (names.tellp() != std::streampos(0))
                names << ", ";

            if (result)
                names << SanitizeBreakthroughField(result->Fetch()[0].Get<std::string>(), ';');
            else
                names << id;
        }

        return names.str();
    }

    std::string ResolveBreakthroughEffectNames(std::string const& idsText)
    {
        if (idsText.empty())
            return "";

        std::ostringstream names;
        for (std::string const& rawId : SplitHermesPayloadFields(idsText, ','))
        {
            uint32 const id = ParseHermesUInt(rawId, 0);
            if (!id)
                continue;

            if (names.tellp() != std::streampos(0))
                names << ", ";

            switch (id)
            {
                case 36: names << "火焰爆炸"; break;
                case 37: names << "寒冰爆炸"; break;
                case 38: names << "奥术爆炸"; break;
                case 39: names << "自然爆炸"; break;
                case 40: names << "神圣爆炸"; break;
                default: names << "特效" << id; break;
            }
        }

        return names.str();
    }

    std::string BuildHermesBreakthroughInfoPayload(Player& player)
    {
        if (!EnsureHermesBreakthroughPlayerData(player))
            return BreakthroughTaggedPayload("INFO", "");

        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult result = CharacterDatabase.Query(
            "SELECT `突破等级`, `突破点数`, `突破经验`, `下级所需` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}",
            guid);
        if (!result)
            return BreakthroughTaggedPayload("INFO", "");

        Field* fields = result->Fetch();
        uint32 const breakthroughLevel = fields[0].Get<uint32>();
        uint64 const breakthroughPoints = fields[1].Get<uint64>();
        uint64 const currentExp = fields[2].Get<uint64>();
        uint64 const requiredExp = fields[3].Get<uint64>();

        uint32 rank = 999;
        QueryResult rankResult = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM `_突破系统_玩家数据` WHERE `突破点数` > (SELECT `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {})",
            guid);
        if (rankResult)
            rank = rankResult->Fetch()[0].Get<uint32>() + 1;

        std::ostringstream data;
        data << player.GetSession()->GetAccountId() << ","
             << guid << ","
             << SanitizeBreakthroughField(player.GetName(), ',') << ","
             << static_cast<uint32>(player.getClass()) << ","
             << breakthroughLevel << ","
             << breakthroughPoints << ","
             << currentExp << ","
             << requiredExp << ","
             << rank << ","
             << time(nullptr);

        return BreakthroughTaggedPayload("INFO", data.str());
    }

    std::string BuildHermesBreakthroughSystemDataPayload(Player& player)
    {
        if (!EnsureHermesBreakthroughPlayerData(player))
            return BreakthroughTaggedPayload("SYSTEM_DATA", "");

        uint32 const guid = player.GetGUID().GetCounter();
        uint32 playerLevel = 1;
        uint64 playerExp = 0;
        uint64 expNeeded = 1000;
        uint64 breakthroughPoints = 0;

        QueryResult playerResult = CharacterDatabase.Query(
            "SELECT `突破等级`, `突破经验`, `下级所需`, `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}",
            guid);
        if (playerResult)
        {
            Field* fields = playerResult->Fetch();
            playerLevel = fields[0].Get<uint32>();
            playerExp = fields[1].Get<uint64>();
            expNeeded = fields[2].Get<uint64>();
            breakthroughPoints = fields[3].Get<uint64>();
        }

        std::string breakthroughName = "初入门槛";
        std::string breakthroughDesc = "初步突破，获得基础属性提升";
        int healthBonus = 0;
        int attackBonus = 0;
        int defenseBonus = 0;
        int spellBonus = 0;
        int speedBonus = 0;
        uint32 effectType = 0;
        std::string skillIds;
        std::string auraIds;
        std::string effectIds;
        std::string effectParam1;
        std::string effectParam2;
        std::string effectParam3;

        QueryResult result = WorldDatabase.Query(
            "SELECT `突破名称`, `突破描述`, `生命加成`, `攻击加成`, `防御加成`, `法术加成`, `速度加成`, `突破技能`, `突破光环`, `突破特效`, `效果类型`, `效果参数1`, `效果参数2`, `效果参数3` "
            "FROM `_突破系统` WHERE `突破等级` = {}",
            playerLevel);
        if (result)
        {
            Field* fields = result->Fetch();
            breakthroughName = fields[0].Get<std::string>();
            breakthroughDesc = fields[1].Get<std::string>();
            healthBonus = static_cast<int>(fields[2].Get<float>());
            attackBonus = static_cast<int>(fields[3].Get<float>());
            defenseBonus = static_cast<int>(fields[4].Get<float>());
            spellBonus = static_cast<int>(fields[5].Get<float>());
            speedBonus = static_cast<int>(fields[6].Get<float>());
            skillIds = fields[7].Get<std::string>();
            auraIds = fields[8].Get<std::string>();
            effectIds = fields[9].Get<std::string>();
            effectType = fields[10].Get<uint32>();
            effectParam1 = fields[11].Get<std::string>();
            effectParam2 = fields[12].Get<std::string>();
            effectParam3 = fields[13].Get<std::string>();
        }

        std::string const skillNames = ResolveBreakthroughSkillNames(skillIds);
        std::string const effectNames = ResolveBreakthroughEffectNames(effectIds);

        std::ostringstream data;
        data << playerLevel << ";"
             << playerExp << ";"
             << expNeeded << ";"
             << breakthroughPoints << ";"
             << SanitizeBreakthroughField(breakthroughName, ';') << ";"
             << SanitizeBreakthroughField(breakthroughDesc, ';') << ";"
             << healthBonus << ";"
             << attackBonus << ";"
             << defenseBonus << ";"
             << spellBonus << ";"
             << speedBonus << ";"
             << effectType << ";"
             << SanitizeBreakthroughField(skillNames.empty() ? skillIds : skillNames, ';') << ";"
             << SanitizeBreakthroughField(auraIds, ';') << ";"
             << SanitizeBreakthroughField(effectNames.empty() ? effectIds : effectNames, ';') << ";"
             << SanitizeBreakthroughField(effectParam1, ';') << ";"
             << SanitizeBreakthroughField(effectParam2, ';') << ";"
             << SanitizeBreakthroughField(effectParam3, ';');

        return BreakthroughTaggedPayload("SYSTEM_DATA", data.str());
    }

    std::string BuildHermesBreakthroughSkillsPayload(Player& player, uint32 page)
    {
        if (!EnsureHermesBreakthroughPlayerData(player))
            return BreakthroughTaggedPayload("SKILLS_PAGE", "page=1,pageSize=40,totalPages=1,totalSkills=0") + "\n" + BreakthroughTaggedPayload("SKILLS", "");

        if (!page)
            page = 1;

        uint32 const pageSize = 40;
        uint32 const guid = player.GetGUID().GetCounter();
        uint32 breakthroughLevel = 1;

        QueryResult levelResult = CharacterDatabase.Query("SELECT `突破等级` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid);
        if (levelResult)
            breakthroughLevel = levelResult->Fetch()[0].Get<uint32>();

        uint32 totalSkills = 0;
        QueryResult countResult = WorldDatabase.Query("SELECT COUNT(*) FROM `_突破系统_技能`");
        if (countResult)
            totalSkills = countResult->Fetch()[0].Get<uint32>();

        uint32 totalPages = (totalSkills + pageSize - 1) / pageSize;
        if (!totalPages)
            totalPages = 1;
        if (page > totalPages)
            page = totalPages;

        std::unordered_map<uint32, uint32> playerSkills;
        QueryResult playerSkillResult = CharacterDatabase.Query("SELECT `技能ID`, `当前等级` FROM `_突破系统_玩家技能` WHERE `角色编号` = {}", guid);
        if (playerSkillResult)
        {
            do
            {
                Field* fields = playerSkillResult->Fetch();
                playerSkills[fields[0].Get<uint32>()] = fields[1].Get<uint32>();
            } while (playerSkillResult->NextRow());
        }

        std::vector<std::string> skillsCompact;
        QueryResult result = WorldDatabase.Query(
            "SELECT `编号`, `技能ID`, `名称`, `最大等级`, `消耗`, `解锁等级` "
            "FROM `_突破系统_技能` ORDER BY `编号` ASC LIMIT {} OFFSET {}",
            pageSize, (page - 1) * pageSize);
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 const id = fields[0].Get<uint32>();
                uint32 const skillId = fields[1].Get<uint32>();
                std::string const name = fields[2].Get<std::string>();
                uint32 const maxLevel = fields[3].Get<uint32>();
                uint64 const cost = fields[4].Get<uint64>();
                uint32 const unlockLevel = fields[5].Get<uint32>();

                uint32 currentLevel = 0;
                auto itr = playerSkills.find(id);
                if (itr != playerSkills.end())
                    currentLevel = itr->second;

                std::ostringstream skillData;
                skillData << (breakthroughLevel >= unlockLevel ? "1" : "0") << ","
                          << cost << ","
                          << currentLevel << ","
                          << id << ","
                          << maxLevel << ","
                          << SanitizeBreakthroughField(name, ',') << ","
                          << skillId << ","
                          << unlockLevel;
                skillsCompact.push_back(skillData.str());
            } while (result->NextRow());
        }

        std::ostringstream out;
        AppendBreakthroughPayloadLine(out, BreakthroughTaggedPayload("SKILLS_PAGE", "page=" + std::to_string(page) + ",pageSize=" + std::to_string(pageSize) + ",totalPages=" + std::to_string(totalPages) + ",totalSkills=" + std::to_string(totalSkills)));

        std::ostringstream skillsData;
        for (std::size_t i = 0; i < skillsCompact.size(); ++i)
        {
            if (i > 0)
                skillsData << "|";
            skillsData << skillsCompact[i];
        }

        AppendBreakthroughPayloadLine(out, BreakthroughTaggedPayload("SKILLS", skillsData.str()));
        return out.str();
    }

    std::string BuildHermesBreakthroughSkillDetailPayload(Player& player, uint32 skillNumber)
    {
        if (!skillNumber)
            return "[突破系统] 显示技能详情失败：技能不存在";

        QueryResult result = WorldDatabase.Query(
            "SELECT `编号`, `技能ID`, `名称`, `描述`, `最大等级`, `效果`, `下级效果`, `消耗`, `解锁等级`, `前置条件`, `技能加成`, `属性值` "
            "FROM `_突破系统_技能` WHERE `编号` = {}",
            skillNumber);
        if (!result)
            return "[突破系统] 显示技能详情失败：技能不存在";

        Field* skillFields = result->Fetch();
        uint32 const guid = player.GetGUID().GetCounter();
        uint32 currentLevel = 0;
        QueryResult playerSkillResult = CharacterDatabase.Query(
            "SELECT `当前等级` FROM `_突破系统_玩家技能` WHERE `角色编号` = {} AND `技能ID` = {}",
            guid, skillNumber);
        if (playerSkillResult)
            currentLevel = playerSkillResult->Fetch()[0].Get<uint32>();

        std::ostringstream convertedPrerequisites;
        for (std::string prereqNumber : SplitHermesPayloadFields(skillFields[9].Get<std::string>(), ','))
        {
            prereqNumber.erase(std::remove_if(prereqNumber.begin(), prereqNumber.end(), [](unsigned char ch) { return std::isspace(ch); }), prereqNumber.end());
            uint32 const prereqNumberValue = ParseHermesUInt(prereqNumber, 0);
            if (!prereqNumberValue)
                continue;

            QueryResult prereqResult = WorldDatabase.Query("SELECT `技能ID` FROM `_突破系统_技能` WHERE `编号` = {}", prereqNumberValue);
            if (!prereqResult)
                continue;

            if (convertedPrerequisites.tellp() != std::streampos(0))
                convertedPrerequisites << ",";
            convertedPrerequisites << prereqResult->Fetch()[0].Get<uint32>();
        }

        std::ostringstream detailData;
        detailData << skillFields[0].Get<uint32>() << ","
                   << skillFields[1].Get<uint32>() << ","
                   << SanitizeBreakthroughField(skillFields[2].Get<std::string>(), ',') << ","
                   << SanitizeBreakthroughField(skillFields[3].Get<std::string>(), ',') << ","
                   << skillFields[4].Get<uint32>() << ","
                   << SanitizeBreakthroughField(skillFields[5].Get<std::string>(), ',') << ","
                   << SanitizeBreakthroughField(skillFields[6].Get<std::string>(), ',') << ","
                   << skillFields[7].Get<uint64>() << ","
                   << skillFields[8].Get<uint32>() << ","
                   << convertedPrerequisites.str() << ","
                   << skillFields[10].Get<uint32>() << ","
                   << SanitizeBreakthroughField(skillFields[11].Get<std::string>(), ',') << ","
                   << currentLevel;

        return BreakthroughTaggedPayload("SKILL_DETAIL", detailData.str());
    }

    std::string GetHermesBreakthroughGuildName(uint32 characterGuid)
    {
        QueryResult guildResult = CharacterDatabase.Query(
            "SELECT g.name FROM guild_member gm INNER JOIN guild g ON gm.guildid = g.guildid WHERE gm.guid = {}",
            characterGuid);
        return guildResult ? guildResult->Fetch()[0].Get<std::string>() : "";
    }

    std::string BuildHermesBreakthroughLeaderboardPayload(Player& player)
    {
        EnsureHermesBreakthroughPlayerData(player);

        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult result = CharacterDatabase.Query(
            "SELECT `角色编号`, `名称`, `职业`, `突破等级`, `突破点数` "
            "FROM `_突破系统_玩家数据` ORDER BY `突破点数` DESC LIMIT 10");

        std::vector<std::string> leaderboard;
        uint32 rank = 1;
        bool playerFound = false;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 const charGuid = fields[0].Get<uint32>();

                std::ostringstream compactRank;
                compactRank << rank << ","
                            << SanitizeBreakthroughField(fields[1].Get<std::string>(), ',') << ","
                            << fields[2].Get<uint32>() << ","
                            << fields[3].Get<uint32>() << ","
                            << fields[4].Get<uint64>() << ","
                            << SanitizeBreakthroughField(GetHermesBreakthroughGuildName(charGuid), ',') << ","
                            << (charGuid == guid ? "1" : "0");

                if (charGuid == guid)
                    playerFound = true;
                leaderboard.push_back(compactRank.str());
                ++rank;
            } while (result->NextRow());
        }

        if (!playerFound)
        {
            uint32 playerRank = 999;
            QueryResult rankResult = CharacterDatabase.Query(
                "SELECT COUNT(*) FROM `_突破系统_玩家数据` WHERE `突破点数` > (SELECT `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {})",
                guid);
            if (rankResult)
                playerRank = rankResult->Fetch()[0].Get<uint32>() + 1;

            result = CharacterDatabase.Query(
                "SELECT `名称`, `职业`, `突破等级`, `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}",
                guid);

            std::ostringstream compactPlayerRank;
            if (result)
            {
                Field* fields = result->Fetch();
                compactPlayerRank << playerRank << ","
                                  << SanitizeBreakthroughField(fields[0].Get<std::string>(), ',') << ","
                                  << fields[1].Get<uint32>() << ","
                                  << fields[2].Get<uint32>() << ","
                                  << fields[3].Get<uint64>() << ","
                                  << SanitizeBreakthroughField(GetHermesBreakthroughGuildName(guid), ',') << ","
                                  << "1";
            }
            else
            {
                compactPlayerRank << playerRank << ","
                                  << SanitizeBreakthroughField(player.GetName(), ',') << ","
                                  << static_cast<uint32>(player.getClass()) << ",0,0,"
                                  << SanitizeBreakthroughField(GetHermesBreakthroughGuildName(guid), ',') << ",1";
            }

            leaderboard.push_back(compactPlayerRank.str());
        }

        std::ostringstream data;
        for (std::size_t i = 0; i < leaderboard.size(); ++i)
        {
            if (i > 0)
                data << "|";
            data << leaderboard[i];
        }

        return BreakthroughTaggedPayload("LEADERBOARD", data.str());
    }

    std::string BuildHermesBreakthroughExpSourcesPayload(Player&)
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT `来源ID`, `来源类型`, `来源名称`, `基础经验`, `等级系数`, `难度系数`, `冷却时间`, `每日上限`, `备注` "
            "FROM `_突破系统_经验值` ORDER BY `来源类型`, `来源ID`");

        std::ostringstream data;
        if (result)
        {
            bool first = true;
            do
            {
                Field* fields = result->Fetch();
                if (!first)
                    data << "|";
                first = false;

                uint64 const dailyLimit = fields[7].Get<uint64>();
                data << fields[0].Get<uint32>() << ","
                     << fields[1].Get<uint32>() << ","
                     << SanitizeBreakthroughField(fields[2].Get<std::string>(), ',') << ","
                     << fields[3].Get<uint64>() << ","
                     << fields[4].Get<float>() << ","
                     << fields[5].Get<float>() << ","
                     << fields[6].Get<uint32>() << ","
                     << dailyLimit << ","
                     << SanitizeBreakthroughField(fields[8].Get<std::string>(), ',') << ",0,0,"
                     << (dailyLimit > 0 ? std::to_string(dailyLimit) : "无限制");
            } while (result->NextRow());
        }

        return BreakthroughTaggedPayload("EXP_SOURCE", data.str());
    }

    uint64 HermesSaturatingAdd(uint64 left, uint64 right)
    {
        uint64 const maxValue = std::numeric_limits<uint64>::max();
        return maxValue - left < right ? maxValue : left + right;
    }

    uint64 HermesSaturatingMul(uint64 left, uint64 right)
    {
        if (!left || !right)
            return 0;

        uint64 const maxValue = std::numeric_limits<uint64>::max();
        return left > maxValue / right ? maxValue : left * right;
    }

    void ParseHermesBreakthroughAttributeValues(std::string const& attributeValues, std::string& attrTypeStr, std::string& valueTypeStr, std::string& attrValueStr)
    {
        std::vector<std::string> attrTypes;
        std::vector<std::string> valueTypes;
        std::vector<std::string> attrValues;

        for (std::string const& pair : SplitHermesPayloadFields(attributeValues, ','))
        {
            std::vector<std::string> parts = SplitHermesPayloadFields(pair, ' ');
            std::vector<std::string> compactParts;
            for (std::string const& part : parts)
            {
                if (!part.empty())
                    compactParts.push_back(part);
            }

            if (compactParts.size() >= 3)
            {
                valueTypes.push_back(compactParts[0]);
                attrTypes.push_back(compactParts[1]);
                attrValues.push_back(compactParts[2]);
            }
        }

        auto Join = [](std::vector<std::string> const& values) -> std::string
        {
            std::ostringstream out;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    out << ",";
                out << values[i];
            }
            return out.str();
        };

        attrTypeStr = Join(attrTypes);
        valueTypeStr = Join(valueTypes);
        attrValueStr = Join(attrValues);
    }

    void RefreshHermesBreakthroughPlayerStats(Player& player)
    {
        player.UpdateAllStats();
        player.UpdateDamagePhysical(BASE_ATTACK);
        player.UpdateDamagePhysical(OFF_ATTACK);
        player.UpdateDamagePhysical(RANGED_ATTACK);
        player.UpdateSpellDamageAndHealingBonus();
        player.UpdateAllCritPercentages();
        player.UpdateAllSpellCritChances();
        player.SendUpdateToPlayer(&player);
    }

    std::string BuildHermesBreakthroughSingleSkillPayload(Player& player, uint32 skillId)
    {
        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult result = WorldDatabase.Query(
            "SELECT `编号`, `技能ID`, `名称`, `描述`, `最大等级`, `效果`, `下级效果`, `消耗`, `解锁等级`, `前置条件`, `技能加成`, `属性值` FROM `_突破系统_技能` WHERE `编号` = {}",
            skillId);
        if (!result)
        {
            result = WorldDatabase.Query(
                "SELECT `编号`, `技能ID`, `名称`, `描述`, `最大等级`, `效果`, `下级效果`, `消耗`, `解锁等级`, `前置条件`, `技能加成`, `属性值` FROM `_突破系统_技能` WHERE `技能ID` = {}",
                skillId);
        }

        if (!result)
            return "[突破系统] 获取技能数据失败，技能ID: " + std::to_string(skillId);

        Field* fields = result->Fetch();
        uint32 const id = fields[0].Get<uint32>();
        uint32 const gameSkillId = fields[1].Get<uint32>();
        std::string const name = fields[2].Get<std::string>();
        std::string const description = fields[3].Get<std::string>();
        uint32 const maxLevel = fields[4].Get<uint32>();
        std::string const effectStr = fields[5].Get<std::string>();
        std::string nextLevelEffect = fields[6].Get<std::string>();
        uint64 const cost = fields[7].Get<uint64>();
        uint32 const unlockLevel = fields[8].Get<uint32>();
        std::string const prerequisites = fields[9].Get<std::string>();
        uint32 const skillBonus = fields[10].Get<uint32>();
        std::string const attributeValues = fields[11].Get<std::string>();

        uint32 currentLevel = 0;
        QueryResult playerSkillResult = CharacterDatabase.Query("SELECT `当前等级` FROM `_突破系统_玩家技能` WHERE `角色编号` = {} AND `技能ID` = {}", guid, id);
        if (playerSkillResult)
            currentLevel = playerSkillResult->Fetch()[0].Get<uint32>();

        std::string effect = "未学习";
        std::string currentEffect;
        std::string initialEffect;
        std::vector<std::string> effects = SplitHermesPayloadFields(effectStr, ',');
        if (!effects.empty())
            initialEffect = effects[0];

        if (currentLevel > 0 && currentLevel <= effects.size())
        {
            effect = effects[currentLevel - 1];
            currentEffect = effect;
        }

        if (currentLevel >= maxLevel)
            nextLevelEffect = "已达到最高等级";
        else if (nextLevelEffect.empty() && currentLevel < effects.size())
            nextLevelEffect = effects[currentLevel];

        std::ostringstream compactSkill;
        compactSkill << id << ","
                     << gameSkillId << ","
                     << SanitizeBreakthroughField(name, ',') << ","
                     << SanitizeBreakthroughField(description, ',') << ","
                     << currentLevel << ","
                     << maxLevel << ","
                     << SanitizeBreakthroughField(effect, ',') << ","
                     << SanitizeBreakthroughField(currentEffect, ',') << ","
                     << SanitizeBreakthroughField(initialEffect, ',') << ","
                     << SanitizeBreakthroughField(nextLevelEffect, ',') << ","
                     << cost << ","
                     << unlockLevel << ","
                     << SanitizeBreakthroughField(prerequisites, ',') << ","
                     << skillBonus << ","
                     << SanitizeBreakthroughField(attributeValues, ',');

        return BreakthroughTaggedPayload("SINGLE_SKILL", compactSkill.str());
    }

    void AppendHermesBreakthroughPostActionRefresh(std::ostringstream& payload, Player& player, uint32 page)
    {
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughSystemDataPayload(player));
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughInfoPayload(player));
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughSkillsPayload(player, page ? page : 1));
    }

    std::string ExecuteHermesBreakthroughUpgrade(Player& player)
    {
        if (!EnsureHermesBreakthroughPlayerData(player))
            return "[突破系统] 获取玩家突破数据失败";

        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult result = CharacterDatabase.Query(
            "SELECT `突破等级`, `突破点数`, `突破经验`, `下级所需` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}",
            guid);
        if (!result)
            return "[突破系统] 获取玩家突破数据失败";

        Field* fields = result->Fetch();
        uint32 level = fields[0].Get<uint32>();
        uint64 points = fields[1].Get<uint64>();
        uint64 currentExp = fields[2].Get<uint64>();
        uint64 requiredExp = fields[3].Get<uint64>();

        if (currentExp < requiredExp)
            return BreakthroughTaggedPayload("UPGRADE", "0," + std::to_string(level) + ",经验不足");

        ++level;
        currentExp -= requiredExp;
        points = HermesSaturatingAdd(points, 5);
        requiredExp = HermesSaturatingAdd(1000, HermesSaturatingMul(level, 500));

        QueryResult breakthroughResult = WorldDatabase.Query("SELECT `突破ID` FROM `_突破系统` WHERE `突破等级` = {}", level);
        if (breakthroughResult)
        {
            uint32 const breakthroughId = breakthroughResult->Fetch()[0].Get<uint32>();
            if (sBreakthroughSystem)
                sBreakthroughSystem->RemoveCachedBreakthroughEffects(&player);

            CharacterDatabase.DirectExecute(
                "UPDATE `_突破系统_玩家数据` SET `突破ID` = {}, `突破等级` = {}, `突破点数` = {}, `突破经验` = {}, `下级所需` = {} WHERE `角色编号` = {}",
                breakthroughId, level, points, currentExp, requiredExp, guid);

            if (sBreakthroughSystem)
                sBreakthroughSystem->ApplyBreakthroughEffects(&player, breakthroughId);
        }
        else
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_突破系统_玩家数据` SET `突破等级` = {}, `突破点数` = {}, `突破经验` = {}, `下级所需` = {} WHERE `角色编号` = {}",
                level, points, currentExp, requiredExp, guid);
        }

        std::ostringstream payload;
        AppendBreakthroughPayloadLine(payload, BreakthroughTaggedPayload("UPGRADE", "1," + std::to_string(level) + ","));
        AppendHermesBreakthroughPostActionRefresh(payload, player, 1);
        return payload.str();
    }

    std::string ExecuteHermesBreakthroughLearnSkill(Player& player, uint32 gameSkillId, bool upgrade)
    {
        if (!gameSkillId)
            return "[突破系统] 技能ID无效";
        if (!EnsureHermesBreakthroughPlayerData(player))
            return "[突破系统] 获取玩家数据失败";

        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult skillResult = upgrade
            ? WorldDatabase.Query("SELECT `编号`, `技能ID`, `最大等级`, `消耗`, `技能加成`, `属性值` FROM `_突破系统_技能` WHERE `技能ID` = {}", gameSkillId)
            : WorldDatabase.Query("SELECT `编号`, `技能ID`, `名称`, `解锁等级`, `消耗`, `技能加成`, `属性值` FROM `_突破系统_技能` WHERE `技能ID` = {}", gameSkillId);
        if (!skillResult)
            return std::string("[突破系统] ") + (upgrade ? "升级技能失败：技能不存在" : "学习技能失败：技能不存在");

        Field* fields = skillResult->Fetch();
        uint32 const skillNumber = fields[0].Get<uint32>();
        uint32 const realGameSkillId = fields[1].Get<uint32>();
        uint32 maxLevel = 1;
        uint32 unlockLevel = 1;
        uint64 cost = 0;
        uint32 bonusType = 0;
        std::string attributeValues;

        if (upgrade)
        {
            maxLevel = fields[2].Get<uint32>();
            cost = fields[3].Get<uint64>();
            bonusType = fields[4].Get<uint32>();
            attributeValues = fields[5].Get<std::string>();
        }
        else
        {
            unlockLevel = fields[3].Get<uint32>();
            cost = fields[4].Get<uint64>();
            bonusType = fields[5].Get<uint32>();
            attributeValues = fields[6].Get<std::string>();
        }

        QueryResult playerSkillResult = CharacterDatabase.Query("SELECT `当前等级` FROM `_突破系统_玩家技能` WHERE `角色编号` = {} AND `技能ID` = {}", guid, skillNumber);
        uint32 currentLevel = 0;
        if (playerSkillResult)
            currentLevel = playerSkillResult->Fetch()[0].Get<uint32>();

        if (!upgrade && currentLevel > 0)
            return "[突破系统] 你已经学习了该技能！技能ID: " + std::to_string(gameSkillId);
        if (upgrade && !currentLevel)
            return "[突破系统] 你还没有学习该技能！技能ID: " + std::to_string(gameSkillId);
        if (upgrade && currentLevel >= maxLevel)
            return "[突破系统] 该技能已经达到最高等级！技能ID: " + std::to_string(gameSkillId);

        QueryResult playerResult = upgrade
            ? CharacterDatabase.Query("SELECT `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid)
            : CharacterDatabase.Query("SELECT `突破等级`, `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid);
        if (!playerResult)
            return "[突破系统] 获取玩家数据失败";

        uint64 breakthroughPoints = 0;
        if (upgrade)
        {
            breakthroughPoints = playerResult->Fetch()[0].Get<uint64>();
        }
        else
        {
            Field* playerFields = playerResult->Fetch();
            uint32 const playerBreakthroughLevel = playerFields[0].Get<uint32>();
            breakthroughPoints = playerFields[1].Get<uint64>();
            if (playerBreakthroughLevel < unlockLevel)
                return "[突破系统] 你的突破等级不足！需要等级: " + std::to_string(unlockLevel) + "，当前等级: " + std::to_string(playerBreakthroughLevel);
        }

        if (breakthroughPoints < cost)
            return "[突破系统] 突破点数不足！需要 " + std::to_string(cost) + " 点，当前有 " + std::to_string(breakthroughPoints) + " 点";

        std::string attrTypeStr;
        std::string valueTypeStr;
        std::string attrValueStr;
        ParseHermesBreakthroughAttributeValues(attributeValues, attrTypeStr, valueTypeStr, attrValueStr);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        if (upgrade)
        {
            trans->Append(
                "UPDATE `_突破系统_玩家技能` SET `当前等级` = `当前等级` + 1, `加成类型` = {}, `属性类型` = '{}', `值类型` = '{}', `加成值` = '{}' WHERE `角色编号` = {} AND `技能ID` = {}",
                bonusType, attrTypeStr, valueTypeStr, attrValueStr, guid, skillNumber);
        }
        else
        {
            trans->Append(
                "INSERT IGNORE INTO `_突破系统_玩家技能` (`角色编号`, `技能ID`, `当前等级`, `加成类型`, `属性类型`, `值类型`, `加成值`) VALUES ({}, {}, 1, {}, '{}', '{}', '{}')",
                guid, skillNumber, bonusType, attrTypeStr, valueTypeStr, attrValueStr);
        }
        trans->Append(
            "UPDATE `_突破系统_玩家数据` SET `突破点数` = `突破点数` - {} WHERE `角色编号` = {} AND `突破点数` >= {}",
            cost, guid, cost);
        CharacterDatabase.DirectCommitTransaction(trans);

        uint32 const newLevel = upgrade ? currentLevel + 1 : 1;
        if (sBreakthroughSkillSystem)
        {
            if (upgrade)
                sBreakthroughSkillSystem->RemoveSkillBonus(&player, skillNumber, currentLevel);
            sBreakthroughSkillSystem->ApplySkillBonus(&player, skillNumber, newLevel);
        }

        if (realGameSkillId > 0 && !player.HasSpell(realGameSkillId))
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(realGameSkillId);
            if (spellInfo)
                player.learnSpell(realGameSkillId);
        }

        QueryResult pointsResult = CharacterDatabase.Query("SELECT `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid);
        uint64 updatedPoints = pointsResult ? pointsResult->Fetch()[0].Get<uint64>() : breakthroughPoints - cost;

        std::ostringstream payload;
        AppendBreakthroughPayloadLine(payload, std::string("[突破系统] ") + (upgrade ? "升级技能成功！技能ID: " : "学习技能成功！技能ID: ") + std::to_string(gameSkillId));
        AppendBreakthroughPayloadLine(payload, BreakthroughTaggedPayload("POINTS_UPDATE", std::to_string(updatedPoints)));
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughSingleSkillPayload(player, skillNumber));
        AppendHermesBreakthroughPostActionRefresh(payload, player, 1);
        return payload.str();
    }

    std::string ExecuteHermesBreakthroughResetSkills(Player& player)
    {
        uint32 const guid = player.GetGUID().GetCounter();
        QueryResult result = CharacterDatabase.Query("SELECT `技能ID`, `当前等级` FROM `_突破系统_玩家技能` WHERE `角色编号` = {}", guid);
        if (!result)
            return "[突破系统] 您还没有学习任何技能";

        uint64 totalPoints = 0;
        std::vector<std::pair<uint32, uint32>> skillsToReset;
        std::vector<uint32> gameSkillIdsToRemove;

        do
        {
            Field* fields = result->Fetch();
            uint32 const skillNumber = fields[0].Get<uint32>();
            uint32 const currentLevel = fields[1].Get<uint32>();
            skillsToReset.push_back(std::make_pair(skillNumber, currentLevel));

            QueryResult costResult = WorldDatabase.Query("SELECT `消耗`, `技能ID` FROM `_突破系统_技能` WHERE `编号` = {}", skillNumber);
            if (costResult)
            {
                Field* costFields = costResult->Fetch();
                totalPoints = HermesSaturatingAdd(totalPoints, HermesSaturatingMul(costFields[0].Get<uint64>(), currentLevel));
                uint32 const gameSkillId = costFields[1].Get<uint32>();
                if (gameSkillId > 0)
                    gameSkillIdsToRemove.push_back(gameSkillId);
            }
        } while (result->NextRow());

        for (uint32 gameSkillId : gameSkillIdsToRemove)
        {
            if (player.HasSpell(gameSkillId))
            {
                player.removeSpell(gameSkillId, SPEC_MASK_ALL, false);
                if (player.HasSpell(gameSkillId))
                    player.removeSpell(gameSkillId, SPEC_MASK_ALL, true);
            }
        }

        if (sBreakthroughSkillSystem)
        {
            for (std::pair<uint32, uint32> const& skillInfo : skillsToReset)
                sBreakthroughSkillSystem->RemoveSkillBonus(&player, skillInfo.first, skillInfo.second);
        }

        QueryResult currentPointsResult = CharacterDatabase.Query("SELECT `突破点数` FROM `_突破系统_玩家数据` WHERE `角色编号` = {}", guid);
        uint64 const currentPoints = currentPointsResult ? currentPointsResult->Fetch()[0].Get<uint64>() : 0;
        uint64 const newPoints = HermesSaturatingAdd(currentPoints, totalPoints);

        CharacterDatabase.DirectExecute("UPDATE `_突破系统_玩家数据` SET `突破点数` = {} WHERE `角色编号` = {}", newPoints, guid);
        CharacterDatabase.DirectExecute("DELETE FROM `_突破系统_玩家技能` WHERE `角色编号` = {}", guid);
        RefreshHermesBreakthroughPlayerStats(player);

        std::ostringstream payload;
        AppendBreakthroughPayloadLine(payload, "[突破系统] 技能重置成功，已返还 " + std::to_string(totalPoints) + " 点突破点数");
        AppendBreakthroughPayloadLine(payload, "[突破系统] 突破点=" + std::to_string(newPoints));
        AppendHermesBreakthroughPostActionRefresh(payload, player, 1);
        return payload.str();
    }

    JsonRpcDispatchResult HermesPayloadResult(std::string const& idText, std::string const& schema, std::string const& payload)
    {
        JsonRpcDispatchResult result;
        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"schema\":\"" + EscapeJsonString(schema) +
            "\",\"payload\":\"" + EscapeJsonString(payload) +
            "\",\"payloadBytes\":" + std::to_string(payload.size()) + "}}";
        return result;
    }

    int32 ParseHermesInt(std::string const& text, int32 defaultValue = 0)
    {
        if (text.empty())
            return defaultValue;

        char* end = nullptr;
        long value = std::strtol(text.c_str(), &end, 10);
        if (end == text.c_str())
            return defaultValue;

        return static_cast<int32>(value);
    }

    char HermesTooltipDbValueToHuanJingMode(int32 value)
    {
        if (value == 1)
            return '+';
        if (value == -1)
            return '-';
        return 'x';
    }

    Item* ResolveHermesTooltipItemByClientPosition(Player* player, int32 bag, uint8 slot, uint32 itemID)
    {
        if (!player || itemID == 0)
            return nullptr;

        Item* item = nullptr;

        if (bag == 255)
        {
            uint8 equipSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : slot;
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);
        }
        else if (bag == 0)
        {
            uint8 bagSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : slot;
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + bagSlot);
        }
        else if (bag >= 1 && bag <= 4)
        {
            uint8 serverBagSlot = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            if (Bag* bagPtr = player->GetBagByPos(serverBagSlot))
                item = bagPtr->GetItemByPos(slot > 0 ? slot - 1 : slot);
        }
        else if (bag == -1)
        {
            uint8 bankSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : slot;
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, BANK_SLOT_ITEM_START + bankSlot);
        }
        else if (bag >= 5 && bag <= 11)
        {
            uint8 serverBagSlot = BANK_SLOT_BAG_START + static_cast<uint8>(bag - 5);
            if (Bag* bagPtr = player->GetBagByPos(serverBagSlot))
                item = bagPtr->GetItemByPos(slot > 0 ? slot - 1 : slot);
        }

        if (!item || item->GetEntry() != itemID)
            return nullptr;

        return item;
    }

    std::string BuildHermesTooltipAllModulePayload(uint32 itemID, uint32 guid, int32 bag, uint8 slot)
    {
        if (!sItemIdentificationSystem || !sItemIdentificationSystem->_enabled || itemID == 0 || guid == 0)
            return "";

        ItemIdentificationSystem::AllModuleData data = sItemIdentificationSystem->QueryAllModuleData(itemID, guid);

        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << bag << ":" << static_cast<uint32>(slot) << ":" << itemID << ":" << guid << ":"
                 << data.baseAttributes << ":"
                 << data.additionalAttributes << ":"
                 << data.identificationDisplayData << ":"
                 << data.growthData << ":"
                 << data.enhancementData << ":"
                 << data.skillsData << ":"
                 << data.magicHitData << ":"
                 << data.runeData << ":"
                 << data.setData << ":"
                 << data.huanjingData;

        if (!data.pendingIdentifyData.empty())
            response << ":" << data.pendingIdentifyData;

        response << ":" << data.templateStatsData;
        return response.str();
    }

    std::string BuildHermesTooltipTemplatePayload(uint32 itemID)
    {
        if (!sItemIdentificationSystem || !sItemIdentificationSystem->_enabled || itemID == 0)
            return "";

        ItemIdentificationSystem::AllModuleData data = sItemIdentificationSystem->QueryAllModuleData(itemID, 0);
        if (data.templateStatsData.empty())
            return "";

        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << 255 << ":" << 0 << ":" << itemID << ":" << 0 << ":"
                 << ":"  // baseAttributes
                 << ":"  // additionalAttributes
                 << ":"  // identificationDisplayData
                 << ":"  // growthData
                 << ":"  // enhancementData
                 << ":"  // skillsData
                 << ":"  // magicHitData
                 << ":"  // runeData
                 << ":"  // setData
                 << ":"  // huanjingData
                 << data.templateStatsData;
        return response.str();
    }

    std::unordered_map<uint32, Item*> BuildHermesTooltipPlayerItemGuidIndex(Player* player)
    {
        std::unordered_map<uint32, Item*> itemsByGuid;
        if (!player)
            return itemsByGuid;

        itemsByGuid.reserve(128);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                itemsByGuid[item->GetGUID().GetCounter()] = item;

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                itemsByGuid[item->GetGUID().GetCounter()] = item;

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (!bag)
                continue;

            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (Item* item = bag->GetItemByPos(slot))
                    itemsByGuid[item->GetGUID().GetCounter()] = item;
        }

        return itemsByGuid;
    }

    std::string BuildHermesTooltipPendingListPayload(Player* player)
    {
        if (!player)
            return "";

        QueryResult result = CharacterDatabase.Query(
            "SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `玩家GUID` = {} ORDER BY `物品GUID` DESC LIMIT 100",
            player->GetGUID().GetCounter());

        if (!result)
            return "PENDING_LIST:0:";

        std::vector<std::string> itemEntries;
        std::unordered_map<uint32, Item*> itemsByGuid = BuildHermesTooltipPlayerItemGuidIndex(player);

        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].Get<uint32>();
            uint32 itemId = fields[1].Get<uint32>();
            uint256 multiplier = fields[2].Get<uint256>();
            char multiplierMode = HermesTooltipDbValueToHuanJingMode(fields[3].Get<int32>());
            uint32 groupId = fields[4].Get<uint32>();

            auto itemItr = itemsByGuid.find(guid);
            Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
            if (!item)
                continue;

            uint8 serverBag = item->GetBagSlot();
            uint8 slot = item->GetSlot();
            int32 clientBag = 255;
            int32 clientSlot = 0;

            if (serverBag == INVENTORY_SLOT_BAG_0)
            {
                if (slot >= EQUIPMENT_SLOT_START && slot < EQUIPMENT_SLOT_END)
                {
                    clientBag = 255;
                    clientSlot = static_cast<int32>(slot) + 1;
                }
                else if (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
                {
                    clientBag = 0;
                    clientSlot = static_cast<int32>(slot - INVENTORY_SLOT_ITEM_START) + 1;
                }
                else
                    continue;
            }
            else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
            {
                clientBag = serverBag - INVENTORY_SLOT_BAG_START + 1;
                clientSlot = static_cast<int32>(slot) + 1;
            }
            else
                continue;

            std::ostringstream entryStream;
            entryStream << clientBag << "," << clientSlot << "," << itemId << "," << multiplierMode << "," << multiplier << "," << groupId;
            itemEntries.push_back(entryStream.str());
        } while (result->NextRow());

        if (itemEntries.empty())
            return "PENDING_LIST:0:";

        constexpr size_t MaxDataSize = 180;
        std::vector<std::string> packets;
        std::ostringstream currentPacketData;
        bool firstInPacket = true;

        for (std::string const& entry : itemEntries)
        {
            size_t newSize = currentPacketData.str().length();
            if (!firstInPacket)
                newSize += 1;
            newSize += entry.length();

            if (newSize > MaxDataSize && !firstInPacket)
            {
                packets.push_back(currentPacketData.str());
                currentPacketData.str("");
                currentPacketData.clear();
                firstInPacket = true;
            }

            if (!firstInPacket)
                currentPacketData << ";";
            currentPacketData << entry;
            firstInPacket = false;
        }

        if (!currentPacketData.str().empty())
            packets.push_back(currentPacketData.str());

        std::ostringstream payload;
        uint32 totalCount = static_cast<uint32>(itemEntries.size());
        uint32 totalPackets = static_cast<uint32>(packets.size());
        for (size_t i = 0; i < packets.size(); ++i)
        {
            if (i > 0)
                payload << "\n";
            payload << "PENDING_LIST:" << totalCount << ":" << (i + 1) << ":" << totalPackets << ":" << packets[i];
        }

        return payload.str();
    }

    bool CanHermesTooltipInspect(Player* requester, Player* target)
    {
        if (!requester || !target)
            return false;

        if (requester->GetGUID() == target->GetGUID())
            return true;

        if (requester->GetSession() && requester->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
            return true;

        return requester->IsWithinDistInMap(target, 30.0f);
    }

    Player* ResolveHermesTooltipInspectTarget(Player* requester, std::string const& targetPlayerName, uint8 clientSlot, uint32 itemID)
    {
        if (!requester)
            return nullptr;

        auto slotMatches = [clientSlot, itemID](Player* candidate) -> bool
        {
            if (!candidate || itemID == 0)
                return false;

            uint8 serverSlot = clientSlot > 0 ? static_cast<uint8>(clientSlot - 1) : clientSlot;
            if (Item* exact = candidate->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot))
                if (exact->GetEntry() == itemID)
                    return true;

            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                if (Item* item = candidate->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    if (item->GetEntry() == itemID)
                        return true;

            return false;
        };

        if (Player* selected = requester->GetSelectedPlayer())
        {
            if (selected != requester && slotMatches(selected))
                return selected;
        }

        std::string lookupName = targetPlayerName;
        size_t realmSep = lookupName.find('-');
        if (realmSep != std::string::npos)
            lookupName.resize(realmSep);

        if (!lookupName.empty())
            if (Player* byName = ObjectAccessor::FindPlayerByName(lookupName))
                return byName;

        if (Player* selected = requester->GetSelectedPlayer())
            if (selected != requester)
                return selected;

        return nullptr;
    }

    uint32 ResolveHermesTooltipInspectItemGuid(Player* target, uint8 clientSlot, uint32 itemID)
    {
        if (!target || itemID == 0)
            return 0;

        uint8 serverSlot = clientSlot > 0 ? static_cast<uint8>(clientSlot - 1) : clientSlot;
        if (Item* exact = target->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot))
        {
            if (exact->GetEntry() == itemID)
                return exact->GetGUID().GetCounter();
        }

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = target->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (item->GetEntry() == itemID)
                    return item->GetGUID().GetCounter();

        return 0;
    }

    bool IsHermesAddonTakeoverPrefix(std::string const& prefix)
    {
        static constexpr char const* prefixes[] =
        {
            "PATTRPANEL",
            "ASCENSION",
            "ABYSS_UI",
            "Breakthrough",
            "Boundary",
            "MIRAGEUI",
            "XIANMEN",
            "XIANQI",
            "WUHUN_SYS",
            "SYNTHSYS",
            "UITQ",
            "ITEMENHANCE",
            "RUNESYSTEM",
            "RuneManagerUI",
            "VIPSYS",
            "MALLSYSTEM",
            "REALMONEY",
            "PROMOREWARD",
            "QuestRewardAttrUI",
            "REINCARNATION",
            "MATWH",
            "CUT_SYS",
            "CULT_SYS",
            "HEAL_RUNE",
            "FASHION_SYS",
            "ZDYUI_CH",
            "ZDYUI_TJ",
            "MAGICHIT",
            "ZODIACOPEN",
            "ZODIACDMG",
            "TALENTSOUL",
            "POPUPTPL",
            "DarkHardcore",
            "HBUI",
            "ITEMRECYCLE",
            "RedemptionCode",
            "VIP_DATA",
            "HuanJingLevelUI",
            "ReincarnationUI"
        };

        if (prefix.empty())
            return false;

        for (char const* candidate : prefixes)
        {
            std::size_t index = 0;
            for (; index < prefix.size() && candidate[index]; ++index)
            {
                unsigned char left = static_cast<unsigned char>(prefix[index]);
                unsigned char right = static_cast<unsigned char>(candidate[index]);
                if (std::tolower(left) != std::tolower(right))
                    break;
            }

            if (index == prefix.size() && candidate[index] == '\0')
                return true;
        }

        return false;
    }

    bool ExtractAddonEnvelope(std::string const& fullMessage, std::string& prefix, std::string& payload)
    {
        std::size_t const separator = fullMessage.find('\t');
        if (separator == std::string::npos || separator == 0)
            return false;

        prefix = fullMessage.substr(0, separator);
        payload = fullMessage.substr(separator + 1);
        return IsHermesAddonTakeoverPrefix(prefix);
    }

    void MarkHermesAddonReady(WorldSession& session, char const* reason)
    {
        if (Player* player = session.GetPlayer())
        {
            bool inserted = false;
            {
                // 【审计修复】就绪表写路径加独占锁（世界线程写，地图线程经 CanPacketSend 读）
                std::unique_lock<std::shared_mutex> guard(g_HermesReadyMutex);
                inserted = g_HermesAddonReadyPlayers.insert(player->GetGUID().GetCounter()).second;
            }
            if (inserted && ConsumeHermesTraceBudget(g_HermesTraceReadyLogBudget))
                LOG_INFO("server.loading", "HermesBridge TRACE server ready-mark account={} player={} guid={} reason={}", session.GetAccountId(), player->GetName(), player->GetGUID().GetCounter(), reason ? reason : "");
        }
    }

    void MarkHermesAddonPrefixReady(WorldSession& session, std::string const& prefix, char const* reason)
    {
        if (prefix.empty() || !IsHermesAddonTakeoverPrefix(prefix))
            return;

        if (Player* player = session.GetPlayer())
        {
            uint32 const guid = player->GetGUID().GetCounter();
            bool inserted = false;
            {
                // 【审计修复】就绪前缀表写路径加独占锁
                std::unique_lock<std::shared_mutex> guard(g_HermesReadyMutex);
                inserted = g_HermesAddonReadyPrefixes[guid].insert(prefix).second;
            }
            if (inserted && ConsumeHermesTraceBudget(g_HermesTraceReadyLogBudget))
                LOG_INFO("server.loading", "HermesBridge TRACE server prefix-ready-mark account={} player={} guid={} prefix={} reason={}", session.GetAccountId(), player->GetName(), guid, prefix, reason ? reason : "");
        }
    }

    void ClearHermesAddonReady(Player* player, char const* reason)
    {
        if (!player)
            return;

        uint32 const guid = player->GetGUID().GetCounter();
        std::size_t erasedPlayer = 0;
        std::size_t erasedPrefixes = 0;
        {
            // 【审计修复】就绪表清除路径加独占锁
            std::unique_lock<std::shared_mutex> guard(g_HermesReadyMutex);
            erasedPlayer = g_HermesAddonReadyPlayers.erase(guid);
            erasedPrefixes = g_HermesAddonReadyPrefixes.erase(guid);
        }
        if ((erasedPlayer || erasedPrefixes) && ConsumeHermesTraceBudget(g_HermesTraceReadyLogBudget))
            LOG_INFO("server.loading", "HermesBridge TRACE server ready-clear player={} guid={} prefixes={} reason={}", player->GetName(), guid, erasedPrefixes ? 1 : 0, reason ? reason : "");
    }

    void ClearHermesAddonReady(WorldSession& session, char const* reason)
    {
        ClearHermesAddonReady(session.GetPlayer(), reason);
    }

    bool IsHermesAddonReady(Player const* player)
    {
        if (!player)
            return false;

        // 【审计修复】就绪表读路径加共享锁（地图线程 CanPacketSend 并发读取）
        std::shared_lock<std::shared_mutex> guard(g_HermesReadyMutex);
        return g_HermesAddonReadyPlayers.find(player->GetGUID().GetCounter()) != g_HermesAddonReadyPlayers.end();
    }

    bool IsHermesAddonPrefixReady(Player const* player, std::string const& prefix)
    {
        if (!player || prefix.empty())
            return false;

        // 【审计修复】就绪前缀表读路径加共享锁
        std::shared_lock<std::shared_mutex> guard(g_HermesReadyMutex);
        auto const playerItr = g_HermesAddonReadyPrefixes.find(player->GetGUID().GetCounter());
        return playerItr != g_HermesAddonReadyPrefixes.end() && playerItr->second.find(prefix) != playerItr->second.end();
    }

    bool TryParseAddonChunkPayload(std::string const& payload, uint32& index, uint32& total, std::string& chunk)
    {
        if (payload.rfind("CHUNK:", 0) != 0)
            return false;

        std::size_t const firstSeparator = payload.find(':', 6);
        if (firstSeparator == std::string::npos)
            return false;

        std::size_t const secondSeparator = payload.find(':', firstSeparator + 1);
        if (secondSeparator == std::string::npos)
            return false;

        std::string const indexText = payload.substr(6, firstSeparator - 6);
        std::string const totalText = payload.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
        char* end = nullptr;
        unsigned long parsedIndex = std::strtoul(indexText.c_str(), &end, 10);
        if (!end || *end != '\0' || parsedIndex == 0 || parsedIndex > 1024)
            return false;

        end = nullptr;
        unsigned long parsedTotal = std::strtoul(totalText.c_str(), &end, 10);
        if (!end || *end != '\0' || parsedTotal == 0 || parsedTotal > 1024 || parsedIndex > parsedTotal)
            return false;

        index = static_cast<uint32>(parsedIndex);
        total = static_cast<uint32>(parsedTotal);
        chunk = payload.substr(secondSeparator + 1);
        return true;
    }

    enum class HermesAddonChunkResult
    {
        NotChunk,
        Waiting,
        Complete,
        Invalid
    };

    HermesAddonChunkResult TryCoalesceAddonChunk(WorldSession& session, std::string const& prefix, std::string const& payload, std::string& coalescedPayload)
    {
        uint32 index = 0;
        uint32 total = 0;
        std::string chunk;
        if (!TryParseAddonChunkPayload(payload, index, total, chunk))
            return payload.rfind("CHUNK:", 0) == 0 ? HermesAddonChunkResult::Invalid : HermesAddonChunkResult::NotChunk;

        std::ostringstream key;
        key << session.GetAccountId() << ':' << prefix;

        // 【审计修复】分片缓冲被地图线程(CanPacketSend)与世界线程(跨模块 API)并发访问，整段读改写加锁；
        // 本函数持锁期间不发包，调用方在返回后才调用 SendHermesAddonMessageEvent，无锁嵌套
        std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);

        HermesAddonChunkBuffer& buffer = g_HermesAddonChunkBuffers[key.str()];
        if (index == 1 || buffer.Total != total || buffer.Parts.size() != total)
        {
            buffer.Total = total;
            buffer.Received = 0;
            buffer.Parts.clear();
            buffer.Parts.resize(total);
        }

        if (buffer.Parts[index - 1].empty())
            ++buffer.Received;
        buffer.Parts[index - 1] = std::move(chunk);

        if (buffer.Received < buffer.Total)
            return HermesAddonChunkResult::Waiting;

        coalescedPayload.clear();
        for (std::string const& part : buffer.Parts)
        {
            if (part.empty())
                return HermesAddonChunkResult::Waiting;
            if (coalescedPayload.size() + part.size() > HERMES_BRIDGE_MAX_PAYLOAD_SIZE)
            {
                g_HermesAddonChunkBuffers.erase(key.str());
                return HermesAddonChunkResult::Invalid;
            }
            coalescedPayload += part;
        }

        g_HermesAddonChunkBuffers.erase(key.str());
        return HermesAddonChunkResult::Complete;
    }

    bool TryReadAddonChatPacket(WorldPacket& packet, std::string& prefix, std::string& payload)
    {
        if (packet.GetOpcode() != SMSG_MESSAGECHAT)
            return false;

        try
        {
            uint8 chatType = 0;
            int32 language = 0;
            ObjectGuid senderGuid;
            uint32 flags = 0;
            ObjectGuid receiverGuid;
            uint32 messageLength = 0;
            std::string fullMessage;

            packet >> chatType;
            packet >> language;
            packet >> senderGuid;
            packet >> flags;

            if (chatType != CHAT_MSG_WHISPER || language != LANG_ADDON)
                return false;

            packet >> receiverGuid;
            packet >> messageLength;
            if (messageLength == 0 || messageLength > packet.size() - packet.rpos())
                return false;

            fullMessage = packet.ReadCString(false);
            return ExtractAddonEnvelope(fullMessage, prefix, payload);
        }
        catch (std::exception const& ex)
        {
            LOG_INFO("server.loading", "HermesBridge: failed to parse addon chat packet: {}", ex.what());
            return false;
        }
    }

    void AppendMethodRegistryJson(std::ostringstream& out)
    {
        out << "\"methods\":[";

        bool first = true;
        for (HermesMethodDescriptor const& descriptor : HERMES_METHOD_REGISTRY)
        {
            if (!first)
                out << ",";

            first = false;
            out << "\"" << EscapeJsonString(descriptor.Name ? descriptor.Name : "") << "\"";
        }

        out << "]";
    }

    void AppendUInt16LE(std::string& out, uint16 value)
    {
        out.push_back(static_cast<char>(value & 0xFF));
        out.push_back(static_cast<char>((value >> 8) & 0xFF));
    }

    void AppendUInt32LE(std::string& out, uint32 value)
    {
        out.push_back(static_cast<char>(value & 0xFF));
        out.push_back(static_cast<char>((value >> 8) & 0xFF));
        out.push_back(static_cast<char>((value >> 16) & 0xFF));
        out.push_back(static_cast<char>((value >> 24) & 0xFF));
    }

    char const* HermesLaneName(uint8 lane)
    {
        switch (lane)
        {
            case HERMES_LANE_CONTROL:
                return "control";
            case HERMES_LANE_RPC:
                return "rpc";
            case HERMES_LANE_EVENT:
                return "event";
            case HERMES_LANE_SNAPSHOT:
                return "snapshot";
            case HERMES_LANE_BULK:
                return "bulk";
            case HERMES_LANE_DEBUG:
                return "debug";
            default:
                return "unknown";
        }
    }

    char const* HermesMessageTypeName(uint8 messageType)
    {
        switch (messageType)
        {
            case HERMES_MESSAGE_REQUEST:
                return "request";
            case HERMES_MESSAGE_RESPONSE:
                return "response";
            case HERMES_MESSAGE_EVENT:
                return "event";
            case HERMES_MESSAGE_ERROR:
                return "error";
            case HERMES_MESSAGE_ACK:
                return "ack";
            case HERMES_MESSAGE_CHUNK:
                return "chunk";
            default:
                return "unknown";
        }
    }

    char const* HermesCodecName(uint8 codec)
    {
        switch (codec)
        {
            case HERMES_CODEC_JSON:
                return "json";
            case HERMES_CODEC_MSGPACK:
                return "msgpack";
            case HERMES_CODEC_BINARY:
                return "binary";
            default:
                return "unknown";
        }
    }

    char const* HermesSecurityName(AccountTypes security)
    {
        switch (security)
        {
            case SEC_PLAYER:
                return "player";
            case SEC_MODERATOR:
                return "moderator";
            case SEC_GAMEMASTER:
                return "gamemaster";
            case SEC_ADMINISTRATOR:
                return "administrator";
            case SEC_CONSOLE:
                return "console";
            default:
                return "unknown";
        }
    }

    uint8 HermesLaneStatsIndex(uint8 lane)
    {
        if (lane < HERMES_LANE_COUNT)
            return lane;

        return HERMES_LANE_DEBUG;
    }

    char const* HermesLaneBackpressurePolicy(uint8 lane)
    {
        switch (lane)
        {
            case HERMES_LANE_CONTROL:
            case HERMES_LANE_RPC:
            case HERMES_LANE_BULK:
                return "reliable";
            case HERMES_LANE_SNAPSHOT:
                return "coalesce-latest";
            case HERMES_LANE_EVENT:
            case HERMES_LANE_DEBUG:
                return "drop-newest";
            default:
                return "drop-newest";
        }
    }

    bool HermesLaneIsReliable(uint8 lane)
    {
        return lane == HERMES_LANE_CONTROL || lane == HERMES_LANE_RPC || lane == HERMES_LANE_BULK;
    }

    bool HermesOutboundFramesCanCoalesce(HermesOutboundFrame const& existing, HermesOutboundFrame const& candidate)
    {
        return existing.Session == candidate.Session
            && existing.Lane == HERMES_LANE_SNAPSHOT
            && candidate.Lane == HERMES_LANE_SNAPSHOT
            && existing.SchemaId == candidate.SchemaId
            && existing.MethodId == candidate.MethodId;
    }

    uint64 HermesOutboundDebugKey(uint8 lane, uint8 messageType, uint8 codec, uint16 schemaId, uint16 methodId)
    {
        return (static_cast<uint64>(lane) << 56)
            | (static_cast<uint64>(messageType) << 48)
            | (static_cast<uint64>(codec) << 40)
            | (static_cast<uint64>(schemaId) << 16)
            | static_cast<uint64>(methodId);
    }

    bool IsHermesAddonMessageFrame(uint8 messageType, uint16 methodId)
    {
        return messageType == HERMES_MESSAGE_EVENT && methodId == HERMES_METHOD_ADDON_MESSAGE;
    }

    bool IsHermesWhiteDamageEventFrame(uint8 messageType, uint16 methodId)
    {
        return messageType == HERMES_MESSAGE_EVENT && methodId == HERMES_METHOD_PLAYER_EMIT_DAMAGE_EVENT;
    }

    bool IsHermesPriorityVitalsAddonMessage(std::string const& prefix, std::string const& payload)
    {
        if (prefix != "PATTRPANEL")
            return false;

        bool const isPlayerVitals = payload.rfind("STATS:", 0) == 0
            && (payload.find("CUR_HEALTH=") != std::string::npos || payload.find("CURRENT_HEALTH=") != std::string::npos);
        bool const isTargetVitals = payload.rfind("TARGET:", 0) == 0
            && payload.find("CUR_HEALTH=") != std::string::npos
            && payload.find("MAX_HEALTH=") != std::string::npos;

        return isPlayerVitals || isTargetVitals;
    }

    void TrackHermesOutboundDebugStats(WorldSession& session, uint8 lane, uint8 messageType, uint8 codec, uint16 schemaId, uint16 methodId, uint32 payloadSize, bool suppressed)
    {
        if (!sConfigMgr->GetOption<bool>("HermesBridge.DebugOutboundStats", false))
            return;

        uint32 const now = getMSTime();
        // 【审计修复】调试统计容器可能被多线程并发写入，整段加锁（仅内存操作与日志，无发包/查库）
        std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
        if (!g_HermesOutboundDebugStats.WindowStartMs)
            g_HermesOutboundDebugStats.WindowStartMs = now;

        uint64 const key = HermesOutboundDebugKey(lane, messageType, codec, schemaId, methodId);
        HermesOutboundDebugBucket& bucket = g_HermesOutboundDebugStats.Buckets[key];
        bucket.Lane = lane;
        bucket.MessageType = messageType;
        bucket.Codec = codec;
        bucket.SchemaId = schemaId;
        bucket.MethodId = methodId;

        if (suppressed)
        {
            ++g_HermesOutboundDebugStats.Suppressed;
            g_HermesOutboundDebugStats.SuppressedBytes += payloadSize;
            ++bucket.Suppressed;
            bucket.SuppressedBytes += payloadSize;
        }
        else
        {
            ++g_HermesOutboundDebugStats.Sent;
            g_HermesOutboundDebugStats.SentBytes += payloadSize;
            ++bucket.Sent;
            bucket.SentBytes += payloadSize;
        }

        uint32 const logIntervalMs = std::max<uint32>(sConfigMgr->GetOption<uint32>("HermesBridge.DebugOutboundStats.LogIntervalMs", 1000), 100);
        uint32 const elapsedMs = GetMSTimeDiffToNow(g_HermesOutboundDebugStats.WindowStartMs);
        if (elapsedMs < logIntervalMs)
            return;

        std::vector<HermesOutboundDebugBucket> buckets;
        buckets.reserve(g_HermesOutboundDebugStats.Buckets.size());
        for (auto const& itr : g_HermesOutboundDebugStats.Buckets)
            buckets.push_back(itr.second);

        std::sort(buckets.begin(), buckets.end(), [](HermesOutboundDebugBucket const& left, HermesOutboundDebugBucket const& right)
        {
            uint64 const leftTotal = left.Sent + left.Suppressed;
            uint64 const rightTotal = right.Sent + right.Suppressed;
            if (leftTotal != rightTotal)
                return leftTotal > rightTotal;
            return (left.SentBytes + left.SuppressedBytes) > (right.SentBytes + right.SuppressedBytes);
        });

        std::ostringstream top;
        std::size_t const topCount = std::min<std::size_t>(buckets.size(), 8);
        for (std::size_t i = 0; i < topCount; ++i)
        {
            if (i)
                top << " | ";

            HermesOutboundDebugBucket const& bucketRef = buckets[i];
            top << "[method=" << bucketRef.MethodId
                << " schema=" << bucketRef.SchemaId
                << " lane=" << HermesLaneName(bucketRef.Lane)
                << " type=" << HermesMessageTypeName(bucketRef.MessageType)
                << " codec=" << HermesCodecName(bucketRef.Codec)
                << "] sent=" << bucketRef.Sent
                << " sentBytes=" << bucketRef.SentBytes
                << " suppressed=" << bucketRef.Suppressed
                << " suppressedBytes=" << bucketRef.SuppressedBytes;
        }

        Player const* player = session.GetPlayer();
        LOG_INFO("server.loading", "[HermesBridgeOutboundStats] player={} elapsedMs={} sent={} sentBytes={} suppressed={} suppressedBytes={} top={}",
            player ? player->GetName() : "<none>",
            elapsedMs,
            g_HermesOutboundDebugStats.Sent,
            g_HermesOutboundDebugStats.SentBytes,
            g_HermesOutboundDebugStats.Suppressed,
            g_HermesOutboundDebugStats.SuppressedBytes,
            top.str());

        g_HermesOutboundDebugStats = HermesOutboundDebugStats();
        g_HermesOutboundDebugStats.WindowStartMs = now;
    }

    bool ShouldSuppressHermesOutboundFrame(WorldSession& session, uint8 messageType, uint16 methodId, uint32 payloadSize, bool bypassRateLimit = false)
    {
        if (!IsHermesWhiteDamageEventFrame(messageType, methodId))
            return false;

        Player const* player = session.GetPlayer();
        bool const inCombat = player && player->IsInCombat();

        if (sConfigMgr->GetOption<bool>("HermesBridge.SuppressOutbound", false))
            return true;

        if (inCombat && sConfigMgr->GetOption<bool>("HermesBridge.SuppressOutboundInCombat", false))
            return true;

        if (inCombat && IsHermesAddonMessageFrame(messageType, methodId) && sConfigMgr->GetOption<bool>("HermesBridge.SuppressAddonMessageEventsInCombat", false))
            return true;

        if (bypassRateLimit)
            return false;

        uint32 const maxPerSecond = sConfigMgr->GetOption<uint32>("HermesBridge.Outbound.MaxPacketsPerSecond", 0);
        uint32 const maxBytesPerSecond = sConfigMgr->GetOption<uint32>("HermesBridge.Outbound.MaxBytesPerSecond", 0);
        uint32 const maxFrameBytes = sConfigMgr->GetOption<uint32>("HermesBridge.Outbound.MaxFrameBytes", 0);
        if (!maxPerSecond && !maxBytesPerSecond && !maxFrameBytes)
            return false;

        if (maxFrameBytes && payloadSize > maxFrameBytes)
            return true;

        uint32 const now = getMSTime();
        uint32 const accountId = session.GetAccountId();
        // 【审计修复】限速桶 operator[] 并发插入会破坏 unordered_map，读改写整段加锁
        std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
        HermesOutboundRateBucket& bucket = g_HermesOutboundRateBuckets[accountId];
        if (!bucket.WindowStartMs || getMSTimeDiff(bucket.WindowStartMs, now) >= 1000)
        {
            bucket.WindowStartMs = now;
            bucket.Count = 0;
            bucket.Bytes = 0;
        }

        uint32 const minIntervalMs = maxPerSecond ? std::max<uint32>((1000 + maxPerSecond - 1) / maxPerSecond, 1) : 0;
        if (minIntervalMs && bucket.LastSentMs && getMSTimeDiff(bucket.LastSentMs, now) < minIntervalMs)
            return true;

        if (maxPerSecond && bucket.Count >= maxPerSecond)
            return true;

        if (maxBytesPerSecond && bucket.Bytes + payloadSize > maxBytesPerSecond)
            return true;

        ++bucket.Count;
        bucket.Bytes += payloadSize;
        bucket.LastSentMs = now;
        return false;
    }

    // 【审计修复】以下 Track*/Finish*Locked 统计函数要求调用方已持有 g_HermesStateMutex
    void TrackHermesOutboundDrop(WorldSession& session, uint8 lane, uint16 methodId, uint32 requestId, uint32 payloadSize)
    {
        uint8 const index = HermesLaneStatsIndex(lane);
        HermesOutboundLaneStats& laneStats = g_HermesOutboundQueueStats.Lanes[index];

        ++g_HermesOutboundQueueStats.Dropped;
        ++laneStats.Dropped;
        LOG_INFO("server.loading", "HermesBridge: outbound queue full account={} lane={} methodId={} requestId={} bytes={}", session.GetAccountId(), static_cast<unsigned int>(lane), methodId, requestId, payloadSize);
    }

    void TrackHermesOutboundCoalesce(uint8 lane)
    {
        uint8 const index = HermesLaneStatsIndex(lane);
        HermesOutboundLaneStats& laneStats = g_HermesOutboundQueueStats.Lanes[index];

        ++g_HermesOutboundQueueStats.Coalesced;
        ++laneStats.Coalesced;
    }

    void TrackHermesOutboundEnqueue(uint8 lane)
    {
        uint8 const index = HermesLaneStatsIndex(lane);
        HermesOutboundLaneStats& laneStats = g_HermesOutboundQueueStats.Lanes[index];

        ++g_HermesOutboundQueueStats.Depth;
        ++g_HermesOutboundQueueStats.Enqueued;
        if (g_HermesOutboundQueueStats.Depth > g_HermesOutboundQueueStats.HighWatermark)
            g_HermesOutboundQueueStats.HighWatermark = g_HermesOutboundQueueStats.Depth;

        ++laneStats.Depth;
        ++laneStats.Enqueued;
        if (laneStats.Depth > laneStats.HighWatermark)
            laneStats.HighWatermark = laneStats.Depth;
    }

    void FinishHermesOutboundSendLocked(uint8 lane)
    {
        uint8 const index = HermesLaneStatsIndex(lane);
        HermesOutboundLaneStats& laneStats = g_HermesOutboundQueueStats.Lanes[index];

        if (g_HermesOutboundQueueStats.Depth)
            --g_HermesOutboundQueueStats.Depth;
        if (laneStats.Depth)
            --laneStats.Depth;

        ++g_HermesOutboundQueueStats.Sent;
        ++laneStats.Sent;
    }

    // 【审计修复】发送路径在锁外执行，统计回写时自行加锁
    void FinishHermesOutboundSend(uint8 lane)
    {
        std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
        FinishHermesOutboundSendLocked(lane);
    }

    void SendQueuedHermesOutboundFrame(HermesOutboundFrame const& frame)
    {
        if (!frame.Session)
        {
            FinishHermesOutboundSend(frame.Lane);
            return;
        }

        // 【审计修复】发送前按账号 ID 验证会话仍存活且指针未被复用，防止会话析构后 SendPacket use-after-free
        if (sWorldSessionMgr->FindSession(frame.AccountId) != frame.Session)
        {
            FinishHermesOutboundSend(frame.Lane);
            return;
        }

        WorldPacket response(SMSG_HERMES_BRIDGE, HERMES_BRIDGE_FRAME_HEADER_SIZE + frame.Payload.size());
        response << uint16(HERMES_BRIDGE_FRAME_MAGIC);
        response << uint8(HERMES_BRIDGE_FRAME_VERSION);
        response << uint8(HERMES_BRIDGE_FRAME_HEADER_SIZE);
        response << uint8(frame.Lane);
        response << uint8(frame.MessageType);
        response << uint8(frame.Codec);
        response << uint8(0);
        response << uint16(frame.SchemaId);
        response << uint16(frame.MethodId);
        response << uint32(frame.RequestId);
        response << uint32(frame.Sequence);
        response << uint32(frame.Payload.size());
        if (!frame.Payload.empty())
            response.append(reinterpret_cast<uint8 const*>(frame.Payload.data()), frame.Payload.size());

        if (HERMES_BRIDGE_TRACE_PACKETS)
            LOG_INFO("server.loading", "HermesBridge: v2 send packet built account={} opcode=0x{:04X} packetBytes={}", frame.Session->GetAccountId(), SMSG_HERMES_BRIDGE, response.size());
        frame.Session->SendPacket(&response);
        FinishHermesOutboundSend(frame.Lane);
    }

    void FlushHermesOutboundQueue()
    {
        // 【审计修复】持锁把待发帧 swap 到局部容器后立即解锁，再逐帧 SendPacket，
        // 避免持锁发包：SendPacket 会再触发 CanPacketSend 钩子（SMSG_HERMES_BRIDGE 在
        // TryReadAddonChatPacket 的 opcode 判断处直接放行，不会再进入加锁路径），
        // 且防止极端情况下重入本模块时二次抢同一把锁造成死锁
        std::deque<HermesOutboundFrame> pending;
        {
            std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
            pending.swap(g_HermesOutboundQueue);
        }

        for (HermesOutboundFrame const& frame : pending)
            SendQueuedHermesOutboundFrame(frame);
    }

    // 【审计修复】要求调用方已持有 g_HermesStateMutex
    bool CoalesceHermesOutboundFrame(HermesOutboundFrame const& candidate)
    {
        if (candidate.Lane != HERMES_LANE_SNAPSHOT)
            return false;

        for (auto queued = g_HermesOutboundQueue.rbegin(); queued != g_HermesOutboundQueue.rend(); ++queued)
        {
            if (!HermesOutboundFramesCanCoalesce(*queued, candidate))
                continue;

            *queued = candidate;
            TrackHermesOutboundCoalesce(candidate.Lane);
            return true;
        }

        return false;
    }

    bool QueueHermesOutboundFrame(WorldSession& session, HermesFrameV2 const& requestFrame, uint8 messageType, uint8 codec, std::string const& responsePayload)
    {
        HermesOutboundFrame frame;
        frame.Session = &session;
        // 【审计修复】入队时记录账号 ID 供发送前会话存活校验
        frame.AccountId = session.GetAccountId();
        frame.Lane = requestFrame.Lane;
        frame.MessageType = messageType;
        frame.Codec = codec;
        frame.SchemaId = requestFrame.SchemaId;
        frame.MethodId = requestFrame.MethodId;
        frame.RequestId = requestFrame.RequestId;
        frame.Sequence = requestFrame.Sequence;
        frame.Payload = responsePayload;

        // 【审计修复】队列与统计的读改写全程持锁；中途需要 flush 时先解锁（flush 内部自行加锁并在锁外发包）
        std::unique_lock<std::mutex> stateGuard(g_HermesStateMutex);

        if (CoalesceHermesOutboundFrame(frame))
            return true;

        bool const reliableFrame = HermesLaneIsReliable(frame.Lane) || frame.MethodId == HERMES_METHOD_ADDON_MESSAGE;

        if (g_HermesOutboundQueueStats.Depth >= HERMES_SERVER_OUTBOUND_QUEUE_CAPACITY)
        {
            if (reliableFrame)
            {
                stateGuard.unlock();
                FlushHermesOutboundQueue();
                stateGuard.lock();
            }
            else if (CoalesceHermesOutboundFrame(frame))
                return true;
        }

        if (g_HermesOutboundQueueStats.Depth >= HERMES_SERVER_OUTBOUND_QUEUE_CAPACITY)
        {
            TrackHermesOutboundDrop(session, frame.Lane, frame.MethodId, frame.RequestId, static_cast<uint32>(frame.Payload.size()));
            return false;
        }

        TrackHermesOutboundEnqueue(frame.Lane);
        g_HermesOutboundQueue.push_back(std::move(frame));
        return true;
    }

    // 【审计修复】RAII 批量守卫，g_HermesOutboundBatchDepth 已为 thread_local：
    // 仅暂缓本线程的 flush，析构（含异常展开路径）时保证配对递减并在归零时 flush
    struct HermesOutboundBatchScope
    {
        HermesOutboundBatchScope()
        {
            ++g_HermesOutboundBatchDepth;
        }

        ~HermesOutboundBatchScope()
        {
            if (g_HermesOutboundBatchDepth)
                --g_HermesOutboundBatchDepth;
            if (!g_HermesOutboundBatchDepth)
                FlushHermesOutboundQueue();
        }
    };

    // 【审计修复】会话登出时清除其滞留在出站队列中的帧，防止之后 flush 对已析构会话发包
    void PurgeHermesOutboundFramesForSession(WorldSession* session)
    {
        if (!session)
            return;

        std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
        for (auto itr = g_HermesOutboundQueue.begin(); itr != g_HermesOutboundQueue.end();)
        {
            if (itr->Session == session)
            {
                FinishHermesOutboundSendLocked(itr->Lane);
                itr = g_HermesOutboundQueue.erase(itr);
            }
            else
                ++itr;
        }
    }

    void AppendHermesOutboundQueueJson(std::ostringstream& out)
    {
        // 【审计修复】统计读取同样持锁，避免读到撕裂数据
        std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
        out << "\"outboundQueue\":{\"capacity\":" << HERMES_SERVER_OUTBOUND_QUEUE_CAPACITY
            << ",\"depth\":" << g_HermesOutboundQueueStats.Depth
            << ",\"highWatermark\":" << g_HermesOutboundQueueStats.HighWatermark
            << ",\"enqueued\":" << g_HermesOutboundQueueStats.Enqueued
            << ",\"sent\":" << g_HermesOutboundQueueStats.Sent
            << ",\"dropped\":" << g_HermesOutboundQueueStats.Dropped
            << ",\"coalesced\":" << g_HermesOutboundQueueStats.Coalesced
            << ",\"lanes\":[";

        for (uint8 lane = 0; lane < HERMES_LANE_COUNT; ++lane)
        {
            if (lane)
                out << ",";

            HermesOutboundLaneStats const& laneStats = g_HermesOutboundQueueStats.Lanes[lane];
            out << "{\"laneId\":" << static_cast<unsigned int>(lane)
                << ",\"lane\":\"" << HermesLaneName(lane)
                << "\",\"policy\":\"" << HermesLaneBackpressurePolicy(lane)
                << "\",\"depth\":" << laneStats.Depth
                << ",\"highWatermark\":" << laneStats.HighWatermark
                << ",\"enqueued\":" << laneStats.Enqueued
                << ",\"sent\":" << laneStats.Sent
                << ",\"dropped\":" << laneStats.Dropped
                << ",\"coalesced\":" << laneStats.Coalesced
                << "}";
        }

        out << "]}";
    }

    uint64 HermesRateLimitKey(uint32 accountId, uint16 methodId)
    {
        return (uint64(accountId) << 16) | methodId;
    }

    bool HermesMethodNameStartsWith(HermesMethodDescriptor const& descriptor, char const* prefix)
    {
        if (!descriptor.Name || !prefix)
            return false;

        char const* name = descriptor.Name;
        while (*prefix)
        {
            if (*name++ != *prefix++)
                return false;
        }

        return true;
    }

    bool HermesMethodNameEquals(HermesMethodDescriptor const& descriptor, char const* expected)
    {
        if (!descriptor.Name || !expected)
            return false;

        char const* name = descriptor.Name;
        while (*name && *expected)
        {
            if (*name++ != *expected++)
                return false;
        }

        return *name == '\0' && *expected == '\0';
    }

    bool IsHermesRateLimitedMethod(HermesMethodDescriptor const& descriptor)
    {
        return HermesMethodNameEquals(descriptor, "player.emitDamageEvent");
    }

    uint16 GetEffectiveHermesRateLimitPerSecond(HermesMethodDescriptor const& descriptor)
    {
        return IsHermesRateLimitedMethod(descriptor) ? descriptor.RateLimitPerSecond : 0;
    }

    bool IsHermesRateLimited(WorldSession& session, HermesMethodDescriptor const& descriptor)
    {
        uint16 const rateLimitPerSecond = GetEffectiveHermesRateLimitPerSecond(descriptor);
        if (!rateLimitPerSecond)
            return false;

        time_t const now = std::time(nullptr);
        uint64 const key = HermesRateLimitKey(session.GetAccountId(), descriptor.MethodId);
        HermesRateLimitBucket& bucket = g_HermesRateLimitBuckets[key];

        if (bucket.WindowSecond != now)
        {
            bucket.WindowSecond = now;
            bucket.Count = 0;
        }

        if (bucket.Count >= rateLimitPerSecond)
            return true;

        ++bucket.Count;
        return false;
    }

    void AppendSchemaRegistryJson(std::ostringstream& out)
    {
        out << "\"schemas\":[";

        bool first = true;
        for (HermesSchemaDescriptor const& descriptor : HERMES_SCHEMA_REGISTRY)
        {
            if (!first)
                out << ",";

            first = false;
            out << "{\"schemaId\":" << descriptor.SchemaId
                << ",\"name\":\"" << EscapeJsonString(descriptor.Name ? descriptor.Name : "")
                << "\",\"lane\":\"" << HermesLaneName(descriptor.Lane)
                << "\",\"laneId\":" << static_cast<unsigned int>(descriptor.Lane)
                << ",\"messageType\":\"" << HermesMessageTypeName(descriptor.MessageType)
                << "\",\"messageTypeId\":" << static_cast<unsigned int>(descriptor.MessageType)
                << ",\"codec\":\"" << HermesCodecName(descriptor.Codec)
                << "\",\"codecId\":" << static_cast<unsigned int>(descriptor.Codec)
                << ",\"status\":\"" << EscapeJsonString(descriptor.Status ? descriptor.Status : "")
                << "\",\"description\":\"" << EscapeJsonString(descriptor.Description ? descriptor.Description : "")
                << "\"}";
        }

        out << "]";
    }

    void AppendMethodRegistryDetailJson(std::ostringstream& out)
    {
        out << "\"methods\":[";

        bool first = true;
        for (HermesMethodDescriptor const& descriptor : HERMES_METHOD_REGISTRY)
        {
            if (!first)
                out << ",";

            first = false;
            out << "{\"methodId\":" << descriptor.MethodId
                << ",\"name\":\"" << EscapeJsonString(descriptor.Name ? descriptor.Name : "")
                << "\",\"lane\":\"" << HermesLaneName(descriptor.Lane)
                << "\",\"laneId\":" << static_cast<unsigned int>(descriptor.Lane)
                << ",\"codec\":\"" << HermesCodecName(descriptor.Codec)
                << "\",\"codecId\":" << static_cast<unsigned int>(descriptor.Codec)
                << ",\"status\":\"" << EscapeJsonString(descriptor.Status ? descriptor.Status : "")
                << "\",\"minSecurity\":" << static_cast<unsigned int>(descriptor.MinSecurity)
                << ",\"minSecurityName\":\"" << HermesSecurityName(descriptor.MinSecurity)
                << "\",\"rateLimitPerSecond\":" << GetEffectiveHermesRateLimitPerSecond(descriptor)
                << "}";
        }

        out << "]";
    }

    std::string JsonRpcIdText(Json::Value const& request, uint32 transportRequestId)
    {
        if (request.isObject() && request.isMember("id"))
        {
            Json::Value const& id = request["id"];
            if (id.isString())
                return "\"" + EscapeJsonString(id.asString()) + "\"";

            if (id.isIntegral())
                return std::to_string(id.asUInt64());

            if (id.isNull())
                return "null";
        }

        if (transportRequestId)
            return std::to_string(transportRequestId);

        return "null";
    }

    std::string JsonRpcErrorPayload(std::string const& idText, int code, char const* message, char const* hermesCode, std::string const& method = "")
    {
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"error\":{\"code\":" << code
            << ",\"message\":\"" << EscapeJsonString(message ? message : "")
            << "\",\"data\":{\"hermesCode\":\"" << EscapeJsonString(hermesCode ? hermesCode : "") << "\"";

        if (!method.empty())
            out << ",\"method\":\"" << EscapeJsonString(method) << "\"";

        out << "}}}";
        return out.str();
    }

    std::string ExtractPayloadParam(Json::Value const& request)
    {
        if (!request.isObject() || !request.isMember("params"))
            return "";

        Json::Value const& params = request["params"];
        if (params.isObject() && params.isMember("payload") && params["payload"].isString())
            return params["payload"].asString();

        return "";
    }

    void AppendPlayerBasicFieldsJson(std::ostringstream& out, WorldSession& session, Player const& player)
    {
        out << "\"accountId\":" << session.GetAccountId()
            << ",\"guid\":\"" << EscapeJsonString(player.GetGUID().ToString())
            << "\",\"name\":\"" << EscapeJsonString(player.GetName())
            << "\",\"level\":" << static_cast<unsigned int>(player.GetLevel())
            << ",\"race\":" << static_cast<unsigned int>(player.getRace())
            << ",\"class\":" << static_cast<unsigned int>(player.getClass())
            << ",\"teamId\":" << static_cast<unsigned int>(player.GetTeamId());
    }

    void AppendPlayerPositionFieldsJson(std::ostringstream& out, Player const& player)
    {
        out << "\"mapId\":" << player.GetMapId()
            << ",\"zoneId\":" << player.GetZoneId()
            << ",\"areaId\":" << player.GetAreaId()
            << ",\"x\":" << player.GetPositionX()
            << ",\"y\":" << player.GetPositionY()
            << ",\"z\":" << player.GetPositionZ()
            << ",\"orientation\":" << player.GetOrientation();
    }

    bool IsJsonSafeUInt(uint256 const& value)
    {
        return value <= static_cast<uint256>(HERMES_JSON_SAFE_UINT_MAX);
    }

    uint64 ToJsonSafeUInt(uint256 const& value)
    {
        return IsJsonSafeUInt(value) ? static_cast<uint64>(value) : HERMES_JSON_SAFE_UINT_MAX;
    }

    bool IsUInt32Exact(uint256 const& value)
    {
        return value <= static_cast<uint256>(std::numeric_limits<uint32>::max());
    }

    uint32 ToUInt32Saturated(uint256 const& value)
    {
        return IsUInt32Exact(value) ? static_cast<uint32>(value) : std::numeric_limits<uint32>::max();
    }

    uint32 ToUInt32SaturatedPositive(int256 const& value)
    {
        if (value <= 0)
            return 0;

        return ToUInt32Saturated(static_cast<uint256>(value));
    }

    bool IsJsonSafeInt(int256 const& value)
    {
        return value >= 0 && static_cast<uint256>(value) <= static_cast<uint256>(HERMES_JSON_SAFE_UINT_MAX);
    }

    uint64 ToJsonSafeInt(int256 const& value)
    {
        if (value <= 0)
            return 0;

        uint256 const unsignedValue = static_cast<uint256>(value);
        return unsignedValue <= static_cast<uint256>(HERMES_JSON_SAFE_UINT_MAX) ? static_cast<uint64>(unsignedValue) : HERMES_JSON_SAFE_UINT_MAX;
    }

    std::string BigUIntToText(uint256 const& value)
    {
        return value.convert_to<std::string>();
    }

    std::string BigIntToText(int256 const& value)
    {
        return value.convert_to<std::string>();
    }

    std::string HeadText(std::string const& value, std::size_t count)
    {
        return value.substr(0, std::min(count, value.size()));
    }

    std::string TailText(std::string const& value, std::size_t count)
    {
        return value.size() <= count ? value : value.substr(value.size() - count);
    }

    void AppendBigUIntMetricJson(std::ostringstream& out, char const* name, uint256 const& value)
    {
        out << "\"" << name << "\":" << ToJsonSafeUInt(value)
            << ",\"" << name << "Text\":\"" << BigUIntToText(value) << "\""
            << ",\"" << name << "Exact\":" << (IsJsonSafeUInt(value) ? "true" : "false");
    }

    void AppendBigIntMetricJson(std::ostringstream& out, char const* name, int256 const& value)
    {
        out << "\"" << name << "\":" << ToJsonSafeInt(value)
            << ",\"" << name << "Text\":\"" << BigIntToText(value) << "\""
            << ",\"" << name << "Exact\":" << (IsJsonSafeInt(value) ? "true" : "false");
    }

    void AppendPlayerVitalsFieldsJson(std::ostringstream& out, Player const& player)
    {
        Powers const powerType = player.getPowerType();
        out << "\"alive\":" << (player.IsAlive() ? "true" : "false")
            << ",\"inCombat\":" << (player.IsInCombat() ? "true" : "false")
            << ",";
        AppendBigUIntMetricJson(out, "health", player.GetHealthForCombat256());
        out << ",";
        AppendBigUIntMetricJson(out, "maxHealth", player.GetMaxHealthForCombat256());
        out << ",\"powerType\":" << static_cast<unsigned int>(powerType)
            << ",";
        AppendBigUIntMetricJson(out, "power", player.GetPowerForCombat256(powerType));
        out << ",";
        AppendBigUIntMetricJson(out, "maxPower", player.GetMaxPowerForCombat256(powerType));
        out << ",";
        AppendBigIntMetricJson(out, "money", player.GetMoney());
    }

    std::string PanelValueText(int64 value)
    {
        return std::to_string(value > 0 ? value : 0);
    }

    std::string PanelValueText(uint64 value)
    {
        return std::to_string(value);
    }

    std::string PanelValueText(int256 const& value)
    {
        return value > 0 ? value.convert_to<std::string>() : "0";
    }

    std::string PanelValueText(uint256 const& value)
    {
        return value.convert_to<std::string>();
    }

    std::string PanelValueText(long double value)
    {
        if (value <= 0.0L)
            return "0";

        return PanelValueText(Acore::Number::ToInt256Saturated(value));
    }

    std::string BuildPanelRangePayload(int256 const& minValue, int256 const& maxValue)
    {
        std::ostringstream range;
        range << (minValue > 0 ? minValue : int256(0)) << "~" << (maxValue > 0 ? maxValue : int256(0));
        return range.str();
    }

    int256 GetPanelStat(Player& player, Stats stat)
    {
        int256 extendedValue = player.GetExtendedStat256(stat);
        return extendedValue > 0 ? extendedValue : Acore::Number::ToInt256Saturated(static_cast<long double>(player.GetStat(stat)));
    }

    int256 GetPanelCombatRating(Player& player, CombatRating combatRating)
    {
        int256 extendedValue = player.GetExtendedCombatRating(combatRating);
        if (extendedValue > 0)
            return extendedValue;

        int32 fieldValue = player.GetInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + combatRating);
        return fieldValue > 0 ? int256(fieldValue) : int256(0);
    }

    int256 GetPanelBaseSpellPowerBonus(Player& player)
    {
        return Acore::Number::ToInt256Saturated(player.GetBaseSpellPowerBonus256());
    }

    int256 GetPanelHealingBonus(Player& player)
    {
        int256 extendedValue = player.GetExtendedHealingBonus256();
        if (extendedValue > 0)
            return extendedValue;

        int32 fieldValue = player.GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS);
        if (fieldValue > 0)
            return fieldValue;

        return GetPanelBaseSpellPowerBonus(player);
    }

    int256 GetPanelSpellDamageBonus(Player& player)
    {
        int256 extendedValue = player.GetExtendedSpellDamageBonus256();
        if (extendedValue > 0)
            return extendedValue;

        int32 fieldValue = 0;
        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        {
            int32 schoolBonus = player.GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i);
            if (schoolBonus > fieldValue)
                fieldValue = schoolBonus;
        }

        if (fieldValue > 0)
            return fieldValue;

        return GetPanelBaseSpellPowerBonus(player);
    }

    int256 GetPanelSpellPowerBonus(Player& player)
    {
        int256 extendedValue = player.GetExtendedSpellPowerBonus256();
        if (extendedValue > 0)
            return extendedValue;

        int32 fieldValue = player.GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS);
        if (fieldValue < 0)
            fieldValue = 0;

        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        {
            int32 schoolBonus = player.GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i);
            if (schoolBonus > fieldValue)
                fieldValue = schoolBonus;
        }

        if (fieldValue > 0)
            return fieldValue;

        return GetPanelBaseSpellPowerBonus(player);
    }

    int64 GetPanelSpellPenetration(Player& player)
    {
        int32 itemMod = player.GetSpellPenetrationItemMod();
        if (itemMod > 0)
            return itemMod;

        int64 fieldValue = static_cast<int64>(player.GetInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE));
        if (fieldValue < 0)
            return -fieldValue;

        return fieldValue > 0 ? fieldValue : 0;
    }

    void AddPanelAttribute(HermesAttributeFields& attributes, char const* key, std::string const& value)
    {
        attributes.emplace_back(key ? key : "", value);
    }

    void AddPanelAttribute(HermesAttributeFields& attributes, uint32 id, std::string const& value)
    {
        attributes.emplace_back(std::to_string(id), value);
    }

    void BuildPlayerAttributeFields(Player& player, HermesAttributeFields& attributes)
    {
        attributes.reserve(56);

        AddPanelAttribute(attributes, "CUR_HEALTH", PanelValueText(player.GetExtendedHealth256()));
        AddPanelAttribute(attributes, "CURRENT_HEALTH", PanelValueText(player.GetExtendedHealth256()));
        AddPanelAttribute(attributes, "CUR_MANA", PanelValueText(player.GetPowerForCombat256(POWER_MANA)));
        AddPanelAttribute(attributes, "CURRENT_MANA", PanelValueText(player.GetPowerForCombat256(POWER_MANA)));
        AddPanelAttribute(attributes, uint32(1), PanelValueText(player.GetExtendedMaxHealth256()));
        AddPanelAttribute(attributes, uint32(0), PanelValueText(player.GetExtendedMaxPower256(POWER_MANA)));

        for (HermesPanelStatDef const& statDef : HERMES_PANEL_PRIMARY_STATS)
            AddPanelAttribute(attributes, statDef.Id, PanelValueText(GetPanelStat(player, statDef.Stat)));

        AddPanelAttribute(attributes, "ARMOR", PanelValueText(player.GetExtendedArmor256()));
        AddPanelAttribute(attributes, 8, PanelValueText(player.GetTrueDamageBonus()));
        AddPanelAttribute(attributes, 9, PanelValueText(player.GetCuttingDamageBonus()));
        AddPanelAttribute(attributes, 10, PanelValueText(static_cast<uint64>(player.GetCooldownReductionBonus())));
        AddPanelAttribute(attributes, 11, PanelValueText(player.GetSkillDamageBonus()));
        AddPanelAttribute(attributes, 12, PanelValueText(GetPanelCombatRating(player, CR_DEFENSE_SKILL)));
        AddPanelAttribute(attributes, 13, PanelValueText(GetPanelCombatRating(player, CR_DODGE)));
        AddPanelAttribute(attributes, 14, PanelValueText(GetPanelCombatRating(player, CR_PARRY)));
        AddPanelAttribute(attributes, 15, PanelValueText(GetPanelCombatRating(player, CR_BLOCK)));
        AddPanelAttribute(attributes, 16, PanelValueText(GetPanelCombatRating(player, CR_HIT_MELEE)));
        AddPanelAttribute(attributes, 17, PanelValueText(GetPanelCombatRating(player, CR_HIT_RANGED)));
        AddPanelAttribute(attributes, 18, PanelValueText(GetPanelCombatRating(player, CR_HIT_SPELL)));
        AddPanelAttribute(attributes, 19, PanelValueText(GetPanelCombatRating(player, CR_CRIT_MELEE)));
        AddPanelAttribute(attributes, 20, PanelValueText(GetPanelCombatRating(player, CR_CRIT_RANGED)));
        AddPanelAttribute(attributes, 21, PanelValueText(GetPanelCombatRating(player, CR_CRIT_SPELL)));
        AddPanelAttribute(attributes, 22, PanelValueText(GetPanelCombatRating(player, CR_HIT_TAKEN_MELEE)));
        AddPanelAttribute(attributes, 23, PanelValueText(GetPanelCombatRating(player, CR_HIT_TAKEN_RANGED)));
        AddPanelAttribute(attributes, 24, PanelValueText(GetPanelCombatRating(player, CR_HIT_TAKEN_SPELL)));
        AddPanelAttribute(attributes, 25, PanelValueText(GetPanelCombatRating(player, CR_CRIT_TAKEN_MELEE)));
        AddPanelAttribute(attributes, 26, PanelValueText(GetPanelCombatRating(player, CR_CRIT_TAKEN_RANGED)));
        AddPanelAttribute(attributes, 27, PanelValueText(GetPanelCombatRating(player, CR_CRIT_TAKEN_SPELL)));
        AddPanelAttribute(attributes, 28, PanelValueText(GetPanelCombatRating(player, CR_HASTE_MELEE)));
        AddPanelAttribute(attributes, 29, PanelValueText(GetPanelCombatRating(player, CR_HASTE_RANGED)));
        AddPanelAttribute(attributes, 30, PanelValueText(GetPanelCombatRating(player, CR_HASTE_SPELL)));
        AddPanelAttribute(attributes, 31, PanelValueText(GetPanelCombatRating(player, CR_HIT_MELEE)));
        AddPanelAttribute(attributes, 32, PanelValueText(GetPanelCombatRating(player, CR_CRIT_MELEE)));
        AddPanelAttribute(attributes, 33, PanelValueText(GetPanelCombatRating(player, CR_HIT_TAKEN_MELEE)));
        AddPanelAttribute(attributes, 34, PanelValueText(GetPanelCombatRating(player, CR_CRIT_TAKEN_MELEE)));
        AddPanelAttribute(attributes, 35, PanelValueText(GetPanelCombatRating(player, CR_CRIT_TAKEN_MELEE)));
        AddPanelAttribute(attributes, 36, PanelValueText(GetPanelCombatRating(player, CR_HASTE_MELEE)));
        AddPanelAttribute(attributes, 37, PanelValueText(GetPanelCombatRating(player, CR_EXPERTISE)));
        AddPanelAttribute(attributes, 38, PanelValueText(static_cast<long double>(player.GetExtendedTotalAttackPowerValue(BASE_ATTACK))));
        AddPanelAttribute(attributes, 39, PanelValueText(static_cast<long double>(player.GetExtendedTotalAttackPowerValue(RANGED_ATTACK))));
        AddPanelAttribute(attributes, 41, PanelValueText(GetPanelHealingBonus(player)));
        AddPanelAttribute(attributes, 42, PanelValueText(GetPanelSpellDamageBonus(player)));
        AddPanelAttribute(attributes, 43, PanelValueText(static_cast<uint64>(player.GetBaseManaRegenBonus())));
        AddPanelAttribute(attributes, 44, PanelValueText(GetPanelCombatRating(player, CR_ARMOR_PENETRATION)));
        AddPanelAttribute(attributes, 45, PanelValueText(GetPanelSpellPowerBonus(player)));
        AddPanelAttribute(attributes, 46, PanelValueText(static_cast<uint64>(player.GetBaseHealthRegenBonus())));
        AddPanelAttribute(attributes, 47, PanelValueText(GetPanelSpellPenetration(player)));
        AddPanelAttribute(attributes, 48, PanelValueText(static_cast<int64>(player.GetShieldBlockValue())));
        AddPanelAttribute(attributes, "MAINHAND_DAMAGE", BuildPanelRangePayload(player.GetExtendedDamageMin(BASE_ATTACK), player.GetExtendedDamageMax(BASE_ATTACK)));
        AddPanelAttribute(attributes, "OFFHAND_DAMAGE", player.HasOffhandWeaponForAttack() ? BuildPanelRangePayload(player.GetExtendedDamageMin(OFF_ATTACK), player.GetExtendedDamageMax(OFF_ATTACK)) : "0~0");
        AddPanelAttribute(attributes, "RANGED_DAMAGE", BuildPanelRangePayload(player.GetExtendedDamageMin(RANGED_ATTACK), player.GetExtendedDamageMax(RANGED_ATTACK)));
    }

    std::string BuildPlayerAttributePayload(HermesAttributeFields const& attributes)
    {
        std::ostringstream payload;
        bool first = true;
        for (auto const& attribute : attributes)
        {
            if (!first)
                payload << "|";

            first = false;
            payload << attribute.first << "=" << attribute.second;
        }

        return payload.str();
    }

    void AppendPlayerAttributesFieldsJson(std::ostringstream& out, Player& player)
    {
        HermesAttributeFields attributes;
        BuildPlayerAttributeFields(player, attributes);
        std::string const payload = BuildPlayerAttributePayload(attributes);

        out << "\"schema\":\"player.attribute-panel.v1\""
            << ",\"source\":\"mod-player-attribute-panel-compatible\""
            << ",\"attributeCount\":" << attributes.size()
            << ",\"attributesPayload\":\"" << EscapeJsonString(payload) << "\""
            << ",\"attributes\":{";

        bool first = true;
        for (auto const& attribute : attributes)
        {
            if (!first)
                out << ",";

            first = false;
            out << "\"" << EscapeJsonString(attribute.first) << "\":\"" << EscapeJsonString(attribute.second) << "\"";
        }

        out << "}";
    }

    std::string SanitizePanelPayloadValue(std::string const& value)
    {
        std::string sanitized;
        sanitized.reserve(std::min<std::size_t>(value.size(), 32));

        for (char ch : value)
        {
            unsigned char byte = static_cast<unsigned char>(ch);
            if (byte < 32 || ch == '|' || ch == '=')
                continue;

            sanitized.push_back(ch);
            if (sanitized.size() >= 32)
                break;
        }

        return sanitized;
    }

    std::string BuildPlayerTargetPayload(Player& player, std::string const& requestToken)
    {
        std::ostringstream payload;
        std::string token = SanitizePanelPayloadValue(requestToken);
        bool hasField = false;

        if (!token.empty())
        {
            payload << "TOKEN=" << token;
            hasField = true;
        }

        auto appendField = [&](char const* key, std::string const& value)
        {
            if (hasField)
                payload << "|";

            payload << key << "=" << value;
            hasField = true;
        };

        Unit* target = player.GetSelectedUnit();
        if (!target)
        {
            appendField("NONE", "1");
            return payload.str();
        }

        Powers const powerType = target->getPowerType();
        appendField("GUID", std::to_string(target->GetGUID().GetRawValue()));
        appendField("CUR_HEALTH", PanelValueText(target->GetHealthForCombat256()));
        appendField("MAX_HEALTH", PanelValueText(target->GetMaxHealthForCombat256()));
        appendField("CUR_MANA", PanelValueText(target->GetPowerForCombat256(POWER_MANA)));
        appendField("MAX_MANA", PanelValueText(target->GetMaxPowerForCombat256(POWER_MANA)));
        appendField("POWER_TYPE", std::to_string(static_cast<uint32>(powerType)));
        return payload.str();
    }

    void AppendPlayerTargetSnapshotFieldsJson(std::ostringstream& out, Player& player, std::string const& requestToken)
    {
        std::string const payload = BuildPlayerTargetPayload(player, requestToken);
        out << "\"schema\":\"player.target-panel.v1\""
            << ",\"source\":\"mod-player-attribute-panel-compatible\""
            << ",\"targetPayload\":\"" << EscapeJsonString(payload) << "\"";
    }

    HermesMethodDescriptor const* FindMethodDescriptor(std::string const& method)
    {
        for (HermesMethodDescriptor const& descriptor : HERMES_METHOD_REGISTRY)
        {
            if (method == descriptor.Name)
                return &descriptor;
        }

        return nullptr;
    }

    void SendHermesFrameV2(WorldSession& session, HermesFrameV2 const& requestFrame, uint8 messageType, uint8 codec, std::string const& responsePayload, bool bypassRateLimit = false)
    {
        if (HERMES_BRIDGE_TRACE_PACKETS)
            LOG_INFO("server.loading", "HermesBridge: v2 send begin account={} type={} codec={} methodId={} requestId={} seq={} bytes={}", session.GetAccountId(), messageType, codec, requestFrame.MethodId, requestFrame.RequestId, requestFrame.Sequence, responsePayload.size());

        uint32 const payloadSize = static_cast<uint32>(responsePayload.size());
        if (ShouldSuppressHermesOutboundFrame(session, messageType, requestFrame.MethodId, payloadSize, bypassRateLimit))
        {
            if (ShouldTraceHermesServerMethod(requestFrame.MethodId) && ConsumeHermesDebugBudget(g_HermesDebugSendLogBudget))
                LOG_INFO("server.loading", "HermesBridge DEBUG server send-suppressed account={} type={} methodId={} requestId={} bytes={}", session.GetAccountId(), static_cast<unsigned int>(messageType), requestFrame.MethodId, requestFrame.RequestId, payloadSize);
            TrackHermesOutboundDebugStats(session, requestFrame.Lane, messageType, codec, requestFrame.SchemaId, requestFrame.MethodId, payloadSize, true);
            return;
        }

        TrackHermesOutboundDebugStats(session, requestFrame.Lane, messageType, codec, requestFrame.SchemaId, requestFrame.MethodId, payloadSize, false);

        if (ShouldTraceHermesServerMethod(requestFrame.MethodId) && ConsumeHermesDebugBudget(g_HermesDebugSendLogBudget))
            LOG_INFO("server.loading", "HermesBridge DEBUG server send-frame account={} player={} lane={} type={} codec={} methodId={} requestId={} bytes={} payload={}",
                session.GetAccountId(),
                session.GetPlayer() ? session.GetPlayer()->GetName().c_str() : "<none>",
                static_cast<unsigned int>(requestFrame.Lane),
                static_cast<unsigned int>(messageType),
                static_cast<unsigned int>(codec),
                requestFrame.MethodId,
                requestFrame.RequestId,
                payloadSize,
                HermesLogPreview(responsePayload));

        if (!QueueHermesOutboundFrame(session, requestFrame, messageType, codec, responsePayload))
            return;

        if (!g_HermesOutboundBatchDepth)
            FlushHermesOutboundQueue();

        if (HERMES_BRIDGE_TRACE_PACKETS)
            LOG_INFO("server.loading", "HermesBridge: v2 send queued account={} requestId={}", session.GetAccountId(), requestFrame.RequestId);
    }

    void SendHermesJsonEvent(WorldSession& session, HermesFrameV2 const& requestFrame, char const* eventName, std::string const& payload)
    {
        HermesFrameV2 eventFrame = requestFrame;
        eventFrame.Lane = HERMES_LANE_EVENT;
        eventFrame.MessageType = HERMES_MESSAGE_EVENT;
        eventFrame.Codec = HERMES_CODEC_JSON;
        ++eventFrame.Sequence;

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"method\":\"" << EscapeJsonString(eventName ? eventName : "")
            << "\",\"params\":{\"event\":\"" << EscapeJsonString(eventName ? eventName : "")
            << "\",\"payload\":\"" << EscapeJsonString(payload)
            << "\",\"sourceRequestId\":" << requestFrame.RequestId
            << ",\"serverTime\":" << static_cast<long long>(std::time(nullptr))
            << "}}";

        SendHermesFrameV2(session, eventFrame, HERMES_MESSAGE_EVENT, HERMES_CODEC_JSON, out.str());
    }

    void SendHermesAddonMessageEvent(WorldSession& session, std::string const& prefix, std::string const& payload)
    {
        HermesFrameV2 eventFrame;
        HermesUtf8Check const payloadUtf8 = CheckHermesUtf8(payload);
        eventFrame.Lane = HERMES_LANE_EVENT;
        eventFrame.MessageType = HERMES_MESSAGE_EVENT;
        eventFrame.Codec = HERMES_CODEC_JSON;
        eventFrame.SchemaId = HERMES_SCHEMA_JSON_RPC;
        eventFrame.MethodId = HERMES_METHOD_ADDON_MESSAGE;
        eventFrame.RequestId = 0;
        eventFrame.Sequence = g_HermesServerEventSequence.fetch_add(1, std::memory_order_relaxed);
        eventFrame.PayloadSize = static_cast<uint32>(payload.size());

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"method\":\"hermes.addon.message\""
            << ",\"params\":{\"event\":\"hermes.addon.message\""
            << ",\"schema\":\"hermes.addon.message.v1\""
            << ",\"prefix\":\"" << EscapeJsonString(prefix) << "\""
            << ",\"payload\":\"" << EscapeJsonString(payload) << "\""
            << ",\"payloadBytes\":" << payload.size()
            << ",\"payloadUtf8Ok\":" << (payloadUtf8.Valid ? "true" : "false")
            << ",\"payloadUtf8Repairs\":" << payloadUtf8.Replacements
            << ",\"serverTime\":" << static_cast<long long>(std::time(nullptr))
            << "}}";

        if (ConsumeHermesDebugBudget(g_HermesDebugAddonLogBudget))
            LOG_INFO("server.loading", "HermesBridge DEBUG server addon-event-out account={} player={} prefix={} payloadBytes={} utf8Ok={} invalidOffset={} invalidByte={} repairs={} preview={}",
                session.GetAccountId(),
                session.GetPlayer() ? session.GetPlayer()->GetName().c_str() : "<none>",
                prefix,
                payload.size(),
                payloadUtf8.Valid ? 1 : 0,
                HermesUtf8OffsetText(payloadUtf8),
                HermesUtf8ByteText(payloadUtf8),
                payloadUtf8.Replacements,
                HermesLogPreview(payload));
        if (ConsumeHermesTraceBudget(g_HermesTraceAddonEventOutLogBudget))
            LOG_INFO("server.loading", "HermesBridge TRACE server addon-event-out account={} player={} prefix={} bytes={} utf8Ok={} invalidOffset={} invalidByte={} repairs={} preview={}",
                session.GetAccountId(),
                session.GetPlayer() ? session.GetPlayer()->GetName().c_str() : "<none>",
                prefix,
                payload.size(),
                payloadUtf8.Valid ? 1 : 0,
                HermesUtf8OffsetText(payloadUtf8),
                HermesUtf8ByteText(payloadUtf8),
                payloadUtf8.Replacements,
                HermesLogPreview(payload));

        bool const priorityVitals = IsHermesPriorityVitalsAddonMessage(prefix, payload);
        SendHermesFrameV2(session, eventFrame, HERMES_MESSAGE_EVENT, HERMES_CODEC_JSON, out.str(), priorityVitals);
    }

    void SendHermesBinaryDamageSmokeEvent(WorldSession& session, HermesFrameV2 const& requestFrame, std::string const& marker)
    {
        HermesFrameV2 eventFrame = requestFrame;
        eventFrame.Lane = HERMES_LANE_EVENT;
        eventFrame.MessageType = HERMES_MESSAGE_EVENT;
        eventFrame.Codec = HERMES_CODEC_BINARY;
        eventFrame.SchemaId = HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1;
        ++eventFrame.Sequence;

        std::string safeMarker = marker.substr(0, 128);
        std::string payload;
        payload.reserve(18 + safeMarker.size());
        payload.append("HDMG", 4);
        AppendUInt16LE(payload, 1);
        AppendUInt32LE(payload, requestFrame.RequestId);
        AppendUInt32LE(payload, 123456);
        AppendUInt16LE(payload, 1);
        AppendUInt16LE(payload, static_cast<uint16>(safeMarker.size()));
        payload.append(safeMarker);

        SendHermesFrameV2(session, eventFrame, HERMES_MESSAGE_EVENT, HERMES_CODEC_BINARY, payload);
    }

    void SendHermesBinaryDamageEvent(WorldSession& session, HermesFrameV2 const& requestFrame, Player& player, std::string const& marker)
    {
        HermesFrameV2 eventFrame = requestFrame;
        eventFrame.Lane = HERMES_LANE_EVENT;
        eventFrame.MessageType = HERMES_MESSAGE_EVENT;
        eventFrame.Codec = HERMES_CODEC_BINARY;
        eventFrame.SchemaId = HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1;
        ++eventFrame.Sequence;

        uint32 amount = ToUInt32SaturatedPositive(player.GetExtendedDamageMax(BASE_ATTACK));
        if (amount == 0)
            amount = ToUInt32Saturated(player.GetHealthForCombat256());
        if (amount == 0)
            amount = 1;

        std::string safeMarker = marker.substr(0, 128);
        std::string payload;
        payload.reserve(18 + safeMarker.size());
        payload.append("HDMG", 4);
        AppendUInt16LE(payload, 1);
        AppendUInt32LE(payload, requestFrame.RequestId);
        AppendUInt32LE(payload, amount);
        AppendUInt16LE(payload, 2);
        AppendUInt16LE(payload, static_cast<uint16>(safeMarker.size()));
        payload.append(safeMarker);

        SendHermesFrameV2(session, eventFrame, HERMES_MESSAGE_EVENT, HERMES_CODEC_BINARY, payload);
    }

    void SendHermesBinaryVitalsSmokeEvent(WorldSession& session, HermesFrameV2 const& requestFrame, Player const& player)
    {
        HermesFrameV2 eventFrame = requestFrame;
        eventFrame.Lane = HERMES_LANE_SNAPSHOT;
        eventFrame.MessageType = HERMES_MESSAGE_EVENT;
        eventFrame.Codec = HERMES_CODEC_BINARY;
        eventFrame.SchemaId = HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1;
        ++eventFrame.Sequence;

        Powers const powerType = player.getPowerType();
        uint256 const health = player.GetHealthForCombat256();
        uint256 const maxHealth = player.GetMaxHealthForCombat256();
        uint256 const power = player.GetPowerForCombat256(powerType);
        uint256 const maxPower = player.GetMaxPowerForCombat256(powerType);
        uint16 flags = 0;

        if (player.IsAlive())
            flags |= HERMES_VITALS_FLAG_ALIVE;
        if (player.IsInCombat())
            flags |= HERMES_VITALS_FLAG_IN_COMBAT;
        if (IsUInt32Exact(health))
            flags |= HERMES_VITALS_FLAG_HEALTH_EXACT;
        if (IsUInt32Exact(maxHealth))
            flags |= HERMES_VITALS_FLAG_MAX_HEALTH_EXACT;
        if (IsUInt32Exact(power))
            flags |= HERMES_VITALS_FLAG_POWER_EXACT;
        if (IsUInt32Exact(maxPower))
            flags |= HERMES_VITALS_FLAG_MAX_POWER_EXACT;

        std::string payload;
        payload.reserve(42);
        payload.append("HVIT", 4);
        AppendUInt16LE(payload, 1);
        AppendUInt32LE(payload, requestFrame.RequestId);
        AppendUInt32LE(payload, static_cast<uint32>(player.GetGUID().GetCounter()));
        AppendUInt32LE(payload, player.GetMapId());
        AppendUInt32LE(payload, player.GetZoneId());
        AppendUInt32LE(payload, ToUInt32Saturated(health));
        AppendUInt32LE(payload, ToUInt32Saturated(maxHealth));
        AppendUInt32LE(payload, ToUInt32Saturated(power));
        AppendUInt32LE(payload, ToUInt32Saturated(maxPower));
        AppendUInt16LE(payload, static_cast<uint16>(powerType));
        AppendUInt16LE(payload, flags);

        SendHermesFrameV2(session, eventFrame, HERMES_MESSAGE_EVENT, HERMES_CODEC_BINARY, payload);
    }

    void SendHermesBinaryBulkChunkSmokeEvent(WorldSession& session, HermesFrameV2 const& requestFrame, std::string const& marker)
    {
        HermesFrameV2 chunkFrame = requestFrame;
        chunkFrame.Lane = HERMES_LANE_BULK;
        chunkFrame.MessageType = HERMES_MESSAGE_CHUNK;
        chunkFrame.Codec = HERMES_CODEC_BINARY;
        chunkFrame.SchemaId = HERMES_SCHEMA_BULK_CHUNK_V1;
        ++chunkFrame.Sequence;

        std::string safeMarker = marker.substr(0, 96);
        std::string chunkData = "bulk-smoke:" + safeMarker + ":abcdefghijklmnopqrstuvwxyz0123456789";
        if (chunkData.size() > 192)
            chunkData.resize(192);

        std::string payload;
        payload.reserve(24 + safeMarker.size() + chunkData.size());
        payload.append("HBLK", 4);
        AppendUInt16LE(payload, 1);
        AppendUInt32LE(payload, requestFrame.RequestId);
        AppendUInt32LE(payload, requestFrame.RequestId ^ 0x48424C4BU);
        AppendUInt16LE(payload, 0);
        AppendUInt16LE(payload, 1);
        AppendUInt16LE(payload, 1);
        AppendUInt16LE(payload, static_cast<uint16>(safeMarker.size()));
        AppendUInt16LE(payload, static_cast<uint16>(chunkData.size()));
        payload.append(safeMarker);
        payload.append(chunkData);

        SendHermesFrameV2(session, chunkFrame, HERMES_MESSAGE_CHUNK, HERMES_CODEC_BINARY, payload);
    }

    void SendHermesBinaryBulkChunk(WorldSession& session, HermesFrameV2 const& requestFrame, uint32 transferId, uint16 chunkIndex, uint16 chunkCount, uint16 chunkFlags, std::string const& marker, std::string const& chunkData)
    {
        HermesFrameV2 chunkFrame = requestFrame;
        chunkFrame.Lane = HERMES_LANE_BULK;
        chunkFrame.MessageType = HERMES_MESSAGE_CHUNK;
        chunkFrame.Codec = HERMES_CODEC_BINARY;
        chunkFrame.SchemaId = HERMES_SCHEMA_BULK_CHUNK_V1;
        chunkFrame.Sequence = requestFrame.Sequence + chunkIndex + 1;

        std::string safeMarker = marker.substr(0, 96);
        std::string safeChunkData = chunkData.substr(0, 150);

        std::string payload;
        payload.reserve(24 + safeMarker.size() + safeChunkData.size());
        payload.append("HBLK", 4);
        AppendUInt16LE(payload, 1);
        AppendUInt32LE(payload, requestFrame.RequestId);
        AppendUInt32LE(payload, transferId);
        AppendUInt16LE(payload, chunkIndex);
        AppendUInt16LE(payload, chunkCount);
        AppendUInt16LE(payload, chunkFlags);
        AppendUInt16LE(payload, static_cast<uint16>(safeMarker.size()));
        AppendUInt16LE(payload, static_cast<uint16>(safeChunkData.size()));
        payload.append(safeMarker);
        payload.append(safeChunkData);

        SendHermesFrameV2(session, chunkFrame, HERMES_MESSAGE_CHUNK, HERMES_CODEC_BINARY, payload);
    }

    void SendHermesBinarySnapshotBulkEvent(WorldSession& session, HermesFrameV2 const& requestFrame, Player& player, std::string const& marker)
    {
        HermesAttributeFields attributes;
        BuildPlayerAttributeFields(player, attributes);

        std::string const playerId = std::to_string(player.GetGUID().GetCounter());
        std::string const healthText = PanelValueText(player.GetHealthForCombat256());
        std::string const maxHealthText = PanelValueText(player.GetMaxHealthForCombat256());
        std::string const mainhandText = BuildPanelRangePayload(player.GetExtendedDamageMin(BASE_ATTACK), player.GetExtendedDamageMax(BASE_ATTACK));

        std::vector<std::string> chunks;
        chunks.emplace_back("player=" + playerId + "|");
        chunks.emplace_back("player=" + playerId + "|attributes=" + std::to_string(attributes.size()) + "|health=" + healthText + "/" + maxHealthText + "|mainhand=" + mainhandText);

        uint32 const transferId = requestFrame.RequestId ^ 0x48424C4BU;
        uint16 const chunkCount = static_cast<uint16>(chunks.size());
        for (uint16 i = 0; i < chunkCount; ++i)
            SendHermesBinaryBulkChunk(session, requestFrame, transferId, i, chunkCount, i + 1 == chunkCount ? 1 : 0, marker, chunks[i]);
    }

    bool ReadPayload(WorldPacket& recvPacket, uint32 payloadSize, std::string& payload)
    {
        payload.resize(payloadSize);
        if (payloadSize)
            recvPacket.read(reinterpret_cast<uint8*>(&payload[0]), payloadSize);

        return true;
    }

    JsonRpcDispatchResult HandleHermesHello(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;
        (void)method;

        Player* player = session.GetPlayer();
        if (HERMES_BRIDGE_TRACE_PACKETS)
            LOG_INFO("server.loading", "HermesBridge: hello handshake account={} playerReady={} player={}", session.GetAccountId(), player ? 1 : 0, player ? player->GetName() : "");

        JsonRpcDispatchResult result;
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"server\":\"azerothcore\",\"bridge\":\"mod-hermes-bridge\",\"frameVersion\":2,\"headerSize\":24,\"maxPayloadSize\":"
            << HERMES_BRIDGE_MAX_PAYLOAD_SIZE
            << ",\"integerTextEncoding\":\"decimal-string\",\"integerTextBits\":" << std::numeric_limits<uint256>::digits
            << ",\"jsonSafeIntegerMax\":" << HERMES_JSON_SAFE_UINT_MAX
            << ",\"playerReady\":" << (player ? "true" : "false");
        if (player)
        {
            out << ",\"player\":{";
            AppendPlayerBasicFieldsJson(out, session, *player);
            out << "}";
        }
        out
            << ",\"lanes\":[\"control\",\"rpc\",\"event\",\"snapshot\",\"bulk\",\"debug\"],"
            << "\"codecs\":[\"json\",\"binary\"],";
        AppendMethodRegistryJson(out);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandleHermesPing(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        JsonRpcDispatchResult result;
        std::string const payload = ExtractPayloadParam(request);
        if (HermesBridge_IsLifecycleResetTrace(payload))
            ClearHermesAddonReady(session, "lifecycle-reset");

        if (IsHermesTracePayload(payload) && ConsumeHermesTraceBudget(g_HermesTracePingLogBudget))
        {
            Player* player = session.GetPlayer();
            LOG_INFO("server.loading", "HermesBridge TRACE server ping account={} player={} requestId={} payload={}",
                session.GetAccountId(),
                player ? player->GetName() : "<none>",
                frame.RequestId,
                HermesLogPreview(payload, 240));
        }
        if (HERMES_BRIDGE_DEBUG_COMM && payload.rfind("addon-compat ", 0) == 0)
        {
            Player* player = session.GetPlayer();
            LOG_INFO("server.loading", "HermesBridge DEBUG server {} account={} player={}", payload, session.GetAccountId(), player ? player->GetName() : "");
        }

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"pong\":true,\"payload\":\"" + EscapeJsonString(payload) + "\"}}";
        return result;
    }

    JsonRpcDispatchResult HandleHermesDebugEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)method;

        JsonRpcDispatchResult result;
        std::string const payload = ExtractPayloadParam(request);
        SendHermesJsonEvent(session, frame, "hermes.debugEvent", payload);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.debugEvent\",\"payload\":\"" + EscapeJsonString(payload) + "\"}}";
        return result;
    }

    JsonRpcDispatchResult HandleHermesGetSchemaRegistry(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)session;
        (void)frame;
        (void)request;
        (void)method;

        JsonRpcDispatchResult result;
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"schemaVersion\":1,\"defaultSchemaId\":" << HERMES_SCHEMA_JSON_RPC
            << ",\"binarySchemasReady\":true,\"binaryEventSmokeReady\":true,\"binarySnapshotSmokeReady\":true,\"bulkChunkSmokeReady\":true,";
        AppendSchemaRegistryJson(out);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandleHermesGetMethodRegistry(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)session;
        (void)frame;
        (void)request;
        (void)method;

        JsonRpcDispatchResult result;
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"registryVersion\":1,";
        AppendMethodRegistryDetailJson(out);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandleHermesDebugBinaryEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)method;

        JsonRpcDispatchResult result;
        std::string const payload = ExtractPayloadParam(request);
        SendHermesBinaryDamageSmokeEvent(session, frame, payload);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.binaryEvent\",\"schemaId\":" + std::to_string(HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1) +
            ",\"payload\":\"" + EscapeJsonString(payload) + "\"}}";
        return result;
    }

    JsonRpcDispatchResult HandleHermesDebugVitalsBinaryEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        SendHermesBinaryVitalsSmokeEvent(session, frame, *player);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.binaryEvent\",\"schemaId\":" + std::to_string(HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1) +
            ",\"schemaName\":\"unit.vitals.snapshot.v1\"}}";
        return result;
    }

    JsonRpcDispatchResult HandleHermesDebugBulkChunkEmit(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)method;

        JsonRpcDispatchResult result;
        std::string const payload = ExtractPayloadParam(request);
        SendHermesBinaryBulkChunkSmokeEvent(session, frame, payload);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.binaryEvent\",\"schemaId\":" + std::to_string(HERMES_SCHEMA_BULK_CHUNK_V1) +
            ",\"schemaName\":\"bulk.chunk.v1\"}}";
        return result;
    }

    JsonRpcDispatchResult HandleAddonDispatch(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::string const dispatchPayload = ExtractPayloadParam(request);
        std::size_t const separator = dispatchPayload.find('\t');
        if (separator == std::string::npos || separator == 0)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32602, "Invalid addon payload", "INVALID_ADDON_PAYLOAD", method);
            return result;
        }

        std::string const prefix = dispatchPayload.substr(0, separator);
        std::string const addonPayload = dispatchPayload.substr(separator + 1);
        if (ConsumeHermesDebugBudget(g_HermesDebugAddonLogBudget))
            LOG_INFO("server.loading", "HermesBridge DEBUG server addon-dispatch-in account={} player={} requestId={} prefix={} payloadBytes={} preview={}",
                session.GetAccountId(),
                player->GetName().c_str(),
                frame.RequestId,
                prefix,
                addonPayload.size(),
                HermesLogPreview(addonPayload));
        if (ConsumeHermesTraceBudget(g_HermesTraceAddonDispatchLogBudget))
            LOG_INFO("server.loading", "HermesBridge TRACE server addon-dispatch-in account={} player={} requestId={} prefix={} bytes={} preview={}",
                session.GetAccountId(),
                player->GetName().c_str(),
                frame.RequestId,
                prefix,
                addonPayload.size(),
                HermesLogPreview(addonPayload));
        if (prefix.size() > 32 || dispatchPayload.size() > 255)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32602, "Addon payload too large", "ADDON_PAYLOAD_TOO_LARGE", method);
            return result;
        }
        if (HermesBridge_ShouldMarkAddonPrefixReadyForMethod(frame.MethodId))
            MarkHermesAddonPrefixReady(session, prefix, "addon-dispatch");

        uint64 const legacySmsgBefore = g_HermesAddonTakeoverBlockedLegacySmsg.load(std::memory_order_relaxed);
        std::string const targetName = player->GetName();
        WorldPacket legacyAddonWhisper(CMSG_MESSAGECHAT, dispatchPayload.size() + targetName.size() + 16);
        legacyAddonWhisper << uint32(CHAT_MSG_WHISPER);
        legacyAddonWhisper << uint32(LANG_ADDON);
        legacyAddonWhisper << targetName;
        legacyAddonWhisper << dispatchPayload;
        session.HandleMessagechatOpcode(legacyAddonWhisper);
        uint64 const legacySmsgAfter = g_HermesAddonTakeoverBlockedLegacySmsg.load(std::memory_order_relaxed);
        uint64 const legacyEvents = legacySmsgAfter >= legacySmsgBefore ? legacySmsgAfter - legacySmsgBefore : 0;

        if (ConsumeHermesDebugBudget(g_HermesDebugAddonLogBudget))
            LOG_INFO("server.loading", "HermesBridge DEBUG server addon-dispatch-out account={} player={} requestId={} prefix={} legacyEvents={}",
                session.GetAccountId(),
                player->GetName().c_str(),
                frame.RequestId,
                prefix,
                legacyEvents);
        if (ConsumeHermesTraceBudget(g_HermesTraceAddonDispatchLogBudget))
            LOG_INFO("server.loading", "HermesBridge TRACE server addon-dispatch-out account={} player={} requestId={} prefix={} legacyEvents={}",
                session.GetAccountId(),
                player->GetName().c_str(),
                frame.RequestId,
                prefix,
                legacyEvents);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"schema\":\"hermes.addon.dispatch.v1\",\"prefix\":\"" + EscapeJsonString(prefix) +
            "\",\"payloadBytes\":" + std::to_string(addonPayload.size()) +
            ",\"dispatched\":true}}";
        return result;
    }

    JsonRpcDispatchResult HandleMallGetCategories(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;
        (void)method;

        if (!session.GetPlayer())
            return HermesPayloadResult(idText, "mall.categories.v1", "ERROR|玩家未就绪");

        if (!sMallSystem->IsMallSystemEnable())
            return HermesPayloadResult(idText, "mall.categories.v1", "ERROR|商城系统当前已禁用");

        return HermesPayloadResult(idText, "mall.categories.v1", BuildHermesMallCategoriesPayload());
    }

    JsonRpcDispatchResult HandleMallGetItems(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        if (!session.GetPlayer())
            return HermesPayloadResult(idText, "mall.items.v1", "ERROR|玩家未就绪");

        if (!sMallSystem->IsMallSystemEnable())
            return HermesPayloadResult(idText, "mall.items.v1", "ERROR|商城系统当前已禁用");

        uint32 const categoryId = ParseHermesUInt(ExtractPayloadParam(request), 0);
        return HermesPayloadResult(idText, "mall.items.v1", BuildHermesMallItemsPayload(categoryId));
    }

    JsonRpcDispatchResult HandleMallPurchase(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        Player* player = session.GetPlayer();
        if (!player)
            return HermesPayloadResult(idText, "mall.purchase.v1", BuildHermesMallStatusPayload(false, "玩家未就绪"));

        if (!sMallSystem->IsMallSystemEnable())
            return HermesPayloadResult(idText, "mall.purchase.v1", BuildHermesMallStatusPayload(false, "商城系统当前已禁用"));

        EnsureHermesMallDataLoaded();

        uint32 const itemId = ParseHermesUInt(ExtractPayloadParam(request), 0);
        if (itemId == 0)
            return HermesPayloadResult(idText, "mall.purchase.v1", BuildHermesMallStatusPayload(false, "商品ID无效"));

        MallSystem::PurchaseResult purchase = sMallSystem->PurchaseItem(player, itemId);
        bool const success = purchase.status == MallSystem::PurchaseStatus::SUCCESS;
        return HermesPayloadResult(idText, "mall.purchase.v1", BuildHermesMallStatusPayload(success, purchase.message));
    }

    JsonRpcDispatchResult HandleZodiacGetState(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;
        (void)method;

        return HermesPayloadResult(idText, "zodiac.state.v1", TwelveZodiacHermesGetState(session.GetPlayer()));
    }

    JsonRpcDispatchResult HandleZodiacGetDetail(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        uint32 const zodiacId = ParseHermesUInt(ExtractPayloadParam(request), 0);
        return HermesPayloadResult(idText, "zodiac.detail.v1", TwelveZodiacHermesGetDetail(session.GetPlayer(), zodiacId));
    }

    JsonRpcDispatchResult HandleZodiacEquip(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        std::vector<std::string> const fields = SplitHermesPayloadFields(ExtractPayloadParam(request), '|');
        uint32 const zodiacId = fields.size() > 0 ? ParseHermesUInt(fields[0], 0) : 0;
        uint32 const slot = fields.size() > 1 ? ParseHermesUInt(fields[1], std::numeric_limits<uint32>::max()) : std::numeric_limits<uint32>::max();
        return HermesPayloadResult(idText, "zodiac.equip.v1", TwelveZodiacHermesEquip(session.GetPlayer(), zodiacId, slot));
    }

    JsonRpcDispatchResult HandleZodiacUnequip(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        uint32 const slot = ParseHermesUInt(ExtractPayloadParam(request), std::numeric_limits<uint32>::max());
        return HermesPayloadResult(idText, "zodiac.unequip.v1", TwelveZodiacHermesUnequip(session.GetPlayer(), slot));
    }

    JsonRpcDispatchResult HandleTooltipQuery(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        Player* player = session.GetPlayer();
        if (!player)
            return HermesPayloadResult(idText, "tooltip.query.v1", "");

        std::string const rawPayload = ExtractPayloadParam(request);
        std::vector<std::string> fields = SplitHermesPayloadFields(rawPayload, '|');
        uint32 itemID = fields.size() > 0 ? ParseHermesUInt(fields[0], 0) : 0;
        uint32 guid = fields.size() > 1 ? ParseHermesUInt(fields[1], 0) : 0;
        int32 bag = fields.size() > 2 ? ParseHermesInt(fields[2], 255) : 255;
        uint8 slot = static_cast<uint8>(fields.size() > 3 ? ParseHermesUInt(fields[3], 0) : 0);

        if (itemID == 0)
            return HermesPayloadResult(idText, "tooltip.query.v1", "");

        if (guid == 0 && fields.size() > 3)
        {
            if (Item* item = ResolveHermesTooltipItemByClientPosition(player, bag, slot, itemID))
                guid = item->GetGUID().GetCounter();
        }

        std::string payload = BuildHermesTooltipAllModulePayload(itemID, guid, bag, slot);
        return HermesPayloadResult(idText, "tooltip.query.v1", payload);
    }

    JsonRpcDispatchResult HandleTooltipQueryTemplate(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        std::string const rawPayload = ExtractPayloadParam(request);
        uint32 itemID = ParseHermesUInt(rawPayload, 0);
        std::string payload = BuildHermesTooltipTemplatePayload(itemID);
        return HermesPayloadResult(idText, "tooltip.template.v1", payload);
    }

    JsonRpcDispatchResult HandleTooltipInspectItemGuid(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        Player* requester = session.GetPlayer();
        if (!requester)
            return HermesPayloadResult(idText, "tooltip.inspect-guid.v1", "");

        std::string const rawPayload = ExtractPayloadParam(request);
        std::vector<std::string> fields = SplitHermesPayloadFields(rawPayload, '|');
        std::string targetPlayerName = fields.size() > 0 ? fields[0] : "";
        uint8 slot = static_cast<uint8>(fields.size() > 1 ? ParseHermesUInt(fields[1], 0) : 0);
        uint32 itemID = fields.size() > 2 ? ParseHermesUInt(fields[2], 0) : 0;
        uint32 realGuid = 0;
        Player* target = nullptr;
        bool canInspect = false;

        if (itemID != 0)
        {
            target = ResolveHermesTooltipInspectTarget(requester, targetPlayerName, slot, itemID);
            if (target)
            {
                canInspect = CanHermesTooltipInspect(requester, target);
                if (canInspect)
                    realGuid = ResolveHermesTooltipInspectItemGuid(target, slot, itemID);
            }
        }

        std::string responseName = targetPlayerName.empty() ? "-" : targetPlayerName;
        std::ostringstream payload;
        payload << "INSPECT_ITEM_GUID_RESPONSE:" << responseName << ":"
                << static_cast<uint32>(slot) << ":" << itemID << ":" << realGuid;
        return HermesPayloadResult(idText, "tooltip.inspect-guid.v1", payload.str());
    }

    JsonRpcDispatchResult HandleTooltipListPending(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;
        (void)method;

        Player* player = session.GetPlayer();
        std::string payload = BuildHermesTooltipPendingListPayload(player);
        return HermesPayloadResult(idText, "tooltip.pending-list.v1", payload);
    }

    JsonRpcDispatchResult HandleServerGetStatus(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;
        (void)method;

        JsonRpcDispatchResult result;
        std::string const requestToken = ExtractPayloadParam(request);
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"server\":\"azerothcore\",\"bridge\":\"mod-hermes-bridge\",\"frameVersion\":2,\"headerSize\":24,\"maxPayloadSize\":"
            << HERMES_BRIDGE_MAX_PAYLOAD_SIZE
            << ",\"accountId\":" << session.GetAccountId()
            << ",\"requestToken\":\"" << EscapeJsonString(requestToken) << "\""
            << ",\"serverTime\":" << static_cast<long long>(std::time(nullptr))
            << ",\"addonTakeoverBlockedLegacySmsg\":" << g_HermesAddonTakeoverBlockedLegacySmsg.load(std::memory_order_relaxed)
            << ",";
        AppendHermesOutboundQueueJson(out);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandleServerGetNumericLimits(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)session;
        (void)frame;
        (void)request;
        (void)method;

        std::string const maxUnsignedText = BigUIntToText(std::numeric_limits<uint256>::max());
        std::string const maxSignedText = BigIntToText(std::numeric_limits<int256>::max());
        std::string const minSignedText = BigIntToText(std::numeric_limits<int256>::min());
        JsonRpcDispatchResult result;
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"integerTextEncoding\":\"decimal-string\""
            << ",\"integerTextBits\":" << std::numeric_limits<uint256>::digits
            << ",\"signedIntegerValueBits\":" << std::numeric_limits<int256>::digits
            << ",\"jsonSafeIntegerMax\":" << HERMES_JSON_SAFE_UINT_MAX
            << ",\"maxUnsignedDigits\":" << maxUnsignedText.size()
            << ",\"maxUnsignedHead\":\"" << HeadText(maxUnsignedText, 16) << "\""
            << ",\"maxUnsignedTail\":\"" << TailText(maxUnsignedText, 16) << "\""
            << ",\"maxSignedDigits\":" << maxSignedText.size()
            << ",\"maxSignedHead\":\"" << HeadText(maxSignedText, 16) << "\""
            << ",\"maxSignedTail\":\"" << TailText(maxSignedText, 16) << "\""
            << ",\"minSignedDigits\":" << minSignedText.size()
            << ",\"minSignedHead\":\"" << HeadText(minSignedText, 17) << "\""
            << ",\"minSignedTail\":\"" << TailText(minSignedText, 16) << "\""
            << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerGetBasicInfo(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{";
        AppendPlayerBasicFieldsJson(out, session, *player);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerGetPosition(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{";
        AppendPlayerPositionFieldsJson(out, *player);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerGetVitals(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{";
        AppendPlayerVitalsFieldsJson(out, *player);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerGetSnapshot(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"basic\":{";
        AppendPlayerBasicFieldsJson(out, session, *player);
        out << "},\"position\":{";
        AppendPlayerPositionFieldsJson(out, *player);
        out << "},\"vitals\":{";
        AppendPlayerVitalsFieldsJson(out, *player);
        out << "},\"attributes\":{";
        AppendPlayerAttributesFieldsJson(out, *player);
        out << "}";
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerGetAttributes(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{";
        AppendPlayerAttributesFieldsJson(out, *player);
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerGetTargetSnapshot(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{";
        AppendPlayerTargetSnapshotFieldsJson(out, *player, ExtractPayloadParam(request));
        out << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandlePlayerEmitDamageEvent(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::string const marker = ExtractPayloadParam(request);
        SendHermesBinaryDamageEvent(session, frame, *player, marker);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.binaryEvent\",\"schemaId\":" + std::to_string(HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1) +
            ",\"schemaName\":\"custom.damage.v1\",\"source\":\"player.current\"}}";
        return result;
    }

    JsonRpcDispatchResult HandlePlayerEmitVitalsBinary(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)request;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        SendHermesBinaryVitalsSmokeEvent(session, frame, *player);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.binaryEvent\",\"schemaId\":" + std::to_string(HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1) +
            ",\"schemaName\":\"unit.vitals.snapshot.v1\",\"source\":\"player.current\"}}";
        return result;
    }

    JsonRpcDispatchResult HandlePlayerEmitSnapshotBulk(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        if (!player)
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::string const marker = ExtractPayloadParam(request);
        SendHermesBinarySnapshotBulkEvent(session, frame, *player, marker);

        result.Payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + idText +
            ",\"result\":{\"emitted\":true,\"event\":\"hermes.binaryEvent\",\"schemaId\":" + std::to_string(HERMES_SCHEMA_BULK_CHUNK_V1) +
            ",\"schemaName\":\"bulk.chunk.v1\",\"chunkCount\":2,\"source\":\"player.snapshot\"}}";
        return result;
    }

    JsonRpcDispatchResult HandleUiGetDashboard(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;
        (void)method;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"schema\":\"hermes.ui.dashboard.v1\""
            << ",\"bridge\":{\"name\":\"mod-hermes-bridge\",\"frameVersion\":2,\"protocolVersion\":2,\"maxPayloadSize\":" << HERMES_BRIDGE_MAX_PAYLOAD_SIZE << "}"
            << ",\"server\":{\"name\":\"azerothcore\",\"accountId\":" << session.GetAccountId()
            << ",\"serverTime\":" << static_cast<long long>(std::time(nullptr)) << "}"
            << ",\"playerReady\":" << (player ? "true" : "false");

        if (player)
        {
            out << ",\"player\":{";
            AppendPlayerBasicFieldsJson(out, session, *player);
            out << "}";
        }

        // 【审计修复】持锁快照统计值后再拼接 JSON
        uint32 outboundDepth = 0;
        uint32 outboundHighWatermark = 0;
        uint64 outboundDropped = 0;
        {
            std::lock_guard<std::mutex> stateGuard(g_HermesStateMutex);
            outboundDepth = g_HermesOutboundQueueStats.Depth;
            outboundHighWatermark = g_HermesOutboundQueueStats.HighWatermark;
            outboundDropped = g_HermesOutboundQueueStats.Dropped;
        }

        out << ",\"modules\":[{\"name\":\"PlayerAttributePanel\",\"status\":\"active\",\"methods\":[\"ui.getModuleStatus\",\"player.getAttributes\",\"player.getTargetSnapshot\",\"player.getVitals\",\"player.getSnapshot\"],\"stateSections\":[\"attributes\",\"vitals\",\"basic\",\"position\"]}]"
            << ",\"queues\":{\"pendingLimit\":128,\"streamQueueLimit\":512,\"nativeSendQueue\":256,\"nativeRecvQueue\":8192"
            << ",\"serverOutboundCapacity\":" << HERMES_SERVER_OUTBOUND_QUEUE_CAPACITY
            << ",\"serverOutboundDepth\":" << outboundDepth
            << ",\"serverOutboundHighWatermark\":" << outboundHighWatermark
            << ",\"serverOutboundDropped\":" << outboundDropped
            << "}"
            << ",\"features\":{\"rpc\":true,\"stateCache\":true,\"binaryEvents\":true,\"bulkChunks\":true,\"debugPanel\":true}"
            << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandleUiGetModuleStatus(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        JsonRpcDispatchResult result;
        Player* player = session.GetPlayer();
        Unit* target = player ? player->GetSelectedUnit() : nullptr;
        std::string const requestToken = ExtractPayloadParam(request);

        std::ostringstream out;
        out << "{\"jsonrpc\":\"2.0\",\"id\":" << idText
            << ",\"result\":{\"schema\":\"hermes.ui.module-status.v1\""
            << ",\"module\":\"PlayerAttributePanel\""
            << ",\"status\":\"active\""
            << ",\"source\":\"mod-hermes-bridge\""
            << ",\"requestToken\":\"" << EscapeJsonString(requestToken) << "\""
            << ",\"serverTime\":" << static_cast<long long>(std::time(nullptr))
            << ",\"playerReady\":" << (player ? "true" : "false")
            << ",\"targetSelected\":" << (target ? "true" : "false");

        if (player)
        {
            out << ",\"player\":{";
            AppendPlayerBasicFieldsJson(out, session, *player);
            out << "}";
        }

        out << ",\"methods\":[\"player.getAttributes\",\"player.getTargetSnapshot\",\"player.getVitals\",\"player.getSnapshot\"]"
            << ",\"stateSections\":[\"attributes\",\"vitals\",\"basic\",\"position\"]"
            << ",\"actions\":["
            << "{\"name\":\"openPanel\",\"clientCommand\":\"/pattr\",\"clientFunction\":\"SlashCmdList.PLAYERATTRIBUTEPANEL\"},"
            << "{\"name\":\"refreshSnapshot\",\"method\":\"player.getSnapshot\"},"
            << "{\"name\":\"refreshAttributes\",\"method\":\"player.getAttributes\"},"
            << "{\"name\":\"refreshTarget\",\"method\":\"player.getTargetSnapshot\"}"
            << "]"
            << ",\"helpers\":[\"HermesDLL.Request\",\"HermesDLL.GetState\",\"HermesDLL.BindState\",\"HermesDLL.RefreshSnapshot\"]"
            << "}}";
        result.Payload = out.str();
        return result;
    }

    JsonRpcDispatchResult HandleAbyssGetEquipmentPage(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)method;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<std::string> fields = SplitHermesPayloadFields(ExtractPayloadParam(request), '|');
        uint32 page = fields.size() > 0 ? ParseHermesUInt(fields[0], 1) : 1;
        uint32 pageSize = fields.size() > 1 ? ParseHermesUInt(fields[1], 24) : 24;
        uint32 filterType = fields.size() > 2 ? ParseHermesUInt(fields[2], 0) : 0;
        uint32 filterMode = fields.size() > 3 ? ParseHermesUInt(fields[3], 0) : 0;
        uint32 filterChapter = fields.size() > 4 ? ParseHermesUInt(fields[4], 0) : 0;
        uint32 filterSlot = fields.size() > 5 ? ParseHermesUInt(fields[5], 0) : 0;
        bool ownedOnly = fields.size() > 6 && ParseHermesUInt(fields[6], 0) != 0;

        page = std::max<uint32>(1, page);
        pageSize = std::max<uint32>(1, std::min<uint32>(60, pageSize));

        std::unordered_map<uint16, HermesAbyssChapter> chapters = LoadHermesAbyssChapters();
        std::vector<HermesAbyssEquipment> equipmentRows = LoadHermesAbyssEquipment();
        std::vector<HermesAbyssEquipment const*> filtered;
        filtered.reserve(equipmentRows.size());

        for (HermesAbyssEquipment const& equipment : equipmentRows)
        {
            if (filterType != 0 && equipment.EquipmentType != filterType)
                continue;
            if (filterMode != 0 && equipment.SourceMode != filterMode)
                continue;
            if (filterChapter != 0 && equipment.SourceChapter != filterChapter)
                continue;
            if (filterSlot != 0 && (equipment.SlotMask & filterSlot) == 0)
                continue;
            if (ownedOnly && !player->HasItemCount(equipment.ItemId, 1, true))
                continue;

            filtered.push_back(&equipment);
        }

        std::sort(filtered.begin(), filtered.end(), [](HermesAbyssEquipment const* left, HermesAbyssEquipment const* right)
        {
            if (left->SourceChapter != right->SourceChapter)
                return left->SourceChapter < right->SourceChapter;
            if (left->ActId != right->ActId)
                return left->ActId < right->ActId;
            if (left->SourceMode != right->SourceMode)
                return left->SourceMode < right->SourceMode;
            if (left->EquipmentType != right->EquipmentType)
                return left->EquipmentType > right->EquipmentType;
            if (left->SlotMask != right->SlotMask)
                return left->SlotMask < right->SlotMask;
            return left->ItemId < right->ItemId;
        });

        uint32 totalCount = static_cast<uint32>(filtered.size());
        uint32 pageCount = std::max<uint32>(1, (totalCount + pageSize - 1) / pageSize);
        page = std::min<uint32>(page, pageCount);
        uint32 start = totalCount == 0 ? 0 : (page - 1) * pageSize;
        uint32 end = std::min<uint32>(totalCount, start + pageSize);

        std::ostringstream payload;
        payload << "EQUIPMENT_PAGE:" << page << '|'
                << pageSize << '|'
                << totalCount << '|'
                << pageCount;

        for (uint32 index = start; index < end; ++index)
        {
            HermesAbyssEquipment const* equipment = filtered[index];
            payload << '~'
                    << equipment->ItemId << '^'
                    << SanitizeAddonPayloadText(equipment->ItemName) << '^'
                    << static_cast<uint32>(equipment->EquipmentType) << '^'
                    << equipment->SourceChapter << '^'
                    << GetHermesAbyssChapterName(chapters, equipment->SourceChapter) << '^'
                    << static_cast<uint32>(equipment->SourceMode) << '^'
                    << equipment->SlotMask << '^'
                    << static_cast<uint32>(equipment->ActId) << '^'
                    << equipment->BaseItemLevel << '^'
                    << (equipment->FromCacheBoss ? 1 : 0) << '^'
                    << (equipment->RequiresFragments ? 1 : 0) << '^'
                    << (player->HasItemCount(equipment->ItemId, 1, true) ? 1 : 0) << '^'
                    << SanitizeAddonPayloadText(equipment->FlavorText) << '^'
                    << SanitizeAddonPayloadText(GetHermesItemIconPathForAddon(equipment->ItemId));
        }

        return HermesPayloadResult(idText, "abyss.equipment-page.v1", payload.str());
    }

    JsonRpcDispatchResult HandleAbyssGetRelics(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        uint32 guidLow = player->GetGUID().GetCounter();
        HermesAbyssPlayerRelicState state = LoadHermesAbyssPlayerRelicState(guidLow);
        std::vector<uint32> collected = LoadHermesAbyssPlayerCollectedRelics(guidLow);
        std::vector<HermesAbyssRelic> relics = LoadHermesAbyssRelics();

        std::ostringstream payload;
        payload << "RELICS:";
        bool first = true;
        for (HermesAbyssRelic const& relic : relics)
        {
            if (!first)
                payload << '~';
            first = false;

            bool const owned = std::find(collected.begin(), collected.end(), relic.ItemId) != collected.end();
            uint8 const activeSlot = owned ? GetHermesAbyssActiveRelicSlot(state, relic.ItemId) : 0;

            payload << relic.ItemId << '^'
                    << SanitizeAddonPayloadText(relic.Name) << '^'
                    << static_cast<uint32>(relic.RelicType) << '^'
                    << relic.RelatedChapterId << '^'
                    << static_cast<uint32>(relic.ActId) << '^'
                    << (owned ? 1 : 0) << '^'
                    << static_cast<uint32>(relic.ActiveRule) << '^'
                    << relic.ExclusiveGroup << '^'
                    << static_cast<uint32>(relic.RecommendedSlot) << '^'
                    << relic.SubSlotScale << '^'
                    << static_cast<uint32>(activeSlot) << '^'
                    << SanitizeAddonPayloadText(relic.BriefDescription) << '^'
                    << SanitizeAddonPayloadText(GetHermesItemIconPathForAddon(relic.ItemId));
        }

        return HermesPayloadResult(idText, "abyss.relics.v1", payload.str());
    }

    JsonRpcDispatchResult HandleCultivationGetAll(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<HermesCultivationRealm> realms = LoadHermesCultivationRealms();
        std::vector<HermesCultivationSkill> skills = LoadHermesCultivationSkills();

        std::string payload = BuildHermesCultivationRealmsPayload(realms);
        payload += "\n";
        payload += BuildHermesCultivationSkillsPayload(skills);
        payload += "\n";
        payload += BuildHermesCultivationStatePayload(player->GetGUID().GetCounter(), realms, skills);

        return HermesPayloadResult(idText, "cultivation.all.v1", payload);
    }

    JsonRpcDispatchResult HandleCultivationGetState(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<HermesCultivationRealm> realms = LoadHermesCultivationRealms();
        std::vector<HermesCultivationSkill> skills = LoadHermesCultivationSkills();
        std::string payload = BuildHermesCultivationStatePayload(player->GetGUID().GetCounter(), realms, skills);

        return HermesPayloadResult(idText, "cultivation.state.v1", payload);
    }

    JsonRpcDispatchResult HandleBoundaryGetAll(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "boundary.all.v1", BuildHermesBoundaryAllPayload(*player));
    }

    JsonRpcDispatchResult HandleBoundaryGetDetail(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<std::string> fields = SplitHermesPayloadFields(ExtractPayloadParam(request), '|');
        uint32 type = fields.size() > 0 ? ParseHermesUInt(fields[0], 0) : 0;
        if (type < 1 || type > 6)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32602, "Invalid boundary type", "INVALID_BOUNDARY_TYPE", method);
            return result;
        }

        uint32 requestedLevel = std::numeric_limits<uint32>::max();
        if (fields.size() > 1 && !fields[1].empty())
            requestedLevel = ParseHermesUInt(fields[1], requestedLevel);

        return HermesPayloadResult(idText, "boundary.detail.v1", BuildHermesBoundaryDetailPayload(*player, static_cast<uint8>(type), requestedLevel));
    }

    JsonRpcDispatchResult HandleBreakthroughGetAll(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        uint32 page = ParseHermesUInt(ExtractPayloadParam(request), 1);
        std::ostringstream payload;
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughSystemDataPayload(*player));
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughInfoPayload(*player));
        AppendBreakthroughPayloadLine(payload, BuildHermesBreakthroughSkillsPayload(*player, page));

        return HermesPayloadResult(idText, "breakthrough.all.v1", payload.str());
    }

    JsonRpcDispatchResult HandleBreakthroughGetSystemData(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "breakthrough.system-data.v1", BuildHermesBreakthroughSystemDataPayload(*player));
    }

    JsonRpcDispatchResult HandleBreakthroughGetInfo(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "breakthrough.info.v1", BuildHermesBreakthroughInfoPayload(*player));
    }

    JsonRpcDispatchResult HandleBreakthroughGetSkills(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        uint32 page = ParseHermesUInt(ExtractPayloadParam(request), 1);
        return HermesPayloadResult(idText, "breakthrough.skills.v1", BuildHermesBreakthroughSkillsPayload(*player, page));
    }

    JsonRpcDispatchResult HandleBreakthroughGetSkillDetail(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        uint32 skillNumber = ParseHermesUInt(ExtractPayloadParam(request), 0);
        return HermesPayloadResult(idText, "breakthrough.skill-detail.v1", BuildHermesBreakthroughSkillDetailPayload(*player, skillNumber));
    }

    JsonRpcDispatchResult HandleBreakthroughGetLeaderboard(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "breakthrough.leaderboard.v1", BuildHermesBreakthroughLeaderboardPayload(*player));
    }

    JsonRpcDispatchResult HandleBreakthroughGetExpSources(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "breakthrough.exp-sources.v1", BuildHermesBreakthroughExpSourcesPayload(*player));
    }

    JsonRpcDispatchResult HandleBreakthroughUpgrade(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "breakthrough.upgrade.v1", ExecuteHermesBreakthroughUpgrade(*player));
    }

    JsonRpcDispatchResult HandleBreakthroughLearnSkill(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        uint32 gameSkillId = ParseHermesUInt(ExtractPayloadParam(request), 0);
        return HermesPayloadResult(idText, "breakthrough.learn-skill.v1", ExecuteHermesBreakthroughLearnSkill(*player, gameSkillId, false));
    }

    JsonRpcDispatchResult HandleBreakthroughUpgradeSkill(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        uint32 gameSkillId = ParseHermesUInt(ExtractPayloadParam(request), 0);
        return HermesPayloadResult(idText, "breakthrough.upgrade-skill.v1", ExecuteHermesBreakthroughLearnSkill(*player, gameSkillId, true));
    }

    JsonRpcDispatchResult HandleBreakthroughResetSkills(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;
        (void)request;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        return HermesPayloadResult(idText, "breakthrough.reset-skills.v1", ExecuteHermesBreakthroughResetSkills(*player));
    }

    JsonRpcDispatchResult HandleSynthesisList(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<std::string> fields = SplitHermesPayloadFields(ExtractPayloadParam(request), '|');
        uint32 classType = 255;
        uint32 offset = 0;
        uint32 limit = 8;
        if (fields.size() >= 3)
        {
            classType = ParseHermesUInt(fields[0], 0);
            offset = ParseHermesUInt(fields[1], 0);
            limit = ParseHermesUInt(fields[2], 8);
        }
        else
        {
            offset = fields.size() > 0 ? ParseHermesUInt(fields[0], 0) : 0;
            limit = fields.size() > 1 ? ParseHermesUInt(fields[1], 8) : 8;
        }

        std::string payload = BuildHermesSynthesisListPayload(*player, offset, limit, classType);
        return HermesPayloadResult(idText, "synthesis.list-page.v1", payload);
    }

    JsonRpcDispatchResult HandleSynthesisDo(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<std::string> fields = SplitHermesPayloadFields(ExtractPayloadParam(request), '|');
        uint32 itemId = fields.size() > 0 ? ParseHermesUInt(fields[0], 0) : 0;
        uint32 upgradeLevel = fields.size() > 1 ? ParseHermesUInt(fields[1], 0) : 0;
        bool useBooster = fields.size() > 2 && ParseHermesUInt(fields[2], 0) != 0;

        return HermesPayloadResult(idText, "synthesis.do.v1", ExecuteHermesSynthesis(*player, itemId, upgradeLevel, useBooster));
    }

    JsonRpcDispatchResult HandleAbyssGetSetBonuses(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)session;
        (void)frame;
        (void)request;
        (void)method;

        std::vector<HermesAbyssSetBonus> bonuses = LoadHermesAbyssSetBonuses();
        std::sort(bonuses.begin(), bonuses.end(), [](HermesAbyssSetBonus const& left, HermesAbyssSetBonus const& right)
        {
            if (left.ActId != right.ActId)
                return left.ActId < right.ActId;
            if (left.SourceMode != right.SourceMode)
                return left.SourceMode < right.SourceMode;
            return left.SetId < right.SetId;
        });

        std::ostringstream payload;
        payload << "SET_BONUSES:";
        bool first = true;
        for (HermesAbyssSetBonus const& bonus : bonuses)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << bonus.SetId << '^'
                    << SanitizeAddonPayloadText(bonus.SetName) << '^'
                    << static_cast<uint32>(bonus.ActId) << '^'
                    << static_cast<uint32>(bonus.SourceMode) << '^'
                    << HermesAbyssOptionalText(bonus.TwoPieceDesc) << '^'
                    << HermesAbyssOptionalText(bonus.FourPieceDesc) << '^'
                    << HermesAbyssOptionalText(bonus.SixPieceDesc) << '^'
                    << HermesAbyssOptionalText(bonus.EightPieceDesc);
        }

        return HermesPayloadResult(idText, "abyss.set-bonuses.v1", payload.str());
    }

    struct HermesAbyssSetGroup
    {
        uint8 SourceMode = 0;
        uint8 ActId = 0;
        uint16 SourceChapter = 0;
        std::vector<HermesAbyssEquipment const*> Items;
    };

    JsonRpcDispatchResult HandleAbyssGetSetOverview(WorldSession& session, HermesFrameV2& frame, Json::Value const& request, std::string const& idText, std::string const& method)
    {
        (void)frame;

        Player* player = session.GetPlayer();
        if (!player)
        {
            JsonRpcDispatchResult result;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32001, "Player is not ready", "PLAYER_NOT_READY", method);
            return result;
        }

        std::vector<std::string> fields = SplitHermesPayloadFields(ExtractPayloadParam(request), '|');
        uint32 page = fields.size() > 0 ? ParseHermesUInt(fields[0], 1) : 1;
        uint32 pageSize = fields.size() > 1 ? ParseHermesUInt(fields[1], 1) : 1;
        uint32 filterMode = fields.size() > 2 ? ParseHermesUInt(fields[2], 0) : 0;
        bool currentActOnly = fields.size() > 3 && ParseHermesUInt(fields[3], 0) != 0;

        page = std::max<uint32>(1, page);
        pageSize = std::max<uint32>(1, std::min<uint32>(10, pageSize));

        std::unordered_map<uint16, HermesAbyssChapter> chapters = LoadHermesAbyssChapters();
        std::vector<HermesAbyssEquipment> equipmentRows = LoadHermesAbyssEquipment();
        std::vector<HermesAbyssSetBonus> bonuses = LoadHermesAbyssSetBonuses();
        uint32 currentActId = GetHermesAbyssCurrentActId(*player, chapters);

        std::vector<HermesAbyssSetGroup> groups;
        std::vector<uint32> actIds;

        for (HermesAbyssEquipment const& equipment : equipmentRows)
        {
            if (equipment.EquipmentType != 1)
                continue;
            if (filterMode != 0 && equipment.SourceMode != filterMode)
                continue;
            if (currentActOnly && currentActId != 0 && equipment.ActId != currentActId)
                continue;

            if (std::find(actIds.begin(), actIds.end(), static_cast<uint32>(equipment.ActId)) == actIds.end())
                actIds.push_back(equipment.ActId);

            HermesAbyssSetGroup* group = nullptr;
            for (HermesAbyssSetGroup& candidate : groups)
            {
                if (candidate.SourceMode == equipment.SourceMode &&
                    candidate.ActId == equipment.ActId &&
                    candidate.SourceChapter == equipment.SourceChapter)
                {
                    group = &candidate;
                    break;
                }
            }

            if (!group)
            {
                HermesAbyssSetGroup newGroup;
                newGroup.SourceMode = equipment.SourceMode;
                newGroup.ActId = equipment.ActId;
                newGroup.SourceChapter = equipment.SourceChapter;
                groups.push_back(newGroup);
                group = &groups.back();
            }

            group->Items.push_back(&equipment);
        }

        std::sort(actIds.begin(), actIds.end());
        actIds.erase(std::unique(actIds.begin(), actIds.end()), actIds.end());

        std::sort(groups.begin(), groups.end(), [](HermesAbyssSetGroup const& left, HermesAbyssSetGroup const& right)
        {
            if (left.ActId != right.ActId)
                return left.ActId < right.ActId;
            if (left.SourceMode != right.SourceMode)
                return left.SourceMode < right.SourceMode;
            return left.SourceChapter < right.SourceChapter;
        });

        for (HermesAbyssSetGroup& group : groups)
        {
            std::sort(group.Items.begin(), group.Items.end(), [](HermesAbyssEquipment const* left, HermesAbyssEquipment const* right)
            {
                if (left->SlotMask != right->SlotMask)
                    return left->SlotMask < right->SlotMask;
                return left->ItemId < right->ItemId;
            });
        }

        uint32 totalActCount = static_cast<uint32>(actIds.size());
        uint32 pageCount = std::max<uint32>(1, (totalActCount + pageSize - 1) / pageSize);
        page = std::min<uint32>(page, pageCount);
        uint32 start = totalActCount == 0 ? 0 : (page - 1) * pageSize;
        uint32 end = std::min<uint32>(totalActCount, start + pageSize);
        uint32 pageStartActId = start < end ? actIds[start] : 0;
        uint32 pageEndActId = start < end ? actIds[end - 1] : 0;

        std::ostringstream payload;
        payload << "SET_OVERVIEW:" << page << '|'
                << pageSize << '|'
                << totalActCount << '|'
                << pageCount << '|'
                << currentActId << '|'
                << pageStartActId << '|'
                << pageEndActId;

        for (HermesAbyssSetGroup const& group : groups)
        {
            bool inPage = false;
            for (uint32 index = start; index < end; ++index)
            {
                if (actIds[index] == group.ActId)
                {
                    inPage = true;
                    break;
                }
            }

            if (!inPage || group.Items.empty())
                continue;

            HermesAbyssEquipment const* representative = group.Items.front();
            HermesAbyssSetBonus const* bonus = FindHermesAbyssSetBonus(bonuses, group.SourceMode, group.ActId);

            std::ostringstream slotSummary;
            for (size_t i = 0; i < group.Items.size(); ++i)
            {
                if (i != 0)
                    slotSummary << " / ";
                slotSummary << GetHermesAbyssSlotName(group.Items[i]->SlotMask);
            }

            std::string groupName = bonus ? SanitizeAddonPayloadText(bonus->SetName) : SanitizeAddonPayloadText(representative->ItemName);
            if (groupName.empty())
                groupName = "Abyss Set";

            payload << '~'
                    << static_cast<uint32>(group.SourceMode) << '^'
                    << static_cast<uint32>(group.ActId) << '^'
                    << group.SourceChapter << '^'
                    << GetHermesAbyssChapterName(chapters, group.SourceChapter) << '^'
                    << group.Items.size() << '^'
                    << groupName << '^'
                    << SanitizeAddonPayloadText(slotSummary.str()) << '^'
                    << representative->ItemId << '^'
                    << SanitizeAddonPayloadText(representative->ItemName) << '^'
                    << SanitizeAddonPayloadText(GetHermesItemIconPathForAddon(representative->ItemId)) << '^'
                    << (bonus ? HermesAbyssOptionalText(bonus->TwoPieceDesc) : " ") << '^'
                    << (bonus ? HermesAbyssOptionalText(bonus->FourPieceDesc) : " ") << '^'
                    << (bonus ? HermesAbyssOptionalText(bonus->SixPieceDesc) : " ") << '^'
                    << (bonus ? HermesAbyssOptionalText(bonus->EightPieceDesc) : " ");
        }

        return HermesPayloadResult(idText, "abyss.set-overview.v1", payload.str());
    }

    JsonRpcDispatchResult DispatchJsonRpcPayload(WorldSession& session, HermesFrameV2& frame, Json::Value const& request)
    {
        std::string const idText = JsonRpcIdText(request, frame.RequestId);
        JsonRpcDispatchResult result;

        if (!request.isObject() || !request.isMember("jsonrpc") || !request["jsonrpc"].isString() || request["jsonrpc"].asString() != "2.0" || !request.isMember("method") || !request["method"].isString())
        {
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32600, "Invalid Request", "INVALID_REQUEST");
            return result;
        }

        std::string const method = request["method"].asString();
        HermesMethodDescriptor const* descriptor = FindMethodDescriptor(method);
        if (!descriptor || !descriptor->Handler)
        {
            if (ConsumeHermesDebugBudget(g_HermesDebugDispatchLogBudget))
                LOG_INFO("server.loading", "HermesBridge DEBUG server dispatch missing account={} requestId={} method={} payload={}", session.GetAccountId(), frame.RequestId, method, HermesLogPreview(frame.Payload));
            frame.MethodId = HERMES_METHOD_UNKNOWN;
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32601, "Method not found", "METHOD_NOT_FOUND", method);
            return result;
        }

        frame.MethodId = descriptor->MethodId;
        if (ShouldTraceHermesServerMethod(frame.MethodId) && ConsumeHermesDebugBudget(g_HermesDebugDispatchLogBudget))
            LOG_INFO("server.loading", "HermesBridge DEBUG server dispatch account={} player={} method={} methodId={} requestId={} bytes={}",
                session.GetAccountId(),
                session.GetPlayer() ? session.GetPlayer()->GetName().c_str() : "<none>",
                method,
                frame.MethodId,
                frame.RequestId,
                frame.Payload.size());
        if (session.GetSecurity() < descriptor->MinSecurity)
        {
            LOG_INFO("server.loading", "HermesBridge: permission denied account={} method={} security={} required={}", session.GetAccountId(), method, static_cast<unsigned int>(session.GetSecurity()), static_cast<unsigned int>(descriptor->MinSecurity));
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32030, "Permission denied", "PERMISSION_DENIED", method);
            return result;
        }

        if (IsHermesRateLimited(session, *descriptor))
        {
            if (ConsumeHermesDebugBudget(g_HermesDebugDispatchLogBudget))
                LOG_INFO("server.loading", "HermesBridge DEBUG server rate-limited account={} method={} methodId={} requestId={}", session.GetAccountId(), method, frame.MethodId, frame.RequestId);
            result.IsError = true;
            result.Payload = JsonRpcErrorPayload(idText, -32031, "Rate limit exceeded", "RATE_LIMITED", method);
            return result;
        }

        return descriptor->Handler(session, frame, request, idText, method);
    }

    void HandleHermesFrameV2Packet(WorldSession& session, WorldPacket& recvPacket)
    {
        HermesOutboundBatchScope outboundBatch;
        HermesFrameV2 frame;
        uint8 version = 0;
        uint8 headerSize = 0;

        if (recvPacket.size() - recvPacket.rpos() < HERMES_BRIDGE_FRAME_HEADER_SIZE - sizeof(uint16))
        {
            LOG_INFO("server.loading", "HermesBridge: dropped malformed v2 frame from account {} (size={})", session.GetAccountId(), recvPacket.size());
            return;
        }

        recvPacket >> version;
        recvPacket >> headerSize;
        recvPacket >> frame.Lane;
        recvPacket >> frame.MessageType;
        recvPacket >> frame.Codec;
        recvPacket >> frame.Flags;
        recvPacket >> frame.SchemaId;
        recvPacket >> frame.MethodId;
        recvPacket >> frame.RequestId;
        recvPacket >> frame.Sequence;
        recvPacket >> frame.PayloadSize;

        if (version != HERMES_BRIDGE_FRAME_VERSION || headerSize < HERMES_BRIDGE_FRAME_HEADER_SIZE)
        {
            LOG_INFO("server.loading", "HermesBridge: dropped v2 frame from account {} version={} headerSize={}", session.GetAccountId(), version, headerSize);
            return;
        }

        uint32 extraHeaderSize = headerSize - HERMES_BRIDGE_FRAME_HEADER_SIZE;
        if (recvPacket.size() - recvPacket.rpos() < extraHeaderSize)
        {
            LOG_INFO("server.loading", "HermesBridge: dropped v2 frame from account {} extraHeaderSize={} remaining={}", session.GetAccountId(), extraHeaderSize, recvPacket.size() - recvPacket.rpos());
            return;
        }

        if (extraHeaderSize)
            recvPacket.read_skip(extraHeaderSize);

        uint32 remaining = recvPacket.size() - recvPacket.rpos();
        if (frame.PayloadSize > HERMES_BRIDGE_MAX_PAYLOAD_SIZE || frame.PayloadSize > remaining)
        {
            LOG_INFO("server.loading", "HermesBridge: dropped v2 frame from account {} lane={} type={} codec={} payloadSize={} remaining={}", session.GetAccountId(), frame.Lane, frame.MessageType, frame.Codec, frame.PayloadSize, remaining);
            return;
        }

        ReadPayload(recvPacket, frame.PayloadSize, frame.Payload);
        if ((frame.MethodId == HERMES_METHOD_HELLO || frame.MethodId == HERMES_METHOD_PING || frame.MethodId == HERMES_METHOD_ADDON_DISPATCH || IsHermesTracePayload(frame.Payload)) && ConsumeHermesTraceBudget(g_HermesTraceRecvFrameLogBudget))
            LOG_INFO("server.loading", "HermesBridge TRACE server recv-frame account={} player={} lane={} type={} methodId={} requestId={} bytes={} payload={}",
                session.GetAccountId(),
                session.GetPlayer() ? session.GetPlayer()->GetName().c_str() : "<none>",
                static_cast<unsigned int>(frame.Lane),
                static_cast<unsigned int>(frame.MessageType),
                frame.MethodId,
                frame.RequestId,
                frame.PayloadSize,
                HermesLogPreview(frame.Payload, 240));
        if (ShouldTraceHermesServerMethod(frame.MethodId) && ConsumeHermesDebugBudget(g_HermesDebugRecvLogBudget))
            LOG_INFO("server.loading", "HermesBridge DEBUG server recv-frame account={} player={} lane={} type={} codec={} methodId={} requestId={} seq={} bytes={} payload={}",
                session.GetAccountId(),
                session.GetPlayer() ? session.GetPlayer()->GetName().c_str() : "<none>",
                static_cast<unsigned int>(frame.Lane),
                static_cast<unsigned int>(frame.MessageType),
                static_cast<unsigned int>(frame.Codec),
                frame.MethodId,
                frame.RequestId,
                frame.Sequence,
                frame.PayloadSize,
                HermesLogPreview(frame.Payload));
        if (HERMES_BRIDGE_TRACE_PACKETS)
            LOG_INFO("server.loading", "HermesBridge: v2 recv account={} lane={} type={} codec={} methodId={} requestId={} seq={} bytes={}", session.GetAccountId(), frame.Lane, frame.MessageType, frame.Codec, frame.MethodId, frame.RequestId, frame.Sequence, frame.PayloadSize);

        if (frame.Flags & (HERMES_FLAG_COMPRESSED | HERMES_FLAG_FRAGMENTED))
        {
            SendHermesFrameV2(session, frame, HERMES_MESSAGE_ERROR, HERMES_CODEC_JSON, JsonRpcErrorPayload(std::to_string(frame.RequestId), -32010, "Frame flags are not supported yet", "UNSUPPORTED_FLAGS"));
            return;
        }

        if (frame.MessageType != HERMES_MESSAGE_REQUEST)
        {
            SendHermesFrameV2(session, frame, HERMES_MESSAGE_ERROR, HERMES_CODEC_JSON, JsonRpcErrorPayload(std::to_string(frame.RequestId), -32012, "Frame message type is not supported yet", "UNSUPPORTED_MESSAGE_TYPE"));
            return;
        }

        if (frame.Codec != HERMES_CODEC_JSON)
        {
            SendHermesFrameV2(session, frame, HERMES_MESSAGE_ERROR, HERMES_CODEC_JSON, JsonRpcErrorPayload(std::to_string(frame.RequestId), -32011, "Frame codec is not supported yet", "UNSUPPORTED_CODEC"));
            return;
        }

        Json::Value request;
        std::string parseError;
        if (HERMES_BRIDGE_TRACE_PACKETS)
            LOG_INFO("server.loading", "HermesBridge: v2 json parse begin account={} requestId={} bytes={}", session.GetAccountId(), frame.RequestId, frame.Payload.size());
        if (!ParseJsonPayload(frame.Payload, request, parseError))
        {
            LOG_INFO("server.loading", "HermesBridge: v2 json parse failed account={} requestId={} error={}", session.GetAccountId(), frame.RequestId, parseError);
            SendHermesFrameV2(session, frame, HERMES_MESSAGE_ERROR, HERMES_CODEC_JSON, JsonRpcErrorPayload("null", -32700, "Parse error", "JSON_PARSE_ERROR"));
            return;
        }
        if (HermesBridge_ShouldMarkAddonReadyForMethod(frame.MethodId))
            MarkHermesAddonReady(session, "hello");

        try
        {
            if (HERMES_BRIDGE_TRACE_PACKETS)
                LOG_INFO("server.loading", "HermesBridge: v2 dispatch begin account={} requestId={}", session.GetAccountId(), frame.RequestId);
            JsonRpcDispatchResult response = DispatchJsonRpcPayload(session, frame, request);
            if (HERMES_BRIDGE_TRACE_PACKETS)
                LOG_INFO("server.loading", "HermesBridge: v2 dispatch done account={} methodId={} requestId={} isError={} bytes={}", session.GetAccountId(), frame.MethodId, frame.RequestId, response.IsError, response.Payload.size());
            SendHermesFrameV2(session, frame, response.IsError ? HERMES_MESSAGE_ERROR : HERMES_MESSAGE_RESPONSE, HERMES_CODEC_JSON, response.Payload);
        }
        catch (std::exception const& ex)
        {
            LOG_INFO("server.loading", "HermesBridge: v2 request failed account={} requestId={} exception={}", session.GetAccountId(), frame.RequestId, ex.what());
        }
        catch (...)
        {
            LOG_INFO("server.loading", "HermesBridge: v2 request failed account={} requestId={} exception=unknown", session.GetAccountId(), frame.RequestId);
        }
    }

    class HermesAddonPacketBridgeScript : public ServerScript
    {
    public:
        HermesAddonPacketBridgeScript() : ServerScript("HermesAddonPacketBridgeScript") { }

        bool CanPacketSend(WorldSession* session, WorldPacket& packet) override
        {
            if (!session)
                return true;

            if (!sConfigMgr->GetOption<bool>("HermesBridge.AddonBridge.Enabled", true))
                return true;

            std::string prefix;
            std::string payload;
            if (TryReadAddonChatPacket(packet, prefix, payload))
            {
                Player* player = session->GetPlayer();
                bool const addonReady = IsHermesAddonReady(player);
                bool const prefixReady = IsHermesAddonPrefixReady(player, prefix);
                if (!HermesBridge_ShouldTakeoverLegacyAddonPacket(true, addonReady, prefixReady, true))
                {
                    if (ConsumeHermesTraceBudget(g_HermesTraceLegacyBypassLogBudget))
                        LOG_INFO("server.loading", "HermesBridge TRACE server addon-legacy-bypass-not-ready account={} player={} prefix={} ready={} prefixReady={} bytes={}",
                            session->GetAccountId(),
                            player ? player->GetName().c_str() : "<none>",
                            prefix,
                            addonReady ? 1 : 0,
                            prefixReady ? 1 : 0,
                            payload.size());
                    return true;
                }

                if (ConsumeHermesDebugBudget(g_HermesDebugAddonLogBudget))
                    LOG_INFO("server.loading", "HermesBridge DEBUG server addon-legacy-smsg account={} player={} prefix={} payloadBytes={} preview={}",
                        session->GetAccountId(),
                        session->GetPlayer() ? session->GetPlayer()->GetName().c_str() : "<none>",
                        prefix,
                        payload.size(),
                        HermesLogPreview(payload));

                std::string coalescedPayload;
                HermesAddonChunkResult const chunkResult = TryCoalesceAddonChunk(*session, prefix, payload, coalescedPayload);
                if (chunkResult == HermesAddonChunkResult::Complete)
                    SendHermesAddonMessageEvent(*session, prefix, coalescedPayload);
                else if (chunkResult == HermesAddonChunkResult::NotChunk || chunkResult == HermesAddonChunkResult::Invalid)
                    SendHermesAddonMessageEvent(*session, prefix, payload);

                g_HermesAddonTakeoverBlockedLegacySmsg.fetch_add(1, std::memory_order_relaxed);
                return false;
            }

            return true;
        }
    };

    class HermesAddonReadyPlayerScript : public PlayerScript
    {
    public:
        HermesAddonReadyPlayerScript() : PlayerScript("HermesAddonReadyPlayerScript", { PLAYERHOOK_ON_LOGOUT }) { }

        void OnPlayerLogout(Player* player) override
        {
            if (player)
            {
                ClearHermesAddonReady(player, "logout");
                // 【审计修复】登出时清除该会话滞留的出站帧，防止会话析构后 flush 发包导致 use-after-free
                PurgeHermesOutboundFramesForSession(player->GetSession());
            }
        }
    };
}

bool HermesBridge_SendAddonMessage(Player* player, std::string const& prefix, std::string const& payload)
{
    if (!player || !player->GetSession() || prefix.empty() || payload.empty())
        return false;

    if (!sConfigMgr->GetOption<bool>("HermesBridge.AddonBridge.Enabled", true))
        return false;

    bool const addonReady = IsHermesAddonReady(player);
    bool const takeoverPrefix = IsHermesAddonTakeoverPrefix(prefix);
    bool const prefixReady = IsHermesAddonPrefixReady(player, prefix);
    if (!HermesBridge_ShouldTakeoverLegacyAddonPacket(true, addonReady, prefixReady, takeoverPrefix))
    {
        if (takeoverPrefix && !addonReady && !prefixReady && ConsumeHermesTraceBudget(g_HermesTraceAddonNotReadyLogBudget))
            LOG_INFO("server.loading", "HermesBridge TRACE server addon-send-defer-legacy player={} prefix={} ready={} prefixReady={} takeover={} bytes={}",
                player->GetName(),
                prefix,
                addonReady ? 1 : 0,
                prefixReady ? 1 : 0,
                takeoverPrefix ? 1 : 0,
                payload.size());
        return false;
    }

    WorldSession* session = player->GetSession();
    std::string coalescedPayload;
    HermesAddonChunkResult const chunkResult = TryCoalesceAddonChunk(*session, prefix, payload, coalescedPayload);
    if (chunkResult == HermesAddonChunkResult::Complete)
        SendHermesAddonMessageEvent(*session, prefix, coalescedPayload);
    else if (chunkResult == HermesAddonChunkResult::NotChunk || chunkResult == HermesAddonChunkResult::Invalid)
        SendHermesAddonMessageEvent(*session, prefix, payload);

    g_HermesAddonTakeoverBlockedLegacySmsg.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void AddHermesBridgePacketBridgeScripts()
{
    new HermesAddonPacketBridgeScript();
    new HermesAddonReadyPlayerScript();
}

void WorldSession::HandleHermesBridgeOpcode(WorldPacket& recvPacket)
{
    uint16 magic = 0;

    if (recvPacket.size() < sizeof(magic))
    {
        LOG_INFO("server.loading", "HermesBridge: dropped malformed packet from account {} (size={})", GetAccountId(), recvPacket.size());
        return;
    }

    recvPacket >> magic;
    if (magic != HERMES_BRIDGE_FRAME_MAGIC)
    {
        LOG_INFO("server.loading", "HermesBridge: dropped non-v2 packet from account {} magic=0x{:04X} size={}", GetAccountId(), magic, recvPacket.size());
        return;
    }

    HandleHermesFrameV2Packet(*this, recvPacket);
}
