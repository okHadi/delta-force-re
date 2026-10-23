# Mission leave

Use `mission_leave` for original `404150`. Bind current memory reads and stores with widths 1, 4, and 8. Keep each 8-byte access atomic at the provider boundary. Supply every reached call synchronously; missing providers throw.

Keep actors, profile, result image, event stage, resources, sound and input owners alive through this call. Route `401E70` to existing `mission_profile_leave`, with `end_events` bound to the current `EventStage::run_bit2_events`. Route CHR release `42CD30` through the current animation owner. Provide all 256 physical G3 rows, including their current tail, for the `+18` scan.

The body dims the captured surface, runs ordered cleanup, restores rules `67F548` from `67DBE4`, runs profile/end-event effects, releases CHR instances, then calls `42C950`, `449620`, and `482910`. The 8-bit dim path reads current width per pixel and current height per row. The 32-bit path captures signed width/2 per row and reads/writes qwords with mask `FEFEFEFEFEFEFEFE` then unsigned right shift 1. Its do-while loop retains the source error prefix for zero/negative widths. Providers enforce the current mapped surface extent.

Retain final results and state-request data in the Game transition owner before destroying the mission. This module uses caller-owned memory and creates no persistent result or actor storage. A failed required call leaves earlier effects committed.

Run `native_mission_leave` for the real profile/end-event/current-actor join, cleanup failure prefix and qword failure prefix. Run `check_mission_leave_original.py GAME PROBE --report FILE` for ordered memory/call comparison with the original executable. Foreign cleanup bodies in that replay are explicit fixtures. The native event result action is an explicit host fixture; actor snapshots and profile/end-event execution use current canonical owners.

The integer qword operation has the same output bits as the source MMX mask/shift. It uses no MMX registers, so the portable body needs no EMMS instruction. Test scope covers memory and call effects.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
