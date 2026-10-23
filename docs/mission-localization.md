# Mission text lookup

Use `mission_localized_text` for `404CF0`. Keep the current `503230` table and `67DBC9` flag byte in the memory owner. The function captures the table before it reads the flag.

Bind `lookup_49a092` for a reached dictionary lookup. Return the source entry pointer. The function reads that entry's current text pointer. Missing tables, entries, and text use `4CED16`. Flag bit `20` with a nonzero table uses `4CED14`. Keep both source sentinel addresses readable for the caller.

Run `check_mission_localization_original.py GAME_FOLDER PROBE` to compare the original instructions, captured table, current entry, and read/call order.
