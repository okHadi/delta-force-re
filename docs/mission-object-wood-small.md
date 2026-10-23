# Small wood update

Use `mission_object_wood_small` for 453810. Keep the canonical actor and `MissionObjectLiveBindings` owners alive. The registered frame binder passes message 0. The object update dispatcher also forwards captured event messages to this body.

With host cell 67F528 zero, message 4 calls 44ED40 and marks the actor destroyed. Other messages return before actor reads. With a nonzero host cell, actor flag mask 4 sets countdown +FC to 400. Positive signed health at +74 sets it to 780. Other health values call 44ED40, read current A158C0, and call 447980. The body then reads current flags, stores countdown 400, and sets flag mask C.

Bind each reached 44ED40 and 447980 body through `call`. Bind current A158C0 through `global`. Missing bindings raise `MissionTickError` at their source site. Earlier callback changes remain in current storage.

The actual C05M07 asset/cache/definition inventory has 12 active actors with callback 453810. The inventory uses supplied configuration attributes 2306. C05M08 has five active actors with this body.

Run `check_mission_object_wood_small_original.py GAME_FOLDER PROBE --report FILE` to compare source instructions, current actor/RNG state, read/call order, and partial failures. Native integration runs the event action through the captured update dispatcher and small-wood body. It also checks frame registration, host message gates, and missing global state.

Source executable SHA256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
