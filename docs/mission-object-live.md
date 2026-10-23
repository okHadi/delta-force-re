# Live object callbacks

Compile `object_live_callbacks.cpp` with `mission_object_live.cpp`.

Bind each required body:

| Binder | Source body |
| --- | --- |
| `bind_mission_object_veh0` | 429BC0 vehicle AI |
| `bind_mission_object_veh0_move` | 429EA0 vehicle movement |
| `bind_mission_object_slgt_ai` | 4519A0 searchlight AI |
| `bind_mission_object_slgt_move` | 451CB0 searchlight movement and451850 |

Keep current definition, type-slot, global and foreign-call providers available when reached. Each binder checks existing ownership.

Use the current mission actor images, person records and `state.behavior.rng`. Keep a cached person record valid across calls. Read the current actor link at each source destination read point. Store flags24,38,44,48,4C,50,A4,AC andB0 in their canonical person fields.

Use nonzero person handles inside the callback view. Convert to the canonical actor image encoding when storing it. A reached null person raises an error. Person pool slot0 is valid.

Flush writes before each foreign call. Reload the actor, cached person records and RNG after return or error. Return the actual output words. Clear an `output_mask` bit when its output remains untouched. A failed45C6A0 leaves its four outputs unchanged.

Use current sine-table entries for477820 and current terrain for47DF40. Bind the actual44F540 target query and42EDD0 event bit write for searchlight AI. Bind destruction and sound bodies when reached.

The four original checkers compare actor bytes, current reads, ordered calls, callback changes and partial error effects. Their foreign fixtures test the live adapter. Test each foreign body separately.
