# Character resources

`mission_character_resource_load` runs `42CBB0` with current source memory. Put the filename pointer at caller frame+14h. Supply frame-7Ch through frame+18h for the current outer and nested local fields. Keep the frame base and scalar arguments stable during callbacks.

The loader uses the current 64-row registry at `6BA440` and count at `6BBD40`. It loads an actual CHR file through required `499410`, relocates its pointers in that same arena allocation, stores its real header label through `498440`, prepares integer fields through `42C9A0`, and publishes the current count. Name comparison `499E5E` and extension `456560` require providers.

`mission_character_resource_relocate` captures the source filename and output-cell arguments across the file call. It publishes the returned allocation before pointer fixup. `mission_character_resource_prepare` requires eight current caller local bytes. Address arithmetic wraps at 32 bits. Scaling uses signed multiplication by `1D2B0h` and the source 16-bit product shift. Track conversion uses current unsigned division; zero divisor raises an error at the reached source prefix. These bodies use integer operations.

The caller owns the registry, file buffer, cache, and arena. Use current read/write/call providers. Keep all reached spans owned. CPU registers, flags, saved stack slots, and stable numeric C arguments are outside this memory interface. Raw source copies and loops retain their source bounds and current reads.

The resource pointer returned by the loader is the physical 64h-byte registry row. Its+60 points to the actual arena payload. Use this same owner for source `42C950` release. Instance allocation and typed-controller attachment use separate services.

Validation compares the four original bodies, ordered memory/call hashes, final owned bytes, callback changes, and failure prefixes. Native asset coverage uses the supplied unmodified `PLAYER01.CHR`, real Sound file/arena services, declared name-comparison boundary, current preparation, deduplication, actual header label, and real `42C950` release. Generated cases cover additional raw arithmetic and failure states.

Normal replay passes 369 cases; sanitizer replay passes 519 cases. Both include the actual 2,117,896-byte CHR payload and zero/negative/INT_MIN decrement counters. Native normal/full-core ASAN/UBSAN tests use disabled copy elision. Source, native test, and probe compile as Windows objects.
