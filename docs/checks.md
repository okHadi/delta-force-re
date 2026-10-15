# Checks

Checked on 2026-10-15 against the archive hashes in [PFF notes](formats/pff.md).

- 20 Python tests and five C++ tests pass locally.
- Both PFF archive tables validate: 1,293 live members.
- All 62 BMS missions resolve to terrain maps; 95 unique map members decode.
- 445 PCX pixel arrays match Pillow 12.3.0 exactly.
- `TEMP.PCX`, `_BACKUP_.PCX`, and `B_TAIWAN.PCX` use other PCX profiles.
- Windows and Linux CI pass the Python tests, C++ build, and CTest. [Run](https://github.com/okHadi/delta-force-re/actions/runs/38771229908).
- The first C++ helper matches 2,072 original-x86 cases under Unicorn 2.1.4.
- The direct network encoder matches 468 original-x86 cases with socket stubs.
- The state loop matches 204 original-x86 cases with clock and state callback stubs.
- The C++ archive reader matches all 1,296 archive records. All 62 missions load their terrain maps.
- All 95 C++ terrain map decodes match Pillow pixels and palettes. Six `CerP` blocks match the Python reader.
- The C++ model reader decodes 361 of 363 files: 1,093 LODs, 168,123 vertices, and 157,702 triangles. Python counts match.
- Asset and model checks pass with AddressSanitizer and UndefinedBehaviorSanitizer.
- REA/Ghidra 12.1.4 decompiles the original executable on this Mac.
- Runtime game checks are pending.

Full local reports: `.local/reports/`.
