# Event storage and execution

Use one `EventStage` engine with either owned fixture tables or borrowed source memory.

For source memory, keep the bindings, `MissionEventMemory`, and memory owner alive for the full stage lifetime. Call `attach_source` before loading. Load the current file through `MissionEventMemory::load_current_file`. Supply the required `42EEA0` and `42F020` marker callbacks. Reset through the stage to run the actual `42EBE0` resource reset.

Use `event_view`, `trigger_view`, and `action_view` for checked current table access. Each view keeps its captured source address. Reads and writes use the current bytes at that address, including retained or reused arena bytes. Use the `*_record` methods when the caller already has an original source argument. Owned reference accessors and typed marker binders require owned mode.

Bind all reached raw host methods: `probe_460330_record`, `action_4601a0_record`, `action_460280_record`, and `fired_byte_record`. Record arguments are physical source addresses. The fired-byte provider reads one BYTE at the wrapping address `captured_base + index * 24 + 20`. Give this provider access to the same current arena. The caller chooses any separate repair policy for owned fixtures.

Source execution reads the actual table counts, cursor, frame counter, flags, and record fields. BYTE and WORD writes preserve adjacent bytes. Frame counter `6E4734` remains at 16 during the event tick and resets after the tick returns. Required provider errors retain completed writes.

`native_events_raw` checks captured records, current base changes, access widths, required host errors, wrapping references, and failed-call state. With a game folder argument, it loads actual C05M07 event bytes through the file owner and source arena, resets during an action, and checks retained bytes, reuse, and reload. Its two marker callbacks are explicit fixtures.

`check_events_raw_original.py` compares real original instructions with the source engine. It checks complete declared memory and ordered reads, writes, calls, and failure state. Foreign game effects are test providers. The owned comparison checks 62 supplied missions and 72 controlled cases.
