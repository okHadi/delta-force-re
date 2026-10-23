# Mission type counts

`mission_type_counts` runs original `44A200`. Bind current memory reads and writes. The canonical factory shares `MissionTickState` and the current `AiTypeSlotTable`.

Each call clears all 64 type counts at `+2C` and increments each age at `+38` with 32-bit wrap. It scans group 1, group 3, then group 0. Byte `+6C` bit 2 excludes a record. Signed high word `+72` selects the count address. Type 0 ends with count zero.

Group 1 and group 0 read the count for entry, then cache a second count. Group 3 reads the count again at each loop end. The raw binding retains source address wrap and valid memory aliases. The canonical factory reports an error for an address outside its current owners. A failed binding keeps all prior writes.

Source: `.local/reports/mission-type-counts-source.json`, evidence `ev_28fdbe5fef7e131b730672550143bc55f72d76f57d94f1b3202975c05dd50f88`. Supplied `Df.exe` SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `check_mission_type_counts_original.py GAME PROBE --report REPORT` to compare actual instructions, ordered memory accesses, count changes, cached age reads, signed indices, wrap, and failed-write prefixes. Native checks cover current actor state and table ownership. The module has no foreign service or audio output.
