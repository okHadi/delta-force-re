# Model material bindings

`prepare_model_bindings32()` ports the original 32-bit model texture and material preparation:
original `485E30` load writes, the `480600` bank palette install, and the `486880` texture remap
and binding pass. It returns prepared copies; the source `Model` stays unchanged.

Target: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01f651ff`.

## API

Header `include/dfre/model_bindings.hpp`, source `src/model_bindings.cpp`.

- `model_texture_span32(model, level)` returns the loaded texture slots for one level, in file
  order. Original `485BA0` reads every texture block and keeps only those whose stored flags byte
  has bit `1 << level`.
- `prepare_model_bindings32(model, bank, level, bank_palette, gamma)` returns
  `ModelPreparedModel32`: prepared textures, one material list per LOD, and a pointer to the bank
  palette that must outlive the value. `model.raster_material(lod, index)` returns a
  `RasterMaterial32` for `draw_model_triangle32()`; the first call for a texture copies that
  texture and the bank shade rows into a cached `RasterTexture32`.
- `apply_model_bindings32(lod, prepared_lod)` and `ModelPreparedModel32::apply(lod_index, source)`
  return a copied `ModelLod` with the prepared material runtime fields applied: `raw`, `flags`,
  `frame_count`, `animation_parameter`, `flat_rgb`, and `texture_key`. Geometry, meshes, faces,
  normals, and the source stay unchanged. `prepare_model_face()` reads a LOD material list, so
  this is how cutout, missing-texture fallback, effect, and animation state reach face routing.
- The bank palette supplies `matching_rgb` (the table `480600` builds from the display palette)
  and the 64 `shades` rows from `4809B0`. `gamma_rgb` and `display_rgb` are not read here.
- Material texture slots are model local (the original stores the global loaded-texture index).

`ModelPreparedTexture32` keeps the loaded record fields (name, pixel count, full sequence, flags,
bank, width, height, stored RGB), the `486880` remap table, the most frequent nonzero texel, and
the remapped pixels.

`ModelPreparedMaterial32` keeps `raw[76]` with the original runtime writes, `flags`, `bank`,
`texture`, `flat_index`, and `bound_colour`, plus helpers `has_texture`, `cutout`, `depth_bias`,
`hidden`, and `effect`.

## Original rules

Original `485BA0` (record `0xc54488 + slot * 0x24`, 36-byte stored header):

- Loaded when `flags & (1 << texture_level)`; bank byte at `+0x17`; pixel buffer at `+0x1c`;
  stored RGB palette at `+0x20` (three-byte entries after the loader drops the fourth byte).
- The name and size share cache is off in the shipped executable: `0xc54484` loads as zero and
  has exactly one reference, the read at `0x485c74`. No writer exists in the image, so no record
  ever gets flag `0x80` and equal names stay separate records. This port does not share.
- All 6,160 corpus textures have zero bytes at record `+0x0c..+0x0f`, so a skipped record binds
  index zero.

Original `485E30` per material (`raw` offsets):

- Flag `0x4000`: stored tick at `+0x1c` becomes `trunc(stored * 31 / 50)` in signed 16-bit units.
  The original `49bae6` loads control word `0x1f1f`, so `_frndint` truncates toward zero. The
  exact value is `stored * 31 / 50`; its fractional part is a multiple of 0.02, so neither x87
  extended nor SSE double rounding can cross an integer. The port uses the exact integer form and
  `tests/native_model_bindings.cpp` checks all 65,536 stored values against the double and
  extended forms, which also covers Windows x64 where `long double` is `double`.
- `+0x1a` is the loader bank word.
- Flag `1`: search the level span for the first record whose stored sequence matches byte
  `raw[52 + level]`. Sequence compare is `(int32)(stored sequence)`, so sequences at or above
  `0x8000` never match a byte key. On a match: `+0x18` texture slot, `+0x20` width bits,
  `+0x22` height bits, `+0x24`/`+0x26` `(1 << bits) - 1` masks, and material flag `0x200` when
  the texture flags byte has `0x20` (cutout). With no match, flag `1` is cleared. A missing
  texture therefore falls back to the flat colour path.
- Width and height bits are the highest set bit index of the sign-extended 16-bit size, capped
  at 15 (`485E30`). The original stores buffer pointers at `+0x44`/`+0x48`; this port keeps those
  bytes as loaded and reports the selected slot instead, so prepared values have a stable
  lifetime.

Original `486880(bank)` texture pass, after `480600` installs the bank palette search table:

- Records with a stored flags byte carrying `0x10` or `0x80` are skipped.
- Entry table: `table[0] = 0`, `table[1..255] = nearest(gamma[stored RGB])`, matched against the
  `480600` search table. `480600` maps its input through `500b0c`, so the target is the bank
  `matching_rgb` table. This port uses `remap_palette8()` with `matching_rgb` and `gamma`.
- Pixels are remapped in place through that table. `most_frequent` is the highest-count nonzero
  remapped texel index; ties keep the lower index and index 0 never wins.

Original `486880` material pass (materials whose bank word matches):

- Flag `0x8000` clear, flag `1` clear: `raw[0x14..0x17]` becomes the flat colour remap of
  `raw[40 + 3 * level .. 42 + 3 * level]`, as a 32-bit write.
- Flag `0x8000` clear, flag `1` set: `raw[0x14..0x17]` becomes the bound texture's
  `most_frequent`.
- Flag `0x8000` set: packed effect colour `B | G << 8 | R << 16 | (R + 2G + B) << 24` and flag
  `0x4` set when `R + 2G + B > 127`.

## Attach and shade tables

Original `4809B0(bank)` allocates `0x10000` bytes once, keeps `0xbfc104 = light` and
`0xbfc108 = light + 0x8000`, then:

- `0x480490(record + 0x300, light)` fills dword rows 0..31 from the gamma-space bank palette as
  a ramp toward black.
- `0x4801B0(record + 0x300, light + 0x8000)` fills dword rows 32..63 as a ramp toward the light
  colour at `0xc22ad0`, through the display table and the saturation adjuster at `0xc22ae4`.

The two builders consume the same `gamma_rgb` table and the same colour control as
`build_terrain_shades32()`, whose differential check already compares all 65,536 bytes of that
combined table against original `0x480490` plus `0x4801B0` over nine fixtures. Therefore:

- `ModelPaletteBank32::shades` is the exact 64-row table, and rows 32..63 are the flat table.
- `raster_material().tiny_colour` uses shade row 32 (the flat table row 0), which the original
  `0x4613c0` reads from `0xbfc108`.

## Renderer mapping

`raster_material()` fills `RasterMaterial32`:

- `texture` points at the cached `RasterTexture32` (remapped pixels, width/height bits, bank
  shade rows). Flat or missing textures leave it null.
- `flat_index` is `raw[0x14]`; `shade_colours` and `tiny_colour` come from the bank shade table,
  with `tiny_colour` on row 32.

Face routing needs the material list itself. `draw_model_mesh32()` passes `lod.materials` to
`prepare_model_face()`, and that function reads material flags, raw fixed fields, frame count,
and animation parameters. Apply the prepared list first:

```cpp
const auto applied = prepared.apply(lod_index, source_lod); // or
const auto applied = apply_model_bindings32(source_lod, prepared.lods[lod_index]);
// draw_model_mesh32(applied, mesh, ...) with prepared.raster_material(lod_index, material)
```

The native test proves that the applied list routes correctly: texture flag `0x20` yields
material flag `0x200` and face flag `0x20`; a failed texture lookup clears flag `1` so the face
is not textured; a `0x8000` luminance above 127 sets flag `0x4` and face flag `0x200`; animation
parameters advance the face material by counter, including the reverse-side parameter 127; the
scaled tick and effect bytes reach the applied `raw`; and geometry plus the source stay
unchanged.

## Checks

Native fixture test with sanitizers:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude \
  src/models.cpp src/renderer.cpp src/terrain.cpp src/assets.cpp src/filename.cpp \
  src/model_bindings.cpp tests/native_model_bindings.cpp -o .local/native_model_bindings_san
codesign --force --sign - .local/native_model_bindings_san
.local/native_model_bindings_san
```

Covers level spans, exact and nearest flat remap through `matching_rgb`, remap identity and most
frequent ties, zero texels, duplicate names with the share cache off, cutout, `0x8000` packing,
the exhaustive `0x4000` truncation proof, `raster_material` wiring, applied material routing
through `prepare_model_face`, unchanged sources, and validation errors.

Original x86 replay through Unicorn (original `485E30`, `480600`, `486880`, `480a30`):

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude \
  src/models.cpp src/renderer.cpp src/terrain.cpp src/assets.cpp src/filename.cpp \
  src/model_bindings.cpp tools/model_bindings_probe.cpp -o .local/model_bindings_probe
codesign --force --sign - .local/model_bindings_probe
.local/tools/python/bin/python tools/check_model_bindings_original.py \
  '/Users/khanh/Desktop/workspace/Delta Force/Df.exe' \
  --probe .local/model_bindings_probe --cases 40 \
  --report .local/reports/model-bindings-original-check.json
```

Cases hold one to three models in one bank, so later texture spans start at a nonzero global
index. Gamma tables include identity, `i*i/255`, `pow(i/255,0.8)*255`, quantized, and reversed
variants; display palettes include random, quantized, constant, and structured tables; the probe
target is the composed `gamma[display]` table that `480600` builds. Duplicate names prove records
stay independent. The replay emulates `485E30` with read and allocation stubs, then runs `480600`
and `486880` and compares texture pixels, most frequent indices, and all 76 material bytes except
the original buffer pointers, which are checked against the texture record addresses.

Result: 40 cases, 121 records, 544 materials, 45,877 fields, zero mismatches. Exit status is 0 on
a full match, 1 on a mismatch or engine failure, and 2 on a usage error; `compared_cases` must
equal the requested count.

## Corpus sanity data

A scan of 362 v5 models in `.local/models`:

- 9,702 textured materials, 667 flat materials; 509 textured materials also use flag `0x8000`.
- Level span matches: 9,317 of 9,702 at level 0 and 9,271 at level 3; the rest use the missing
  texture fallback.
- 357 textures use flag `0x10` (remap skip); 1,683 use `0x20` (cutout).

## Gaps

- The global texture level `0xC80D10` range beyond 0 through 3 needs a runtime check; the port
  rejects levels above 3.
- `0x486880` runs once per bank from `0x402B20`. A model loaded after that pass would need the
  same preparation again; the port prepares per model and bank on demand.
- If a future build ever sets `0xC54484`, records with equal names and sizes would share
  buffers. That branch is unreachable in this executable and is not emulated; the port always
  keeps records separate.
- The material bank word is always the loader bank here. A runtime bank change would need a
  re-prepare with that bank.

## Integration status

- Required fix applied: `apply_model_bindings32()` / `ModelPreparedModel32::apply()` copy a LOD
  with prepared material fields so `draw_model_mesh32` -> `prepare_model_face` sees original
  `485E30`/`486880` routing state. Covered by the native test above.
- Comment fix: `src/model_bindings.cpp` now names BFBB00 (display palette), not 480bb00.
- The lead must still add `src/model_bindings.cpp` to `dfre_core`, register
  `native_model_bindings` and `model_bindings_probe` in CMake, and run the Windows replay before
  claiming cross-platform equality.
