# Mission muzzle

Keep the mission cache and `MissionAnimations` alive while `MissionMuzzle` uses them.

- `normal` runs the 482D90 offset and 4847D0 barrel-tip paths. Supply current view offsets and eye height. The hardpoint comes from the primary item model (+94).
- `animated` runs 45E7A0 with the current actor image and character pose. The hardpoint comes from the secondary item model (+98). The shot callback runs after pitch restore and slot-seven writes.
- Share mutable slot seven across calls. Use `set_slot_seven` to supply prior scratch state for a callback with fewer than eight meshes.
- Supply `set_slot_source` for other model callback tags. The callback must return its shared mutable matrix.
- Use `attachment()` for current C22B40 state. Its initial matrix comes from 481AF0.
- Bind models with real hardpoints. Route melee, placement, and other weapon actions through their own weapon paths.

Run `tools/check_mission_muzzle_original.py EXE PROBE --items ITEMS.DEF --models MODELS --chr PLAYER01.CHR`. Use `--report PATH` to save results. This check executes the original weapon initializer, muzzle functions, character callback, and controller. Name services and shot dispatch use test callbacks.

Original executable SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
