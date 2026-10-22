# Input state

Original: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Addresses are original virtual addresses. Read-only game files.

This stage turns per-profile bindings and raw device state into the exact action records the original feeds to the command dispatcher `477D40`, which drives the movement control bytes `C23A00`/`C23A01` that `488200` reads.

## Binding chain

| Address | Role |
|---|---|
| `41CE20–41D42F` | Apply profile bindings: 47 `(command name, DFDK.KEY action row)` pairs |
| `41CD30–41CD69` | Command index by name (`499E5E` strcasecmp) |
| `41CD70–41CDF8` | Set row binding: key word `+24`; mouse 1/2/3 → `1`/`2`/`10`; joystick never written |
| `4CF2E4` | Current profile pointer; action rows at `+6A0 + action*12` |
| `41D430`, `41D486` | Game-view start clearing `A900`/`62E2`; calls `41CE20` |
| `47A300–47A3E2` | Polled list `BD2870` = indices of rows with flags bit 2; count `BD4848`; resets queue and key state through `477D00` |
| `4FB2FC` | Command table, 121 rows of `2C`: id, name, key `+24`, mouse `+26`, joystick `+28`, flags `+2A` |

`DFDK.KEY` action rows reach the table by name: row 0 `slide_left` … row 45 `server`; row 8 (`Fire`) binds both `attack_1` and `attack_2`. 24 of the 121 shipped rows carry flags bit 2 and are polled every frame; the rest are discrete commands.

## Frame

`404340` (game-loop update) runs `479750(tick)` then `479450(tick)`.

`479750`: push `{A3, tick, 0, 0}`; drain key events `479380`; when `BD482C==0` run `479630` (mouse) and `479590` (empty); when `BD4F78==0` run `4795A0` (held keys).

| Producer | Behavior |
|---|---|
| `479380` | One record per key event. Ring `D094B0` (`498AE0` press sets `D093B0[vk]`, `498B20` pop). Mode `BD4F78` 0 command / 1 chat / other none. Filter `4790A0` first. Scan pushes the first row whose key matches, flags bit 2 clear, then restarts at the next event. Event word: key, byte1, byte2 `80` extended, byte3 shift `1`/ctrl `4` |
| `4795A0` | Polled rows with the key down push every frame. Flags `80` needs Alt (`D093C2`). When `4FCBDE>>16` (always 0 here) is up, `C23A10` is set |
| `479630` | Only when `503130==0` and `CC0884!=0` (windowed off, window active). `497D20` fills `D0934C`/`D09350`; flip `67F4FC==0` negates Y; scale `(67F500<<14)>>2` when `484740` is nonzero; value `(axis<<16 * scale)>>16`; X>0 `9E`, X<0 `9D`, Y>0 `9B`, Y<0 `9C` with positive magnitudes; then polled rows with `mouse & D09348` push |
| `47A150` | Mouse message pulses, registered by `497A30` and called from `497A40`: `2` left, `8` right (menu `67DBC0` or actor `+6C` bit 2 queue `D`), `20` middle, `100` queue `C0`, `200` queue `BF`. First matching row only, tick 0 |

The queue `BD4850` holds 32 records of `{id, tick, value, extra}` (`479FA0` push, `47A000` pop). `47A070(tick)` counts leading records with signed32 `tick <= query`, and query 0 reports all. `479450` pops every due record and calls `477D40(id, value, extra)`.

Discrete commands (`attack_1` 140, `attack_2` 141, `scope` 6, `Prone` 162, weapons, menus) fire once per key event or mouse press. Polled commands (`move_forward` 143, `move_back` 142, jump 144, look 145/146, slide 147/148, turn 149/150, `NoRun` 159, zoom, camera) push while held.

## Code

Use `FrameInput::current` for current source cells at each key and sampling
stage. Use `next_event` for the live ring. Its zero word ends the drain.
Execute `on_global_key` and `on_chat_key` bodies synchronously. Result lists
contain observations. `on_mouse_sampled` marks consumption of the axes.
Providers can change source state or append ring entries before the next read.
`command_active` accepts a table row index.

`include/dfre/input_state.hpp`, `src/input_state.cpp`:

- `CommandTable` with `find_by_name`, `set_binding`, `apply_profile_bindings`, `active_command_indices`.
- `profile_command_map()` with the 47 pairs.
- `InputQueue` with push, pop, `due_count`.
- `InputStage` with `activate`, `push_mouse_pulse`, `push_key_event`, `global_key`, `push_mouse`, `push_held_keys`, `command_active` (`47A290`), `begin_frame` (`479750`), `take_due` (`479450` live path).

```sh
/usr/bin/clang++ -std=c++17 -Iinclude src/input_state.cpp src/input_config.cpp tests/native_input_state.cpp -o build/native_input_state
/usr/bin/clang++ -std=c++17 -Iinclude src/input_state.cpp src/input_config.cpp tools/input_state_probe.cpp -o build/input_state_probe
codesign --force --sign - build/native_input_state build/input_state_probe
.local/tools/python/bin/python tools/check_input_state_original.py "/path/to/Delta Force" build/input_state_probe --report .local/reports/input-state-check.json
```

The checker replays `41CE20`, `47A300`, `4795A0`, `479630`, `479380`, `47A150`, `4790A0` and `479750`+`479450` under Unicorn 2.1.4 with stubs only for `477D40`, `4717E0`, `42AD50`, `429700`, `497D20`, `484740` and `404CF0`, then compares every result with the port: the 121 table rows, set/apply bindings, the 24-entry polled list, held keys, mouse axes/buttons, key events, pulses, the menu/chat filter and full frames. It passes with the shipped files and generated cases. Local evidence: `.local/reports/input-state-check.json`, `.local/opencode/input_state-handoff.md`.
