# Generic object update

Use `mission_object_generic` for 4533F0. Keep the current actor and required `MissionObjectLiveBindings` owners alive. The registered frame binder passes message 0.

With host cell 67F528 zero, message 4 calls 453270, then sets countdown 400 and current flag mask C. Other messages return before actor reads.

With a nonzero host cell, flag mask 8 sets countdown 400. Flag mask 4 calls 453270, reads current A158C0, calls 447980, then sets countdown 20 and current flag mask 8. Otherwise, positive signed health at +74 sets countdown 780. Zero or negative health sets countdown 20 and cached flag mask 4. This prepares the next update's destruction step.

Bind each reached 453270 and 447980 body through `call`. Bind current A158C0 through `global`. Missing bindings raise `MissionTickError` at their source site. Earlier callback changes remain in canonical storage.

The supplied C05M07 assets and configuration contain six active generic actors. Native tests run the real event action, message 1 health change, both destruction steps, frame registration, and partial failures. Run `check_mission_object_generic_original.py GAME_FOLDER PROBE --report FILE` for exact source-state and callback comparisons.

Source executable SHA256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
