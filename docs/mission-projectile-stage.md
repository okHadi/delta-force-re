# Mission area stage

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

- Bind `mission_listener_stage` to `45E050`. Supply the local actor, alternate pose and synchronous `43EBA0` listener provider.
- Bind `mission_pending_area_stage` to `465BD0`. It checks area effects in canonical actor groups 3, 0, 1, in that order, for each queued record. Group 0 is table `7DDE80`; group 1 is table `867E80`.
- Supply live actor counts, images, identities, definitions and model bounds `+64/+68`.
- Bind `segment` to the real `42E2B0(actor,start,0,end)` query. Preserve its signed return value.
- Execute actor update callbacks and local-hit effects when called. Bind `random` to the canonical `MissionTickState.behavior` object.
- Keep the pending vector live. Effects can append records or change current fields. The host stage reads each record by index and clears the queue after the pass.
- Supply `incoming_dispatch` when the first queue type is outside 0..5. This binds the original incoming EDI and local callback slot. Later invalid types use the last dispatch state.

Run the signed native test and probe. Run `tools/check_mission_projectile_stage_original.py GAME PROBE --report REPORT` with the local Python tools. The replay checks `45E050`, `465BD0` and damage helpers `465830`, `465A60`, `465B30`, including full actor/queue/RNG state and ordered callback snapshots. Query and effect providers are declared fixtures. Facing uses axis-aligned fixtures and the separately checked original angle helper.
