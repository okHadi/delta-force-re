# Mission explosion queue

`mission_explosion_enqueue` ports `4657D0`. Use the current `MissionTickState.pending_spawns` vector as the active `9E2E40` queue and its size as `A15898`. The function reads current multiplayer and host cells, accepts at most 30 records, and returns whether it added a record.

Pass a canonical owner handle to `mission_explosion_enqueue`. Pass a raw source actor address to `mission_explosion_enqueue_source`. The source form resolves the owner only after the gates and capacity check. Null stays null. Source address `7CCA80` maps to group-three slot zero. Require a unique aligned address in the current actor tables.

Use the same queue for projectile impacts, peer effects, and the `465BD0` area stage. `mission_area_live_stage` owns area damage and current actor callback delivery. A callback can add another queue record; the running area stage sees it. Keep its required queries and effects bound. The enqueue body reaches no foreign calls.

Run the source comparison:

```sh
.local/tools/python/bin/python tools/check_mission_explosion_runtime_original.py .local/reference-game/Df.exe build/mission_explosion_runtime_probe --report explosion-runtime-report.json
```
