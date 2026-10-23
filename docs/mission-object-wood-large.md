# Large wood update

Use `mission_object_wood_large` for 453930 with the current actor and `MissionObjectLiveBindings`. The frame binder passes message 0.

With host cell 67F528 zero, message 4 calls 44F270, then sets countdown 400 and current flag mask C. Other messages return before actor reads. With a nonzero host cell, flag mask 4 sets countdown 400. Positive signed health at +74 sets it to 780. Other health values call 44F270, read current A158C0, call 447980, then set countdown 400 and current flag mask C.

Bind each reached 44F270 and 447980 body through `call`. Bind current A158C0 through `global`. Missing bindings raise `MissionTickError` at the reached source site. Earlier callback changes stay in canonical storage.

C05M07 has one active actor with this body: group 1, slot 9, definition crate6. The source byte and asset inventory reports identify that actor. Native integration checks the event message 1 path and frame registration. Run `check_mission_object_wood_large_original.py GAME_FOLDER PROBE --report FILE` for original state, call order, mutation, and partial-failure checks.

Source executable SHA256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
