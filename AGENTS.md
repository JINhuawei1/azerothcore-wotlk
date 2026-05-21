# 交流规则

- 始终使用中文回复，除非用户明确要求使用英文
- 未经用户明确要求，不要创建或更新任何文档文件（README、文档等）
- 未经用户明确要求，不要执行任何编译或构建命令
- 修改任何现有文件时，必须保持原文件编码格式，不得擅自转换编码（如 UTF-8、UTF-8 with BOM、GBK/ANSI 等）

# 项目信息

这是一个 AzerothCore 魔兽世界服务器模拟器项目（基于 WotLK 3.3.5）。

# 日志约定

- 需要确认能直接输出到 `worldserver` 控制台的定位/排查日志时，优先使用 `LOG_INFO("server.loading", "...")`，参考现有 `[性能监控-登录总耗时]` 日志；不要默认使用 `entities.unit`、`entities.player`、`module` 等可能不显示在控制台的日志分类。

# 本地工具链

- 涉及 `agent-harness`、`az`、`acore-cli`、服务器管理、账号、角色、物品、NPC、公会、GM、数据库、日志、进程、监控、配置、校验等任务时，优先使用 `E:\azerothcore-wotlk\agent-harness`。短命令别名使用 `az`。首选入口是 `acore-cli` 或 `az`；若 PATH 中没有这些命令，则在 `E:\azerothcore-wotlk\agent-harness` 下使用 `python -m cli_anything.azerothcore`。需要结构化结果时优先加 `--json`。
- 涉及 `tools`、`CLI-Anything`、harness 生成/补全/测试/验证、`codex-skill`、方法论或模板复用等任务时，优先使用 `E:\azerothcore-wotlk\tools\CLI-Anything` 作为本地工具链根目录。优先参考 `E:\azerothcore-wotlk\tools\CLI-Anything\codex-skill\SKILL.md` 与 `E:\azerothcore-wotlk\tools\CLI-Anything\cli-anything-plugin\HARNESS.md`，除非任务确实需要，不必重新检索外部文档或再次向用户确认这些路径。
- 已安装仓库专属 skill `azerothcore-local-toolchains`。匹配上述任务时，应优先使用该 skill，而不是重新摸索工具链结构。
- 涉及 `DBC`、`MPQ`、`万剑补丁`、`SpellVisual`、客户端补丁打包等任务时，优先使用 `E:\azerothcore-wotlk\agent-harness\patch_toolchain\wanjian` 作为本地补丁工具链根目录。首选通过 `az patch paths`、`az patch status`、`az patch run ...` 发现和执行脚本，而不是再次要求用户提供 `E:\azerothcore-wotlk\tools` 路径。

# 环境约定

- 对当前仓库而言，如果 `az/acore-cli validate report`、`self-check` 或 `doctor` 显示 `soap=false`，但数据库三库连接正常、`worldserver/authserver` 路径存在，则默认先判断为“服务器或 SOAP 服务尚未启动”，而不是 harness 配置错误。
- 遇到上述模式时，后续回复应直接按“需要先启动服务器，再验证 SOAP”来表述；除非用户明确要求排查 SOAP 配置，否则不要把它反复当成独立异常点强调。


# 生图 MCP（game-image-gen）

- 已在 `.kiro/settings/mcp.json` 安装 MCP server `game-image-gen`，绑定到 `kiro_default` agent，开箱即用。后端 `gpt-image-2`（OpenAI 兼容），endpoint `https://moai.top/v1`。
- 当用户说"调用生图模型/生成素材/生图"之类，应使用 MCP 工具：
    - `@game-image-gen/generate_game_asset(prompt, filename, size, subdir, overwrite)` — 异步下单，立刻返回
    - `@game-image-gen/check_asset_status(filename, subdir)` — 轮询完成状态
    - `@game-image-gen/list_generated_assets(subdir)` — 列已生成素材
    - `@game-image-gen/server_status()` — 查 MCP 自检
- 单张 PNG 生成通常 60–180s。下单后用 `check_asset_status` 轮询到 `[OK]` 再继续后续步骤，不要假设立刻完成。
- 素材默认落到 `E:\azerothcore-wotlk\agent-harness\art\ai_generated\<subdir>\<filename>.png`。
- gpt-image-2 的 size 限制：宽高都必须是 16 的倍数，总像素 655360–8294400，长短边比 ≤ 3:1。常用 `1024x1024`（正方形）或 `1536x512`（超宽条）。
- Prompt 写英文效果更稳，务必加 `no text, no letters, no characters` 避免 AI 画乱码文字。
- WoW 3.3.5 UI 素材流程：生成 PNG → auto-trim（黑色转透明 + bbox 裁剪让装饰贴满画布） → resize 到 2 的幂 → 保存为 32bit 未压缩 TGA → 放到 `<AddOn>\Assets\`，XML 里用 `Interface\AddOns\<Name>\Assets\<file>.tga`。已有脚本：`agent-harness\tools\png_to_tga.py`、`agent-harness\tools\build_promo_tga_v2.py`（auto-trim 参考实现）。
- 让素材"不拉伸"的关键：**XML Size 的宽高比 = TGA 装饰实际 bbox 的宽高比**，不要强行用和 TGA 不同比例的 Size 拉伸贴图。
