# Terrain data

## Checked bytes

Archive hashes are in [PFF notes](formats/pff.md). All archive offsets below are decimal.

`Df.pff:C2M6.TRN` starts at 60468290 (557 bytes; SHA-256 `8f8762a6c732200231a040bef732dcf0adf5f64b0eb6742c3461261712271e20`). It is ASCII configuration. Its `color_map`, `elev_map`, and `detail_map` fields name the following members:

| Member in Df.pff | Offset | Bytes | SHA-256 |
|---|---:|---:|---|
| `DFG3_C.PCX` | 66536598 | 901963 | `d69dd6e7ed8a7b40dfabfa7ddc5fa437ca420fb2d2a43f2ef2ca4cd086d25000` |
| `DFG3_D.PCX` | 67438561 | 730163 | `c5588fd0e453c0f56ca02fc90b6b216ffb77b681298f2da3bfa8892f573f9b41` |
| `DFG3_M.PCX` | 68168724 | 1507919 | `07f8e7dfaabf52c4039d51de664203144ac1f63f104a4d818f73ac23669c24d4` |

All three decode to 1024 × 1024 byte indices with PCX RLE, one 8-bit plane, and 1024-byte row stride. Each has a 256-entry RGB palette. The elevation file has a grayscale palette and index range 7–252. The color file has index range 1–255. The detail file has index range 64–255.

Six sky-gradient PCX members use 4 × 4 indices, a 1028-byte `CerP` block, and a final RGB palette. The decoder preserves that block as opaque data. Its field meanings require code checks.

Run `python3 tools/pcx.py FILE` for data checks. Add `--pgm .local/FILE.pgm` to export raw indices. PGM values are byte indices; physical height units require code checks.

Run `python3 tools/mission.py GAME_FOLDER C04M07.BMS` to resolve one mission, its terrain configuration, and its three maps as JSON. Each name must match exactly one live archive member, ignoring letter case.

`Dfbase.pff` has 62 `.BMS` members. All start with `BMS` plus byte `0x0d`; all contain a null-terminated `.TRN` name at member offset 84. Example: `C04M07.BMS` (archive offset 560499, 102632 bytes) names `C4M7.TRN`. Its SHA-256 is `8e08d647436678fc6634e8782cf24efdcd2e204d9fe3914696afc55b58b333d8`.

## Code search seeds

`Df.exe` SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

| Field | String VA | Address-use VA |
|---|---|---|
| `terrain_name` | `0x4cd718` | `0x47ab1a` |
| `color_map` | `0x4cd784` | `0x47ad15` |
| `elev_map` | `0x4cd794` | `0x47ad76` |
| `detail_map` | `0x4cd7a0` | `0x47add7` |

These addresses come from string and pointer-byte checks. Check their instructions in Ghidra, then follow the image-load calls and height-sample uses.

## Open checks

Height scale, world spacing, axis order, map wrapping, and detail-map use require code or runtime checks. BMS mission fields beyond the observed header require separate mapping.

[NovaHQ's format index](https://github.com/Novahq-net/NovaResearch) supplies PFF research. Its current index has no DF1 TRN or BMS layout. The terrain facts above come from local files.
