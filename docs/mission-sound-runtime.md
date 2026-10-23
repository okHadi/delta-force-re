# Mission sound owner

Keep one `MissionSoundRuntime` for the current shared sound state. Supply the supported exact `Df.exe` bytes, the current asset store, shared audio-frame cells, and the current terrain pointer. The owner checks the executable hash before it uses its tables.

Bind `script_key` to current `503228`. Bind current actor and projectile records through `record_base`, `record_read`, and `record_store`. Validate the complete access width. Frame reads use `actor_word` when supplied, or the claimed record reader. Unclaimed original record storage raises an error.

Copy the complete 268-byte mission header to shared `8CF908`. `mission_dialogue_archive(mission)` reads the 16-byte dialogue archive field at offset `0x64`, mapped at `8CF96C`. Call `initialize_definitions("LOOP.DEF")`. Then call `resolve_definitions("SNIPER.PWF", "NEW.PWF")` at the startup resolver site. The owner uses the ambient parser, definition sort, PWF graph, cache conversion, and voice modules. Use `global`, `positional`, `attach`, and `loudness` for reached requests. The registration array, listener, yaw, and scheduler counter use the supplied frame cells.

Pass `definition_id(name)` as the `ItemSoundResolver` when binding item callbacks. It searches current sound slots 1 through 511 and returns zero for an empty or missing name. Public queries capture their input bytes for the call, use the same case comparison as raw `43F490`, and leave the arena list and generation unchanged. A NUL ends the query. Nonempty queries accept up to 4095 bytes.

Call `frame` at the mission scheduler site. Call `publish` and `advance_frames` with actual elapsed sample frames. The software voice owner stays enabled and opens no output device. Supply the async `completed` host when a completion message is reached. Route `4A2712` to the current playback disable cell. Use source `4A2AED` to reset the same voice queues and filters while control cells remain current.

The required `service` provider handles reached mission script loading and registration (`455C90`, `456070`, `456080`, `456090`), diagnostics, loose-file loading, and archive name remapping. Unknown services raise an error when no provider is bound. The normal PFF file path, allocation, bank/cache calls, channel reservation, extension helper, and playback run inside this owner.

Call `cleanup()` for source `43E6D0` while the script, playback and frame owners remain alive. Route reached `49DB20` to `release_banks(context)`. Bank teardown uses its own current argument and 16-byte frame inside the same memory owner.

Each active request has a separate stable scratch block. Keep the owner and all referenced data alive through callbacks. The original comparison covers ambient callbacks, reused token storage, actual `LOOP.DEF`, lazy WAV reuse/load/failure, channel writes, extension aliases, and the complete resolved definition table. The stock test uses actual `SNIPER.PWF` and sound `499` with zero device output.

Validate every byte address before a width-two or width-four memory store commits any byte. Valid stores can cross current bound fields, including listener and yaw. File copies write each byte through the current source owner. Run `native_mission_sound_runtime GAME_FOLDER` for rejected-store prefixes, controlled file bytes across live fields, and repeated public lookup checks against the current heap list and generation.

The virtual height view starts at `15000000`. The main arena starts at `28000000`; keep `23000000..27FFFFFF` for DF socket frames, lookup data and the default work heap. Source `401050` supplies its 24 MiB budget. The source header list controls live allocations, alignment, labels, release callbacks and hole reuse. Released bytes remain mapped. The host supplies zero bytes for untouched arena storage; source allocation keeps current payload bytes. Keep source DLL image storage at `10000000` separate from terrain storage. These addresses are host memory rules.
