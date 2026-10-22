# Projectiles

Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Use original units: 16.16 positions, unsigned full-turn angles, and Q22 matrices.
The module builds muzzle poses, shot plans, packed spawn records, and projectile
records. `tick_projectile` runs one `464EB0` record pass.

Supply terrain, model, character, and water queries through `ProjectileQuery`.
Keep query order and retain each selected hit result. Use the helpers in
`projectile_hits` for model, bone, water, damage, and impact operations.

Bind `ProjectileQuery::on_event` to execute effects at each source call site.
Impact runs before TTL and item removal. Movement and edge effects run before
the final position, gravity, and TTL writes. Keep projectile storage stable
during callbacks. The events vector records these calls for inspection.

Run `check_projectiles_original.py GAME_FOLDER PROBE --report REPORT` with the
local Python tools. It compares 370 cases with original x86 code, including
the 2,561-entry sine table, matrices, muzzle poses, shot plans, packed records,
spawn state, and motion. Collision and effect fixtures are explicit inputs.
