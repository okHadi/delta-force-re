# Effect emission

Use `mission_effect_emit` for `42A440`. Keep three current argument words and a distinct aligned `114h` local frame in the same read/write owner. Keep both addresses stable through callbacks. The frame holds the emitted raw actor record. The argument words supply X, Y and Z when the source reads them after the clear call.

Bind the required source bodies:

- `4828C0`: lookup item 7016 in the current definition table.
- `49A620`: clear the current local frame.
- `477860`: advance the shared behavior RNG twice.
- `45D970`: insert the completed frame into the canonical extra pool with mode 1.

Use `mission_effect_definitions_initialize` for `45DD40`. Route `8E44B0..8E44F0` to the shared effect-definition table. Bind each `45D6F0` call to the current resource owner. The producer calls that owner for item zero, sixteen table entries and fifteen further effect items. Keep each lookup result captured through the table store and resource call.

The emitter captures the definition address after lookup, reads the model after clearing, and reads both callbacks after the random draws. Keep these reads current. Preserve earlier frame and table stores when a later provider fails. An allocator result of zero preserves the completed frame and consumed random draws.

Run `native_mission_effect_emit` and `check_mission_effect_emit_original.py GAME PROBE --report FILE`. The native test uses the real extra-actor insertion owner and shared RNG. It checks current arguments and callback metadata, pool capacity and the required resource gate. The checker executes both parent bodies, the clear body and RNG. Lookup, allocation and resource services are explicit fixtures in that comparison. Saved registers, return address and CPU stack state are outside this data-binding API.
