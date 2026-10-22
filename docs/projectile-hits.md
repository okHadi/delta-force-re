# Projectile hits

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

- Use `projectile_model_hit` for `464400` model queries and `projectile_bone_hit` for `463790` character queries. Keep each query result until the tick selects a hit.
- Bind actor images and identities to the live actor tables. Set `WorldHitQuery.skip[0]` to the original skip identity.
- Supply model radii, current mesh matrices, geometry tables and material identities through `ProjectileModelBindings`. State callbacks run after the original bounds and sphere checks.
- Use `projectile_character_spheres` with the prepared CHR model and current pose to build character hit spheres.
- Use `projectile_water_hit` for `464BA0`. Keep the distance unchanged on a clear ray.
- Use `projectile_apply_damage` for `464C50`. Pass the selected mesh or bone index as `hit_kind`. Bind the definition, owner, person alert byte and live callbacks.
- Call `projectile_impact_effects` from the synchronous tick impact callback, before projectile removal. Bind the target from the selected actor list and result record. Execute each effect when the provider receives it.
- Use `projectile_animated_muzzle` for `45E7A0`. Bind weapon `+98` through item `+6C`, weapon `+90` as the hardpoint index, the mutable graphic slot-seven matrix, `C22B40`, and the `45EA00` shot provider.
- Supply every provider on a reached path. Missing providers and invalid views raise an error.

Run the native test and signed probe. Run `tools/check_projectile_hits_original.py GAME PROBE --character CHR --report REPORT` with the local Python tools. The replay checks original instructions, full damage record bytes, shared damage fields, ordered effect snapshots, model and bone hit outputs, mutable muzzle matrices and shot poses. Query and effect fixtures are listed in the report. The stock CHR sphere check executes `46BC00` and `45CDA0` with real frames.

The live caller supplies rendering, sound, allocations, damage callbacks and the shot provider. Original-instruction fixtures check their call order and inputs. General facing angles use the separately checked `477770` helper; this replay uses axis-aligned damage velocities and quarter-turn animated muzzle yaw.
