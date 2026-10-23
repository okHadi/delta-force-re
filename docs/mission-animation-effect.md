# Animation effect

`mission_animation_effect` runs 45E0F0. Keep its argument array current through callbacks: unused flag, step flag, raw actor pointer.

Supply current reads of 1, 4 and 8 bytes. The PE double at 4CCB2C is -52428.8. The function reads water before actor positions. Wet effects call 45DC70 with the first water level, then read the current step flag and water for 43E7D0.

Dry effects use the current COLOR grid and 256-byte terrain type table. Bind 43E7D0 to the real positional sound owner. Supply each reached visual or sound body.
