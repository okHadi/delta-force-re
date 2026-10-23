# Primary actor memory

Construct `MissionPrimaryActorLive` after mission initialization and before callback installation. Keep the state, adapter, definition resolvers, and delegated services alive together. Use one adapter for the mission lifetime.

Supply exactly `1024 - state.tables.groups[0].size()` current raw tail images. A zero tail requires the verified449620 full45000-byte clear at7DDE80 or current source readback. Supply current raw definition pointers for the counted prefix through `initial_definition`.

The adapter owns raw header metadata and uncounted tail images. Counted actor fields stay in `MissionTickState`. It reserves1024 slots in the canonical actor and side vectors. Count remains the canonical group0 vector size.

Route SoundRuntime record lookup, read, store, and actor-word access through this adapter before the general sound record provider. Route8CFA1C reads and insertion stores to the same adapter. Use `insertion_bindings()` for actual45D970 mode0.

A source+00 store changes the raw active index. A source+04 store resolves its current definition pointer and publishes the matching typed header. Raw reads preserve the mixed header between these stores. Partial definition and person pointer stores require a separate binding and raise an error before mutation.

The count store publishes completed tail rows into the same canonical actor, image, extra, runtime, and person-encoding vectors. The mission-lifetime insertion API accepts aligned DWORD count stores that keep or extend the count up to1024. Cold reset starts a separate adapter lifetime. Partial count stores and count reduction raise an error before mutation.

Bind actual44A0C0 through the delegated person provider. That service must establish the canonical person relationship before the source person-pointer store. New tail rows with a person or character pointer require an explicit publication binding and raise an error at publication. The460D20 FCE image has zero person and character fields.

Definition resolvers read the current resource table. They must keep the actor pool and adapter call stable while they run. Route pool changes through this adapter. Current typed field changes and full canonical actor clears remain visible in raw reads.

The original test runs real45D970 instructions and compares ordered access, all1024 physical records, count publication, person callback changes, and failure prefixes. Native checks cover mixed headers, current typed aliases, tail publication, pointer validation, count limits, stable actor references, current person binding, and the last physical slot. They also join447610 through460D20 and45D970 to the canonical group0 actor and shared2B outbox publication.
