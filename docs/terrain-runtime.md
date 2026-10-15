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

`load_terrain_gamma` reads the first 256 bytes of `INTRO.KDV`, as original startup `0x401C60` does.
`load_terrain_palette8` loads the mission `.PAL` PCX palette; BMS byte 122 bit `0x10` selects `NIGHT.PAL`.
All 62 stock missions have palette members. Nine select the night palette.
Fog and light RGB use sky-gradient palette entries 0 and 16, mapped through gamma.

`initialize_voxel_columns` implements original function `0x489A00` with shared fixed-point sine and cosine.
`build_voxel_projection` implements the LOD and depth tables from `0x489440`.
Normal play uses density 2.2; zoom uses 3.2. The draw loop uses the first 11 of 14 LOD records.
Projection arithmetic uses `long double`. The 64 original output hashes also pass with binary64 `long double` on Clang; native tests check these hashes on each build.
`build_terrain_detail_tile` implements the base 16 × 16 render tile from `0x489CC0`.
Its inputs include the original palette blend/shade lookups and authored blend selectors.
`build_terrain_blend_tables` builds those lookups from a gamma-mapped 256-color palette and light RGB value.
It preserves the original candidate bins, weighted color distance, index-zero rules, and 32-step shade gradients.
`build_terrain_shades32` implements the original 64 × 256 color rows with brightness, saturation, and RGB controls.
Its word layout is luminance, red, green, blue. The high byte stores `(R + 2G + B) / 4`.
Inverse gamma and brightness curves are generated from verified power and square-root formulas.
`build_terrain_palettes32` implements the coarse, detail, and sky color tables from `0x47D080` / `0x47C6A0`.
It supplies 10 coarse palettes, 256 detail colors, and 256 sky rows with 64 cloud values and paired pixels.
`build_terrain_detail_mips32` uses gamma color matching, average shade, and maximum elevation at five detail levels.
`build_terrain_detail_tile32` implements all four 16 × 16 cache levels from `0x489FF0`, `0x48A9C0`, `0x48B420`, and `0x48BD20`.
It preserves centered heights, byte color interpolation, paint rules, and signed shade factors.
`build_terrain_color_mips32` supplies 10 coarse color levels using gamma RGB averages and the original palette matcher.
`build_terrain_horizon32` supplies the horizon row and 256-step sky gradient from `0x48C990`.
`load_terrain_blend_selectors` reads the local `detail-blend.u32le` sidecar: exactly 256 little-endian words with values 0 through 11.
Detail render heights use centered interpolation and differ from collision detail samples.
`render_voxel_pass_8` implements original function `0x459635` in its normal two-pixel mode.
Its coordinates use wrapping 10.22 values. The depth table includes one guard entry.
The API checks map, column, frame, depth, and clip bounds.

Original executable SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Original x86 checks match 2192 generated and 3720 mission sample points, 30 raster passes, 11200 camera columns, 64 projection tables, 64 detail tiles, and 1398100 height-bound bytes.
All 62 missions load through the native terrain API. Local proof is in `.local/reports/terrain-*-differential.json` and `native-terrain-runtime-corpus.json`.

`prepare_terrain_scene32` owns the source terrain, palettes, detail levels, and coarse maps.
`render_terrain_frame32` draws the original sky and first 11 ground LODs into RGB words and 16-bit depths.
`render_terrain_sky32` implements `0x48C650` / `0x459532`: row rays, cloud-plane intersection, nine map levels, wrapped sampling, and two-pixel color pairs.
Sky height and horizon scale use TRN values. Frame options supply the time value for cloud movement.
Cloud checks match 64 camera cases, 9216 row-ray values, and 786432 pixel bytes against original instructions.
The scalar ground loop uses the selected `0x458BFE` / `0x458F14` sample, row rollback, and depth rules.
Ground tests compare 30 coarse passes and 30 detail passes with original instructions. Detail draw tests use uniform cache samples; cache builders have separate full-data checks.
All 62 stock missions produce 640 × 480 terrain frames. Water and world-object pixels use separate passes.
`remap_palette8` maps source RGB through gamma and matches target gamma RGB with original `0x480A30` rules; palette entry zero stays zero.

Export one local terrain frame with `terrain_probe --frame32 GAME_FOLDER BMS_MEMBER .local/reports/NEW.ppm`.
The probe uses x/y zero, ground height plus two game units, zero yaw/pitch, focal distance 320, and FOV 90.
Output files use a new path under `.local`. `--frame32 GAME_FOLDER --all` reports hashes for all missions.

Run the original x86 checks with Python packages `unicorn`, `pefile`, and `Pillow`:

```sh
python tools/check_terrain_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_raster_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_camera_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_projection_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_detail_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_bounds_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_palette_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_palette_assets_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_shades32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_palettes32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_detail32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_colormips32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_sky32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_clouds32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_raster32_original.py GAME_FOLDER TERRAIN_PROBE
python tools/check_terrain_raster32_original.py GAME_FOLDER TERRAIN_PROBE --detail
```

Use the built `terrain_probe` path for `TERRAIN_PROBE`. Reports use `.local/reports`; `--report PATH` selects another report file.

`TerrainFrame32.row_depths` supplies the original row table for water and model culling. Cache rows follow `4892E0`; coverage follows `489260`. `48DC70` counts passes that start with an unfinished column. The row checker compares 512 coverage cases, 512 cache-row producer cases, and 104,566 row values with original x86 code.
