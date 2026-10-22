# Motion clearance

Source: `Df.exe`, SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Addresses are original virtual addresses.

`include/dfre/motion_clearance.hpp` supplies the ground clearance the walking and gravity update needs, plus the entity ground query behind it.

## Field labels (corrected)

The enclosing `0x487370` code (`animation-runtime-callers.json`) supplies the CHR instance, current animation, frame, and root position list.

| Original | Meaning |
|---|---|
| `local_2c = actor +0x18` | CHR instance |
| `local_28 = instance +0x10` | current frame |
| `local_24 = instance +0x0C` | character definition |
| `definition +8` | pose count |
| `instance +0x8C` | pose position list, 12-byte records |
| `current frame +4` | pose bit mask |

## Ported rules

| Port | Original | Rule |
|---|---|---|
| `ground_clearance` | `4876E0–487A9A` | For each pose record, a clear frame bit at `16 + index` (the x86 `shl` uses `count & 31`, so bones beyond 15 reuse low bits) qualifies; the candidate is `(record word 1 * 0x1D2B0) >> 16` and the result is the signed minimum, starting at `0xFFFFFFF`. |
| `ground_entity_height` | `486FD0` | Walks the actor's entity list (`+0x104`, count `+0x108`), picks the alternate definition when `entity +0x6C` bit 8 is set and `+0x14` is non-null, gates on the definition radius (`+0x64`) around the entity position, then walks the piece list once: instance flags at `+0x3C` (bits 0 and 1 skip), the `0x45CE60` query point against the part bounds (`+0x40`–`+0x4C`), the `0x70`-stride part records' volume lists (count `+0xC`, records `+0x10`, `0x48` stride), each volume's words 11–14 bounds (`+0x2C`–`+0x38`), the material skip bit at `(volume +0x44) +0x13` bit 0, then the accept rule: reject `z < current`, reject `position.z + depth < z`, else store the entity at actor `+0x100` and take `z` as the new height. |

The scan starts from `0x47DF40` terrain height in the entity query, so `terrain_height` is an input. All arithmetic is 32-bit wrap (`wrap_add`, `wrap_sub`, `wrap_neg`) and the products use the original `shrd` truncation.

## Explicit inputs

| Input | Why |
|---|---|
| `GroundPiece::point` | The original gets it from `0x45D170` + `0x45CE60`, which the collision and model modules own. |
| `GroundVolume::accepted`, `GroundVolume::height` | `0x42D2A0` (point-in-face, ported in `dfre/collision.hpp`) and `0x45CDA0` (sub-part surface) verdicts. |
| `GroundDefinition::radius` | Definition `+0x64`. |
| `GroundVolume::skip` | Material record `(volume +0x44) + 0x13` bit 0. |
| Vertex table at `part +0x18` | Only read by the mesh stage; not part of the walk. |

No limb semantics are inferred: the pose record's second word is used exactly as the original reads it, and the geometry stages stay inputs.

## Checks

Native: `native_motion_clearance` checks the scan (empty list, signed words, mask bits, 33-bone wrap) and the entity walk (accept, skip flags, radius gate, depth gate, lower surface, second entity, alternate definition) against values read from the original replay.

Original x86: `python3 tools/check_motion_clearance_original.py GAME_FOLDER PROBE --report REPORT.json` replays `0x4876E0–0x487734` and `0x486FD0` in Unicorn with `0x45D170`, `0x45CE60`, `0x45CDA0` and `0x42D2A0` hooked to the same values the probe receives. It asserts the stop address and a bounded instruction count, requires every case to run, and exits non-zero on a mismatch, a missing probe line, an unexpected stop, or an entity section that never accepts an entity. Current result: 244 cases, 0 mismatches, 13 accepted entities.

| Section | Original | Cases |
|---|---|---:|
| `clearance_0x4876e0` | `4876E0–487734` | 144 |
| `entity_0x486fd0` | `486FD0` | 100 |

The original file is read-only. The definition callback at `+0x4C` is stubbed at `0x1F00000` and returns the entity's instance records, matching the original call.
