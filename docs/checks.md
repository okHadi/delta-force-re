# Checks

Checked on 2026-10-15 against the archive hashes in [PFF notes](formats/pff.md).

- 20 synthetic tests pass.
- Both PFF archive tables validate: 1,293 live members.
- All 62 BMS missions resolve to terrain maps; 95 unique map members decode.
- 445 PCX pixel arrays match Pillow 12.3.0 exactly.
- `TEMP.PCX`, `_BACKUP_.PCX`, and `B_TAIWAN.PCX` use other PCX profiles.
- Runtime game checks are pending.

Full local reports: `.local/reports/`.
