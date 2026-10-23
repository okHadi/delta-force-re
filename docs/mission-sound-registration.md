# Attached sound

Use `mission_sound_attach` for `43EF80` and `mission_sound_loudness` for
`43F4D0`. Bind `6E0700` to the current 1024-row registration array in
`MissionAudioFrameCells`. Keep identity counter `4F7EA4` in shared storage.

Each row has 16 bytes. The first row with actor zero receives the current
definition, actor, identity, scale byte, and ceiling byte. Actor `+1C` receives
the raw row address. Each completed registration advances the identity.

Bind `43EE80` to the current ambient sample loader. Read the current definition
again after that call. Preserve earlier stores when a later actor store fails.
Keep audio output muted.

Run `check_mission_sound_registration_original.py GAME PROBE --report FILE`.
The comparison uses a declared ambient loader fixture and original fault cases.

Source `Df.exe` SHA-256:
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
