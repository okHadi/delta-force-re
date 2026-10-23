# Projectile owners

Use `mission_projectile_owner` at each live projectile boundary. Keep one canonical `MissionTickState`. Store `ProjectileEntity::owner_slot` as the physical index from `7CCA80`, with stride `114h`. Keep `-1` as null and `0` as a valid owner.

| Actor pool | First index | Record limit |
|---|---:|---:|
| Group 3 | 0 | 256 |
| Group 0 | 256 | 1024 |
| Extra group 4 | 1280 | 1024 |
| Group 1 | 2304 | 1024 |
| Group 2 | 3328 | 512 |

Require alignment, the physical limit, and a current row. Inactive rows remain valid records. Use typed actor handles only after explicit conversion. Keep the projectile target scans in their source order.

Original `46FB40` stores the low 16 bits of `(owner - 7CCA80) / 114h`. Receive body `46EA40` restores the signed index. Spawn body `462F20` stores the raw owner at `+F8` and uses that owner's team and flags.

Run the four projectile native tests and their original checkers. Add `--physical-owners` to the spawn checker. The peer checker carries physical owners through packet delivery and the ledger. The live checker tests damage ownership across all five pools. Foreign transport, terrain and effect providers remain explicit in each fixture.
