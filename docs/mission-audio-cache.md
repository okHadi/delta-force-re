# Mission audio cache

Use `MissionAudioCacheBindings` with current byte, word and dword storage.
Preserve aliasing between asset fields, loaded records, allocation headers and
file/archive cells. Each reached read, write and backend body is required.
Missing bindings raise `MissionAudioCacheError`.

| Source | Function |
| --- | --- |
| 49D050 | `mission_audio_cache_load(asset, alternate)` |
| 494C50 | `mission_audio_cache_convert(record)` |
| 498120 | `mission_audio_cache_allocate_alternate(size)` |
| 498230 | `mission_audio_cache_tag(pointer, tag)` |
| 4992D0 | `mission_audio_cache_seek(offset, origin)` |
| 499280 | `mission_audio_cache_read(destination, size)` |
| 499020 | `mission_audio_cache_close()` |
| 498D50 | `mission_audio_cache_close_loose()` |

The loader returns when asset byte 23 is set. Otherwise it allocates the current
asset dword 1C size. A zero allocation clears asset dword 24 and leaves byte 23
current. A successful allocation seeks to the current asset dword 18, reads the
size again after seek, converts the loaded record, sets byte 23, then stores the
allocation at dword 24. A reached error preserves completed effects.

Alternate allocation checks D09398, selects D09398 through 5013A0, calls 497F60,
and sets 5013A0 to D09384 after return. The normal allocation passes
`{size,4,0,4CE42C}` to 497F60; alternate allocation passes `{size,4,0,0}`.
Bind 497F60 to the real allocator, including its selected heap and memory writes.

Read and seek use current D09730, D096E0, D096D8 and D09738. D09730 selects the
archive pointer stored through D09738. With D09730 clear, D096E0 selects the
D096D8 archive; otherwise the loose-file body runs. Required backend bodies are
498D30/498D60 for loose reads/seeks and 49E6B0/49E6E0 for archive reads/seeks.
They complete their file state and memory effects before return.

Close calls import slot 4C6854 (`_lclose`) with D096D0 only when D09730 and
D096E0 are clear. Bind that import to the real file close. The bank selector owns
the preceding 499120 file-open call. These source services manage file state.

498230 writes a tag at pointer minus C and returns pointer minus 10 when the
pointer is nonzero and the unsigned tag is below 01000000. Other inputs return
the pointer. Bind the same allocation header storage used by 497F60.

The conversion tag at 4CDFF0 is `SAMP` (504D4153) in the supplied executable.
494C50 returns immediately when the record already has that tag. It captures
length and bits before header writes, reads the current rate after the tag
write, then writes volume, rate step and output length in source order. It
reuses `audio_mixer_step` for the exact signed rate calculation.

The 16-bit conversion finds the peak from signed words and scales each sample's
signed low byte, as the original instructions do. A zero peak raises the
original divide-fault boundary after earlier header writes. Empty or negative
loop counts raise an explicit buffer boundary error before the original loop
would run beyond its buffers. Eight-bit conversion copies the source count;
the host provides valid bounds instead of reducing that count.

Bind muted playback through the separate playback services. Cache loading and
conversion keep their full state effects and produce no device output.

## Checks

The source SHA-256 is
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
REA entry/nested evidence is retained in
`.local/reports/mission-audio-cache-entry-source.json` and
`.local/reports/mission-audio-cache-nested-source.json`. Exact instructions are
retained in `.local/reports/mission-audio-cache-disassembly.json`.

Run `tools/check_mission_audio_cache_original.py GAME PROBE --report REPORT`.
The check executes all eight original bodies. It compares full asset/allocation
bytes, shared cells, ordered memory accesses and widths, and calls under backend
changes. Allocator, file/archive and close services use explicit test providers.
The original zero-count loop guard stops before its first access; division faults
use the actual original DIV instruction. Native tests check missing services,
partial effects, current file routing, heap selection, cached records, allocation
failure and conversion faults. These are component checks; the real bank/file
resource graph and playback services supply the live integration.
