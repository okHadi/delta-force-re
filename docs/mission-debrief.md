# Debrief state

Use `mission_debrief_enter/update/render/leave` for original `41F390/41F510/41F570/41F5E0`. Use `mission_debrief_accept` and `mission_debrief_quit` for `41F700/420E50`. Supply current memory and synchronous required services through `MissionDebriefBindings`.

Keep the result image, selected profile, menu, campaign RTXT, text allocation and display cells alive through every callback. Map a stable caller-owned 64h-byte entry frame. The entry copies current result bytes from `67F714` to `4E7544` in source DWORD order. It replaces campaign RTXT `503238`, allocates text `68BEF0`, runs current campaign/profile/text services, and sets the source text bounds and current campaign/mission fields.

Route `41EF40`, `41F320`, `41EF80`, and text descriptor services `413440/413460/4132F0/4133C0` to their current owners. Bind all reached formatting, allocator, RTXT, profile-save, display and menu services. Missing providers throw. Keep indirect display function pointers current. A failed service leaves earlier effects committed.

Leave frees the captured current text pointer and clears `68BEF0`. Keep campaign RTXT `503238` alive until its next source release. The Game transition owner retains final mission results before mission teardown. This module owns no persistent profile, result, menu or asset storage.

Use existing `DebfMenu` assets, fonts, labels and positions. Accept action 2 requests Campaign `4D0CD4`; action 8 calls action 2. Quit action 1 or 8 requests End `4E6784`. Preserve the source button state changes and Quit action 1 service `40B9A0`. The supplied menu maps Enter and Escape to Accept. Apply the emil-design-eng skill when binding the Game UI.

Run `native_mission_debrief` for current-memory mutation, partial failures and StateLoop routing. Supply the game folder as its first argument for actual DebfMenu asset and Enter/Escape/Exit-key checks. These checks use explicit required text/display fixtures; the raw text builder and descriptor services require their own proofs. Run `check_mission_debrief_original.py GAME PROBE --report FILE` for all six original bodies, ordered effects and callback mutations. Keep audio device output at zero.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
