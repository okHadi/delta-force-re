# Live RTXT lookup

Use `mission_rtxt_load` with a source filename pointer. Bind 499410 to the file owner. Supply a fresh, unfixed file allocation for each successful load. A zero or signed-negative load result publishes zero.

Keep the raw file in current writable storage. `mission_rtxt_fixup` adds the allocation address to the group-table pointer. It fixes entry text pointers and group key-list pointers in place. Keep all entry metadata and string bytes intact.

Use `mission_rtxt_lookup` with current table, group-name and key-name pointers. Names compare byte by byte and are case-sensitive. The result points to the 16-byte entry record. Read its first word to get the text pointer.

Use `mission_rtxt_entry` with a signed 32-bit index. A null table, negative index or index at least the signed entry count publishes zero. Keep table counts and string terminators valid.

Share the 499E70 through 499E94 cells with all RTXT calls. Memory providers run synchronously. Read current cells at each source boundary so changes to counts, groups, pointers and published results remain visible.

Run `check_mission_rtxt_original.py GAME_FOLDER PROBE --report REPORT` for full-file fixups, global cells, read/write order, aliases, case-sensitive lookup and failed loads. Run `native_mission_rtxt GAME_FOLDER` to check DFDLG05.BIN indexed records, DFMENU.BIN named lookup, the shared sound-memory file owner and the real 404CF0 localization lookup.
