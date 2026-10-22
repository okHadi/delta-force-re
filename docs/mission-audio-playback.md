# Mission audio playback

Use one `MissionAudioPlaybackCells` owner and stable bindings for the mission. Share `enabled_d08940` with the sound bank. Keep it enabled during muted runs. The playback class opens no audio device.

`initialize` owns the source queue and filter-cell initialization. `play`, `play_channel`, and `start_ambient` return the raw generation value. The value can wrap to zero. Keep the full value in the caller's source cell.

The sample provider returns a current writable sample record and its source address. Keep the bytes stable during the call. PCM and static-byte providers are read views. Bind foreign completion calls when a registered address can be reached. The callback receives the address, token, and sample address. A host that binds `4BCE20` must post the source completion message through its message queue.

`publish` owns pending record publication and the current code operands. `advance_frames` owns the queue, position, completion, and PCM-read part of `4A35BC`. Supply real elapsed sample frames. Zero frames follows the source one-frame loop rule. PCM accumulation and output filters require their own owner.

The records retain source byte widths, stale trailer bytes, signed comparisons, and wrapping arithmetic. Initialization preserves the generation, master volume, registered completion address, and compiled operands.

Run the original comparison:

```sh
.local/tools/python/bin/python tools/check_mission_audio_playback_original.py .local/reference-game/Df.exe build/mission_audio_playback_probe --report playback-report.json
```
