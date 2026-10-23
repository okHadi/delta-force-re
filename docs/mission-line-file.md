# Line file reader

Use `mission_line_file` for source body `4568E0`. Supply current memory, file calls, the `4565F0` tokenizer, and reached callbacks.

Keep the source frame and caller arguments mapped through each call. The frame uses `0x3C0` bytes. Its filename cell is at `+0x3D4`; its callback cell is at `+0x3D8`. Each read uses the current file cell at `+0x3BC`.

The reader removes the newline, copies all 31 token words, skips empty records and slash comments, and passes other records with a one-based line number. End of file calls the current callback with a null record. A nonzero callback result ends the loop. The current file is then closed.

Supply newline-ended lines. An unterminated line reaches the source null write; the memory provider raises an error. Keep all writes made before a later service error.

Evidence: `.local/reports/startup-banned-file-parser-source.json`, original `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `check_mission_line_file_original.py GAME PROBE --report REPORT`. The check compares the actual body, ordered memory access, token record copies, line numbers, and changes to the current file and callback. File and tokenizer services use explicit fixtures.
