# DF1 models

`decode_model()` reads complete 3DI v2–v5 model files. Counts and indexes have bounds checks. Texture palettes retain RGB plus the stored padding byte. Geometry uses raw game coordinates. Mesh transforms remain separate.

Run `models_probe FILE...` for native counts. Run `python3 tools/model_report.py Df.pff --archive` for a JSON report. Use `--geometry` with one extracted model to include vertices and faces. Keep reports and assets in `.local/`.

## Layout

All numbers are little-endian. Offsets are bytes.

| Block | Fields |
| --- | --- |
| File header, 124 bytes | `3DI` at 0; version at 3; name at 4 (12 bytes); LOD count at 20 |
| Texture count | `u32` at 124 |
| Texture | 36-byte header; pixel byte count at +16; sequence `u16` at +20; flags at +22; width/height `u16` at +24/+26; pixels; 1,024 palette bytes |
| LOD header | 176 bytes for v2–v4; 184 for v5; data size at +20 |
| LOD counts | vertices +128; normals +136; faces +144; meshes +152; hardpoints +160; materials +168; v5 volumes +176 |
| LOD data order | vertices; normals; faces; meshes; hardpoints; collision volumes; materials |
| Vertex, 8 bytes | signed XYZ `i16`; raw mesh value `u16` |
| Normal, 8 bytes | signed XYZ `i16` in 1.14 format; raw flags `u16` |
| Face, 72 bytes | flags +0; face normal +2; U `i32[3]` +4; V +16; local vertex `i16[3]` +28; local normal indexes +34; material `u32` +68 |
| Mesh | 88/104/112 bytes for v2/v3–v4/v5; vertex/face/normal counts at +4/+12/+20 |
| v5 mesh transform | parent `u32` +36; delta origin `i32[3]` +40; origin +52, in 24.8 format |
| Hardpoint, 12 bytes | XYZ `i32` in 24.8 format |
| v5 collision volume | 72 stored bytes |
| Material, 76 bytes | name +0; flags `u32` +16; frame count +30; raw animation parameter +31; flat RGB +40; texture key +52; secondary key +55 |

Faces refer to the current mesh. The decoder adds each mesh's vertex and normal start to make global indexes. U and V use 16.16 format. Texture candidate indexes match the low sequence byte. Flags `0x8000` without `0x1` select the secondary key. Key 255 has no candidate.

## Checked assets

Source: `Df.pff`, SHA-256 `75e0231cbf53d838b0ac9e6b7670083aae61040e0560525cd5f0de91e79ee29a`.

- 363 live `.3DI` members: 355 v5, 6 v3, 1 v2, 1 other magic.
- 361 complete decodes: 355 v5, 5 v3, 1 v2.
- 1,093 LODs; 168,123 vertices; 157,702 triangles; 6,152 embedded texture records.
- Every decoded LOD has the exact declared size and exact mesh totals. Every triangle vertex, corner normal, and material index fits its table. Every decoded file ends at the final LOD.
- Every material with a texture flag and key other than 255 has at least one sequence candidate. Exact runtime texture selection still needs a rendering check.
- `KABOOM.3DI` begins with `kaboom`, not `3DI`; its format is unknown.
- `MC.3DI` has a v3 header. A texture range starts at `0x5150` and requires 16,172 bytes, beyond its 24,576-byte file.

| Asset | SHA-256 | First LOD offset | Vertices / faces / meshes |
| --- | --- | --- | --- |
| `M16.3DI` | `c42c293d6fdb49bb8d3654700568f642da2d5ddc0312c1f6931faa3a3a0f51f7` | 144748 | 132 / 194 / 2 |
| `PLAYER01.3DI` | `8ce6ebab00b3469c0440aab502af95e44179e139b96b8fecdc325b2e8645399a` | 420852 | 578 / 749 / 15 |
| `BARREL.3DI` | `351159a290fc1df23a0dcd80f1f673ce967f4700a2d299e388440f4cb630a04a` | 15596 | 30 / 28 / 1 |

## Remaining fields

Character motion, root transforms, collision rules, texture blend modes, and material animation timing need executable and runtime checks. Raw mesh, collision, material, and LOD header bytes remain available to code. These static checks establish file decoding, not game rendering or animation.

Format references: [NovaResearch](https://github.com/Novahq-net/NovaResearch/blob/6413caa5def19c0885e38f77d28454348ab54e69/Shared/3DI%20File%20Format.md), [Nova3di](https://github.com/Novahq-net/Nova3di/blob/cb4f76c963a6db6ee3a68f31d3af611124b0ac19/src/format/t3di.cpp). Neither checked repository has a license file. This decoder is an independent implementation checked against the user's assets.

Executable check: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`, function `0x486b80`, instructions `0x486d3a–0x486d5a`, tests material flag `0x4000` and requires material index plus byte `+0x1e` to fit the material table. REA Evidence `ev_f5267c299e00d76adb7c38a5b9cad3df991e5813e4754fccdef07220ac7ba1f1`. All 603 flagged materials pass this check. The role of byte `+0x1f` needs further checks.

## Original origin step

`localize_model_lod()` returns a copy with the original v5 loader origin step. LOD flag bit 0 enables the step. The original global skip flag is zero in this mode. Each vertex subtracts its referenced mesh origin shifted right by 8, with signed arithmetic and 16-bit wrap. Each mesh delta origin becomes its origin minus its parent origin, with 32-bit wrap. Parent indexes have bounds checks.

REA function `0x485e30`, Evidence `ev_e7a945ff61755259945edfa1111a0498ec0f661f5c660c35df383734a02c0d33`, contains the step at `0x486128–0x486202`. The rebuilt step matches that original instruction block for all 192 flagged v5 LODs in 50 models: 53,212 vertices and 2,880 meshes; zero mismatches. Block SHA-256: `e39ea68ac7f92f13c2f5ae2182cf95aefb9f0fc98c5b7b5c29d8d5a1728dd3dc`.

Repeat with the local Python environment containing `pefile` and `unicorn`:

```sh
python tools/model_report.py /path/to/Df.pff --archive --check-origins /path/to/Df.exe --probe /path/to/models_probe
```

## CHR1 character frames

`decode_character()` reads `PLAYER01.CHR`. It keeps the full file, bone records, bounded track ranges, bone map, animation slots, frame headers, and packed angle bytes. `character_frame_pose()` performs the original frame angle read: each unsigned XYZ byte shifts left by 17; scale starts at 65,536 on each axis. This step supplies a pose before motion and interpolation.

| Field | Offset / size |
| --- | --- |
| Magic | `CHR1` at 0 |
| Bone records | file offset at +4; count +8; each record is 92 bytes when its flag at +24 has bit 0, otherwise 60 |
| Bone map | file offset +12; count +16; `u32` indexes |
| Animation table | file offset +20; 255 `u32` file offsets; zero means empty |
| Declared animation count | +24 |
| Bone track offsets / counts | +0/+4/+8 and +12/+16/+20; observed strides 68/88/24 |
| Animation header | 20 bytes; frame count +0; mapped bone count +4 |
| Frame header | 52 bytes; next stride +0; stored pointers +28/+32/+40 |
| Packed angles | four bytes per mapped bone after the frame header; XYZ use the first three bytes |

Original relocation: function `0x439d20`, REA Evidence `ev_96ed2af0a04f4a0f21cecb04a12bcdb562b59be178ea2e92d3bc4fdfec9515c1`. Constructor `0x439e48` selects the 52-byte CHR1 frame header, Evidence `ev_fb9ca480ec8035bc479449f2a12da0b077931ea0ce2a01cf7f2aac3175e4129f`.

Checked file SHA-256: `85811988a20f2c40f80376a5b321b33f2dafa9eb96fe8fc46ec8bc7077974158`. It contains 15 bones, 204 populated animation slots, and 12,284 frames. Each animation's last frame has stride zero; the other 12,080 frames have stride 160. Track strides fit every observed bone's adjacent data ranges.

The rebuilt pose read matches original function `0x43a0e0` for all 12,284 frames and 184,260 bone poses, with zero mismatches. REA Evidence: `ev_5aa7409cb2f8a4f8648446a8cefefde56498d9ef6511f14a9888fb3f40d1ff4f`. The 101-byte instruction block has SHA-256 `70cdecf863f6e8c3e4ef130bd94fd632f4debf9651df4fe637e32e9fe1b2ce36`.

```sh
python tools/model_report.py /path/to/PLAYER01.CHR --check-character /path/to/Df.exe --probe /path/to/models_probe
```

Frame transitions, root motion, bone matrices, and the remaining frame flags need further executable checks.

## Pose interpolation

`character_pose_delta()` computes per-tick bone deltas. Angle differences use signed 25-bit wrap. Scale differences use signed 32-bit wrap. Division by the frame duration truncates towards zero. `advance_character_pose()` adds one tick, masks angles to 25 bits, and wraps scale to 32 bits. Both functions keep their input poses unchanged.

The rebuilt core matches the original delta block `0x43a2c7–0x43a45a` and tick block `0x43a7ea–0x43a847` for all 12,284 frames: 184,260 bone deltas and 184,260 bone advances; zero mismatches. Each case starts at the prior stored frame pose. All checked frames use unit scale and have no head-look override. Frame durations range from 1 to 45 ticks.

Delta block SHA-256: `a61afc8736f9fdda9629a29c113c762511ddb7336a6302b1f071f92c10369243`. Tick block SHA-256: `0a7dbd4f7590c1daa21eacb20ff991915ad378aa1527f18669c8153ef374c79d`. REA delta Evidence: `ev_c97764fdc10a0b312089af3879d895df1d72da0c6026fdacf768c4b6adcd9f04`.

```sh
python tools/model_report.py /path/to/PLAYER01.CHR --check-deltas /path/to/Df.exe --probe /path/to/models_probe
```

Frame transitions, head-look overrides, root motion, and bone matrices need separate checks.

## Game character preparation

`prepare_character_runtime()` returns a prepared copy. It applies the original game factor `0x1d2b0 / 65536` to track A XYZ values, header fields +56/+60/+64, and bone fields +28 through +52 and +60 through +80. Signed products shift right by 16 and keep the low 32 bits. Track B's first three stored pointers become track A indexes; field +40 becomes zero. The function checks key alignment and range and accepts the observed 92-byte bones. A prepared copy has `runtime_prepared=true`.

Game loader `0x42cbb0` calls this step after CHR relocation. REA Evidence: `ev_eae55a343a655fc9694ef6f45fa538550ed943a34f71fcb2aef7b7c2350ab78c`. Original step `0x42c9a0`, Evidence `ev_a5cb4fefd1ab7a5b319af1e007e446c55caea7579d6bfd0a0f46f3f6b1b38f9b`, matches all 2,117,896 output bytes after process pointers are normalized to file offsets: 15 bones, 684 track A keys, zero mismatches. Its 525-byte block has SHA-256 `2fe67a637da2d1fd52dd60240c28b1add64a227eeb2e9fc699d9b3d448432ed8`.

```sh
python tools/model_report.py /path/to/PLAYER01.CHR --check-prepare /path/to/Df.exe --probe /path/to/models_probe
```

The actor tick bridge at `0x42ce40` converts game position and heading before calling CHR tick `0x43a7a0`. REA Evidence: `ev_927f0931ff3114f1d5fff8d903e7e4b8ffad60720b053d9782faead40d218321`. Vertex posing needs the draw path next.

## Bone matrices

`character_bone_matrices()` builds the original 3x4 bone matrices from a prepared character and a pose. Rotation uses 22 fractional bits. Parent positions use the stored CHR hierarchy. The function supports the original pitch adjustment and an optional world matrix. `model_local_matrix()`, `model_multiply_matrix()`, and `model_transform_point()` keep the original per-product shifts and 32-bit wrap.

`fixed_sine_cosine()` returns sine and cosine in 22-bit fixed units for an unsigned full-turn angle. Its table uses `int(sin(i * 0.00306796162109375) * 4194304)`. Original initializer `0x477510` produces the same 2,561 entries: SHA-256 `26388a2149d995579e6814e15a8ee65baee3f19446e037c08e3219f305c149af`. Function `0x4776c0` interpolates with the low 21 angle bits; cosine starts 512 entries after sine. REA Evidence: `ev_86b26411d600ec8a6f59fa51f5857ba1eca575702a891c8ab87595bf138d9724`.

The rebuilt matrices match original function `0x46bc00`, Evidence `ev_a44bd337e601e1534fa60a247f1a5af58cc4618ca7709715353e2a715fb6bdf3`: 12,284 stored poses plus 1,224 cases with fractional angles, six pitch values, and world transforms. All 202,620 matrices and 2,431,440 components match. The 517-byte function span has SHA-256 `e3a2ba99b436227f35d444243cd26f30b8b31577de6919e5ed91a29c808344f3`. Matrix padding is outside the comparison.

```sh
python tools/model_report.py /path/to/PLAYER01.CHR --check-matrices /path/to/Df.exe --probe /path/to/models_probe
```

This is static function and instruction-emulation proof. The checked caller at `0x463a37` uses bone centres against a ray; draw-path use still needs tracing. Mesh vertex posing, projection, and face drawing need their own checks.

`ITEMS.DEF` has no whole-word `kaboom` or `mc` reference. This search alone does not establish whether either unsupported model is used by executable code.

## Character draw matrices

`character_mesh_matrices()` uses the CHR parent table and each localized 3DI mesh's `delta_origin`. Localize flagged LODs once before use. The function checks mesh and pose counts. Bone collision matrices use the separate CHR positions.

Original callback table `0x500cc8` assigns `org0` draw function `0x481330` and matrix function `0x481130`. The draw function combines these matrices with actor and camera matrices before mesh dispatch `0x46b1d0`. These callback entries were recovered with direct Ghidra entry definitions after REA could not define them.

The rebuilt function matches original `0x481130` for all 50 primary `org0` models and 194 LODs, with three sampled poses per LOD and zero pitch: 582 cases, 8,730 matrices, 104,760 components, zero mismatches. The original loader origin step runs before each comparison. The 503-byte function span has SHA-256 `f26c548638e5c9453a878b9641bc0bc5ed941d6b683e8b555b977886a1e556b4`.

```sh
python tools/model_report.py /path/to/PLAYER01.CHR --check-mesh-matrices /path/to/Df.exe --models-archive /path/to/Df.pff --probe /path/to/models_probe
```

This check covers character draw matrices. Vertex projection, face drawing, and held-item attachments need separate checks.

## Mesh vertices and projection

`model_mesh_vertices()` applies the original mesh flag rules. Flag bit 0 selects the matrix from each vertex's owner index. Other meshes use their own matrix. Signed vertex XYZ values shift left by 8 before the scalar transform. The MMX mode truncates rotation entries to signed 16-bit values after a shift by 8, sums products with 32-bit wrap, shifts by 6, and adds translation. Keep the selected math mode explicit; the results can differ. Owner-indexed meshes use scalar math in both modes.

`model_project_vertex()` uses camera component 0 as depth. It returns 16.16 screen coordinates for points at or beyond the near depth. The projection uses the original integer reciprocal and product shifts. Set `near_depth` to at least 3 so the signed division fits. Points below the near depth have `projected=false`; triangle clipping is a later step.

The rebuilt functions match original vertex functions `0x469dc0` and `0x469fc0` and projection `0x46a040` across all 355 bounded v5 models: 1,081 LODs, 8,408 mesh/mode cases, and 331,680 vertices. The generated transforms cross the near plane: 273,419 points project and 58,261 points stay below it. All compared values match. Both scalar and MMX cases run after the original origin conversion. The other eight archive members use other headers or versions.

REA Evidence: scalar/MMX `ev_02c96e2fff9e68a566a8d692adf9b720b0fbfd6433048175533c05ddaf872328`; owner matrices `ev_ad44175b879e5a33745c4afcb584a3d4728e70793fd8024b7747fbe7be75db89`; projection `ev_7cef0043c47975e86877d60dfdd8da98bfe1b1677b86bc7f2c5aa8e42900eabd`. The replay report includes the executable, archive, and function hashes.

```sh
python tools/model_report.py /path/to/Df.pff --archive --check-vertices /path/to/Df.exe --probe /path/to/models_probe
```

These checks cover camera-space vertices and screen projection. Face clipping, lighting, texture sampling, and pixel output need their own checks.

## Triangle spans and textured pixels

`renderer.hpp` provides the original triangle sort, inclusive screen clip, fixed-point gradients, and scanline spans. Gradient flags select texture (1), depth (2), and shade (4). Each span carries its initial values and per-pixel changes. Set raster depth to camera depth shifted left by 6 with 32-bit wrap. Scale normalized 16.16 UV values by the material's texture-size bit counts before span generation.

`draw_textured_span32()` supports modes `0x14` (uniform shade), `0x18` (varying shade), and `0x34` (uniform shade with zero-texel cutout). Each mode wraps texture coordinates and uses a strict unsigned 16-bit depth test. Each pixel pair shares one depth value. Mode `0x18` also shares one shade value per pair. Mode `0x34` keeps both colour and depth when a texel is zero. Supply 64 shade rows of 256 packed colours. Target, texture, table, mode, and span bounds are checked.

Instruction replay matches 10,000 classifications and all 133,840 spans from 1,000 triangles, including clipped, tiny, tied, and reversed cases. It also matches 1,000 spans for each of the three textured modes. All 1,536,000 target colour and depth pairs match. Texture bit counts range from 0 to 8; UV and depth steps include signed wrap. The replay uses original `0x462280`, `0x461690`, `0x461b10`, `0x461cd0`, `0x461e30`, `0x468a6c`, and `0x468cda`. The report includes source and code-region hashes.

REA scanline Evidence: `ev_6321ef8512cf179f8fe85414996fd40e27d7c8fb0a4120f2b136aec8d03da0ab`. Textured span Evidence: `ev_0e3c16aa98c3958487228952d0bacfa6e44b3dee2b3548083208d50c09e56d4c`. Setup `0x468a6c` uses direct Ghidra entry definition because REA could not define the entry. Mode `0x18` matches original setup `0x469258` and span `0x4694df`, Evidence `ev_81d4e19ef5238b09557d1d2b7609af46b856cdf0f5b8dd23b4a3270f9539455d`. Mode `0x34` matches setup `0x468e56` and span `0x4690cc`, Evidence `ev_d45d24a45767dbdd4261c3aa57548c18886a77cd1e971960af51c12d6d08b0e1`. The setup checks use original bytes; those two setup entries are not defined in the REA function map.

```sh
python tools/check_renderer_original.py /path/to/Df.exe --probe /path/to/renderer_probe
```

This covers span setup and three pixel modes. Tiny-triangle pixels, shade-table creation, near-plane face clipping, lighting, blending, and other pixel modes need separate checks.
