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
