# Live extra actors

Call `prepare_mission_extra_actor_pool` at the449620 clear stage. It clears the1,024 physical records in `MissionTickState::tables.extra` and their image, encoding, extra and runtime fields. Keep count8CFA14 in its current external owner.

Bind `mission_actor_insert_live_bindings` to this state, the count and current delegated memory. Supply a required raw-definition-pointer resolver against the current runtime item table. The adapter accepts matching source index/pointer pairs. A full+04 store resolves the pointer at that store boundary. A partial pointer store reports an unsupported binding before mutation. Reached unknown, null active or mismatched definitions report an error.

Raw reads use source pointers. `snapshot_mission_actor(state,{4,index})` returns the current normalized image. Its+04 field keeps the previous definition until the copy stores+04. Current typed positions, extra fields, runtime fields and preserved bytes share the same record. New person or CHR references require an explicit existing binding.

Use `mission_extra_actor_record_base` before the four-group sound record mapper. It returns the exact extra record base. Invalid widths, partial pool overlap and cross-record ranges report an error. Call the adapter's current read/store functions for that record.

At488138, the AI clear hook clears the full image and all side fields before the next secondary callback read. The lifetime increment wraps at32 bits.

Validation:361 cases each with the normal and ASAN/UBSAN/no-elision probe match the original488118 lifetime block. Native checks cover real45DAB0→45D970→canonical extra insertion, mixed copy prefix, required resolver failures, current resolver mutations and full expiry clear.
