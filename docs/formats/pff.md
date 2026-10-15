# DF1 PFF3

Use little-endian integers. This layout is checked against both local archives.

| Header offset | Type | Value |
|---|---|---|
| 0 | u32 | Header size: 20 |
| 4 | bytes[4] | `PFF3` |
| 8 | u32 | Record count |
| 12 | u32 | Record size: 32 |
| 16 | u32 | Record table offset |

Each record stores four u32 fields: deleted flag, data offset, byte count, timestamp. A 16-byte, null-terminated ASCII name follows. Flag 1 uses `<DEAD SPACE>`. Both archives have 12 bytes after the table.

| Source | Records | Live | SHA-256 |
|---|---:|---:|---|
| `Df.pff` | 656 | 656 | `75e0231cbf53d838b0ac9e6b7670083aae61040e0560525cd5f0de91e79ee29a` |
| `Dfbase.pff` | 640 | 637 | `92a1bd79e47a2d566e25ee187c0346c1707e88d867c9d807f3b179e5a4575471` |

Run `python3 tools/pff.py list ARCHIVE` for JSON metadata. Run `python3 tools/pff.py extract ARCHIVE NAME OUTPUT` to create one new file. Keep game data under `.local/`.

[NovaHQ reference](https://github.com/Novahq-net/NovaResearch/blob/main/Shared/PFF%20File%20Format.md): use its PFF3 variant 1 record layout. The first header field in these local files is 20.
