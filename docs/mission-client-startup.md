# Mission client startup

`mission_client_startup` runs `43FF30` against shared current memory. Use the same 17 client records for startup, actor placement, frame logic, message queues, and results.

The body clears `17 * 9B84` client bytes, copies the current server name, sets slot 0 fields, builds server strings, resolves the server address, sets counters, assigns the local team, registers callbacks and six input records, starts the DF socket service, and resets the banned list.

Owned helpers:

- `4406E0`: assign the current client and actor team fields. Read actor `+8` again before the second actor write.
- `444C50`: copy local peer, profile name, face, and side into client 0.
- `4715B0`: update one of 16 ten-byte input records. Preserve each four-byte and two-byte copy step.
- `4974C0`, `4974D0`, `4974E0`, `497500`: store the source callback pointers.
- `4487E0`: clear banned-list fields and call the file parser.

Bind current `read` and `write` providers with widths 1, 2, and 4. Bind reached foreign services `49A620`, `404CF0`, `454DF0`, `454BB0`, and `4568E0`. Each service must finish its real state changes before return. Missing services raise an error. `before_call` observes or changes state at the source call point.

Keep the client clear, localization, DNS, DF socket startup, and banned-file parser as explicit service joins. Run `444C50` with the current profile before actor placement. Use the supplied executable's constant strings and input records. Keep the actor link as a physical source address.

Evidence: `.local/reports/mission-server-init-43ff30-source.json`, `mission-server-init-context-444c50-source.json`, and `mission-server-init-0x4406e0-source.json` plus the retained helper reports. All reports identify `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `check_mission_client_startup_original.py GAME PROBE --report REPORT`. The check runs the actual owned helper instructions and compares ordered memory accesses, call arguments, final input cells, current actor links, team modes, copy aliases, and the full client-clear extent. Foreign services use explicit fixtures. Native checks cover current-link changes, a failed second actor write, input-table limits, copy aliases, exact field widths, and startup calls. Tests produce no device audio.
