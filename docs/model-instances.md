# Model instances and static object matrices

`model_instances.hpp` and `model_instances.cpp` rebuild the original
mesh-instance matrices used to draw loaded world placements. Targets: `Df.exe`
SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Original entries, all replay-checked in
`tools/check_model_instances_original.py`:

| Original | Port |
| --- | --- |
| `0x45d1e0` instance local matrix | `model_instance_local_matrix()` |
| `0x45d2f0` scaled instance local matrix | `model_instance_local_matrix_scaled()` |
| `0x45cb40` matrix product | `model_multiply_matrix()` from `dfre/models.hpp` |
| `0x480ff0` crng matrix callback slots | `model_instance_local_matrices()` |
| `0x481060` crng frame counter | `model_instance_animation_frame()` |
| `0x481060` crng draw decision | `build_model_instance_draw()` |
| `0x46b830` static object draw decision | `build_model_static_draw()` |

## Instance fields

One instance block is object `+0x24`: translation `+0x24/+0x28/+0x2c` (16.16)
and full-turn angles `+0x30` (yaw), `+0x34` (pitch), `+0x38` (roll). `0x45d1e0`
composes roll, then pitch, then yaw, then the translation; the replay proves it
is exactly `model_local_matrix()` from `dfre/models.hpp` for the same fields.
`0x481060` and `0x46b830` then run `0x45cb40(world_to_camera, local, local)`,
which is exactly `model_multiply_matrix()`; `0x480ff0` keeps the local matrix
without the camera product.

`0x45d2f0` starts from a scale matrix `diag(scale << 6)` and multiplies each
axis rotation in the same order, then the translation matrix: `T x R x S`. The
original rounds after every product, so the port keeps that order instead of
scaling a finished local matrix. Its observed callers are the `face` (`0x480f40`)
and `ef3d` (`0x480e70`) matrix callbacks; those tag paths stay outside this
module.

`localize_model_lod()` from `dfre/models.hpp` supplies the mesh pivots
(`origin`, `delta_origin`) for every path here; the generic/static paths use the
same localized LODs as the visibility and mesh draw modules.

## Runtime LOD records

The loader `0x485e30` reads one 184-byte LOD header into a runtime record and
patches pointer tables. The draw callbacks read:

| Field | Use |
| --- | --- |
| `+0x40` dword | frame count; the whole count is `raw >> 16` |
| `+0x58` dword | frame duration, `raw >> 16`; the high word `+0x5a` is the non-zero guard |
| `+0x98` | mesh count |
| `+0x9c` | mesh array, `0x70`-byte entries in file order |

`ModelLod` keeps the same order and `raw_header[0x40..0x43]`/`[0x58..0x5b]`
carry those two dwords, so the decoder needs no change. Stock values show the
count/duration words are almost always zero; `M16.3DI` uses count 3 and
duration 5 on every LOD, `AK47.3DI` LOD 1 has duration 0 (counter untouched),
and `BARRET.3DI` LOD 3 has count 0 and duration 0.

`model_instance_animation_frame()` implements the `0x481060` rule
`(time / (duration >> 16)) % (count >> 16)` with the original unsigned `div`
and signed `idiv`. A zero duration returns `nullopt`: the original leaves the
global `0xad3cf4` unchanged (it is cleared to zero after the mesh loop). A zero
count with a non-zero duration, or the `INT32_MIN % -1` overflow, throw
`std::invalid_argument`; the original raises the divide error there.

## Generic draw (`crng`)

`0x481060(record, clip_mask, object)` is the live `crng` callback dispatched by
`0x46b780` for generic models (callback table row `0x500c80`, tag `0x676e7263`):

1. Sets `0xad3cf4` from the record's frame words and the global time `0x6e471c`.
2. Builds the local matrix from `object+0x24`, then `0x8d1340 x local` in place.
3. Draws every mesh of the record (`+0x98`/`+0x9c`) through `0x46b1d0` with
   clip mask and level 0.
4. Clears `0xad3cf4`.

`build_model_instance_draw()` returns the frame value, both matrices, the mesh
count, and one camera matrix per mesh. The original passes its single stack
slot as the array base, so every mesh draws with slot 0; the port repeats that
matrix per mesh so `draw_model_mesh32()` can index one matrix per mesh without
changing any drawn value.

`0x480ff0(record, object)` is the `crng` matrix callback: slot 0 is the
`0x45d1e0` output and slots 1.. `mesh_count-1` copy it; no camera product.
It also clears the dword at `0xc22bbc` (slot 0's padding word). Both are
replay-checked.

## Static draw (`0x46b830`)

`0x46b830(object, screen_radius, clip_mask)` is the standalone static-object
draw:

1. Flag byte `+0x6c` bit 2 with a non-null `+0x14` selects the secondary model,
   otherwise `+0x10`; a null model returns.
2. Builds the local and camera matrices (always, before the radius gate).
3. Radius `<= 0x8000` draws nothing. Otherwise the file thresholds
   `model+0x18..+0x24` select a LOD exactly like `0x46b780`
   (`select_model_level` from `dfre/model_visibility.hpp`).
4. Fills matrix slots `1..first-LOD mesh count-1` with copies of slot 0. The
   fill count always comes from the `+0x50` record, even when a later record is
   selected; `FIR2.3DI` (mesh counts 1/2/1) exercises that shape.
5. Draws every mesh of the selected record (`+0x50/+0x54/+0x58/+0x5c`) through
   `0x46b1d0` with clip mask and level 0.

The fill read of `+0x50` and the selected record read have no null check, so a
null record faults in the original; the port throws `std::invalid_argument`.
This escapes when a model has fewer than four LODs and a zero later threshold
can select the empty record (`stock_static` covers three real models with that
shape). The shipped image has no direct or stored reference to `0x46b830`
(full-image `E8`/`E9` and dword scan), so its runtime reachability is a caller
question; the port keeps the body exact.

Observed cache bounds: the original stack buffer holds 32 slots; all stock
mesh counts are 18 or below, so a caller-supplied count above 31 would smash
the original stack.

## Caller recipe for loaded world placements

Generic path (the live `0x46b780` dispatch):

1. Collect placements with `place_single_player_objects()`; each
   `WorldPlacement::pose` is the `ModelInstanceFields` block.
2. Resolve the level callback with `select_model_draw()` from
   `dfre/model_visibility.hpp`. The `crng` row is
   `{0x676e7263, 0x481060, 0x480ff0}` (`find_model_callback_tag`).
3. Build the record from `Model::lods[level]`: `mesh_count =
   lods[level].meshes.size()`, frame words from `lods[level].raw_header`.
4. `build_model_instance_draw()` and draw each mesh with
   `draw_model_mesh32(lod, index, draw.matrices, ...)`.

Static path: fill `ModelStaticObject` from the object and model records and
call `build_model_static_draw()`; draw the returned mesh list the same way.

## Verification

`tools/check_model_instances_original.py` replays the original x86 with
Unicorn. The five `0x46b1d0` call sites inside `0x46b830`/`0x481060` are
redirected to a one-byte `ret` in emulator memory only; a code hook records the
four stack arguments plus `0xad3cf4` at each call. Every case asserts the
return trap, the untouched matrix tail bytes, the `0xad3cf4` clear, and every
draw record (mesh pointer, matrix pointer, clip mask, level). Fault cases
assert the exact faulting instruction against the native exception.

Report: `.local/reports/model-instances-original-check.json` (zero mismatches).

- `0x45d1e0` and `0x45cb40` 1,200 instance cases and 600 product cases.
- `0x45d2f0` 500 scaled cases.
- `0x481060` counter 603 cases (101 divide faults) and draw decision 800 cases
  (80 faults); `0x480ff0` 1,215 slot cases.
- `0x46b830` 900 generated cases (95 faults).
- Stock fixtures: all 355 v5 models for `0x46b830` (1,780 cases, 5,391 draws,
  three null-record faults), all 815 LOD records of the 276 `crng` models for
  `0x480ff0` and `0x481060` (44 active counters, 771 inactive).

```sh
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude \
  src/model_instances.cpp src/model_visibility.cpp src/models.cpp \
  tools/model_instances_probe.cpp -o .local/model_instances_probe
codesign --force --sign - .local/model_instances_probe
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude \
  src/model_instances.cpp src/model_visibility.cpp src/models.cpp src/renderer.cpp \
  tests/native_model_instances.cpp -o .local/native_model_instances
codesign --force --sign - .local/native_model_instances
.local/native_model_instances
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude \
  src/model_instances.cpp src/model_visibility.cpp src/models.cpp src/renderer.cpp \
  tests/native_model_instances.cpp -o .local/native_model_instances_san
codesign --force --sign - .local/native_model_instances_san
.local/native_model_instances_san
.local/tools/python/bin/python tools/check_model_instances_original.py \
  '/Users/khanh/Desktop/workspace/Delta Force/Df.exe' \
  --probe .local/model_instances_probe --models .local/models \
  --report .local/reports/model-instances-original-check.json
```

Evidence: REA `ev_c6ef2259331fa563f35f5e15a05130fb601aa97245ad0f610c392090b2533648`
(`0x45d1e0`, `0x45cb40`), `.local/reports/callbacks-lobby-audio-20261016.json`
(`0x481060`, `0x46b830`), `.local/reports/model-instance-draw-assembly.txt`
(`0x46b780`, `0x46b830`), and the visibility evidence for the callback table
(`ev_b60ed2651d43b8c94c884df1165fe880e0799882a10d9110fb68ed4214e7499e`).

The replay is static. Runtime checks against the game, the special tag
callbacks, and the `0x46b830` caller remain open; see
`.local/opencode/model_instances-needs.md`.
