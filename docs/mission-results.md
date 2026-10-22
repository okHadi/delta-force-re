# Mission results

Use `mission_profile_leave` for the full `401E70` exit stage. Bind current
cells, the result image, the current profile, the x87 control word, and the
end-event stage. Keep profile buffers alive through the call.

`mission_profile_record` ports `407DE0`. It copies the result image, writes
completion and single-player totals on success, and updates multiplayer totals
and team wins. Statistic conversion keeps x87 precision, rounding, signed
truncation, and the integer overflow result.

`mission_profile_settings` ports `407980`. It saves current settings and the
current campaign mask. It captures the profile before the final settings store.
Supply campaigns 1..5, mission slots 0..15, and result rows 0..16.

Use `apply_mission_result` for explicit completion and mask updates. Use
`debrief_won`, `campaign_complete`, and `mission_available` for campaign checks.
Use `debrief_text_request` for the campaign table, briefing group, and Win/Lose key.

Run `native_mission_results` for rules and bounds.
Run `check_mission_results_original.py GAME --core CORE --report FILE` for
campaign and text checks. Run `check_mission_profile_original.py GAME PROBE
--report FILE` for full profile images, ordered stores, and original x87 checks.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
