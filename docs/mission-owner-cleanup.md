# Sound and resource cleanup

The module owns source `43E6D0`, `494DC0/4A3590`, `43E640`, `49DB20`, `402D40`, `44B940` and `44B920` (611 bytes).

Provide current raw read/write/call bindings. Keep one ScriptRegistry, Playback, AudioFrame and allocation owner alive through cleanup. The module borrows their storage.

`mission_sound_cleanup` clears the script queue, resets enabled playback, stops current handles, clears reached ambient samples and all 1024 registrations, cleans script voices/registry, clears reserved channel bytes 10..15, then destroys primary and alternate PWF contexts. Bind each reached child to its real current owner.

Map raw `4A2712` to current Playback disabled state and bind `4A2AED` to the proved Playback initialization. A reset error keeps disabled state one. Enabled zero skips that reset. The source keeps generation, master and handle cells current.

`mission_sound_bank_cleanup` takes one current context argument DWORD and 16 aligned caller-owned scratch bytes. Keep both in the same memory owner. It reads current bank/layer/member counts, pointer slots, asset flags, allocation fields and argument/frame spills at their source sites. A null context or null bank pointer skips the frame. Released asset allocation fields stay current; the source clears loaded flags and the final bank pointer.

The source `49CF70` releases its sample table before `49DB20` reads member asset pointers into that table. The shared source arena unlinks allocation headers and retains mapped bytes. Check the live header list for allocation ownership. Released addresses remain readable and can be reused by later allocations. `4980D0` returns `FFFFFFFFh` for a payload with no linked header.

`mission_resource_cleanup` captures one record address, reads and releases its three current pointers in order, then clears 32 bytes. `mission_resource_cleanup_global` uses actual `503304`. Bind its current producer explicitly at the host boundary.

Run `native_mission_owner_cleanup` for release order, current resource pointers and partial errors. Its supplied-asset mode executes complete `43E6D0` with real SNIPER and NEW PWF graphs, one canonical ScriptRegistry/Playback/AudioFrame and the same source arena. It compares the exact live allocation set before and after cleanup, checks all context/bank/layer/member/sample fields, observes a heap callback after unlink, and checks reset state, ambient release and all 1024 registration clears. Three resource allocations use that same heap. Unrelated allocations stay live. Retained reads and repeated unowned releases follow the source contract.

Run `tools/check_mission_owner_cleanup_original.py GAME_FOLDER PROBE` for original comparisons. It checks declared current raw state, ordered owned accesses/calls, current pointer/count/argument changes and partial errors. Foreign effects are explicit fixtures. CPU register/saved-stack and allocator internals have separate host contracts.
