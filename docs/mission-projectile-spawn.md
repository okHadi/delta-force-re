# Live projectile spawn

Use `receive(bytes,length)` with a mutable 15-byte message buffer. Keep its
storage stable during terrain and queue callbacks.

Create `MissionProjectileSpawn` with the current mission, shared spawn cells, loaded item definitions, and reached effect providers. Keep these objects alive. Initialize `MissionTickState::projectiles` with 512 empty records. The ring index starts at the original `A158A4` value, 0.

The loaded definitions include IDs `FA1`, `FC1`, and `FCA`, as selected by `4633F0`. Load all three items through the resource loader before binding the game. Their current model and sound fields supply the projectile resources. Main and alternate item fields use loaded definition indexes. Owner fields use physical indexes from `7CCA80`, with stride `114h`; `-1` means no owner and slot 0 is valid.

Connect `MissionPlayerProviders::projectile` to `spawn.request(actor, kind, muzzle)`. A request clears the low position bytes in the caller's muzzle, reads terrain, delivers the 15-byte message, and creates the local projectile. Host delivery uses `445B80`. Client delivery queues message 9 through `46F3E0`. Each provider runs before the next source step.

Pass a received message-9 payload to `spawn.receive(packet)`. The receive body saves the signed owner slot before terrain. It reads aim and kind from the current packet after terrain. An inactive owner queues message `17` with its current two-byte packet slot, then the body creates the projectile for the saved owner.

Bind terrain height, host delivery, queue delivery, and each reached sound or alert call. Queue providers preserve the message system's delivery time. Effect providers read and change the same mission and spawn cells. A missing reached provider throws; completed actor and projectile writes remain current.

`spawn.spawn(actor, kind, muzzle)` runs `462F20`: advance the 512-slot ring, write the selected entity, execute attached sound, positional sound, loudness, and alert calls in source order, then update current owner flags. Keep the shared ring index and class table current across these calls. The return value is the current ring index after effects.

Run `native_mission_projectile_spawn` for ring wrap, callback changes, inactive owners, required providers, and errors. Run `check_mission_projectile_spawn_original.py GAME PROBE --report FILE` for packet, field, and call-order checks against the original executable. Terrain, message delivery, sounds, and alert calls use explicit test providers in that check.

Use the shared [physical owner codec](mission-projectile-owner.md) to check each current actor binding.
