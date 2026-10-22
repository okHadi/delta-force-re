# Gameplay command dispatch

Source: `Df.exe`, SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. The dispatcher is `0x477D40(id, value)`.

`dispatch_game_command(id, value, globals, actor, commands)` applies the original state writes to `DispatchGlobals`/`DispatchActor` and returns the exact typed calls in order. `LoadoutMenu`-style drawing is not involved. Command ids and values come from the input stage (`CommandTable`, queue records).

## Gate

The gate reads the flags byte at `0x4FB326 + id*0x2C`: the row whose **row index equals the command id**, not the row whose id field matches. Rows with index above 120 read the zeroed bytes after the shipped table. A row flag bit 0 blocks the command when the local actor flags `[0xC23820+0x6C]` bit 2 is set, when `0x6E4714` (pause) is set, or when `0x67DBC0` (menu) and `0x67F520` (multiplayer) are both set. Flag bit 4 blocks the command in multiplayer. `CommandTable::rows()[id].flags` supplies the byte.

## Owned state

`DispatchGlobals` mirrors every original global the dispatcher reads or writes, including the movement control word `0xC23A00` (bits in `movement.hpp`), the stance pulses `0xC23A04/08/0C`, the look scalars `0xC23A28/2C/30/34`, the camera words `0x6BA424/427/428/42C`, and the option/message toggles. Byte-sized writes keep byte semantics: `0x67DBC8`/`0x67DBC9` are single bytes, `0x6BA427` is the high byte of the `0x6BA424` dword, and `0x67DBXX` toggles only the low byte.

Movement ids write the control bits or call `0x484320(n)` when `0x67DBC8` bit `0x20` is set. Absolute look ids `9B..9E` add or subtract the signed value with 32-bit wrap. `0x9A` re-dispatches id `0x0D` when the actor gate or menu flag is set. Zoom ids `0x168`/`0x169` compute `trunc(value * 0.9/1.1)` with x87 truncation (the `0x49BAE6` helper) and store the integer indefinite `0x80000000` out of range, then clamp to `0x4000`/`0x39999`.

## Typed calls

`DispatchCall` records the exact address, the semantic argument values and a category. Stack buffers and call results are named with `dispatch_arg_*` tokens: the `%i` buffer (`0x493A80`), the `0x404CF0` dialog handle, the `0x401D50` text handle, the uninitialized 16-byte event payload, the local actor pointer and the `-1` timeout pass-through. `0x404CF0` has a 3-argument message form (timeout `-1`) and a 5-argument dialog/value form; the caller threads the returned handle into `0x4799D0`/`0x401D50`/`0x479F00` (`threaded_result`).

Weapon commands report `0x483C70` fire, `0x483BD0` reload, `0x483DD0(slot)` select, `0x484B70(±1)` cycle, `0x484C20` aim and the `0x4848C0`/`0x484320`/`0x484A50`/`0x484C80`/`0x484D00`/`0x484D80` supply helpers. Id `0x79` models the `0x484960` thunk (`0x67DBC9` bit 0 toggle) and reports the `0x4848C0` resupply. Team toggles `0x6C`/`0x6D`/`0x6E` are owned (globals `0x6E558C/90/94`); `0x6D` reports the `0x446630` call when the toggle dword is nonzero. UI dialogs, messages, screen-mode calls, the direct-input camera modes, picture calls, screenshot and mission/multiplayer calls keep their exact addresses and arguments.

## Checks

- `clang++ -std=c++17 -Iinclude src/gameplay_dispatch.cpp src/input_state.cpp src/input_config.cpp tests/native_gameplay_dispatch.cpp` runs the native branch checks; also clean under ASan+UBSan.
- `gameplay_dispatch_probe fields|kinds|commands|run` prints the seed-field order, call-kind names, the shipped command rows and stdin-driven dispatch cases.
- `check_gameplay_dispatch_original.py GAME_FOLDER PROBE --report REPORT.json` replays `0x477D40` under Unicorn with strict result counts and nonzero exit on mismatch. 4,407 cases: every id `0x00..0x1FF`, five gate variants, signed values, toggled/multiplayer seeds, 300 random command cases, zoom/camera edges, the command table and all external call arguments. Current result: zero mismatches. The replay resets the x87 state before each case because Unicorn's control word drifts after the `0x49BAE6` helper.
