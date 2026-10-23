# Smoke callbacks

Use `mission_object_smoke_generator` for `sgen` at `42A520` and `mission_object_smoke_move` for `smok` at `42A590`. Bind current physical actor records, including the canonical extra pool. `bind_mission_object_smoke` installs both callbacks and checks physical limits and current rows.

Bind generator services explicitly:

- `42A440`: emit at the captured source position.
- `477860`: advance the shared `state.behavior.rng` with `behavior_rng_next`.
- `45DA30`: remove the actor through its current lifecycle owner.

A nonnull `+F8` owner supplies the position when its `+6C` flag 4 is set. A null owner uses the generator position. An owner without that flag causes removal. After emission, increment current `+7C`. A signed count below 100 sets `+FC` from the random draw; other counts cause removal. Preserve earlier changes when a required service fails.

The puff move body reads and stores the current pose, velocity, growth and byte `+3B` in source order. Keep all these bytes in the same actor owner. It uses an unsigned growth cap of `20000h` and 32-bit wrap arithmetic.

Run `native_mission_object_smoke` and `check_mission_object_smoke_original.py GAME PROBE --report FILE`. The checker runs the two callbacks and original RNG. Its emitter and removal services are explicit fixtures. The native test uses one canonical primary/extra state, the shared RNG and actual primary lifecycle removal.
