# Live mission bindings

Call `init_mission_tick` with the loaded mission. Bind its `MissionAnimations`
and `MissionGeometry` objects. Keep these objects and their asset cache alive.

Animation updates and frame masks use the same controller. Ground queries read
current actor records and retain stable definition pointers. Supply matrices
for each reached special model tag.

Bind all other reached world and stage functions before calling `mission_tick`.
Run `mission_live_probe GAME_FOLDER MISSION` to check the initial join.

`mission_actor_query_data` binds current animation weights, terrain, trigonometry,
and sight rays. Supply current zone records and the score flag. Sight uses group
1 then group 0, with matrix callbacks after the bounds and sphere gates.

`initialized_mission_matrices` supplies current character poses and initializes
each callback's scratch and copied stack flag bytes to zero. This is the rebuilt
engine's defined state at the callback boundary. The geometry API also accepts
explicit prior flags for original-state comparisons.

Use `apply_mission_actor_image` after a raw collision or damage helper. It keeps
record bytes and current tick fields in the same state. Replace person and
character bindings through their allocation APIs. Apply a cleared image to
remove an actor and its current animation binding.

Call `bind_mission_actor_collision` with the current geometry and trigger effects.
Effects read and change the live tick state. Each effect sees earlier collision
writes. Its changes remain current on return or error. Supply a handler for each
reached pickup, objective, sound, or removal call. Actor `+90` holds a raw source
address. The collision join resolves it before it sends typed IDs to effects.
The physical table limits for groups 0–3 are 1024, 1024, 512, and 256 rows.
The separate pool at `0x822E80` requires its own binding.

Call `bind_mission_linked_position` after mission setup. Carried actors receive
the current carrier position at the end of each actor update. See
[linked actor position](mission-linked-position.md).

Call `bind_mission_player_burst` after player setup. Weapons read and write the
local actor's current person counter. Use `mission_player_burst` as the storage
provider for full player weapon calls.
