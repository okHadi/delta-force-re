# Audio mixer helpers

Target: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Use `audio_mixer` for record inspection, voice selection, start fields, sample conversion, and kernel lookup. Selection takes the first free slot, then the first eligible slot with a lower signed volume.

`advance_voice` inspects one original code block with its unpatched operands. Use `MissionAudioPlayback::advance_frames` for live queue progress. Supply elapsed sample frames from the audio clock.

Kernel lookup uses the original shift bytes at `4A29E0` and code bytes at `4B5940`. The embedded windows contain 384 shift bytes and 512 code bytes. Check `in_window` before using a result. Out-of-window reads require the source image.

Run the original comparison:

```sh
.local/tools/python/bin/python tools/check_audio_mixer_original.py .local/reference-game/Df.exe build/audio_mixer_probe
```
