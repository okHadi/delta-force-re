# Current actor creation

`mission_actor_create` runs `45D520`. Bind current mode, night flag, definition table, group-three actor memory and high-water cell `8CFA18`.

Use one canonical actor pool at `7CCA80`, with 256 physical records of `114` hex bytes. Supply current source pointers for definitions, characters and people.

Bind definition lookup `4828C0`, clear `49A620`, character allocation `42CCE0`, and person allocation `44A0C0`. Keep all reached callbacks required. Keep partially written records current when a callback fails.

The factory returns zero when all 256 actor slots are occupied. Character and person results are stored at actor `+18` and `+0C`.
