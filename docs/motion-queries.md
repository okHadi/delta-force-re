# Motion queries

Source: `Df.exe`, SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Addresses are original virtual addresses.

`include/dfre/motion_queries.hpp` wraps the terrain queries the motion step needs and states every unresolved query as an explicit input.

## Ported queries

| Port | Original | Rule |
|---|---|---|
| `ground_height_query` | `47DF40` | Bilinear elevation sample. `x` and `y` are world words; the cell index is `(pos >> 16) & 0x3FF`, the fraction is `pos & 0xFFFF`, the four corners come from the elevation map at `(x,y)`, `(x+1,y)`, `(x,y+1)`, `(x+1,y+1)`, the interpolated byte value is multiplied by `0x40`. The original keeps the map pointer at `BD5284` and the map is 1024×1024 with a `0x400` row stride. This is `TerrainGeometry::base_height`, already used by the player runtime and terrain checks. |
| `ground_slope_query` | `47E5D0` | Two elevation byte differences at ±2 cells on the same map: `x = (byte[x+2] - byte[x-2]) * 0x1000`, `y = (byte[y+2] - byte[y-2]) * 0x1000`. This is `TerrainGeometry::slope`. |
| `ground_height_for_actor` | `48775F` | Height source choice: actors without a ground entity (`+0x108 == 0`) use `47DF40`; actors with one use `486FD0`. |

The original `47DF40` and `47E5D0` read the map through the pointer at `BD5284`, not with the map inline at that address. Both samples are used with 22-bit fixed sine/cosine products in `motion_step`.

## Explicit inputs

| Unresolved query | Why it stays an input |
|---|---|
| Minimum ground clearance | The `4876E0` scan walks definition lists at `[def+0xC]` (count) and `[def+0x8C]` (entries with a `+4` value and `0xC` stride) and tests bits from `[def+0x10]+4` starting at bit 16. The scan is not reconstructed; callers pass `GroundQuery::clearance` (`0xFFFFFFF` when the list is empty). |
| Actor ground entity height | `486FD0` walks the actor's sub-entity list (`+0x104`, `+0x108`), calls per-object virtual functions and intersects parts. Callers pass `ActorGroundQuery::entity_height`. |
| Steady predicate | `484740` returns `[C23A24]` only for an active slot with no movement or look bits set and `C23B64 == 0`. `movement_animation` takes it as the `steady` argument. |
| Frame time | `4718D0` returns `(frames << 4) / 1000` only when `67F520 != 0` and `67F528 == 0`; in single player it is zero. |

## Checks

Native: `native_motion_queries` builds a 1 MiB stripe map (`byte(x,y) = (x*3 + y*5) & 0xff`), checks height, slope and the height-source branch against values read from the original replay.

Original x86: `python3 tools/check_motion_queries_original.py GAME_FOLDER PROBE --report REPORT.json` writes two deterministic 1 MiB maps (random seed 7 and the stripe pattern), points `BD5284` at them, replays `47DF40`, `47E5D0` and the `48775F` branch in Unicorn, and compares them with `motion_queries_probe`. It exits non-zero on a mismatch, a missing probe line, a changed section set, or a section that did not run every case. Current result: 416 cases, 0 mismatches.

| Section | Original | Cases |
|---|---|---:|
| `height_0x47df40_map-random` / `map-stripe` | `47DF40` | 54 + 54 |
| `slope_0x47e5d0_map-random` / `map-stripe` | `47E5D0` | 54 + 54 |
| `actor_height_0x48775f` | `48775F` with `486FD0` hooked | 200 |

Positions include `0`, `0xFFFF`, `0x10000`, cell edges, negatives, `0x7FFFFFFF`, `0x80000000` and 40 random pairs. The original file is read-only.
