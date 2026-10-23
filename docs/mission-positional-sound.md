# Positional sound

`mission_sound_positional_request` ports `43E7D0`. Use the same `MissionSoundBankBindings` owner as global sounds, the sound cache, and playback. The function calls the real sound bank resolver.

Map `6E06A8`, `6E06AC`, `6E06B0`, and `6E06B4` to the existing `MissionAudioFrameCells` listener references. Map `BD5284` to the current terrain height-byte source handle. Read samples from that handle plus `Y * 1024 + X`. The coordinates wrap to ten bits per axis. Each sample reloads the pointer and reads one byte.

Pass a separate 32-byte parameter block for each active request. Keep it alive through all cache and voice callbacks. The block holds rate, pan, volume, channel, zero, and adjusted distance. Later bank callbacks read and write the same block and shared scratch.

`mission_sound_occlusion` ports `43E940`. It uses the source signed divisions, wrapped products, endpoint order, and height sum. Supply finite signed coordinate traversals. A signed loop wrap raises an error.

Keep reached file, allocator, cache, and playback services bound. Keep the sound engine enabled and mute device output at the sink.

Run the source comparison:

```sh
.local/tools/python/bin/python tools/check_mission_positional_sound_original.py .local/reference-game build/mission_positional_sound_probe --report positional-sound-report.json
```
