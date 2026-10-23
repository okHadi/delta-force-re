# Object target queries

Use `mission_object_target` for 44F540. Supply the current canonical shooter reference. The shooter can be in group 0. The result contains a raw group-3 source address and the final signed fixed-point score.

The selector checks exactly 256 physical group-3 slots. Current counted rows come from `MissionTickState`. Bind `physical_slot` to current shared bytes for each slot beyond that prefix. Each reached word gets a fresh view. Keep the pool alive. The selector preserves the counted table size. An unbound reached tail raises `MissionTickError`.

Bind `sight_line` to the synchronous 42E2B0 body. Values 1 and 2 accept a candidate. Bind `frame_weight_10` to current definition +78, actor +80, and frame +10. The selector reads this weight after the sight call. It then reads current +6D posture and +6C flags. Shooter person +74 and +9C are read after the full scan. Required missing bindings raise an error at their call site.

The selector uses the original angle, integer square root, and three-axis distance helpers. Team checks use the low word at +70. Yaw and pitch each have a folded 16-step limit. Scores use wrapping fixed-point products and signed comparisons. Equal scores keep the earlier slot.

Use `mission_object_region_mark` for 42EDD0 with the shared `EventStage`. That owner stores the 128 current words at 6E0050. The source word index is `2*region + (signed(bit) >> 5)` with wrapping arithmetic. The mask is `1u << (bit & 31)`. Reached storage bounds are checked.

The original comparison uses Df.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. It runs real selector math and bit writes, with explicit sight callbacks that change current flags, shooter person, and yaw. A frame-read fixture also changes posture before its source read. Native tests cover slot 255, missing sight and tail providers, current tail reads, local-player exclusion, signed bit rows, and table bounds. Compile checks cover Windows objects. Sanitizer tests use address and undefined-behavior checks with copy elision off.
