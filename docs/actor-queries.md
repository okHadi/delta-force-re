# Actor queries

`actor_queries` implements NPC target selection (`45E280`), combat (`45F2C0`),
teammate alerts (`45F090`), nearby and aim-ray checks (`45F010`, `45FB00`),
facing scans (`45EDD0`), zones (`45FD90`, `45FE30`), animation IDs (`460410`),
and angles (`477770`). Source: Df.exe SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Bind `ActorQueryProvider` to current actor/person records. Supply terrain,
sine tables, sight tests, weapon frames, and actor pointer values through
`ActorQueryData`. Execute emitted effects at their call points. Keep the same
RNG state as the behavior callback. `weapon_frame_10` supplies the raw frame
word; `weapon_frame_14` supplies its signed high word.

Run `check_actor_queries_original.py GAME PROBE` for original-code comparison
with declared foreign fixtures. Run `check_actor_angle_native.py GAME PROBE`
on an Intel Mac or Linux host for extracted original-instruction angle checks.
The native check covers x87 boundary results with full precision.

The game loop supplies these bindings before it can run live NPC behavior.
