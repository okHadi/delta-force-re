# Per-tick actor update

`dfre::tick_ai`, `dfre::rebuild_target_lists`, and `dfre::rebuild_actor_targets` rebuild the original per-tick actor stage `487EA0` and the per-actor target-list build `487C50` (entered through `487E10`, thunk `488970`). Evidence: `.local/reports/ai-next-stage.json` (batch decompile of `487EA0`, `487C50`, `45D970`, `404E30`, `486AF0`). Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01f7f651ff`.

Inputs: `AiActorTables` (the four BMS group tables plus the original `822E80` extra table), `AiUpdateState` (`C92A98` call counter, `6E4714` pause flag, `6E4718` demo flag, `C23820` local group 3 actor, `C8FF50`/`C92A90` shared target list), the item definitions (`+28` type, `+9C` timer limit), and an `AiBoundsSource` for the object at actor `+10` (fields `+64` radius and `+60` height; the object registry lives in the model/resource module). Every call the original makes into code this module does not own is returned as a typed action or call; nothing is executed.

`tick_ai(tables, state, definitions, bounds, tick)`:

1. If `call_counter < 0x40`, increment it. Otherwise run `rebuild_target_lists` (which zeroes the counter and list) and continue.
2. If `paused`, return.
3. On odd ticks, request local player control `488200` and `45E050`.
4. In demo mode, update the local group 3 actor through `487370`, then when `+FC > 0` decrement and return, else call `[+110](record, 0)`, decrement, and return.
5. Group 3: for records with `+18 != 0`, request `487370(index, record, tick)`; when `+FC < 1`, mark `0xC`, call `[+110](record, 0)`, mark `0xB`; always `-1` on `+FC` and, when nonzero, on `+78`. Marks `0xB` opens the loop.
6. Group 0: for records with `+00 != 0`, call `[+110]` when `+FC < 1`, call `[+10C]` when it is neither zero nor `4563A0` (a single `RET`), and `-1` on `+FC`.
7. Extra table `822E80` (0x400 slots): for `+00 != 0`, `+FC += 1`; when definition `+9C <= +FC`, clear the whole 0x114-byte record; then call `[+10C]` under the same default rule (a cleared record has no callback). Marks `10`, request `464EB0` and `465BD0`, mark `9`.
8. Group 1 from slot `tick & 31` stepping 32: `+FC -= 0x20`, or call `[+110]` when `+FC < 1`. Group 2 from slot `tick & 63` stepping 64: `+FC -= 0x40`, or `[+110]` when below 1. Only `+00 != 0` records are touched.

`rebuild_target_lists` zeroes the list count and call counter, then calls `rebuild_actor_targets` for group 3 records and then group 0 records whose `+00 != 0` and `+6C` bit 2 is clear (`487E10` selection). `rebuild_actor_targets` clears `+108`, stores the current list start in `+104`, and for definition type `+28` equal to `1` uses range `140000`, equal to `3` or `6` uses `A0000`, otherwise returns. For each record of group 1 and then group 0 with `+00 != 0`, `+6C` bit 2 clear, `+10 != 0`, and (group 0 only) not the actor itself, it compares wrapped absolute differences of `+24/+28/+2C` against the candidate object's radius `+64` for x and y and height `+60` for z, each plus the range; matches append to the shared list (cap 2000; returning stops the scan) and increment `+108`.

Guarded divergences: the original has no bounds checks on definition indexes, a zero object handle dereference, list overflow, or a missing demo local actor; this module throws `AiError` instead. The extra-table definition is read through `+00`/the definition table (original reads the `+04` pointer) and is equivalent for records built by the factory or `45D970`. Field meanings, the extra table's role, and the `822E80` producers stay unknown.

Run `ai_update_probe rebuild|acquire|tick < cases` for case-mode output. Run `check_ai_update_original.py GAME_FOLDER AI_UPDATE_PROBE --report REPORT.json` for original x86 checks: `487EA0` ticks (calls, marks, actions, counters, countdowns, record clears, list fields), `487E10` selection and rebuild order, and direct `487C50` calls including list prefill and the 2000-entry cap. Every probe result line is length-checked and every emulation must reach one of the original return addresses.
