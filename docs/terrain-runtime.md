# Terrain runtime

`load_terrain_runtime` loads original mission terrain and keeps its TRN fields.
Height, color, and character maps use 1024 × 1024 cells. Runtime rows are flipped.
Detail atlases use 256 tiles of 16 × 16 cells; each tile has flipped rows.

Ground coordinates and results use signed 16.16 values. Maps wrap at 1024 cells.
One height byte equals 16384 fixed-point units. `base_height` interpolates Y, then X.
`surface_height` adds detail elevation above byte value 128. `slope` uses cells two steps away.
These values define game units; their physical length is unconfirmed.

Height mips select the maximum of four cells. Sky and water mips use rounded mean minus one for nonzero values.
`build_terrain_height_bounds` implements `0x47D9E0`: wrapped neighbor maxima, rounded positive detail bounds, and 10 max-height levels from width 1024 to width 2.
Water height uses the TRN integer multiplied by 16384. Missing water assets disable water.
Sky and water index conversion follows the selected original 8-bit or 32-bit mode.

`initialize_voxel_columns` implements original function `0x489A00` with shared fixed-point sine and cosine.
`build_voxel_projection` implements the LOD and depth tables from `0x489440`.
Normal play uses density 2.2; zoom uses 3.2. The draw loop uses the first 11 of 14 LOD records.
Projection arithmetic uses `long double`. The 64 original output hashes also pass with binary64 `long double` on Clang; native tests check these hashes on each build.
`build_terrain_detail_tile` implements the base 16 × 16 render tile from `0x489CC0`.
Its inputs include the original palette blend/shade lookups and authored blend selectors.
`build_terrain_blend_tables` builds those lookups from a gamma-mapped 256-color palette and light RGB value.
It preserves the original candidate bins, weighted color distance, index-zero rules, and 32-step shade gradients.
`load_terrain_blend_selectors` reads the local `detail-blend.u32le` sidecar: exactly 256 little-endian words with values 0 through 11.
Detail render heights use centered interpolation and differ from collision detail samples.
`render_voxel_pass_8` implements original function `0x459635` in its normal two-pixel mode.
Its coordinates use wrapping 10.22 values. The depth table includes one guard entry.
The API checks map, column, frame, depth, and clip bounds.

Original executable SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Original x86 checks match 2192 generated and 3720 mission sample points, 30 raster passes, 11200 camera columns, 64 projection tables, 64 detail tiles, and 1398100 height-bound bytes.
All 62 missions load through the native terrain API. Local proof is in `.local/reports/terrain-*-differential.json` and `native-terrain-runtime-corpus.json`.

Full frame work requires the selected draw kernels, higher detail levels, sky and water passes, and palette allocation from loaded assets.

Run the original x86 checks with Python packages `unicorn`, `pefile`, and `Pillow`:

```sh
python tools/check_terrain_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_raster_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_camera_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_projection_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_detail_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_bounds_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_palette_original.py GAME_FOLDER TERRAIN_PROBE
```

Use the built `terrain_probe` path for `TERRAIN_PROBE`. Reports use `.local/reports`; `--report PATH` selects another report file.
