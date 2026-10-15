# Checks

Checked on 2026-10-15 against the archive hashes in [PFF notes](formats/pff.md).

- 20 Python tests and two C++ tests pass locally.
- Both PFF archive tables validate: 1,293 live members.
- All 62 BMS missions resolve to terrain maps; 95 unique map members decode.
- 445 PCX pixel arrays match Pillow 12.3.0 exactly.
- `TEMP.PCX`, `_BACKUP_.PCX`, and `B_TAIWAN.PCX` use other PCX profiles.
- Windows and Linux CI pass the Python tests, C++ build, and CTest. [Run](https://github.com/okHadi/delta-force-re/actions/runs/38771229908).
- The first C++ helper matches 2,072 original-x86 cases under Unicorn 2.1.4.
- The direct network encoder matches 468 original-x86 cases with socket stubs.
- REA/Ghidra 12.1.4 decompiles the original executable on this Mac.
- Runtime game checks are pending.

Full local reports: `.local/reports/`.
