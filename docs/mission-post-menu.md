# Post Menu

Use the four `mission_post_menu` functions for source state `4EB834`: enter `424540`, update `4245B0`, render `424730`, and leave `424780`. Keep current menu, mode, status and display cells in one memory owner. Bind every reached foreign service.

Update returns one after a complete route. Status two requests shutdown. Status five reads current RTXT text, shows the required error, then requests shutdown. Status one selects Debrief in single player and the current host/client menu in multiplayer. Other routes use current host and game-mode cells.

Send `461340` requests to the state loop. Keep the selected target through required cleanup. Process state leave before the next state enters. Unknown destinations need their actual handler.

Entry retains the loaded handle for its first call and reads the current handle for the next call. Render reads the display gate again after drawing. Required callback errors preserve completed effects.

The original checker compares all four source bodies, ordered reads, stores, calls, current-cell changes and failure state. Foreign services use explicit fixtures. The native test checks route branches, captured arguments, current display gates and the existing `StateLoop` transition order. These checks cover the service module; Game owns the final result lifetime and actual Debrief handler.
