# Actor insertion

`mission_actor_insert` runs45D970 against current source memory. Supply required read, store and44A0C0 person providers.

Mode0 requires definition type6. It scans1,024 physical group0 slots at7DDE80. The caller supplies the canonical records and count8CFA1C. A definition with flag4 requires44A0C0. Its result is stored at actor+0C, including zero. A zero result returns zero after the record copy and count store.

Other modes scan the separate1,024-record pool at822E80 and use count8CFA14. `MissionTransientActorPool` stores raw bytes for standalone comparisons. `mission_transient_actor_bindings` maps these bytes and delegates other addresses to the supplied providers. Live callers bind the canonical `AiActorTables::extra` records, current extra fields and runtime fields to the same raw memory view.

Each copy uses69 ordered four-byte reads and stores. Keep source and destination views live. Overlapping source records use the same memory owner. Count updates use a signed comparison after the copy.

`mission_transient_actor_clear` applies the449620 clear of45000hex bytes. It preserves count8CFA14. A new process owner starts with zero records and a zero count.

Keep all referenced owners alive. Supply the physical group0 tail when reached. Required providers report a missing owner before that read, store or call completes.

The native test joins45DAB0 to the real insertion body with declared sound and zone fixtures. Runtime sound, zone and pool providers are separate required joins.

Validation:292 original cases each with the normal and ASAN/UBSAN/no-elision probe, zero differences. Native checks pass with the real terrain-mark body and declared sound/zone fixtures. Windows compilation passes for the module, probe and native test.
