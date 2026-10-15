# DF1 BMS v13

Use little-endian fields. `decode_mission` preserves all header, object, fixed-block, and event bytes. The checked corpus has 62 missions and 25,492 objects.

| Section | Layout |
|---|---|
| Header | 268 bytes; `BMS` plus byte 13 |
| Terrain | Null-terminated filename at header byte 84, capacity 16 |
| Object counts | Four u32 fields at header bytes 152, 156, 160, 164 |
| Objects | Four arrays in count order; 96 bytes per object |
| Marker-index lists | 128 records, each 136 bytes: flags, count, 32 u32 indices |
| Group records | 64 raw records, each 32 bytes |
| Text slots | 32 raw records, each 20 bytes; null-terminated text in bytes 4–19 |
| Event counts | Three u32 fields: events, triggers, actions |
| Events | 24 bytes each |
| Triggers | 32 bytes each |
| Actions | 32 bytes each |

Object fields: type ID at byte 0, name ID at 4, object ID at 8, flags at 12, three signed raw position values at 16/20/24, signed degree angles at 52/54. Bytes 67 and 72 remain raw. All corpus object IDs are unique within each mission. All position values are multiples of 256; physical units require runtime checks.

Each event stores a trigger-table index at byte 4 and action-table index at byte 8. Bytes 21/22 give their respective counts. Remaining event, trigger, and action fields stay raw.

Proof: original `Df.exe` functions `0x449c70` (sections), `0x449710` (object fields), `0x42ec90` (event tables), `0x42eea0` and `0x42f020` (event spans). SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `python3 tools/mission_report.py GAME_FOLDER [BMS_MEMBER]` for Python metadata. Run `mission_probe GAME_FOLDER [BMS_MEMBER]` for native checks and object data. Keep generated reports under `.local/`.

Team rules, route execution, trigger conditions, and action effects require more code mapping.
