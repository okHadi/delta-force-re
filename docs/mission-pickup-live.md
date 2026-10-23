# Live pickups

Use `mission_pickup_live_effect(shared, bindings)` as the actor collision effect provider. Keep the shared client records and counters alive while the provider is in use.

The provider reads current mission actors and people. It writes actor changes before each foreign call and reads the current actor state after that call. It preserves actor changes when a foreign call throws.

Set host and local-player state through `MissionTickState`. Set client records, rule flags, and team counters through `ActorPickupState`.

Bind each reached foreign effect. The removal provider executes the full `45DA30` person, character, and actor cleanup. The allocator returns a group-zero actor. Wire indexes use `mission_actor_source_address`.

Pickup call arguments use typed actor handles. The stored `+90` link uses the target's `mission_actor_source_address`. Pass that stored link to source-address consumers such as actor lifecycle and event queries.

The objective provider receives the current source actor and target actor reference. Read the target's full 16-byte pose at `+24`, including yaw at `+30`.

`can_use` returns the original restore-home value, 0 or 1. Trigger effects use the collision call's void return rules. The sound call returns its provider result.

Run `native_mission_pickup_live` for callback changes, errors, clients, packets, removal ownership, and the real pickup-to-lifecycle drop join. Run `check_mission_pickup_live_original.py GAME PROBE --report FILE` for original-executable checks of raw pickup links across all four groups, live healing, person home, allocation, and packets. Sound, allocation, transport, and the local flag effect are explicit test providers in these checks.
