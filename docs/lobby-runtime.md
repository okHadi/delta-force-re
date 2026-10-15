# Multiplayer settings

Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`MultiplayerSettings` keeps the original `5C4`-byte record at `4CED20`.
Field offsets are checked at compile time. `multiplayer_defaults` follows
`407120`; `multiplayer_config_lines` follows the multiplayer fields written
by `407360`. Text helpers keep each fixed field within its capacity.

Default values include service 4, 16 players, 20 lives, 20 kills, team
requirement 1, attributes 2, time and hill limits 30, ammo respawn 2, GPS
icons 1, verbose status 1, and address `0.0.0.0`.

The writer returns 24 multiplayer lines and four shared system lines.
The caller selects the save file and writes the returned text.

Run `native_lobby` for layout, defaults, text bounds, and output checks.
Run `tools/check_lobby_original.py GAME_EXE LOBBY_PROBE` to compare the
record and writer lines with the original x86. The 27-case check replaces
file and config calls with test hooks. It opens no files or sockets.

Original menu entry points include status `416500`, sessions `416670`,
host `416F10`, join `416F80`, search `4173B0`, and host accept `418CA0`.
