# Mission sound banks

Use `mission_sound_global_request` for `43E720`. Keep one caller-owned 32-byte parameter block per active request. Supply an ID from the 512-record sound table. The request reads the bank pointer at `4F2EA8 + id*0x28`, writes the source parameters, and selects context `6E0518` or `6E05E0` from definition flag mask 4.

Use `mission_sound_bank_play` for `49D5A0`. Bind `read` and `write` to current shared bytes with widths 1, 2, and 4. Keep bank, variant, asset, parameter, context, and scratch handles stable through callbacks. The resolver reads callback changes before later source operations.

Share RNG cell `5014FC`, filter cell `501500`, and the 256 DWORD attenuation coefficients at `501504`. Keep the eight 20-byte candidate records at `D099C0`. A null bank returns before scratch writes. Use valid round-robin indices and at most eight produced candidates. Boundary errors retain earlier writes.

Supply each reached foreign body through `call`:

| Site | Arguments |
| --- | --- |
| `499120` | Current context address + `3C`; real file open |
| `49D050` | Cached asset handle, zero |
| `499020` | None; close current input |
| `498230` | Current asset allocation, current context tag |
| `494DD0` | Current sample, pan, volume, rate |
| `494E80` | Current channel, sample, pan, volume, rate |

The global request uses automatic channel `FFFFFFFF`. Generic bank playback reads the current channel for each candidate. Keep voice state active with a muted output sink. This module opens no audio device.

`mission_audio_cache_load`, `mission_audio_cache_close`, and `mission_audio_cache_tag` own the corresponding cache calls. The playback owner supplies automatic and fixed-channel voice calls. The bank loader supplies current PWF records and global sound definitions.

Run `native_mission_sound_bank` for aliases, required bodies, boundary errors, and full DWORD attenuation. Run `check_mission_sound_bank_original.py GAME PROBE --report FILE` for exact original request, resolver, RNG, filter, and attenuation instructions. The check compares all supplied shared bytes and ordered foreign effects. Loader and playback effects are explicit fixtures.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Retained REA reports: `.local/reports/mission-global-sound-source.json`, `mission-global-sound-nested-source.json`, and `mission-audio-helpers-source.json`.
