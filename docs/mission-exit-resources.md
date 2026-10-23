# Mission exit resources

Use `mission_input_record_close` for 47A640 and `mission_input_replay_close` for 47A660. Bind BD4828 and BD482C to the current FILE cells. Bind 67DBBC to the current input status. Supply the real 49A840 FILE owner. Replay close stores status 2 after the FILE cell is zero. Keys, focus, pointer deltas, and command queues keep their current state.

Use `mission_terrain_resources_release` for 47A690. Supply the current physical pointer table and source allocator. The helper makes 64 release calls in source order, then calls 49A620 to clear BD4F94 through BF6BFF. Each pointer read occurs after the preceding call. Release return values are ignored. Provider errors stop at the reached operation.

The native asset test uses the real source arena and loose-file owner. Its terrain pointer table is an explicit test fixture. Bind the real terrain allocation producer before using this helper for a loaded mission. The typed terrain view at BD5284 needs the same physical allocation identity.

The original checker compares the complete test image, input status, ordered memory accesses, call arguments, callback changes, and failure prefixes. Foreign close, release, and clear calls have declared fixture bodies. CPU registers and saved stack bytes are outside this API.
