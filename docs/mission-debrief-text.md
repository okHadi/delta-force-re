# Debrief text

Use `mission_debrief_campaign_audio`, `mission_debrief_won_audio`, and `mission_debrief_text` for original `41EF40`, `41F320`, and `41EF80`. Reuse `MissionDebriefBindings` with current memory and synchronous required calls.

Keep one selected profile, result image, campaign/general RTXT owner and text allocation. Bind the current campaign count table at `4D0D08`. The campaign wrapper rereads the profile and count for each completion word. The won wrapper runs the source parameter call before its late profile read when result mode is zero, then publishes current `4E6CFC` before the final parameter call. Supply `4C4F40` through `mission_parameter_set`; missing providers throw.

Map a caller-owned `128h`-byte frame. It contains a 100-byte key buffer, three 64-byte buffers, and the current scratch pointer at `+124h`. Keep that frame stable through every callback. The text body uses current `503238` campaign RTXT, `503234` general RTXT, copied result row/kills, and current destination `68BEF0`. Route `49A092` through authoritative `mission_rtxt_lookup`.

Supply `493A80` as an explicit required formatting service. The body uses the source Win/Lose/Briefing, error and localized statistics formats. Preserve source byte copies, current pointer reads, one-pass `{ADD}` scanning and the unconditional skip after its token delimiter. Terminal tokens read one byte after their NUL. Providers enforce mapped frame, scratch and destination bounds; earlier writes remain committed on failure.

The final source, destination and free-pointer reads occur separately. Keep their current aliases visible. The body frees its captured current scratch pointer and leaves persistent text/profile/RTXT ownership with the caller.

Run `native_mission_debrief_text` for current profile/parameter and required-service error checks. Supply the game folder for all 80 actual Win/Lose texts, 18 terminal `{ADD}` references, signed statistics, and frozen Debrief entry→helper→RTXT→leave joins. Formatting, parameter, menu and display operations in this test are declared fixtures. Use `native_mission_parameter` for the exact parameter/format/Text/RTXT join. The real RTXT, heap and profile owners are shared. Audio device output stays zero.

Run `check_mission_debrief_text_original.py GAME PROBE --report FILE` for ordered original reads/stores/calls, full images, current pointer mutations and raw-bound error prefixes. Its closed format, RTXT-entry and parameter fixtures define the foreign boundary; they require their own production bindings.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
