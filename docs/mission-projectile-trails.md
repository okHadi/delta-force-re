# Projectile trail records

Keep one `MissionProjectileTrailCells` owner for the shared 9E3098 ring and 4F9148 index. The ring holds 2048 records of eight 32-bit words. Start the process index at zero, as stored in the supplied executable.

Use `mission_projectile_trail_record` for 463350. Supply the old XYZ, new XYZ and color as seven current arguments. The body increments and masks the index, stores six coordinates, stores lifetime 30, then stores color. The return value reads the current index after all stores.

Use `mission_projectile_trail_bindings` for the shared owner. Keep argument storage valid through all synchronous reads and writes. Custom bindings can change later arguments and the current index at those boundaries.

Use `mission_projectile_trail_clear` for 4633C0. It clears the first 512 records and preserves the index and remaining records. Keep this owner alive across mission changes.

Bind the projectile move effect to this producer. The renderer can read the same records through its own live join.
