# Model texture loader

`mission_model_texture_load` ports source `485BA0..485CFF`. Provide current raw read/write/call bindings and one current bank DWORD at the argument address. The function owns no image or descriptor cache.

Use the same physical image and heap as the model registry, cold loader and LOD loader. Texture count `C54480` is a DWORD. Sharing gate `C54484` precedes 2000 descriptors at `C54488`, stride `24h`. The table ends at `C65DC8`, the separate disable gate. Texture level is read as BYTE `C80D10`.

Bind each reached service:

- `499280(destination,size)`: current file read.
- `497F60(size,10h,0,descriptor)`: payload allocation.
- `4992D0(size,1)`: current file seek.
- `499E5E(prior,current)`: source string comparison with current addresses.
- `4980D0(pointer)`: current owned allocation release.

The loader stores a bank byte, pixel pointer and palette pointer. It packs 256 four-byte RGB entries into three-byte entries within the same allocation. Keep current palette pointer reads active through this pass. A skip advances the file and keeps the descriptor count. A shared match releases the new allocation, reads the earlier pointers after release, sets flag `80h` and publishes the current count.

`mission_model_texture_initialize` ports `485B10`. Bind `49A620(C54488,0,11940h)` to the real current memory clear. It zeros the count and all 2000 descriptors while keeping the sharing gate current.

Keep the bank argument live. Loading reads its low byte; earlier-record comparisons read its full DWORD. Keep allocation-zero behavior, source arithmetic, width aliases and completed writes on errors. A missing reached service raises an error.

Run `native_mission_model_texture GAME_FOLDER` to load supplied Gazelle and bexp through actual `486370 -> 485BA0 -> 485E30`, using the real SoundRuntime image, file and allocator. The check compares source flags, loaded and skipped records, pixels, packed palettes, localized vertices, hardpoints and exact file consumption. Its LOD x87 provider validates current `100.0/62.0` constants under a declared numeric-only contract. A reached sharing comparison requires its source string provider.

Run `tools/check_mission_model_texture_original.py GAME_FOLDER PROBE` for original instruction comparisons. The replay checks complete declared fixture state and ordered parent accesses/calls, with mutable pointers, arguments, counts, gates and partial errors. Foreign child effects are explicit fixtures. The initialization replay checks its full clear call and the declared descriptor subset; the native test clears all 2000 descriptors.
