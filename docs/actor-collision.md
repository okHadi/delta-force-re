# Actor collision

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

- Bind `actor_target_collision` to the mission tick's `4889C0` target collision query.
- Supply live actor images and identities, selected model radius `+64`, collision presence, current forward mesh matrices, mesh bounds `+40..+54`, and stored collision volume slices.
- Preserve mesh and volume order. Use the model callback's null result to reject a target.
- Apply each mesh position change before the next mesh. Trigger definitions use flag `10` and keep the actor position unchanged.
- Bind `488F00` inventory and mission operations through `ActorCollisionBindings.call`. Execute each request when it arrives. Keep its return value and state changes.
- Resolve the raw source address at actor `+90` through the current physical actor tables. Pass the resolved typed actor ID to delivery effects.
- Keep 32-bit wrap, signed bounds checks, the asymmetric actor height, and box axis tie order.

Run the signed native test and probe. Run `tools/check_actor_collision_original.py GAME PROBE --report REPORT` with the local Python tools. It compares full actor, target and carried records, return values and ordered foreign call snapshots. Model and operation fixtures are listed in the report. Cylinder fixtures use axis directions; the facing helper has a separate native original-instruction check.
