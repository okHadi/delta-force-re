# Audio data

Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`convert_wav_record` follows `494C50`. Supply a canonical 44-byte WAV header
and PCM bytes. The function moves PCM to offset `10` and writes the sample
record: tag at `00`, length at `04`, rate step at `08`, volume at `0C`.
The rate step is `(rate * 16 / 22050) << 24` with original integer rules.
A matching tag identifies an existing record.

8-bit PCM stays unchanged. The 16-bit path uses original peak normalization
and low-byte conversion. All-zero 16-bit input produces unsigned silence
`80`. The 47 WAV files checked in `Dfbase.pff` use mono 8-bit PCM.

`compute_voice_gain` follows `4B4D3A` and uses the 512-byte pan table at
`4B5639`. Pan wraps to one byte; shift counts wrap to five bits. Effect
volume clamps to `0..255` (`422BD0`); master volume uses the unsigned upper
limit (`4947E0`). These functions process data without opening a device.

Run `native_audio` for fixed cases. Run `tools/check_audio_original.py
GAME_EXE AUDIO_PROBE --archive DFBASE_PFF --manifest MANIFEST` for original
x86 comparisons. The checked set includes 47 real WAVs, 10 synthetic WAVs,
repeat conversion, 8,505 gain cases, volume bounds, and all pan-table bytes.
The play-wrapper check captures driver arguments with sample lookup and
driver calls replaced by test hooks.
