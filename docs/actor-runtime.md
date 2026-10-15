# Actor runtime factory

Runtime actor creation for Delta Force (1998). Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Addresses are original virtual addresses. The stage is data-only: person
slots and resource handles are caller inputs, so no models or pools are
allocated here.

| Original | Role |
|---|---|
| `45D520` | factory: class item id -> definition -> new `0x114` actor slot |
| `43F610` | bind: copy six marker pose words into the actor |
| `440640` | finalize pass over the 17 placement slots |
| `4829F0` | local player actor scan and writes (actor side only) |

## Factory (45D520)

`actor_class_item_id(mode, night, side)` returns the original class id:
`138B/138C`, mode 2 `1392/1393`, night `13B4/13B5`, night + mode 2
`13B6/13B7` (`mode` = `8CF97C`, `night` = `8CF982` bit `10`). The definition
is found with `4828C0` (first record whose `+20` matches, index 0 fallback).
The first slot with a zero `+00` dword in the 256 x `0x114` table is zeroed
and filled:

| Actor | Value |
|---|---|
| `+00` | definition index |
| `+04` | definition pointer (`C23810 + index*13C`); the port keeps the index |
| `+0C` | `44A0C0` person slot handle (caller input) |
| `+10` | definition `+6C` graphic model handle |
| `+14` | 0 (factory; the husk handle is written by the object pass) |
| `+18` | `42CCE0(definition +74)` character handle (caller input) |
| `+6C` | `3` (active | reset-skip flags) |
| `+72` | `1` |
| `+74` | definition `+94` hp, u16; `67F549` bit 0 shifts it left 2 with wrap |
| `+98` | factory argument 1 (placement `+A0`) |
| `+10C` | definition `+90` move-function handle |
| `+110` | definition `+84` AI-function handle |

The high-water count is `8CFA18`; all 256 slots used returns null. The port
also builds the item table's definition lookup itself (`ActorDefinitionTable`).

## Bind (43F610)

`game_type` (`67F53C`) chooses a marker id and an ordinal:

- 0: `1771`, ordinal = placement index `%` `8CF9F4`.
- 1, 3: `1772`, ordinal = counter `%` `8CFA00`; counter `4F820C` advances.
- 2, 4, 5: actor `+70` == 1 -> `1773` `%` `8CF9F8`, else `1774` `%` `8CF9FC`;
  counter advances.
- above 5: scans for definition index 0 (nothing copies).

The scan walks the `8ACE80` marker table (`8CF900` records of `0x114`) for
the requested definition index; the ordinal-th match contributes six dwords
at `+24..+3B` to the actor. The counts have no proven writer, so the caller
supplies them.

## Finalize pass (440640)

With `67F528 != 0`, each of the 17 placement slots (table `6E9A90`, stride
`9B84`) with a nonzero `+18C` byte creates an actor from `+A0`/`+A4`, stores
it at `+08`, copies `+00` to actor `+08`, `+9C` to actor `+70` (u16), binds
the pose, and copies the `+0C` name into the person record at `+4`. The port
returns `ActorSpawn` rows for the caller to apply to its person records.

## Definition init (4829F0)

The actor side of the local player init: the first actor with `+6C` bit 0
set and `+08` equal to `50133C` (original `C23820`, `C23A38`) receives
`+94` = the current weapon slot map value, `+98` = `67F430`, and the
`67F420` name goes to the person record. The `C23820` runtime record
(camera height `B333`, loadout values) stays with the player/camera modules.

## Checks

```sh
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Iinclude tools/actor_runtime_probe.cpp src/actor_runtime.cpp -o actor_runtime_probe
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Iinclude tests/native_actor_runtime.cpp src/actor_runtime.cpp -o native_actor_runtime
.local/tools/python/bin/python tools/check_actor_runtime_original.py "/path/to/Delta Force" actor_runtime_probe [--report report.json]
```

2026-10-16 (Unicorn 2.1.4): 452 comparison cases, 0 mismatches. Sections:
class selector 30, create 202 (including 256-slot fill), bind 80, finalize
80, definition init 60. Hooks: `44A0C0` and `42CCE0` return caller handles,
`484970`/`483DD0` weapon table work is skipped, `+04` is compared as a table
index. `native_actor_runtime` passes at `-O2` and under ASan+UBSan.
Full output: `.local/reports/actor-runtime-original-check.json`.
