# Mission frame

Target: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`mission_frame` implements `0x404340` and timer `0x43F560`. It preserves the
stop query, event release, timer gates, stage order, second counters, menu
branches, and return value. Current raw globals control each branch.

`mission_tick` joins the command, single-player message, and actor stages.
Bind the complete player stage `0x482B00`. Its prefix binding owns current
person burst state and the reached `0x47A290(0x8C)` input query.
`mission_weapon_part(state, held_fixture)` is an isolated weapon test helper
with an explicit held value and internal WeaponState burst storage. Use the
full player binding for live camera, target, and weapon state.
Bind `world.external_stage` to complete each reached foreign call in order.
A missing binding raises `MissionTickError`. Bind each actor callback, ground
query, collision query, frame mask, and linked record that the mission uses.
The built-in `0x484740` steady test reads the current weapon, scope, aim blocker,
and control. Local control clears the live animation condition state first.
Actor clearance keeps the animation root count read at entry. Local clearance
uses the current root count. Tagged actions complete
`0x460D20` before the animation tick and then read current driver inputs.
Command `0x484C20` requires its full blocker, view sway, and RNG binding.
Multiplayer `0x46F5C0` requires a complete external body. Bind unowned
command effects through `world.dispatch_effect`; threaded calls receive the
prior result in argument zero. Bind `world.weapon_effect` and
`world.projectile_query.on_event` for reached effects. Weapon effects run at
`WeaponState::emit`; drains clear the recorded events. Scoped handlers restore
the prior sink and input query after return or error. The gun expiry query
requires `world.trigger_held_47a290(0x8C)` only when its source gate is reached.

`MissionStageCall` contains the original address, up to four arguments, and
an optional byte payload. The provider returns the original result in an
`optional<uint32_t>`; `nullopt` means unbound. The report contains ordered
frame calls, the result, and the exit reason.

Run the original frame comparison with the local game and native probe:

```sh
.local/tools/python/bin/python tools/check_mission_tick_original.py \
  .local/reference-game .local/root-mission-loop-review/mission_tick_probe
```

The checker executes original `0x404340` and `0x43F560` in Unicorn. Other
callees use explicit return and state-change fixtures. It compares 20 globals,
return status, exit reason, and call arguments in order. This check covers the
frame contract. Test each bound stage body separately before game integration.

Probe mode `steady` reads `SCOPE CONTROL WEAPON_ID AIM_BLOCK [TOGGLE]` per
line and prints the raw `0x484740` result. A nonzero optional toggle runs
`0x4847C0` through the command pump first. Weapon IDs select the definition
directly.

Use `mission_dispatch_command(state, id, value, input.command_table())` for
synchronous `477D40` calls from global-key callbacks. Keep the current input
table alive. This entry reads the current local actor, applies each effect,
and writes current dispatch fields back through the same helper as the queued
command pump. An active frame receives its trace. Outside a frame, trace
storage is local to the call. Required effects stay synchronous; exceptions
restore the previous weapon callbacks, trigger query, and active report.
