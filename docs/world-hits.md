# World hit queries

`world_hits` implements the segment query (`42E2B0`), model-list hit scan
(`42DB80`), local transforms (`45CE60`, `45CF30`), integer square root
(`477620`), and approximate length (`477670`). Source: Df.exe SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Supply current model records, mesh matrices, face tables, terrain bounds,
water level, and skip identities. Keep record order. Read the exact return
code from `WorldSegmentResult`; the extra fields identify the selected face.
Material mask `868C` counts transparent surfaces. A solid hit returns its
negative distance.

Run `check_world_hits_original.py GAME PROBE` to compare point and direction
transforms, math helpers,
model-list scans, and segment queries with original instructions.
