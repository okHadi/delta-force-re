# Mine movement

Use `mission_object_mine_move` for `451DD0..451E66`. Bind each read and store
to the current physical actor. The body moves position and updates the sway
angle and direction. Arithmetic wraps at 32 bits. Comparisons are signed.

Register the secondary callback with `bind_mission_object_mine_move`. Keep
the shared actor owner alive. Each callback requires a counted actor within
its physical pool. Original checks compare every actor byte and ordered
access, including current speed changes and stopped read and store paths.
