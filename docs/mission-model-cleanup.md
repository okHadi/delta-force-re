# Model cleanup

`mission_model_cleanup` ports the complete `486660` chain: texture release `485B30`, LOD release `485DD0/485D70/485D50`, then registry clear `486510`.

Borrow the same physical image and heap as the model loaders. Provide current raw read/write/call bindings. Bind `4980D0` to the current allocation release and `49A620` to the current memory clear. Use source return values for unowned releases. The module owns no cache or storage.

Texture release uses current DWORD count `C54480`, shared flag `80h` and captured pixel pointers. It rereads count after each row, writes count zero, then clears all 2000 descriptors. Sharing and disable gates stay current. A null captured pixel pointer skips its explicit pixel store.

LOD release reads current count `C65DCC` after each completed record. It releases a captured nonzero payload pointer, then clears that record's `B8` bytes. It writes count zero after the loop. Unreached LOD headers stay current.

The final clear is `49A620(C80D14,0,F23Ch)`. This includes the model count, localization gate and 500 graphic records. Texture level at `C80D10` stays current in the normal 600-record span.

Preserve slot-600 aliases through the same image: count 601 reaches a LOD record at `C80D10`, reads its payload pointer at `C80D28` and clears its `B8` bytes. This also changes adjacent globals and graphics. Use the physical source address at raw boundaries and explicit cache conversions at typed consumers.

Errors retain completed releases and writes. Later stages execute only after the preceding stage returns.

Run `native_mission_model_cleanup GAME_FOLDER` for supplied Gazelle and bexp load, cleanup and reload. It uses actual registry, cold, texture and LOD parents with the real SoundRuntime file/image/heap/free/clear owner. It checks first-load flags, cached lookup, source count/table clears, live-header removal with retained arena bytes, same-cache geometry/hardpoints/pixels/palettes and exact file consumption. Load-context calls and bounded ASCII comparison are explicit native test services. The LOD numeric provider validates its fixed constants under a numeric-only FP contract.

Run `tools/check_mission_model_cleanup_original.py GAME_FOLDER PROBE` for original-code comparisons. It checks the complete shared image range `C54480..C8FF50`, ordered accesses and foreign calls, current counts/pointers, shared/null branches, slot-600 aliases and partial faults. Release and clear effects are explicit test services. CPU registers and saved stack bytes have a separate host contract.
