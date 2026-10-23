# Object destruction

Use `mission_object_destroy_generic` for the 4533F0 caller. Use the raw form with the incoming EBX value when checking 453270 alone. The caller supplies the raw actor address in EBX.

Use `mission_object_destroy_wood` for 44ED40 (`large=false`) and 44F270 (`large=true`). Keep actor records, the shared RNG, item records and model records current during every call.

Bind each reached foreign body. A missing body raises an error at its source call point. Supply the current item table base after each item lookup. Keep captured item addresses valid through the helper.

Run `call` synchronously. For 460A30, the mutable buffer holds the 24-byte pose and scalar argument 1 represents its pointer. For 45D970, the buffer holds the complete 276-byte actor (0x114) and scalar argument 0 represents its pointer. These pointer arguments contain zero in the portable call. Other scalar arguments retain their source values. Use the buffer before the callback returns. Changes to the pose affect the later 447610 call. Changes to the debris record remain in the next insertion.

Bind 42A440 particles, 43E7D0 positional sound, 460A30 effects, 447610 objectives, 45D970 actor insertion and 44FCB0 area effects to their live owners. Keep sound device output muted.

Use `mission_object_destroy_broadcast` for 447980. It sends message 39 through the shared `MissionClientOutbox`. The four-byte payload holds the unsigned source-pool index and the value, each reduced to 16 bits.

Run `check_mission_object_destruction_original.py GAME_FOLDER PROBE --report REPORT` against the supplied executable. The check covers all four bodies, full debris records, pose copying, shared RNG, source read order, callback changes and partial failures. The native check joins the generic and large-wood bodies to the helper and the actual client queue.
