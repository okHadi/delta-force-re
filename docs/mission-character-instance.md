# Character instances

Use `MissionCharacterInstanceBindings` with current memory and the real arena allocator. Keep the pool, CHR resource and arena alive through each call.

| Entry | API | Input |
|---|---|---|
|439E48|`mission_character_instance_construct`|ESI instance, EDI prepared CHR|
|43A1C0|`mission_character_instance_auxiliary`|ESI instance|
|43A690|`mission_character_instance_frame`|ESI instance, EDI frame|
|43A2C7|`mission_character_instance_delta`|ESI instance, EDI frame, EBX signed duration|
|43A4DC|`mission_character_instance_immediate`|ESI instance, EDI frame|
|43A730|`mission_character_instance_rotate`|current heading and motion globals|

Supply all seven current register values. Read the seven output values after success or failure. Keep saved stack slots, ESP and EFLAGS outside this interface. The allocation provider implements `497F60(size,4,0,0x439E3B)` and returns the payload address. It follows the C register-preservation contract. Bind required `43B6D5` with the current EDX message address.

Use `MissionCharacterAllocateState` for the physical pool and the existing allocate/init bodies for slot selection, pose seed and action selection. Store pose, delta, root and auxiliary data in the source allocation. Read current rotation tables at42F1D0/42F3D0 and shared439A08/0C/10 directly.

Keep wrapping integer operations and signed division faults. The immediate head path uses the last Y-angle delta as its divisor. Each source do-while loop performs its first iteration before its signed counter test.

Run the native test with a game folder to load the actual PLAYER01.CHR, create and seed its instance, select an action, release through42CD30 and reuse its physical slot. Run `check_mission_character_instance_original.py GAME PROBE --report FILE` for ordered source memory/call traces, seven register values, mutations and failure prefixes. CPU save-stack traffic is outside that comparison.
