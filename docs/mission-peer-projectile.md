# Peer projectile calls

Use one `MissionPeerProjectile` with the same live state and `MissionProjectileSpawn` owner. Keep its request bindings alive. Route only addresses accepted by `owns` to `call`. Other reached bodies use their required live owners.

The adapter owns three calls from message `09`:

- `47DF40` reads the current `state.world.terrain` through `ground_height_query`.
- `445B80` passes the current mutable 15-byte packet to `mission_peer_record_shot`.
- `462F20` maps the captured raw physical owner to the canonical actor handle and calls the existing pool's `spawn_view` directly.

Raw owner zero is null. Source address `7CCA80` is valid group-three slot zero. Owner addresses align to `114` bytes and fit the current physical actor pools.

`spawn_view` borrows the caller's complete 20-byte pose: X, Y, Z, yaw, and pitch. Initial entity fields use its initial values. Later sound and alert calls read the current pose after earlier effects. Keep those bytes live through the call. The typed `spawn(MuzzleResult&)` path uses the same implementation and keeps its current caller reads.

The request/parser owns delivery, ledger, ammo, and cooldown order. The adapter uses the existing shared ring and 512-record pool. Complete current packet and pose views are required. Missing terrain, malformed arguments, invalid owners, and unowned call addresses raise errors.

The differential checker runs real client request, enqueue, serialization, receive, parser, ledger, spawn, and terrain instructions. Network send/receive and effect backends are explicit fixtures. The client and host each own their canonical pool. Cases cover current ammo/cooldown gates, ledger capacity, ring wrap, owner changes, mutable caller poses, and faults after earlier stores.

The native check sends the spawn alert through `mission_behavior_zone_add`. It reads the shared zone records through `query_zone_45fd90` and checks the current actor and person changes.

Use the shared [physical owner codec](mission-projectile-owner.md) to check each current actor binding.
