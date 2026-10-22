# Live projectile queries

Use `mission_projectile_live_query` with the current mission, geometry, animation
poses, item damage limits, and stable material identities. Keep these owners
alive while the query exists.

Bind `state.world.terrain` to the current terrain geometry. The default terrain
query runs base-height `47E390`. Supply an optional terrain override and actor
update, alert, local-hit, impact, move, and edge handlers
for each reached path. Handlers run at the source call site. Actor images write
back before each handler and refresh after return or error.

Model queries use stored model +60 bounds and visit group 1, then group 0. Character queries visit group 3.
Water follows the character query. Equal distances keep the first group hit.
Use current person slots, health, owner, animation, and damage cells.

Run the checker with local original files:

```sh
python tools/check_mission_projectile_live_original.py Df.exe PROBE --models MODELS --report REPORT
```

Model fixtures supply clear terrain. Foreign handlers record calls without audio. Model callbacks, face selection, damage,
and impact branches run from the original instructions.

Set `ProjectileEntity::owner_slot` to the group 3 slot, or -1 for no owner.
Slot 0 is valid. The live query maps this slot to the same actor identity for
ray exclusion, damage ownership, team checks, and impact effects.

The terrain query maps a clear march result to code 1. It keeps hit codes
unchanged, including zero and positive values from a ray that starts below
the surface. Each call reads the current terrain binding.

Add `--game GAME_FOLDER` to the checker command to compare terrain queries
for each stock mission with the original instructions.
