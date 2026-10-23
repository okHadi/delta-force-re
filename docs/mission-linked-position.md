# Linked actor position

Call `bind_mission_linked_position(state)` after mission setup. Keep `state`
alive while its world bindings are in use.

Actor `+90` stores a physical source address. The binding writes only target
X, Y, and Z. It accepts inactive rows. The current actor tables must contain
the target row. Groups 0–3 have physical limits of 1024, 1024, 512, and 256 rows.

The original stores are at `487C24..487C3F`. X uses the initial link. Y and Z
read the owner link again. Aligned actor position stores preserve that link.
A self-link uses the same current actor record.

Run `native_mission_linked_position` for pickup and full actor-update checks.
Run `tools/check_mission_linked_position_original.py GAME --probe PROBE` for
full-record comparisons with the supplied executable. The checker also uses
an explicit memory-store fixture to check the original link read order.
