# Input config and action map

Original: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Addresses are original virtual addresses. Read-only game files; the port never writes df.cfg itself.

## df.cfg

The settings block is 0x5c4 bytes at `4CED20`.

| Address | Role |
|---|---|
| `407120–407354` | Defaults, then `4568E0("df.cfg", 406640)`, then `407040` |
| `4568E0–4569DE` | Line driver: `fopen(path,"r")`, `fgets` 0x2bc, cut at `0x0A`, tokenize, skip empty and `/` comments, call back per line |
| `4565F0–456715` | In-place tokenizer in the reused `8E4040` buffer; space, tab and comma split; `"` groups and is removed; 30 tokens; the terminating NUL is processed and can start an empty token |
| `406640–406F3A` | Key dispatcher: case-insensitive `499E5E` compares, `49AFA0` atoi, string copies, `49A150` strupr for the five password fields |
| `407360–40784C` | Writer: exact format strings `4C86A8–4C8BD8`, `fprintf` then `fclose` |

51 saved keys in file order: cd_play, menu_track, game_track, cd_volume, fx_volume, dialog_volume, game_res, window_size, windowed, renderer, max_res, gamma, turbo, flipmouse, crosshairs, mousescale, showgun, picinpic, login_password, game_password, squad_password, game_name, phone_number, internet_address, mp_service, mp_gametype, maxplayers, dedicated, com_port, baud_rate, max_lives, max_kills, team_req, attributes, replay, time_limit, koth_limit, ammo_respawn, timeout, gpsicon, showtags, contest, map_infrared, map_iff, map_AWAC, preempt_pff, whichbrowser, country, red_password, blue_password, mp_verbose.

Defaults (`407120`): volumes 255/255/200, tracks 0/2/2, game_res 3, window_size 1, windowed 0, renderer `family>5`, max_res `(family>5)+3`, gamma 128, turbo 0, mouse 0/1/128, gun 1/0, internet_address `0.0.0.0`, mp_service 4, maxplayers 16, max_lives 20, max_kills 20, team_req 1, attributes 2, time_limit 30, koth_limit 30, ammo_respawn 2, gpsicon 1, maps 1, mp_verbose 1. `4F8704` holds the CPUID family byte.

Load semantics that the port reproduces: a line must split so that the first token is the key and the third token is the value (`gamma = 7`). `gamma=7` is one token and never matches. Keys compare case-insensitively. Values use the wrapping 32-bit atoi; `maxplayers` clamps above 0x10 to 0x10 and keeps negatives. A line with fewer than three tokens re-reads the previous third-token position in the reused buffer, so a later line can pick up stale bytes; the value after a closing quote on CRLF files is a single carriage return, which is what the shipped df.cfg produces for every `""` field. String fields are copied without a length check; the port keeps full strings instead of spilling into the next field. Field capacities: passwords 16 (red, blue) or 32, squad 48, game_name 36, country 4, phone 32, internet 32.

## DFDK.KEY action map

Shipped in `Dfbase.pff`; 46 rows.

| Address | Role |
|---|---|
| `407040–407116` | Loader: open, section, read `KEY` lines, `strtok(", ")` + atoi into the table |
| `49B010` | Config-file open: CRLF becomes `\n` + NUL, one line counted per pair, tabs become spaces, every byte uppercased (`49BF20`) |
| `49B230`, `49B580` | `[KEYS]` section scan, line reader; reads only as many NUL-terminated strings as there were CRLF pairs |
| `4E7DDC` | Table, 46 rows of 12 bytes: key code, mouse button, joystick button |
| `4E7AEC`, `681E20+i*0A90+10` | 0x228-byte copies for the key menu and per-profile data |
| `4C869C`, `4C8674` | `df.cfg` and `dfdk.key` names |
| `41CE20` | Later applies the rows to the `4FB2FC` command table by name (see [input state](input-state.md)) |

Row format `KEY=index,key,mouse,joy`; comments start with `;`; the section ends at the next `[` line. A file with no CRLF pair loads nothing, and lines after the last CRLF pair are not read. mouse: 0 none, 1 left, 2 right, 3 middle. Action IDs and shipped defaults (names are file comments):

| ID | Name | key | mouse | ID | Name | key | mouse |
|---|---|---|---|---|---|---|---|
| 0 | Move Left | 37 | 0 | 23 | Zoom In | 187 | 0 |
| 1 | Move Right | 39 | 0 | 24 | Zoom Out | 189 | 0 |
| 2 | Move Forward | 38 | 0 | 25 | Talk | 84 | 0 |
| 3 | Move Back | 40 | 0 | 26 | Squad Talk | 89 | 0 |
| 4 | Go Prone | 34 | 3 | 27 | Next waypoint | 87 | 0 |
| 5 | Crouch | 35 | 0 | 28 | Audio Command | 65 | 0 |
| 6 | Stand | 46 | 0 | 29 | Resolution | 86 | 0 |
| 7 | Jump | 32 | 0 | 30 | Drop | 68 | 0 |
| 8 | Fire | 17 | 1 | 31 | Change Magazine | 77 | 0 |
| 9 | Scope | 83 | 2 | 32–35 | Look Up/Down/Right/Left | 104,98,102,100 | 0 |
| 10 | Walk | 16 | 0 | 36 | Cycle FOB | 9 | 0 |
| 11 | Jump | 8 | 0 | 37 | Mission Briefings | 79 | 0 |
| 12 | Binocs | 66 | 0 | 38 | Recent Messages | 82 | 0 |
| 13–15 | Primary Weapon Func 1–3 | 49,50,51 | 0 | 39 | Friendly Tags | 70 | 0 |
| 16–18 | Secondary Weapon Func 1–3 | 52,53,54 | 0 | 40 | Goals | 71 | 0 |
| 19–20 | Claymore/Satchel Charge | 55,56 | 0 | 41 | Command Map | 67 | 0 |
| 21 | Frag Grenade | 57 | 0 | 42 | Turbo Mode | 72 | 0 |
| 22 | Laser | 48 | 0 | 43 | Next Flag | 81 | 0 |
|  |  |  |  | 44 | Previous Waypoint | 69 | 0 |
|  |  |  |  | 45 | Server Screen | 36 | 0 |

Joystick defaults are 0 for every row. The game table starts zeroed; rows missing from the file stay zero. The port rejects rows with fewer than four fields or an index outside 0..45 where the original would read stale data or write past the table.

## Hotkey command table

`4FB2FC`, 121 rows of 0x2C: id, name[0x22], key `+24`, mouse `+26`, joystick `+28`, flags `+2A`. `47A2A4` evaluates a row against keyboard state `D093B0`, mouse bits `D09348` (left 1, right 2, middle 0x10) and joystick bits `D09944`. Rows 142/143/144/161/162/164 repeat the DFDK movement key codes; row 162 Prone encodes the middle button as mouse bit 0x10 where DFDK.KEY uses 3.

## Code

`include/dfre/input_config.hpp`, `src/input_config.cpp`: `GameConfig` with defaults, load and save text; `ActionBindings` with the shipped defaults and the DFDK.KEY reader; `hotkey_commands()` and `hotkey_active()`. The probe `tools/input_config_probe.cpp` prints JSON for a game folder, an optional `--config` or `--keys` file, and `--cpu-family N`.

```sh
/usr/bin/clang++ -std=c++17 -Iinclude src/input_config.cpp tests/native_input_config.cpp -o build/native_input_config
/usr/bin/clang++ -std=c++17 -Iinclude src/input_config.cpp src/assets.cpp src/filename.cpp tools/input_config_probe.cpp -o build/input_config_probe
codesign --force --sign - build/native_input_config build/input_config_probe
.local/tools/python/bin/python tools/check_input_config_original.py "/path/to/Delta Force" build/input_config_probe --report .local/reports/input-config-check.json
```

The checker replays the original x86 under Unicorn 2.1.4 with stubs only for the file helpers and the CRT scan engine: it runs `407120` for defaults, `4565F0`/`406640` per df.cfg line, and `49B010`+`407040` for DFDK.KEY (including the real CRLF/tab/uppercase preprocessing), then compares all 51 df.cfg values, the 46 action rows and the 121 hotkeys. It reads the installed df.cfg, the shipped DFDK.KEY from `Dfbase.pff`, and generated edge and random cases. Current result: pass, with long country values recorded as original buffer-overflow notes. Local evidence: `.local/opencode/input_config-handoff.md`, `.local/reports/input-config-check.json`.
