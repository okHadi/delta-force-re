# Lobby session runtime

Target: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Host/join/session callback state from the original menu handlers. The module returns record data
and menu intents only; the caller owns UI wiring, text resolution, and network calls.

## Code

- `include/dfre/lobby_session.hpp`, `src/lobby_session.cpp`.
- Original callbacks: `mnu_mp_host` `0x416f10`, `mnu_mp_join` `0x416f80`, `mnu_mp_search` `0x4173b0`,
  `mnu_mp_status` `0x416500`, `mnu_mp_sessions` `0x416670`, host fields `0x418110`-`0x418ca0`,
  host options `0x41b490`-`0x41c050`.
- Button action ids from `0x40def0`: 1 down, 2 up, 3 slider, 4 text commit, 5 text change,
  6 hover in, 7 hover out, 8 hotkey. Draw states from `0x40e2a0`: 0 null, 1 normal, 2 lit,
  3 pushed, 4 disabled.
- Session browser model: list `0x501328`/`0x501330`, selected `0x4d6b68`, status `0x4d6b58`,
  dots `0x4d6b64`, local id `0x50362c`. Row formats `0x4c9ff0`, `0x4ca00c`, `0x4ca024`,
  `0x4ca03c`; game type keys `0x4c9f60`-`0x4c9fc8`.
- Pre-connect block `0x67f520`: mode, service, hosting, flag, maxplayers, gametype, map name
  `0x67f438`. Host map tables: counts `0x4e65ac`-`0x4e65bc`, bases `0x4e65c0`-`0x4e65d0`,
  offset `0x4e65d4`, names at `0x4d6ba8`, `0x4d9da8`, `0x4dcfa8`, `0x4e01a8`, `0x4e33a8`.
- External calls become explicit intents: `StartSessionSearch` `0x4172f0`,
  `StopSessionSearch` `0x417370`, `JoinSession` `0x41e2d0` (stores to `0x4e6800`),
  `SessionObjectCall30` `0x497580`, plus `SwitchMenu`, `RefreshHostMenu` `0x417b60`,
  `RefreshHost2Menu` `0x41ac00`, `UpdateHostMapList` `0x417530`, `ResetMenuInput` `0x40b9a0`.

## Checks

401 cases match the original x86 under Unicorn 2.1.4 with strict per-case intent counts and
zero mismatches. The check replays every callback with stubbed menu APIs, settings memory, and
network helpers, and reads or writes no files or sockets.

```sh
.local/tools/python/bin/python tools/check_lobby_session_original.py "/path/to/Delta Force/Df.exe" build/lobby_session_probe
```

On a Windows multi-configuration build, use `build/Release/lobby_session_probe.exe`.
Full result: `.local/reports/lobby-session-check.json`.

## Gaps

Remaining callbacks and data producers are listed in `.local/opencode/lobby_session-needs.md`
(mp service/login/team rows, host map select handlers, session list producers, dot counter).
