# Parameter services

Use `mission_parameter_set` for source `4C4F40`/`4B69D0` and `mission_parameter_get` for `4C4F60`/`4B69F4`. Supply current width-four memory through `MissionDebriefBindings`. Keep one source table pointer at `4B691C` and one shared return cell at `4B69F0`.

Set captures the current table pointer, writes `table + index*4` when nonzero, and returns the supplied value. Get captures the current pointer, reads a reached parameter, stores it to `4B69F0`, then reads the current shared return. A null pointer skips the parameter access. Get still reads the retained return cell. Address arithmetic wraps at 32 bits. Each missing or rejected access throws at that access; earlier stores remain committed.

The caller owns table initialization and lifetime. Original `4B6B50` resolves the current definition registry, allocates and clears parameter storage, and publishes state `4B6910+8/+C`. Keep this producer as a separate service. The parameter wrappers use the current published pointer and do not allocate memory.

Run `native_mission_parameter` for current pointer capture, null return, wrap and failure checks. Supply the game folder for all 80 campaign texts through real RTXT, Text, Format and Debrief bodies. The test declares the allocated parameter table at the `4B6B50` boundary. Menu, display and profile-save calls use test providers. Device output stays zero.

Run `check_mission_parameter_original.py GAME PROBE --report FILE` to compare actual wrapper and direct-callee instructions, ordered current accesses, aliases, mutations and failures.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
