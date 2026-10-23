# Mission fragment spawn

`MissionFragmentSpawn` ports `463200`. Share the mission's 512-projectile pool and the `MissionProjectileSpawnCells` ring with ordinary projectile spawning. Bind the real `4776C0` sine/cosine helper; `sincos_q22` provides that body. This function uses no random state.

Pass the current six-word position record, raw source owner address, and raw speed bits. Keep the input address stable through both sine calls. The function saves the yaw before the first call, reads the final pose after both calls, and returns the current shared ring value. Callback changes to the ring do not change the selected output slot.

The selected record is cleared in full. Its 204 reserved source bytes have one owner in `ProjectileEntity`. Use `snapshot_projectile_record` and `apply_projectile_record` for complete 276-byte record views. A null owner maps to slot -1. Source address `7CCA80` maps to slot zero. Require aligned owners within the current physical actor pools.

The fragment stores class flag `20`, life 400, color `01010101`, and the source fixed-point velocity. The shared projectile tick processes its life, movement, collision, and gravity. Keep reached effects bound and audio output muted.

Run the original comparison:

```sh
.local/tools/python/bin/python tools/check_mission_fragment_spawn_original.py .local/reference-game build/mission_fragment_spawn_probe --report fragment-spawn-report.json
```

Use the shared [physical owner codec](mission-projectile-owner.md) to check each current actor binding.
