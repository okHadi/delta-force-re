# Banned address record

Use `mission_banned_record` at callback `448690`. Supply current memory, the required `49AFA0` integer parser, and an eight-byte source local frame. Keep the frame valid through all calls.

The callback splits the address at its first three dots. It calls the integer parser for all four parts and combines their full 32-bit results with shifts and addition. It stores the address and label at the captured list index. A label of at least 16 bytes uses the current default string at `4CC6EC`. The final count uses the current `6E5888` value.

A null record returns zero after it captures the current count. The file reader supplies token records and line numbers. Keep token strings writable and all list storage mapped.

Evidence: `.local/reports/mission-banned-record-source-bytes.json` covers `448690` through `4487D2`; `.local/reports/mission-atoi-wrapper-source.json` describes the required integer parser. Original `Df.exe` SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `check_mission_banned_record_original.py GAME PROBE --report REPORT`. It checks the actual callback instructions, ordered memory, string changes, labels, current counts, and local-frame aliases. Integer-parser calls use an explicit fixture.
