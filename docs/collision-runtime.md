# Collision

`collision` decodes stored 3DI collision volumes and face bounds. It implements
terrain marches (`47E390`, `47E4B0`), face tests (`42D2A0`, `42D460`), and
segment bounds checks. Source: Df.exe SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Read stored volume records in mesh order. Supply current terrain geometry,
detail maps, height bounds, and mesh transforms. Keep fixed-point units and
32-bit wrap at each arithmetic step.

Run `check_collision_original.py GAME PROBE` to compare terrain marches,
face tests, bounds checks, and four stock models with original instructions.
