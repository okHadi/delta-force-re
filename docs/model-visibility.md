# Model visibility and LOD selection

`model_visibility.hpp` and `model_visibility.cpp` rebuild the original scene
model selection, frustum bounds, clipping masks, and LOD choice used by the
`0x485610` world frame. Targets: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Pass order in `0x485610`: `0x485290` collect, `0x485420` view setup and large
list, sky/ground, `0x4854d0` cull, `0x485580` draw. Draw order, actor
animation, effects, terrain, and water stay outside this module.

## Queues

Three lists of 24-byte records, cleared by `0x485290` each frame:

| List | Base | Count | Records | Drawn by |
| --- | --- | --- | --- | --- |
| Large | `0xc23e70` | `0xc54478` | object, radius, mask | `0x485420` |
| Near | `0xc3cd70` | `0xc54474` | full screen fields | `0x485580` |
| Character | `0xc3b570` | `0xc54470` | full screen fields | `0x485580` |

Record fields: `+0x00` object pointer, `+0x08` screen radius (`0x8d1384` =
camera `+0x1964`), `+0x0c` clip mask, `+0x10` int16 screen x, `+0x12` int16
screen y, `+0x14` camera depth. The draw passes skip records whose object is
null. The large list fills only the first three fields and is trusted not to
hold null records (`0x485420` has no null check, unlike `0x485580`).

`0x485290` collects from four record arrays with stride `0x114`: `0x7cca80`
(count `0x8cfa18`), `0x7dde80` (count `0x8cfa1c`), `0x822e80` (fixed `0x400`),
`0x867e80` (count `0x8cf904`). It then walks 512 records at `0x9f3098` gated by
`[record+0xfc] > 0` and `[record] != 0`.

## Collection (`0x485040`, `0x485290`)

1. Skip when the model pointer (`+0x10`) is null, when flag byte `+0x6c` bit 2
   is set, or when `|position.x - camera.x|` or `|position.y - camera.y|`
   exceeds `0x3fc` (1020 whole units). Position is `+0x24` in 16.16 and camera
   position is `0x8cfa24`/`0x8cfa28`.
2. Transform `+0x24` through the world-to-camera matrix `0x8d1340`
   (`0x45cda0`) and skip when `|camera depth >> 16| >= 0x3fc`.
3. Test the sphere (`0x44acf0`) and skip on the outside result.
4. Generic records (`[[+4]+0x28] != 3`): radius `[model+0x60]`; radius above
   `0x640000` (100.0 px) goes to the large list, otherwise the near list.
   Character records go to the character list with a fixed `0x10000` radius and
   also write their class pointer to `[record+0x8c]`. The `0x9f3098` loop has no
   hidden check and always fills the near list.

The 3DI file header supplies the query fields: bounding radius at `+0x60`,
LOD far thresholds at `+0x18..+0x24`, and callback tags at `+0x28..+0x34`
(`crng`, `org0`). The decoder already exposes the last two as
`Model::lod_thresholds` and `Model::render_types`.

## Sphere projection (`0x44acf0`)

Camera fields: focal `+0x54` in whole pixels (the original shifts it left 16),
screen centres `+0x60`/`+0x64` in 16.16, five 16-byte plane records at `+0x1880`
as `{nx, ny, nz, offset}`. The test uses x as depth:

- Plane 0: `t = (nx * depth) >> 22 - offset`; `t < -r` rejects; `t <= r` sets
  bit 2; `t + r > 0x3fc0000` (1020.0) rejects.
- Planes 1 and 2 use `nx * depth + ny * y`; planes 3 and 4 use
  `nx * depth + nz * z`. Each term shifts by 22 before adding. Offsets of
  planes 1-4 are not subtracted.
- Masks: 2 near, 4 and 8 side, 0x10 and 0x20 top or bottom. The mesh
  dispatch clips a face only against planes whose bit is set (`0x46a930`,
  dispatch order 0, 1, 2, 4, 3). Mask 0 or bit 0x40 skips plane clipping
  entirely.
- `depth < radius`: the camera is inside the sphere; the original returns mask
  `0x3e`, stores radius `0x4000000`, and leaves the point unwritten.
- Otherwise the point is rewritten: `x = centre_x - (scale * y) >> 16`,
  `y = centre_y - (scale * z) >> 16`, `depth` unchanged, with
  `scale = ((2^32 / depth) * (focal << 16)) >> 16` and radius
  `(scale * radius) >> 16`. Depths below 2 (16.16) are the original division
  overflow; the module throws instead.

## Cull and draw passes

`0x4854d0` runs `0x484fb0` per record and clears the object field on zero.
`0x484fb0` clamps the record screen box to view bounds `0x8cfa54` (left),
`0x8cfa58` (top), `0x8cfa5c` (right), `0x8cfa60` (bottom), then:

1. `row = max(y - radius, top)`; `row > bottom` is invisible.
2. `row_keys[row] <= depth` is invisible (`0xc93ea0`, 32-bit key per row).
3. `first = max(x - radius, left)`, `last = min(x + radius, right)`;
   `last < first` is invisible.
4. Invisible unless some column in `first..last` has
   `row_words[row][column] >= uint16(depth >> 10)` (`0x8d0688` row pointers).
   The y-from-above case is not tested; an object entirely above `top` still
   tests the top row.

The original reads x and y through packed dwords (`+0x0e` and `+0x10`), which is
exact while the clip mask stays below `0x10000`.

The character list additionally clears the record when its object equals
`[0x6ba3f0]` while `[0x6ba3ec] == 0` (`0x466cd0` sets these around the scene
call to hide the local player body). The near list has no such rule.

## View and LOD selection (`0x46b780`)

Flag byte `+0x6c` bit 8 selects the secondary model at `+0x14`, else the
primary at `+0x10`. A null model returns. The selection value is the projected
screen radius in 16.16 pixels:

- `<= 0x8000` (0.5 px): no draw.
- `> threshold[0]`: slot 0, else `> threshold[1]`: slot 1, else `> [2]`: slot 2,
  else `> [3]`: slot 3, else no draw.
- Slot 0 uses file LOD 0 (the most detailed); thresholds descend with detail.

Each slot calls `[model+0x3c+4*slot]` with `([model+0x50+4*slot], mask, object,
slot)`. A null slot data pointer skips the call. Callbacks come from
`0x4818f0` by tag (`0x481a00` for the matrix at `+0x4c`): first matching row of
the proven 16-row table at `0x500c1c` (stride `0x14`, count `0x500d54`),
otherwise row 0. The table bytes and every key are in `model_callback_tags`,
with `find_model_callback_tag` and `model_callback_rows` as the typed lookup.
Keys are the stored 4-byte tags; read right to left for the name
(`0x6f726730` is `org0` at row `0x500cd0`). The older `0x500cc8` note does not
align with a row base of the proven table.

`draw_model_mesh32` integration: per queued model, resolve the localized LOD by
slot, build the object matrix from `+0x24` (as `0x45d1e0` does), multiply by the
camera matrix `0x8d1340`, clip faces only with the mask planes, and draw the
meshes through the existing renderer path. `0x46b1d0` also selects its matrix
slot with the level index.

## Open items

The caller supplies view bounds and row tables. `TerrainFrame32.row_depths`
contains the original `0xc93ea0` keys from `0x489260`; depth buffer rows supply
the `0x8d0688` words. The caller also supplies the local player to hide.

## Verification

`tools/check_model_visibility_original.py` replays the original x86 with
Unicorn. Canonical bodies `0x484fb0`, `0x485040`, `0x485290`, and `0x44acf0`
are confirmed by REA evidence `ev_b60ed2651d43b8c94c884df1165fe880e0799882a10d9110fb68ed4214e7499e`.
Cases: `0x44acf0` (1500), callback table and `0x4818f0`/`0x481a00` (16 keys,
16 near-miss words, 64 random keys), `0x46b780`/`0x485580` LOD (750), `0x485040`
(750), `0x485290` (250), `0x484fb0` (1600), `0x4854d0` (1600), draw pass (120
batches), and large pass through `0x485420` (120 batches). The four level
callbacks are one-byte `ret` stubs whose stack arguments are recorded; all other
code is original image code. Zero mismatches.

```sh
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude \
  src/model_visibility.cpp src/models.cpp src/renderer.cpp \
  tools/model_visibility_probe.cpp -o .local/model_visibility_probe
codesign --force --sign - .local/model_visibility_probe
/usr/bin/clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude \
  src/model_visibility.cpp src/models.cpp src/renderer.cpp \
  tests/native_model_visibility.cpp -o .local/native_model_visibility
codesign --force --sign - .local/native_model_visibility
.local/native_model_visibility
.local/tools/python/bin/python tools/check_model_visibility_original.py \
  '/Users/khanh/Desktop/workspace/Delta Force/Df.exe' \
  --probe .local/model_visibility_probe --output .local/reports/model-visibility-original-check.json
```

The replay is static. Runtime checks against the game remain open.
