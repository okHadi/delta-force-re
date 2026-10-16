# Actor behavior

`actor_behavior` implements the human and local-player callbacks at
`45B5B0` and `45BDF0`, plus the null, waypoint, marker, and animation helpers.
Source: Df.exe SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Set `BehaviorState.host` from `67F528 != 0`. Share one `rng` value between
callbacks. Supply queries through `on_query` and execute effects through
`on_effect` at their call points. Scripted checks can supply ordered `inputs`.
Keep actor and person records valid while a callback runs.

`check_actor_behavior_original.py GAME PROBE --live-queries` compares record
bytes, RNG, and call order. The check supplies foreign query answers and effect
sinks. `actor_queries` supplies the NPC queries used by the live loop.
