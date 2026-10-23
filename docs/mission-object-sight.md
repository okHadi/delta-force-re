# Object sight

Use `mission_object_sight` for 42E2B0. Supply current canonical source and target references. The result is the signed source code: 1 is clear; masked surfaces and water add to it. Terrain and solid surfaces can return zero or a negative distance.

Keep one `MissionObjectSightBindings` alive during each call. The required terrain body receives the source reference, normalized direction, and length. The required model-list body receives group 1, then group 0, each with its current signed count and the prepared query. Set the hit distance when the list returns zero. Missing reached bodies raise `MissionTickError`.

`mission_object_sight_bindings` supplies real terrain and model-list services. Keep its mission state, geometry, height bounds, and matrix data alive. It reads each actor from canonical storage at its turn. The list count is fixed for that call. Matrix callbacks run after the bounds and sphere gates. Their changes to later records, the next group count, water, and endpoint positions remain visible. Bind each required model matrix body through the existing geometry API.

The function reads endpoint deltas first. It then builds query positions and bounds. Terrain runs before either list count is read. Group 0 is read after group 1 clears. Water and current endpoint Z values are read after both lists clear. `before_read` observes those outer-body reads. The factory's terrain and model helpers use their own current storage reads.

The length is the original integer square root shifted left by 11 with wrapping arithmetic. A zero length returns 1. A signed length of at least `3200000` returns zero. The signed reciprocal check guards the original divide range. Every nonzero source length is a multiple of 2048.

Run `native_mission_object_sight` for live matrix effects, current later records and counts, water changes, source skips before model lookup, and required bindings. Run `check_mission_object_sight_original.py GAME PROBE --report FILE` for original result, read-order, and call-payload comparisons. The source executable SHA256 is `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
