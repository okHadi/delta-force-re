# Current profile memory

Keep `MissionProfileMemory` with the current `ProfileStore` and selection owner. Use `PlayerProfile::source_bytes()` for source profile routines. Keep each record at its fixed `A90`-byte size.

The view reads the current selected pointer at `4CF2E4`. Nine stable profile slots start at `21100000`, stride `1000`. This bank is separate from model, animation, sound scratch and sound heap storage. Reserve the full `21100000..21108FFF` bank.

Reads and writes use the same current profile bytes. Widths one, two and four support overlapping byte views. Captured pointers stay with their assigned slot when selection changes. Pointer selection remains with the selection owner. Record gaps, cross-record accesses and invalid owners raise errors.

Game routes this view before its source-memory fallback and sets current mission cell `68BA34` from the selected campaign mission. Profile data stays with Game across mission resource lifetimes. Save through `ProfileStore` after the source effects complete.
