# Event allocation resources

Use `mission_event_resources_load` for 42EC90 and `mission_event_resources_reset` for 42EBE0. Supply current memory and required call services through `MissionEventResourceBindings`. The module owns no table or event engine and uses no caller frame.

Load consumes the current mission file cursor through 499280. It reads three counts, allocates 16-byte-aligned Events, Triggers, and Actions blocks through 497F60, reads their payloads, and changes event trigger/action indices to raw pointers. It then calls required marker services 42EEA0 and 42F020 and changes only the low loader-flag byte. Keep the first payload pointer captured before the action-pointer store.

Reset reads and releases each current nonzero table pointer through 4980D0. After all releases, it zeros the three counts, clears the three flag arrays through 49A620, and zeros 6E0254 then 6E0250. Table pointer globals retain their values. Source arena release retains payload bytes. Cursor, loader flag, and 67DBEC keep their current values. Provider errors stop at the reached operation.

Route the same current tables and flags into the mission event engine. Typed event words1/2 contain indices; raw loaded event fields+4/+8 contain pointers. Use an explicit codec and source-order count/flag writes. Keep one active record owner and define retained-byte access after release.

The native asset check uses actual C05M07 and C06M01 BMS bytes, the real PFF file stream at the validated event-block offset, and the source arena. It checks allocation sizes/names, exact payloads and fixups, a missing-marker failure, release, retained bytes, and two reloads. Marker services are declared mutation fixtures in this test. Bind their real current marker owners for mission use. The canonical EventStage memory bridge is a separate host join.

The original checker compares both source bodies over complete declared globals, flag arrays, and payloads. It checks ordered access/call hashes, callback changes, captured counts and pointers, null allocations, byte writes, and failure prefixes. Foreign file, allocator, release, clear, and marker bodies are declared fixtures. CPU registers, flags, and saved stack bytes are outside this API.
