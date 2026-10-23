# Live sound records

Use `mission_sound_live_bindings(state, script_key)` to bind `MissionSoundRuntime` to the mission's current actors and projectiles. Keep `state` alive. The factory can run before `init_mission_tick`; each access reads the current tables.

Supply a current script-key callback. Bind `completed` and `service` when their required bodies are reached.

Use raw source addresses and exact record bases for direct record reads and stores. Access widths are 1, 2 or 4 bytes. Keep each access within one 276-byte record (0x114). Actor pool limits for groups 0, 1, 2 and 3 are 1024, 1024, 512 and 256 rows. The projectile pool holds at most 512 rows. Each claimed row must have a current owner.

Actor stores use the canonical snapshot/apply path. Projectile stores use the complete source record image, including reserved bytes. Keep definition, person, character and projectile-owner fields valid when writing those fields.

Run `native_mission_sound_live` for range, width, current-state and alias checks. Supply `GAME_FOLDER` to also check real sound attachment, the shared ambient frame and lifecycle removal. Keep device output muted.
