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
