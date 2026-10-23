# Peer requests

Use the current shared memory owner with `MissionPeerRequestBindings`. Reads and writes use source addresses and widths 1, 2, and 4. Actor addresses are original wire addresses. Client records stay shared with `MissionClientOutbox`.

`mission_peer_trigger_charges` implements `447150`. It scans current group-zero records, accepts owned `FC8` and `FB4` items, clears their low word at `74`, and calls the current behavior pointer at `110` with `(actor, 1)`. Bind that pointer to its real actor behavior body. The scan reads the current count after each actor and callback.

`mission_peer_request_strike` implements `447760`. It reserves the first nonpositive timer in the shared 16-slot table at `6E5778`, stores the current owner and position, decreases current `6E5884`, and uses the real outbox to enqueue `2D`. An exhausted count or full timer table produces `2E`. Share these cells with `mission_peer_timers` and the expiry owner.

`mission_peer_record_shot` implements `445B80`. It records the mutable 15-byte packet in the current 100-row shot ledger at `6E4760`, reads real terrain through `47DF40`, updates current `6E5570`, and publishes the matching client's shot count and current actor countdown. The captured ledger row stays fixed across callbacks. Later packet, counter, and owner reads remain current.

Bind `MissionProjectileSpawnBindings::host_message` to `mission_peer_record_shot(bindings, bytes.data(), bytes.size())`. Keep the existing `MissionProjectileSpawnCells` and `MissionTickState::projectiles` owner. The request path records the shot before the existing spawn function creates its projectile.

Bind every reached terrain or actor behavior body. Errors retain earlier stores. Active shot-ledger counters are 0 through 99; a count of 100 or more returns before packet access. The module rejects a negative ledger counter, an incomplete reached shot packet, a client slot outside 0 through 16, or a null reached behavior pointer.

Run `tools/check_mission_peer_requests_original.py GAME_DIRECTORY PROBE --report REPORT` for the actual instruction comparison. The checker runs the real `441750` enqueue body. Required terrain and behavior returns, mutations after source stores, and provider faults are explicit test fixtures.
