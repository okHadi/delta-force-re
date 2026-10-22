# Mission audio frame

Run `mission_audio_frame(cells, bindings)` at stage `43F0B0`. Keep the shared cells and providers alive. Bind listener position and yaw to `6E06A8` and `6E06B4`.

Keep the 1024 registrations, 64 candidates, six selected records, and six channel keys and handles in the shared cells. Actor references point to the current mission records. Supply actor activity at `+0`, current rate at `+20`, and current position at `+24..+2C`. Supply loaded sound definition words at `+18` and `+1C`, and volume bytes at `+24` and `+25`.

The scheduler runs each eighth frame. It clears inactive registrations, accepts the first 64 records within range, and selects up to six sounds. It uses original `477670` length, `477770` angle, and `49DC20` attenuation rules.

Supply synchronous backend update `494FA0`, stop `495070`, and start `494F40` providers. Keep backend handles and voice state current while output is muted. The scheduler opens no audio device and changes no system volume. Backend changes and errors remain visible in shared cells.

Registration creation, sample loading, backend voice lifetime, and device output belong to their providers. A missing reached provider throws. Invalid attenuation division inputs throw at the original divide boundary.

Run `native_mission_audio_frame` for provider and error checks. Run `check_mission_audio_frame_original.py GAME PROBE --report FILE` for the original scheduler, selection, channel state, and ordered backend calls. The comparison uses muted test providers for the three backend calls.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
