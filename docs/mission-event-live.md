# Current event memory

`mission_event_live_bindings` returns `MissionEventPersonBindings` for current `MissionTickState` records. Keep the state, definition codec, foreign memory, and x87 control owner alive. The facade uses the shared `mission_event_person_current_bindings` field adapter.

Reads support widths 1, 2, and 4. Counts `8CF900`, `8CF904`, `8CFA18`, and `8CFA1C` read the current group extents. Physical caps are group 2:512, group 1:1024, group 3:256, and group 0:1024. Partial count accesses use byte aliases. Count writes require the actual allocator owner.

Actor records use the physical bases from `mission_actor_source_address` and stride `114`. A read must fit one current row. The definition callback maps each active actor's current index to a source address. Index 0 is valid. Inactive actors return a null definition pointer. Person pointers use `794680 + slot * B4`; slot 0 is valid. Pointer byte aliases use the same converted DWORD. The adapter rejects unowned rows, pool gaps, record crossings, and address wrap.

Keep a separate physical primary-record owner in front of this adapter when that owner has current raw header data. This adapter maps the canonical typed actor view. Each mapped record has one authoritative owner.

Person reads and writes use the existing current typed fields. Supported BYTE fields are `24`, `25`, and `9C`. Supported DWORD fields are `54` through `68`, `70` through `98`, `AC`, and `B0`, in steps of four. Scalar accesses can use byte aliases across supported adjacent fields. The adapter checks the complete access before a write. Other person bytes require their explicit field owner.

Foreign read/write providers supply raw action records, event allocations, flags, tables, and other non-record cells. Missing providers throw when reached. Foreign writes receive the value masked to the requested width. Actor writes require their current field owner.

The existing owned-action `mission_event_person_bindings` helper wraps its action array around the shared adapter. It keeps its typed person image reads for fixture callers. Event callbacks with raw action pointers use the live bindings directly.

The native test loads actual C05M07 event bytes through the source file and arena, converts its actual actors and people, binds physical markers, and dispatches raw region and side person edits. It checks shared person writes, definition/person pointer aliases, count caps, required memory errors, fired-byte arena reads, objective flags, and retained bytes after reset. Run with the game folder to include this asset test. The adapter opens no audio output device.
