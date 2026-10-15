# Motion step runtime

Source: `Df.exe`, SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Addresses are original virtual addresses. This stage ports the step vector, slope response, ground/gravity update, step cache and movement animation choice from `FUN_00487370` and `FUN_00488200`.

All arithmetic follows 32-bit x86 wrap: adds, subtracts, negates, the shrd products and the terrain slope samples never overflow. `wrap_add`, `wrap_sub` and `wrap_neg` in `src/motion_step.cpp` are the only signed operations; the ground-object rotation, the `z + clearance - height` difference and every snap use them.

Units and angles stay original: 32-bit full-turn angles, 22-bit fixed sine/cosine, positions in original units.

## Ported rules

| Port | Original | Notes |
|---|---|---|
| `frame_step` | `487432` 3D, `487834` 2D, `487883` vehicle | Frame angle `[frame+0]` adds to the yaw; frame speed `[frame+8]`; frame flag `[frame+0xC]` selects the vehicle rule. 2D halves both horizontal parts (`sar 1`) and uses `definition_vertical * 0x1D2B0 >> 16` for z. 3D multiplies by the pitch cosine, then by the base cosine/sine, and halves the vertical part. |
| `frame_step` water rule | `4874A7` | Above-zero 3D vertical is zeroed when the water level is below `z + 26214.4` (the double constant at `4CDD48`). |
| `terrain_slope_response` | `4874BF–487561` | Gate: active flag, not airborne, not in water. Small slopes (`<= 14CCC`) leave the step alone, medium slopes above `B333` test `(cos*sx >> 22) + (sin*sy >> 22) > B333` for the wall bit `80000`, large slopes set the blocked bit `40000`. Blocked with large slopes pushes back by `-(slope * 0x1999 >> 16)`. |
| `cached_step` | `487991–4879CC` | Airborne actors reuse the cached `x,y` and keep the new `z`; grounded actors store `x,y,z` in the per-slot cache at `C91E90 + slot*0xC`. |
| `ground_update` | `487565–4877DE` | Ground-object offset and rotation (x uses `cos*ox - sin*oy`, y uses `sin*ox + cos*oy`, both wrapping), then `x += dx`, `y += dy`, `z += vertical + dz`. Ground snap compares `z + clearance - height` with signed zero. Airborne: snap up when short (clearing only the `2000` bit), enter water (`20000`) when the water level is above z, else subtract `B2` (local) or `B20` (other). In water: leave when z is above water, else damp vertical by `0xCCCC >> 16` (local) or `0x68DB >> 16` (other). Blocked: snap and subtract gravity. Grounded: enter water when below the water level, gravity and the airborne bit when `h > 800` or `h > 8000`, snap when short. |
| `movement_animation` | `48834E–488969` | Octants: 0 forward, 1 forward-left, 2 left, 3 back-left, 4 back, 5 back-right, 6 right, 7 forward-right; conflicting bit pairs (3, 12, 15) clear the octant. Animation ids follow the original branches for water (`8C`/`8D`), airborne (`82`–`85`, `6E`/`6F`/`70`/`74`), slope-blocked (`82`), idle (`1`/`2`/`4`/`7`/`3`/`A`–`E`/`64`–`66`/`6A`) and movement (`direction + 14/1C/28/30/3C/44/50/58/78`). Jump, prone, crouch and stand pulses apply their original z nudges (`1000`, `4000`, `8000`) and state flips. |

`sine/cosine` inputs use `fixed_sine_cosine` from `dfre/models.hpp`. The original helper is `4776C0`: `param_2` is the sine table at `BD0068`, `param_3` the cosine table at `BD0868`, both 22-bit fixed. The original tables are built at run time by `477510`; the analytic reconstruction in `models.cpp` matches them within 1 LSB on 646 of 4098 entries, so the checker feeds the same pairs to both sides through the `4776C0` hook and the step rules are compared exactly.

## Checks

Native: `native_motion_step` covers the step branches, slope paths, ground cases and animation ids. Expected values were taken from the original replay (`dump-original.py`), not from the port.

Original x86: `python3 tools/check_motion_step_original.py GAME_FOLDER PROBE --report REPORT.json` replays the original ranges in Unicorn and compares them with `motion_step_probe`. It exits non-zero on any mismatch, a probe line-count error, a missing section, or a section that did not run every case. Current result: 8280 cases, 0 mismatches, including positions, heights, clearances, slopes and sine/cosine inputs at the signed 32-bit limits.

| Section | Original | Cases |
|---|---|---:|
| `frame_step_0x487370` | `487370–487565` frame step, slope response, cache skip | 2880 |
| `step_cache_0x487991` | `487991–4879CC` step cache | 24 |
| `ground_update_0x487565` | `487565–4877DE` ground, gravity, water | 720 |
| `ground_object_0x487573` | `487573–487655` ground-object transform | 120 |
| `movement_animation_0x48834e` | `48834E–488969` animation choice and pulses | 4536 |

Hooks: `4776C0` (writes the probe sine/cosine), `42CE40`, `45DC70`, `43E7D0` (return 0), `47DF40` (case ground height), `47E5D0` (case slope samples), `484740` (case steady value). The original file is read-only. The terrain queries themselves are ported in `dfre/motion_queries.hpp` and checked by `tools/check_motion_queries_original.py`; the height map pointer the original keeps at `BD5284` is the elevation map, and `47E5D0` samples that same map.

## Not ported

- The `4876E0` clearance scan stays an explicit input; it reads definition lists at `[def+0xC]`/`[def+0x8C]`.
- Splash and footstep effects (`45DC70`, `43E7D0`), sounds and network records.
- The animation-change path at `4877F4` (`42CEF0`/`460D20`), the per-slot cache storage policy, and actor fields outside `+24..+34`, `+3C..+44`, `+6C..+6E`, `+90`, `+100`, `+108`.
- Ground height and slope queries themselves: `47DF40`, `486FD0`, `47E5D0` are inputs.
