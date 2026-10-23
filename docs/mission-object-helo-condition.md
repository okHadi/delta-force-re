# Helicopter condition

Use `mission_object_helo_condition` for `450AE0` and `mission_object_helo_marker` for `45C6A0`. Keep actor, person, marker, list and side settings in their current shared owners.

Supply sixteen aligned frame bytes through the same read/write bindings. Keep their initial values. The condition uses frame offsets `+8` for heading, `+C` for distance, `+0` for radius and `+4` for yaw. Marker output addresses can alias current memory. False marker paths preserve all output words.

Bind these source calls:

- `45C6A0`: the raw marker helper in this module.
- `477670`: the existing signed length body, `collision_approximate_length`.
- `477770`: the existing angle body, `query_angle_477770`.
- `42EE30`: the current `EventStage::set_flag_42ee30` owner.

Route person `+AC` as a raw actor address when `+A4` is 3. For state 1, route the current marker lists at `78F280` with stride `88h`, and current group-2 actors at `8ACE80`. Route the side speed at `7936BC + side * 40h`. Preserve the initially captured person through callbacks and use current actor `+C` at each later source read.

Run `native_mission_object_helo_condition` and `check_mission_object_helo_condition_original.py GAME PROBE --report FILE`. The checker executes both parent bodies and both math bodies. Its event callback supplies explicit state changes and failures. The native test joins the canonical actor/person/list owners to the actual event flag owner.
