# Live area stages

Call `mission_area_live_stage(state, geometry, call, bindings)` from the mission stage provider. It handles `45E050` and `465BD0`; other addresses return `nullopt`.

- Keep `MissionTickState` and `MissionGeometry` alive during the call.
- Bind external rules and alternate listener cells through the read-only `globals` provider.
- Supply real segment, actor update, local-hit and listener callbacks for each reached path.
- Use the segment actor handle as the first skip identity and zero as the second skip identity.
- Use the update callback address and message supplied by the adapter.
- Change canonical mission state inside callbacks. Actor changes, queue appends, water, local-player selection, rules and RNG remain live after each call.
- Use `apply_mission_actor_image` for raw callback changes that need mission write-back.

The adapter writes cached raw actor views to canonical state before foreign calls. It refreshes the views after callbacks, including errors. Pending records and the shared behavior RNG use canonical storage. Raw partial helper writes are retained when a required callback is missing or fails.

Run the signed `native_mission_area_live` test. The signed `mission_area_live_probe` accepts rows listed at the top of its source and prints canonical state and call order. The area algorithm's original-instruction evidence is in `check_mission_projectile_stage_original.py`.
