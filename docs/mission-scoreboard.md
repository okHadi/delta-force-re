# Mission scoreboard

`mission_scoreboard_receive` runs message `15` from `46D9F0`. Map the caller frame through `6B` and set its packet pointer at `+44`.

Use current actor and person records for names. Bind `404CF0` for the localized client name. Keep packet bytes, frame locals and scoreboard cells in shared memory through each call.

The receiver writes the current count, names, signed scores, actor indices, sides and three team pairs. Each reached read and write needs a valid current owner.

The original comparison covers `46EBC7` through its branch to `46DAA3`. Localization uses an explicit mutable test provider. The native test joins the live message stage to canonical actor and person names.
