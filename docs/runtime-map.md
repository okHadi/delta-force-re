# Original runtime map

Source: `Df.exe`, SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Addresses are original virtual addresses. Role names below describe observed calls; they are not original symbols.

## Startup

| Address / observed body | Role |
|---|---|
| `4A1258–4A125C` | Entry jump to `4BD7D0` |
| `4BD7D0–4BD87F` | Watcom runtime setup; calls `490E60` |
| `490E60–490FAD`, `490FC3–490FD8` | WinMain: register window, set game defaults, create window, start timer, launch game, stop timer, close window |
| `490460–4904D6` | Register `NLGAMECLASS`; window procedure `48FEC0` |
| `401050–401139` | Game defaults, version `1.0.3.3P`, memory setup |
| `4904E0–490691` | Create centered 640×480 window; store HWND at `CC087C` |
| `4908E0–490931` | Input/timer state setup |
| `490750–490872` | WinMM timer setup; configured interval 50 ms |
| `401860–4018A4` | Initialize game through `401490`, run first state through `461270`, save config, clean up |
| `401490–401831` | Input, audio, graphics, archives, strings, game systems, display modes |

`401490` opens `dfbase.pff` and `df.pff` through `48FC10`. Archive handles are `CC0850` and `CC0854`. It loads `dferror.bin`, `dfmenu.bin`, `dfgame.bin`, `loop.def`, and `items.def` before state dispatch.

`495DE0` calls DirectDrawCreate, queries DirectDraw2, and sets default bit depth 8. `496510(640,480,1)` selects the menu display mode. `496C50` presents all rows through `496C70`, which calls the selected backend pointer at `D0924C`.

Evidence: `.local/reports/startup-{entry,crt,winmain,game-init-config,game-launch,game-init,window-register,window-create,directdraw-create,archive-open,present-backend}.json`.

## State loop

`461270–46133F` runs a state table. The table contains a 32-byte name followed by four function pointers:

| Offset | Function |
|---|---|
| `20` | Enter |
| `24` | Update; nonzero ends this state |
| `28` | Render |
| `2C` | Leave |

Current state: `8EE1D0`. Next state: `8EE1D4`. `461340` sets next state; `461350` reads it. The loop starts with a 16 ms accumulator and carries its remainder across states. Updates consume 16 ms each; rendering follows completed updates. It yields with Sleep(0) while less than 16 ms is available.

| State table | Enter | Update | Render | Leave |
|---|---|---|---|---|
| `4CFBC0`, New Soldier | `408260` | `4083F0` | `408550` | `4086A0` |
| `4D3510`, Start Screen | `413A70` | `413AF0` | `413B60` | `413BE0` |
| `4CF2EC`, Main Menu | `405FB0` | `4060A0` | `406220` | `406340` |
| `4CECE0`, Game Loop | `403A60` | `404340` | `404B00` | `404150` |

`401860` starts New Soldier. Its first entry requests Start Screen through `461340`.

Evidence: `startup-menu-start.json`, `startup-transition-state.json`, `startup-*-state*.json`, and `startup-callbacks-ghidra.json` in `.local/reports/`.

## Menu data and drawing

`DFMENU.BIN` is an RTXT string table. `DF.MNU` supplies menu layout, images, fonts, buttons, and action names.

`40D550` opens `df.mnu`, then parses colors (`40D470`), fonts (`40D0C0`), images (`40CDF0`), buttons (`40C790`), menus (`40C050`), and button links (`40BD70`).

Main Menu enter `405FB0` selects `menu1` through `40BFD0`. Its background is `ImageTemplate` (`df_bkgd.pcx`); `ImageMainPlt` names `main_plt.pcx`. Menu lookup searches the list at `68B8E4`; `40BCA0` sets active menu `68B8E0`.

Main Menu render `406220` pumps messages (`490880`), draws the active menu (`40FA90`), draws extra content (`4062A0`), and presents (`496C50`).

`40FA90` expands a changed background through `40E5B0` into buffer `4D193C`, copies it through `40E760`, and draws each attached button through `40F660`. Image loader `40BA40` calls `475280` for PCX pixels and palette. It stores byte indices and a 256-entry, four-byte color table.

Evidence: `.local/reports/startup-menu-{prepare,lookup,activate,draw,background-load,background-draw,image-load,button-draw}.json` and `startup-callbacks-ghidra.json`.

## Menu input

`40D9E0` pumps Windows messages and consumes keyboard events from `498B20`. Enter calls the accept button at `4D1900`; Escape calls the back button at `4D1908`. Arrow keys use active-menu fields `2C/30/34/38`. Keyboard activation calls the button handler at offset `0C` with action 1, then action 2.

Mouse input uses `40DD80 → 40DD20 → 40DEF0`. Mouse event 2 gives action 1; event 4 gives action 2; hover gives action 6; leaving gives action 7. Hit test `40DE60` uses button x/y at `54/58`, width/height at `6C/70`, and the attached-button list. State 0 or 4 blocks button activation.

Main New Soldier action `4058B0` uses action 1 for pressed state 3, action 2 to request state `4CFBC0`, action 6 for hover state 2, and action 7 for normal state 1. Action names map to original function pointers in 36-byte records: name[32], function pointer.

Evidence: `.local/reports/startup-menu-{input,mouse-handler,mouse-dispatch,button-input,hit-test}.json`, `startup-main-actions-bytes.json`, `startup-main-newsoldier-action.json`.

## Font atlas

`40CF40` loads a font through `44BBA0`. `44BB50` allocates 256 eight-byte glyph entries; `44BAC0` scans row bands and calls `44B9A0` for columns.

Each row reserves 16 glyph slots. Palette index 1 in the first column ends a band; the next band begins two rows later. Ink columns contain index 3 or an index of at least 8. Index 3 becomes transparent; index 2 sets the baseline and becomes transparent.

Glyph entry: x:u16, width:u16, height:u8, valid:u8, y:u16. `44C190` draws nonzero pixels in the requested color and advances by width+1. Font flag 1 selects palette-based intensity blending. `411A30` handles inline `{C:...}` color names before glyph drawing.

Evidence: `.local/reports/startup-font-{load,file-load,glyph-table,glyph-scan-real,glyph-row-scan,glyph-draw,text-draw}.json`.

## Mission and world frame

Game enter `403A60–404141` calls mission loader `449C70(67F438, [503238])`. It then calls `402EC0`, `4633F0`, terrain setup `47A7A0`, world setup `47B590(8CF95C)`, and later model check `486B80`. Terrain field details are in `terrain-notes.md`.

Game update `404340–404532` pumps messages, advances tick `67DBF4`, updates game systems, and selects a next state. Game render `404B00–404B91` calls scene frame `404640`; a dedicated-server branch calls `440870` instead.

`404640–404AEF` locks the frame through `404540`, updates the camera through `42A6E0`, copies camera state `6BA3F4–6BA408`, builds view data through `44A820`, and calls world rendering `485610`. It then draws extra scene/HUD paths and ends through `404610`. Their precise roles require separate checks.

Evidence: `.local/reports/startup-callbacks-ghidra.json`, `startup-bms-loader-function.json`, `startup-world-frame.json`.

## Calling conventions and reconstruction limits

WinMain `490E60` and window procedure `48FEC0` use four stack arguments and `RET 10h`: stdcall. Internal `401860`, `401490`, `490460`, `4904E0`, `48FC10`, and `461270` use stack arguments, caller stack cleanup, and plain RET: cdecl at these boundaries. State callbacks receive no pushed arguments; update results use EAX. Other Watcom helpers can use register arguments; check each call site.

`401050` makes its code range writable with VirtualProtect(..., PAGE_EXECUTE_READWRITE). The reason and later code writes remain unverified. Indirect state, menu, and DirectDraw calls need explicit targets for a full rebuild. No complete whole-program lifting result is established.

REA/Ghidra supplied the main dossiers. Callback bodies and window procedure in `startup-callbacks-ghidra.json` use direct Ghidra 12.1.4 after defining function entries from the original state tables. The original file is unchanged. Decompiled code is an analysis result, not original source; assembly is retained for checks.
