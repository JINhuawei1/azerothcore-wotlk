# Hermes Bridge Client DLL

This directory contains the WoW 3.3.5 client-side native bridge DLL and focused probe utilities used to validate the local AzerothCore `mod-hermes-bridge` module.

## Known Client Addresses

The target client is the fixed 32-bit `wow.exe` build at `E:\World of Warcraft335\wow.exe`:

- Image base: `0x00400000`
- `FrameScript::Execute`: `0x00819210`
- Packet factory: `0x00467540`
- Upper send path: `0x004675F0`
- Receive after-opcode hook point: `0x00632001`
- Lua `SendAddonMessage` handler: `0x00500560`
- Lua string argument helper used by `SendAddonMessage`: `0x0084E0E0`

## Formal DLL Capabilities

`hermes_bridge_dll.c` installs three low-level hooks:

- Upper-send hook captures the per-session connection `self` and sends CMSG `0x0990` through the encrypted client send path.
- Receive after-opcode hook watches for SMSG `0x0991`, parses the packet payload at `([packet+0x04] - [packet+0x08]) + [packet+0x14]`, logs it, and dispatches it into Lua via `HermesDLL._NativeReceive(channel, payload)`.
- `SendAddonMessage` hook intercepts only prefix `HERMESDLL` so `/hdll send ...` can trigger native bridge sends without registering a new Lua C global.

The `SendAddonMessage` bridge copies the payload into DLL-owned storage, returns immediately from the Lua C-function hook, then sends from a short delayed worker thread. Calling the native send path directly inside the Lua hook can reach the server but crash the client while unwinding Lua/addon-message state.

The injected Lua API provides:

- `/hdll state` — show native hook state and inbox count.
- `/hdll send <text>` — send `<text>` to the server through CMSG `0x0990`.
- `/hdll inbox` — show the most recent received bridge message.
- `HermesDLL.Register(channel, callbackName)` — register a global Lua callback name.
- `HermesDLL.Pop()` — remove and return the oldest inbox message.

## Build

Run from Git Bash/MSYS with `cmd.exe //c`, not `cmd.exe /c`, to avoid MSYS path conversion:

```bash
cmd.exe //c build_hermes_bridge_v053_20260618_1835_x86.bat
```

Use a unique DLL output name for each injection attempt. Windows does not rerun `DllMain` for an already loaded DLL path.

## Current Keep List

Keep these files in the root/client build path for the v0.5.3 workflow:

- `hermes_bridge_dll.c` and `build_hermes_bridge_v053_20260618_1835_x86.bat` — formal bridge DLL source/build.
- `build/hermes_bridge_v053_20260618_1835.dll` — current validated DLL artifact.
- `verify_hermes_bridge_v053.sh` — automated real slash-send verifier; builds unique restore/bridge/trigger DLLs per run.
- `hermes_inject_existing.c` / `build/hermes_inject_existing.exe` — generic injector.
- `hermes_restore_hooks.c` / `build/hermes_restore_hooks_20260618_1824.dll` — restore helper for hook collisions, including upper-send, receive dispatch, and `SendAddonMessage`.

One-off v0.5.2/v0.5.3 slash triggers and unsafe Lua registration scans are archived under `archive/20260618-send-bridge-probes/`.

## Verification Pattern

For the current v0.5.3 bridge, the fastest automated check is:

```bash
bash verify_hermes_bridge_v053.sh ping-v053-script
```

The script locates the current `wow.exe`, builds and injects unique restore/bridge DLLs, runs the real slash handler path `SlashCmdList['HERMESDLL']('send <payload>')`, checks for `SendAddonMessage hook installed` and `SendAddonMessage intercepted`, checks client/server logs, and fails if `wow.exe` exits or protocol errors appear.

Manual verification:

1. Confirm current `wow.exe` PID:
   ```bash
   ps -W | grep -i '[w]ow.exe'
   ```
2. Restore hook bytes if old probes are already installed, or restart the client and re-enter world:
   ```bash
   cmd.exe //c build_hermes_restore_hooks_20260618_1824_x86.bat
   ./build/hermes_inject_existing.exe <pid> 'E:\azerothcore-wotlk\modules\mod-hermes-bridge\client\build\hermes_restore_hooks_20260618_1824.dll'
   ```
3. Inject the formal bridge DLL:
   ```bash
   ./build/hermes_inject_existing.exe <pid> 'E:\azerothcore-wotlk\modules\mod-hermes-bridge\client\build\hermes_bridge_v053_20260618_1835.dll'
   ```
4. Check `build/hermes_bridge.log` for:
   - `HermesBridge upper send hook installed`
   - `HermesBridge recv hook installed`
   - `HermesBridge SendAddonMessage hook installed`
   - `HermesBridge native send result=0 ... hello-from-hermes-dll`
   - `HermesBridge recv smsg ... pong:hello-from-hermes-dll`
   - `HermesBridge recv smsg ... ready:player-entered-world`
5. Run in-game:
   ```text
   /hdll send ping-v053
   /hdll inbox
   ```
6. Passing slash-send validation includes:
   - Client log: `HermesBridge native send result=0 channel=1 bytes=...`
   - Client log: `HermesBridge recv smsg ... pong:ping-v053` or another expected response.
   - Server log: `HermesBridge: recv account=1 channel=1 bytes=... payload='ping-v053'`.
   - `wow.exe` remains running after the response.
7. Check server log under `E:\azerothcore-wotlk\build\bin\RelWithDebInfo\logs\Server.log` for matching `HermesBridge: recv ...` lines and no fresh malformed/unknown opcode lines.

Known-good manual in-game validation:

```text
/hdll send ping-manual
/hdll inbox
HermesDLL.Send native queued debug len=11
HermesDLL.Dispatch 1 len=33
HermesDLL inbox 6
last ch=1 len=16 pong:ping-manual
```
