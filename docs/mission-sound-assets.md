# Sound assets

Use `MissionSoundAssetBindings` for current byte-addressed storage. Read and
write widths are 1, 2, or 4. Keep handles and callback bindings stable during
each call. Complete each reached allocator, file, cache, and playback service.

`mission_sound_context_load` ports `49CE00`. Supply a separate four-byte scratch
block for its stack word. The function clears the 200-byte context, opens the
file, reads sample and trigger tables, fixes up sound sets, and registers the
voice completion callback. `mission_sound_fixup` ports `49D8C0` and builds
current layer/member pointers. Preload marks and sample loads share those
records with the sound-bank resolver.

`mission_sound_find_sample` and `mission_sound_find_bank` use case-sensitive
names. Sample lookup requires a nonzero sample size. Allocation class values
below `1000000` are stored in the allocation header at pointer minus `C`.
Offsets and constants use hexadecimal values.

`mission_sound_file_open` ports `499120`. It tries the source-selected loose,
primary archive, and secondary archive paths. It writes current selection
cells after success. Supply the caller's incoming EDI for a failure path that
has no backend return value. A reached missing value raises an error after
the source diagnostic. All file operations use read-only game data.

Run `native_mission_sound_assets` for errors, aliases, lookup, and selection
rules. Run `check_mission_sound_assets_original.py GAME PROBE --report FILE`
for original loader and file-router comparisons.

Run `mission_sound_stock_probe GAME_FOLDER` for supplied `SNIPER.PWF` loading,
the `FANFARE` bank, sample conversion, voice publication, and voice completion.
It reads static tables from the supplied `Df.exe`. File/archive and allocation
providers are local memory services. Definition sorting, full mission startup,
window messages, and device output require their live host bindings.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
