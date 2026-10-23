# Sound definitions

Bind `43E340` to `mission_sound_definitions_initialize`. Bind its `43EDB0`
call to the current ambient definition loader. Keep the 512 records at
`4F2EA4` in current shared sound storage. Each record has 40 bytes.

The sort keeps record zero in place. It uses signed 16-bit IDs and swaps
all ten words of each selected record. Reads and stores follow source order.

Run `check_mission_sound_definitions_original.py GAME PROBE --report FILE`.
The comparison uses a declared ambient loader fixture.

Source `Df.exe` SHA-256:
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
