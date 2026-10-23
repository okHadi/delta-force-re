# Mission dialogue registry

`MissionScriptRegistry` loads the supplied DLG0 dialogue archive through the shared sound memory. It uses one `MissionScriptQueueCells` queue and the current script voice cells.

1. Keep the sound owner, script cells, backend bindings, and scratch memory alive.
2. Allocate at least `0x200` aligned scratch bytes. Use `0x4000` for nested calls.
3. Supply the current CRT thread pointer and current dialogue volume.
4. Copy the full mission header to `8CF908` before sound resolution. The dialogue archive field starts at offset `0x64`.
5. Route required script services through `service()`. Use `frame_bindings()` for the frame script stage.
6. Route claimed raw cell accesses through `owns()` and `memory()`.
7. Call service `455EA0` before the shared sound memory is released.

The loader owns `455C90`, node creation `455C30`, queue clear `4560A0`, and registry cleanup `455EA0`. The audio path owns `49D400` and `49CF70`. It uses the real sound cache, file loader, playback state, and RTXT entry service. The tokenizer uses the current thread's `+10` cursor. The step callback reads current volume at each source read.

Supply reached text display, missing-sample diagnostics, message publication, and unknown callback bodies. A bank-set cleanup reaches required service `49DB20`.

The native asset test uses `C05M07.BMS`, `DFDLG05.DBF`, `DFDLG05.PWF`, and `DFDLG05.BIN`. It loads 120 dialogue records and 142 steps. Event 706 exercises the real queue, frame stage, sample cache, silent playback state, and cleanup. Device output stays off.

`check_mission_script_registry_original.py` runs the original hash-bound x86 bodies with explicit file, allocator, and backend effect fixtures. It compares complete watched bytes, ordered memory accesses, and calls. The native asset test checks the real joined services.
