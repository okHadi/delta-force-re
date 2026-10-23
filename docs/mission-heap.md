# Arena heap

Use `mission_heap_initialize`, `mission_heap_allocate`, `mission_heap_release`, `mission_heap_find_previous`, `mission_heap_notify_free` and `mission_heap_find_hole` for the nine source bodies. Supply current width-four memory and required calls through `MissionPeriodicBindings`. Keep one raw arena owner. All reached missing or rejected accesses throw and retain earlier writes.

Keep the current context pointer at `5013A0`. Its cold source value is `D09384`. A context has five DWORDs: arena base, usable size, sentinel/head, generation and optional callback-table pointer. Each allocation header has four DWORDs: payload size, generation, label and next header. The payload starts at header `+10h`.

Map a caller-owned `30h`-byte allocation frame. The body uses locals `+0/+4/+8`, current size at `+20h`, alignment at `+24h`, and label at `+2Ch`. The argument at `+28h` is unused. Preserve other bytes. Supply the actual incoming EBX value. A null or changed head can reach the source path that uses this register and the current cached-next local. Keep these values explicit.

Map a caller-owned `Ch`-byte release frame. Its current payload argument is at `+8`. Release captures that argument for unlink, then rereads it for a reached callback scan. Keep frame addresses and all captured helper arguments stable through calls. Frame, arena, context and callback views use the same current source memory.

Initialize calls required `49A620` to clear the 20-byte context and required `49DC80` for backing storage. It writes the source sentinel and generation. It leaves other arena bytes as supplied by the host. Source startup `401050` sets the main budget to `1800000h` bytes (24 MiB). The initializer uses current `CC0870`; a zero value selects the `F00000h` fallback. A larger reserved host capacity leaves the source usable-size field at its requested value.

Treat `49DC80` as a required CRT malloc boundary: one requested-size argument returns a stable payload pointer or zero. It has no output-size pointer or zero-fill contract. Keep the arena backing mapped and stable for its lifetime. Its initial bytes are explicit host input.

Allocation uses the original alignment mask, hole search, unsigned bounds and wrapping arithmetic. It writes source header fields and links in order. Release changes only the predecessor link before its reached callbacks. Keep freed headers and payloads mapped. Later allocations reuse the raw bytes through the current list.

The optional callback table has 2048 eight-byte rows. A scan captures the table base, reads each current key, checks a matching row's function pointer, then rereads that pointer for the required indirect call. The argument is the captured key. Keep callback changes to later rows, context, arguments and arena links visible. A callback error retains the unlink.

Run `native_mission_heap` for same-address reuse, retained PCM bytes, real cache conversion/tagging and the real `49CF70` audio-destroy prefix. The required `49DB20` consumer remains an explicit boundary in this test. Callback tests check unlink order, current final-row changes and failure prefixes. Backend failure tests cover 24 MiB and the fallback request. Sixteen allocation→middle free→reuse sequences cover sizes zero through 512 bytes with alignments four and sixteen.

Run `check_mission_heap_original.py GAME PROBE --report FILE` for all nine original bodies. It compares whole arena/context/callback/frame images, current access order, address arithmetic, callback changes and failures. Stack frame addresses map to the caller frame; register-save and return-address slots are excluded. Backing, memset and dynamic callback calls use declared fixtures. It also runs sixteen retained-byte reuse sequences on the original allocator and free bodies.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
