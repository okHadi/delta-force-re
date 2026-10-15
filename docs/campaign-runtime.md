# Campaign selection

Use `DFMENU.BIN` for campaign names, `DFCAMPS.BIN` for overviews, and `DFCAMP02.BIN` through `DFCAMP06.BIN` for mission names. The five campaign indices are 1–5. Their mission counts are 6, 6, 8, 10, 10.

Read the selected campaign at profile `28`, and its mission index at `470 + campaign*4`. Read completion words at `30 + campaign*44 + mission*4`, and the prerequisite completion mask at `70 + campaign*44`. Offsets and strides are hexadecimal.

Mission header word `7C` gives prerequisite bits. Show a mission when all required bits are present. Header word `78` holds equipment restriction bits. Original `40B410` selects the last clear bit in each equipment group and maps its index to the original weapon ID table.

Campaign state table `4D0CD4` uses enter/update/render/leave at `40AC70/40AF90/40B010/40B0B0`. Callback records use name[32], then the function address.

| Handler | Address | Action |
| --- | --- | --- |
| `mnu_camp_select` | `40B770` | Select campaign on action 1 |
| `mnu_camp_map` | `40B220` | Select available map point; test repeated clicks |
| `mnu_camp_accept` | `40B410` | Apply equipment limits; request `4D3DEC` |
| `mnu_camp_back` | `40B660` | Request Main Menu `4CF2EC` in single player |
| `mnu_camp_exit` | `40B0F0` | Request End Menu `4E6784` |
| `mnu_camp_textbox` | `40B3A0` | Draw the transparent overview panel |

Map centers are numeric facts from `4D0AF4`: campaign stride `50`, mission stride 8. Map hit boxes include their edges and extend 10 pixels from each center. A click on the selected mission accepts when fewer than 20 updates have passed since the previous click. Enter clears both click counters.

Draw `ImageCampMapN` at the DF.MNU map button position. Draw available mission dots and centered LCD7 names with original `40A620` rules. Draw overview text with LCD10, spaces and `~` line breaks. The current paragraph renderer supports those operations; other rich-text commands require their own original parser evidence.

State `4D3DEC` selects `MissionMenu` through `413E10`. It shows the mission briefing and tactical map. Its serialized table label is `Options2 Menu`; use the observed menu selection for its function.

`campaign_probe GAME_FOLDER SAVE_FOLDER [OUTPUT.ppm]` loads all 40 missions and renders the selected campaign. It does not save profiles. Synthetic checks cover unlock masks, profile selection, equipment lookup, and retention of unknown profile fields. Native output has no original-frame pixel comparison yet.

## Mission briefing

`MissionSetupMenu::enter(selection)` selects the original `MissionMenu`. Send pointer positions, menu actions, and updates to the controller. `draw()` returns the 640×480 frame. Accept returns `4E6878` with the selected mission; Back returns `4D0CD4`; Loadout returns `4ECF88` with the mission.

Expand `{ADD}` and `{CV:bit}` references from the mission RTXT group. Preserve byte text and link word indices. Original `414520` replay matches all 40 briefings with both zero and full completion masks: 80 cases.

Read tactical icons from BMS record byte `4C`, team byte `43`, and flags word `0C`. Skip flags `50`. Type 6038 in object group 2 sets the initial map center. Read map link coordinates from RTXT records. The native screen supports mission text, clickable map links, scroll arrows, map arrows, and state changes. Route line pixels, animated route icons, and slider dragging need further original-code checks. Pixel equality with the original briefing frame is not yet checked.

## Slider drawing

Original `40C350` keeps the script width for type 2 and script height for type 3. Type 1 uses one quarter of the image height; type 6 uses the full image size. Original `40F660` draws slider images as opaque pixels.

Use the stored binary64 range/extent ratio, then reproduce x87 truncation for the signed position. Replay of original `40F660` matches 6,845 cases, with positive and negative ratios and positions. The source SHA-256 is `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

The game window uses these campaign and briefing controllers. Run `game_flow_probe GAME_FOLDER` for a silent check of Soldier → Main → Campaign → Briefing → Campaign → Main. It uses the real game controller, simulated window input, real assets, and temporary saves. It checks six displayed frames and nine separate profile files. Window display and OS input use the platform tests.
