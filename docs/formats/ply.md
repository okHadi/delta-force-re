# Soldier profiles

Use nine `playerNN.ply` slots. Each file has 2,704 bytes. Read the original game folder. Write saves to a separate folder. Preserve fields that have no defined meaning.

| Offset | Field |
| --- | --- |
| `00` | `PLY` signature; version byte at `03` |
| `04` | Name, 16 bytes including NUL |
| `14` | Slot filename, 16 bytes including NUL |
| `24` | Active flag, bit 0 |
| `28` | Selected campaign index |
| `74 + campaign*44 + mission*4` | Mission completion word; 5 campaigns, 16 missions each |
| `4B0` | Ten macros, 40 bytes each |
| `640` | Portrait index, 0–10 |
| `648` | Multiplayer kills |
| `650` | Multiplayer deaths |
| `65C` | Multiplayer team games won |
| `664` | Campaign kills |
| `680,684,688,68C` | New-profile values: 6, 9, 1, 8 |
| `6A0` | 46 control records, 12 bytes each |

Offsets and strides are hexadecimal. Integer fields are little-endian 32-bit words.

Load controls from `DFDK.KEY`: each `KEY=index,key,mouse,joystick` row defines one record. Load macros and default slot names from `DFMENU.BIN`. A new name has 1–15 ASCII characters; convert lowercase letters to uppercase.

Reader `409060` accepts the `PLY` signature and a fixed-size record. Versions below `1B` receive the last 24 default-control bytes at `8B0`. The reader sets the version to `1B`, including files with a higher version. Writer `409180` sets `PLY1B` before it writes the record.

Profile initialization is at `408E30`; control loading is at `407040`; default fields are at `408790`; statistics are at `408800`. Portrait loading is at `40A2E0`. Original executable SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`profiles_probe GAME_FOLDER SAVE_FOLDER [OUTPUT.ppm]` reads all slots and draws the soldier screen. It does not write profile files.
