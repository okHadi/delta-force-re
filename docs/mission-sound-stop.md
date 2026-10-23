# Attached sound stop

Route `43F020(actor, scale)` to `mission_sound_stop(sound.bank_bindings(), actor, scale)`. Use the current `MissionSoundRuntime` registration and actor record owners. Supply its real `49A620` clear service.

The body scans all 1024 registrations. It compares the raw actor and the registration's scale byte with the full scale argument. For each match, it clears sixteen bytes, reads the current actor `+1C`, and clears that link when it points to the same row. Preserve earlier clears when a later provider fails.

Run the current audio frame after the stop. Its shared voice owner stops handles that have no selected registration. Keep software playback enabled and device output off.

Run `native_mission_sound_stop` and `check_mission_sound_stop_original.py GAME PROBE --report FILE`. The native test joins attach, scheduler, registration stop and the same software voice. The checker executes `43F020` and `49A620` and compares all registration bytes, the actor image and ordered accesses under state changes and failures.
