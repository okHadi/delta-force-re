# Mission animation

Keep one `MissionAnimations` object per loaded mission. Keep its asset cache
alive. Each instance owns a separate controller and uses its definition's CHR
and ANM resources.

Bind actor instance handles to `controller()` and `actions()` for updates.
Use `pose()` as the renderer's live pose source.
